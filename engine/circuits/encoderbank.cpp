// encoderbank — a bank of up to 8 virtual knobs from E4/DB8E encoders.
// Spec: manual/circuits/encoderbank.md. Shares the whole virtual-value engine
// with `encoder` (engine/src/encodercore.hpp); per encoderbank.md it drops only
// the encoder-only features: movedup/moveddown/valuechanged triggers, the
// override input, and sharewithnext.
//
// The number of encoders is NOT a parameter: it is the highest output<N> /
// led<N> / button<N> jack used (encoderbank.md "Defining the number of
// encoders"). The bank uses that many physical encoders in chain order starting
// from `firstencoder` (register handle or bare global number, resolved once at
// load, exactly like `encoder`). All shape parameters (mode, discrete, sensivity,
// notch, snapto, smooth, ...) are shared across the whole bank.
//
// Conventions / SPEC-GAPs: identical to `encoder` for everything shared — see
// encoder.cpp / encodercore.hpp (value-engine mappings, discrete = raw integers,
// panel-only LED ring / led input / color / display / header, snapto/smooth/
// autozoom curves). encoderbank-specific:
//   * count: the max connected index among output/led/button (>=1..8). An
//     out-of-range firstencoder+i leaves that lane inert (output frozen at
//     startvalue, button 0).
//   * select/selectat gate the button<N> outputs AND (on hardware) the LED rings,
//     and gate whether the bank consumes physical movement; the value math and
//     output<N> keep running while deselected (manual: "it will nevertheless work
//     ... its outputs"). snapto still pulls while deselected.
//   * presets: 8 slots (preset 0..7), each holding all lanes' positions; three
//     standard modes; precedence clearall > clear > savepreset > loadpreset. SD
//     persistence is a headless no-op.
#include "../src/registry.hpp"
#include "../src/uihelpers.hpp"
#include "../src/encodercore.hpp"
#include <cmath>

namespace droid {

namespace ec = encodercore;

class EncoderBank : public Circuit {
    static constexpr int kMaxEnc = 8;
    static constexpr int kPresets = 8;

public:
    void tick(EngineState& s) override {
        if (!inited_) init(s);
        ec::Params p = readParams(s);
        float dt = s.secondsPerTick();
        bool selected = ui::isSelected(*this, s);

        handlePresets(s, p);

        for (int i = 0; i < count_; i++) {
            EncoderState* enc = s.controllers.encoder(firstGlobal_ + i);
            long detents = enc ? enc->pendingDetents : 0;
            bool pushed = enc ? enc->pushed : false;

            if (selected) state_[i].applyMovement(p, detents);
            state_[i].applySnap(p, dt);
            float emitted = state_[i].smoothStep(p, dt);
            if (enc) enc->ringDisplay = ec::clampf(state_[i].pos, 0.0f, 1.0f);  // panel-only

            out("output", i + 1).set(s, state_[i].output(p, emitted));
            out("button", i + 1).set(s, (selected && pushed) ? 1.0f : 0.0f);
        }
    }

    // Persisted: every lane's virtual position + all 8 preset banks + slot.
    // Serialized at the fixed maximum (8 lanes) for a version-stable length.
    int stateVersion() const override { return 1; }
    void saveState(StateWriter& w) const override {
        for (int i = 0; i < kMaxEnc; i++) w.f(state_[i].pos);
        for (int b = 0; b < kPresets; b++)
            for (int i = 0; i < kMaxEnc; i++) w.f(preset_[b][i]);
        w.n(prevPreset_);
    }
    void loadState(EngineState& s, int version, const std::vector<double>& in) override {
        if (version != 1 || in.size() != (size_t)(kMaxEnc + kPresets * kMaxEnc + 1)) return;
        if (!inited_) init(s);
        StateReader r{in};
        for (int i = 0; i < kMaxEnc; i++) state_[i].pos = (float)r.f();
        for (int b = 0; b < kPresets; b++)
            for (int i = 0; i < kMaxEnc; i++) preset_[b][i] = (float)r.f();
        prevPreset_ = (int)r.n();
        ec::Params p = readParams(s);
        for (int i = 0; i < kMaxEnc; i++) state_[i].smoothed = state_[i].logical(p);
    }

private:
    void init(EngineState& s) {
        count_ = encoderCount();
        firstGlobal_ = resolveFirst(s);
        ec::Params p = readParams(s);
        for (int i = 0; i < count_; i++) {
            state_[i].seed(p);
            for (int b = 0; b < kPresets; b++) preset_[b][i] = state_[i].pos;
        }
        prevPreset_ = ui::clampPreset(std::lround(in("preset").value(s)), kPresets - 1);
        inited_ = true;
    }

    // Bank size = highest connected output/led/button lane (manual). >=1 in any
    // real patch; 0 makes the circuit inert.
    int encoderCount() {
        int n = 0;
        for (int i = 1; i <= kMaxEnc; i++)
            if (out("output", i).connected() || out("button", i).connected() ||
                in("led", i).connected())
                n = i;
        return n;
    }

    int resolveFirst(EngineState& s) {
        const Operand& op = in("firstencoder").primary();
        if (in("firstencoder").connected() && op.kind == Operand::Kind::Register &&
            op.reg.type == 'E') {
            if (op.reg.ctrl == 0) return s.controllers.validateGlobal(op.reg.num);
            return s.controllers.globalIndexForReg(op.reg);
        }
        long g = std::lround(in("firstencoder").value(s));
        return s.controllers.validateGlobal((int)g);
    }

    ec::Params readParams(EngineState& s) {
        ec::Params p;
        p.mode = (int)std::lround(in("mode").value(s));
        int d = (int)std::lround(in("discrete").value(s));
        p.discrete = (d >= 2) ? d : 0;
        p.sensivity = in("sensivity").value(s);
        p.autozoom = in("autozoom").value(s);
        p.notch = in("notch").value(s);
        p.outputscale = in("outputscale").value(s);
        p.outputoffset = in("outputoffset").value(s);
        p.startvalue = in("startvalue").value(s);
        p.snapConnected = in("snapto").connected();
        p.snapto = in("snapto").value(s);
        p.snapforce = in("snapforce").value(s);
        p.smooth = in("smooth").value(s);
        return p;
    }

    void handlePresets(EngineState& s, const ec::Params& p) {
        bool clearAll = risingEdge(caPrev_, in("clearall").value(s));
        bool clr      = risingEdge(clPrev_, in("clear").value(s));
        bool loadPatched   = in("loadpreset").connected();
        bool savePatched   = in("savepreset").connected();
        bool presetPatched = in("preset").connected();
        bool immediate = presetPatched && !loadPatched && !savePatched;

        if (clearAll) {
            for (int i = 0; i < count_; i++) {
                state_[i].seed(p);
                for (int b = 0; b < kPresets; b++) preset_[b][i] = state_[i].pos;
            }
        } else if (clr) {
            for (int i = 0; i < count_; i++) state_[i].seed(p);
            if (immediate)
                for (int i = 0; i < count_; i++) preset_[prevPreset_][i] = state_[i].pos;
        }
        bool saveTrig = risingEdge(spPrev_, in("savepreset").value(s));
        bool loadTrig = risingEdge(lpPrev_, in("loadpreset").value(s));
        if (savePatched && saveTrig) {
            int b = ui::presetNum(*this, s, in("savepreset").value(s), presetPatched, kPresets - 1);
            for (int i = 0; i < count_; i++) preset_[b][i] = state_[i].pos;
        }
        if (loadPatched && loadTrig) {
            int b = ui::presetNum(*this, s, in("loadpreset").value(s), presetPatched, kPresets - 1);
            for (int i = 0; i < count_; i++) { state_[i].pos = preset_[b][i]; state_[i].smoothed = state_[i].logical(p); }
        }
        if (immediate) {
            int cur = ui::clampPreset(std::lround(in("preset").value(s)), kPresets - 1);
            if (cur != prevPreset_) {
                for (int i = 0; i < count_; i++) {
                    preset_[prevPreset_][i] = state_[i].pos;
                    state_[i].pos = preset_[cur][i];
                    state_[i].smoothed = state_[i].logical(p);
                }
                prevPreset_ = cur;
            }
        }
    }

    bool inited_ = false;
    int  count_ = 0;
    int  firstGlobal_ = 0;
    ec::State state_[kMaxEnc];
    float preset_[kPresets][kMaxEnc] = {};
    int  prevPreset_ = 0;
    bool caPrev_ = false, clPrev_ = false, spPrev_ = false, lpPrev_ = false;
};

DROID_REGISTER_CIRCUIT(encoderbank, EncoderBank)

} // namespace droid
