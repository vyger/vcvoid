// encoder — one virtual value driven by an E4/DB8E rotary encoder.
// Spec: manual/circuits/encoder.md. The value engine lives in
// engine/src/encodercore.hpp (shared with encoderbank); this file adds the
// encoder-only features: movement triggers, override, sharewithnext, the push
// button output, select/selectat overlay and 16 presets.
//
// Encoder handle: `encoder` is resolved ONCE at load (manual: "read just once
// ... making this parameter dynamic does not work"). An E<ctrl>.<num> register
// handle resolves through the controller chain; a bare number is a 1-based
// global encoder index. An unresolved handle leaves the circuit inert (no
// movement, button always 0). Physical movement is read from ControllerState's
// per-tick pendingDetents; several encoder circuits may share one physical knob
// (select overlay / sharewithnext) and all see the same detents before the
// Engine drains them at end of tick.
//
// Conventions / SPEC-GAPs (manual silent, qualitative, or panel-only):
//   * value engine mappings (sensivity/autozoom/notch/snapto/smooth) —
//     see encodercore.hpp. Only the linear sensivity cases are pinned by the
//     manual; autozoom/snapto/smooth curves are documented chosen mappings.
//   * discrete output: the manual states in two places (Inputs table + first
//     discrete example) that `discrete = N` yields the raw integers 0..N-1, so
//     output = index*outputscale + outputoffset. The octave example's code block
//     `outputscale = 4V` is a manual typo — index*1V + (-2V) is what yields the
//     stated -2V..+2V octave switch, and that is the value the golden uses.
//   * select/selectat gate the `button` output AND (on hardware) the LED ring;
//     they also gate whether THIS circuit consumes the physical movement (the
//     turn is routed to the currently selected function). snapto still pulls the
//     value while deselected (manual: "works if the encoder is not selected").
//     output/movedup/moveddown/valuechanged keep the value math running; the
//     movement triggers only fire while selected (physical turn belongs to the
//     selected function).
//   * override: display-only. The knob is ignored; the value comes from the
//     override input, clamped/rounded into the mode/discrete range (notch is not
//     applied to an override value). No movement triggers in override mode;
//     valuechanged still fires when the displayed value changes.
//   * movement triggers: movedup/moveddown emit a 10 ms trigger every
//     `movementticks` detents (96 per turn); to keep fast turns from merging,
//     triggers are queued with a >=10 ms gap (period 20 ms). valuechanged fires
//     when the virtual value changes (never while clamped at a limit; on the
//     snap-to-next in discrete mode). The exact fast-turn gap timing is a
//     SPEC-GAP simplification of the manual's queueing prose.
//   * LED ring / color / negativecolor / ledfill / led input / display / header
//     are panel-only (like pot's LED gauge): not rendered headless. The ring
//     position is stashed in ControllerState.ringDisplay for a future Rack
//     adapter but is not golden-observable.
//   * sharewithnext: suppresses this circuit's `output` (handed to the next
//     encoder circuit). The manual's "operate on the SAME virtual value across
//     the pair" coupling is NOT modelled headless — each circuit keeps its own
//     value (documented gap; verification is requires-human anyway).
//   * presets: 16 slots holding the raw virtual position, three standard modes
//     (triggered / immediate auto-save-old+load-new / value-carrying trigger);
//     precedence clearall > clear > savepreset > loadpreset. SD persistence is a
//     headless no-op; state/presets seed to startvalue at load.
#include "../src/registry.hpp"
#include "../src/uihelpers.hpp"
#include "../src/encodercore.hpp"
#include <cmath>

namespace droid {

namespace ec = encodercore;

class Encoder : public Circuit {
public:
    void tick(EngineState& s) override {
        if (!inited_) init(s);
        ec::Params p = readParams(s);
        float dt = s.secondsPerTick();
        bool selected = ui::isSelected(*this, s);
        bool overridden = in("override").connected();

        EncoderState* enc = s.controllers.encoder(encIdx_);
        long detents = enc ? enc->pendingDetents : 0;
        bool pushed = enc ? enc->pushed : false;

        long trigTicks = std::lround(0.01 * s.tickRateHz);
        if (trigTicks < 1) trigTicks = 1;

        // --- clear / clearall / presets (always run) ------------------------
        handlePresets(s, p);

        // --- value update ----------------------------------------------------
        float emitted, logicalNow;
        long moveDetents = 0;
        if (overridden) {
            ec::Params po = p; po.notch = 0.0f;   // override value is exact
            float v = in("override").value(s);
            state_.pos = overrideToPos(po, v);
            emitted = state_.smoothStep(po, dt);
            logicalNow = state_.logical(po);
            // knob ignored -> no movement triggers
            if (!(in("sharewithnext").value(s) >= kGateHighThreshold))
                out("output").set(s, state_.output(po, emitted));
        } else {
            if (selected) { state_.applyMovement(p, detents); moveDetents = detents; }
            state_.applySnap(p, dt);
            emitted = state_.smoothStep(p, dt);
            logicalNow = state_.logical(p);
            if (!(in("sharewithnext").value(s) >= kGateHighThreshold))
                out("output").set(s, state_.output(p, emitted));
        }
        // Ring display, select-gated like the button/LEDs (issue #15): on
        // hardware the ring belongs to the selected overlay circuit. mode 0
        // (non-discrete) keeps the LEDs off per the manual's mode table.
        if (selected && enc) {
            enc->ringDisplay = ec::clampf(state_.pos, 0.0f, 1.0f);  // legacy readback
            if (p.discrete >= 2 || p.mode != 0) {
                enc->ring.active = true;
                ec::ringDisplayValue(state_, p, enc->ring.bipolar, enc->ring.value);
                enc->ring.fill = std::lround(in("ledfill").value(s)) != 0;
                enc->ring.color = in("color").connected() ? in("color").value(s)
                                                          : ec::kDefaultRingColor;
                enc->ring.negColor = in("negativecolor").connected()
                    ? in("negativecolor").value(s) : enc->ring.color;
            }
            enc->ring.overlay = ec::clampf(in("led").value(s), 0.0f, 1.0f);
        }

        // --- button output (select-gated) -----------------------------------
        out("button").set(s, (selected && pushed) ? 1.0f : 0.0f);

        // --- movement triggers (movedup / moveddown), select-gated ----------
        long movTicks = std::lround(in("movementticks").value(s));
        if (movTicks < 1) movTicks = 1;
        movementAccum_ += moveDetents;
        while (movementAccum_ >= movTicks)  { upPending_++;   movementAccum_ -= movTicks; }
        while (movementAccum_ <= -movTicks) { downPending_++; movementAccum_ += movTicks; }
        if ((upPending_ > 0 || downPending_ > 0) && (long)s.tick >= nextEmitTick_) {
            if (upPending_ > 0)      { upPending_--;   upUntil_   = (long)s.tick + trigTicks; }
            else                     { downPending_--; downUntil_ = (long)s.tick + trigTicks; }
            nextEmitTick_ = (long)s.tick + 2 * trigTicks;   // 10 ms high + 10 ms gap
        }
        out("movedup").set(s,   (long)s.tick < upUntil_   ? 1.0f : 0.0f);
        out("moveddown").set(s, (long)s.tick < downUntil_ ? 1.0f : 0.0f);

        // --- valuechanged ----------------------------------------------------
        if (std::fabs(logicalNow - prevLogical_) > 1e-6f)
            vcUntil_ = (long)s.tick + trigTicks;
        prevLogical_ = logicalNow;
        out("valuechanged").set(s, (long)s.tick < vcUntil_ ? 1.0f : 0.0f);
    }

    // Persisted: the virtual position + all 16 presets + current preset slot.
    // (smoothed is a runtime low-pass; re-derived from the loaded position.)
    int stateVersion() const override { return 1; }
    void saveState(StateWriter& w) const override {
        w.f(state_.pos);
        for (int i = 0; i < 16; i++) w.f(preset_[i]);
        w.n(prevPreset_);
    }
    void loadState(EngineState& s, int version, const std::vector<double>& in) override {
        if (version != 1 || in.size() != 18) return;
        if (!inited_) init(s);
        StateReader r{in};
        state_.pos = (float)r.f();
        for (int i = 0; i < 16; i++) preset_[i] = (float)r.f();
        prevPreset_ = (int)r.n();
        ec::Params p = readParams(s);
        state_.smoothed = state_.logical(p);
        prevLogical_ = state_.smoothed;
    }

private:
    void init(EngineState& s) {
        encIdx_ = resolveEncoder(s);
        ec::Params p = readParams(s);
        state_.seed(p);
        for (int i = 0; i < 16; i++) preset_[i] = state_.pos;
        prevPreset_ = ui::clampPreset(std::lround(in("preset").value(s)), 15);
        prevLogical_ = state_.logical(p);
        inited_ = true;
    }

    // Resolve the `encoder` handle exactly once (register or global number).
    int resolveEncoder(EngineState& s) {
        const Operand& op = in("encoder").primary();
        if (in("encoder").connected() && op.kind == Operand::Kind::Register &&
            op.reg.type == 'E') {
            if (op.reg.ctrl == 0) return s.controllers.validateGlobal(op.reg.num);
            return s.controllers.globalIndexForReg(op.reg);
        }
        long g = std::lround(in("encoder").value(s));   // number (or default 1)
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

    static float overrideToPos(const ec::Params& p, float v) {
        if (p.discrete >= 2) return ec::clampOverride(p, v);
        switch (p.mode) {
            case 2:  return ec::clampf((v + 1.0f) * 0.5f, 0.0f, 1.0f);
            case 3:  return std::max(0.0f, v);
            case 4:  return std::min(0.0f, v);
            case 5:  return v;
            case 6:  return ec::wrap01(v);
            default: return ec::clampf(v, 0.0f, 1.0f);
        }
    }

    void handlePresets(EngineState& s, const ec::Params& p) {
        bool clearAll = risingEdge(caPrev_, in("clearall").value(s));
        bool clr      = risingEdge(clPrev_, in("clear").value(s));
        bool loadPatched   = in("loadpreset").connected();
        bool savePatched   = in("savepreset").connected();
        bool presetPatched = in("preset").connected();
        bool immediate = presetPatched && !loadPatched && !savePatched;

        if (clearAll) {
            state_.seed(p);
            for (int i = 0; i < 16; i++) preset_[i] = state_.pos;
        } else if (clr) {
            state_.seed(p);
            if (immediate) preset_[prevPreset_] = state_.pos;
        }
        bool saveTrig = risingEdge(spPrev_, in("savepreset").value(s));
        bool loadTrig = risingEdge(lpPrev_, in("loadpreset").value(s));
        if (savePatched && saveTrig)
            preset_[ui::presetNum(*this, s, in("savepreset").value(s), presetPatched, 15)] = state_.pos;
        if (loadPatched && loadTrig) {
            state_.pos = preset_[ui::presetNum(*this, s, in("loadpreset").value(s), presetPatched, 15)];
            state_.smoothed = state_.logical(p);
        }
        if (immediate) {
            int cur = ui::clampPreset(std::lround(in("preset").value(s)), 15);
            if (cur != prevPreset_) {
                preset_[prevPreset_] = state_.pos;
                state_.pos = preset_[cur];
                state_.smoothed = state_.logical(p);
                prevPreset_ = cur;
            }
        }
    }

    bool inited_ = false;
    int  encIdx_ = 0;
    ec::State state_;
    float preset_[16] = {};
    int  prevPreset_ = 0;
    float prevLogical_ = 0.0f;
    long movementAccum_ = 0, upPending_ = 0, downPending_ = 0;
    long nextEmitTick_ = 0, upUntil_ = 0, downUntil_ = 0, vcUntil_ = 0;
    bool caPrev_ = false, clPrev_ = false, spPrev_ = false, lpPrev_ = false;
};

DROID_REGISTER_CIRCUIT(encoder, Encoder)

} // namespace droid
