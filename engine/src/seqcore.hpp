#pragma once
// seqcore.hpp — SeqCore, the shared performance-sequencer engine behind BOTH
// motoquencer (M4 motor faders) and encoquencer (E4 encoders). encoquencer.md
// states it is "an exact replica of the motoquencer circuit" on encoders, so the
// entire sequencer — steps, pages, fadermodes, transport, quantization, gate
// timing, presets — lives here and each circuit only supplies its editing surface
// (SeqCore::editSurface / availableLanes). Spec: manual/circuits/motoquencer.md
// (the longest circuit in the manual, ~700 lines) + encoquencer.md.
//
// A step sequencer whose per-step parameters are edited live on the controller:
// each lane shows one parameter (chosen by `fadermode`) of one step of the current
// `page`, and its button (M4 touch plate / E4 encoder push) edits gates/skip/
// gate-pattern (chosen by `buttonmode`). On the M4 the motor recalls the stored
// value whenever the page or mode changes (observable headless via `expect F<n>`).
//
// This shares the engine's scale/quantization core (engine/src/notes.hpp, same as
// minifonion) for `quantize = 2`, the fader value/notch snapping model
// (engine/src/fadercore.hpp helpers) for the per-lane dents, and the standard
// select/preset conventions of the other controller circuits.
//
// ==========================================================================
// SCOPE — what is implemented vs. deferred (honest ledger; motoquencer's feature
// surface is huge and the manual itself says "you probably will fail to map all
// existing inputs"). IMPLEMENTED (and golden-tested where deterministic):
//   * fader auto-config: firstfader / numfaders (default = all M4 faders) /
//     numsteps (default = numfaders, max 32) and paging (page, currentpage).
//   * the 8 fadermodes (0 pitch/CV, 1 randomize-CV/accumulator, 2 gate
//     probability, 3 repeats, 4 gate pattern, 5 ratchets, 6 gate, 7 skip) with
//     per-mode notch counts, editing, and motor recall on page/mode change.
//   * touch buttons (buttonmode 0 gates, 2 gate-pattern cycle, 3 skip) with
//     gate auto-on when a step's pitch is moved to a new notch.
//   * quantization: 0 continuous, 1 semitone, 2 scale (root/degree/select1..13/
//     selectfill1..5/harmonicshift/noteshift/selectnoteshift via notes.hpp),
//     plus cvbase / cvrange / cvnotches / invert / transpose / tuningmode.
//   * transport: clock / reset (step-0 arming) / run / mute, direction, pingpong,
//     startstep / endstep, numsteps, shiftsteps, autoreset.
//   * per-step timing: repeats (step duration), ratchets (sub-clock), gate
//     patterns (once/all/long/tie), repeatshift / ratchetshift, gatelength,
//     holdcv.
//   * gate probability: the full musical table (always / random 50-25-12% /
//     even / odd / every-4th / conditional) with a per-turn counter + engine RNG.
//   * pitch accumulator (accumulatorrange) — the four accumulator fader
//     positions (idx 4..7 of randomize-CV) shifting the note per turn.
//   * outputs: cv, gate, startofsequence, currentstep, currentpage, accumulator,
//     startstepout, endstepout.
//   * select/selectat overlay, 4 presets, clear / clearall / clearskips /
//     clearrepeats, defaultcv (a notch index when cvnotches >= 2) / defaultgate.
//   * composemode: while high the transport ignores clock edges; a CV edit
//     (fadermode 0) jumps to that step, outputs its CV and opens a short gate.
//   * "I Feel Lucky": all 16 one-time-randomization triggers (luckyfaders,
//     luckybuttons, luckycvs, luckycvdrift, luckyspread, luckyinvert,
//     luckyrandomizecv, luckygates, luckyskips, luckyties, luckygatepattern,
//     luckygateprob, luckyrepeats, luckyratchets, luckyshuffle, luckyreverse) with
//     luckychance / luckyscope / luckyamount / luckycvbase. Each trigger permanently
//     mutates the dialed sequence and re-commands the motors so the reroll shows.
//
// DEFERRED (documented, NOT implemented — every one is either a live-performance
// convenience the manual frames as advanced, an interactive gesture with no
// headless analog, or panel-only):
//   * `form` (AAAB/ABAC/…) and the `startofpart` output — song-form step slicing.
//   * movement `pattern` 1..7 (two-forward-one-back etc.) — pattern 0 (linear)
//     only. The others interact with direction/pingpong/forms; deferred whole.
//   * `metricsaver` and `constantlength` — polymetric clock-snap-back and
//     repeat/skip length compensation (both read but inert).
//   * `linktonext` multi-track linking: the FADER/LED editing surface IS shared
//     across the chain (only the instance addressed by the main's fadermode —
//     `fadermode / 10 == chain index`, editing with the local `fadermode % 10` —
//     drives/lights the physical faders; the others release them, so the boot
//     page shows the addressed instance's steps, not a collision of all of them).
//     The linked instances' TRANSPORT is NOT yet remote-controlled from the main
//     (step/skip/repeat mirroring, `fadermode/buttonmode` +10 buttonmode side) —
//     each still steps on its own clock, so a linked lane with no clock holds its
//     first step. Enough for the editing surface; full remote stepping deferred.
//     LINKED-LUCKY semantics are likewise not implemented (motoquencer.md
//     :682-705): `luckyshuffle`/`luckyreverse` fired on the MAIN should rearrange
//     every linked instance's steps by the SAME permutation, and `luckyskips`,
//     `luckyrepeats`, `luckyshuffle`, `luckyreverse` should be IGNORED when read
//     on a linked (non-main) instance. Today each instance edge-detects and
//     applies these four to its own steps independently. `luckyfaders` IS honoured
//     correctly across the chain — it rerolls only the fader-owner's shown lane
//     (see applyLuckyOp case 0); the other value/CV lucky ops apply per-instance.
//   * keyboard recording: keyboardcv/keyboardgate/keyboardmode/recordmode/
//     recordsilence.
//   * copy / paste / pastefaders / pastebuttons / stepcopy / doublerange / bulkedit.
//   * interactive start/end (buttonmode 1) and setendstep / clearstartend — the
//     input-driven startstep/endstep ARE honoured; the two-finger button gesture
//     and its override state are not.
//   * pitch randomization (randomize-CV positions when accumulatorrange = 0, and
//     idx 1..3 when it is > 0): the manual says only "a different random offset
//     each time" with no distribution — left inert (the value is still stored/
//     edited, it just does not perturb the pitch).
//   * taptempo (gate-length stabiliser), DB8E display (cvname/gatename/display),
//     dontsave/SD persistence, buttoncolor/LED feel — panel-only or no-op headless.
//
// SPEC-GAPs (literal readings where the manual is silent; deterministic paths):
//   * motor speed instant (fadercore.hpp / controllerstate.hpp).
//   * ratchet gate length = gatelength × (period / ratchets); a value ≥ 1 makes
//     successive notes legato (matches the gatelength prose for the un-ratcheted
//     case). Before the clock period is known, gate follows the clock level.
//   * gate pattern "long"/"tie" = one gate spanning the whole step (repeats ×
//     period), ratchets ignored for those two; "tie" additionally abuts the next
//     step so a following played step is contiguous (legato) — the manual pins
//     the intent ("lets the gate open when the step ends") but no exact shape.
//   * CV latched at step entry (honours "changes only take effect on the next
//     step"): the raw pitch is computed only at step entry and at pulse/ratchet
//     boundaries and cached, so mid-step cvbase/cvrange/scale changes never drift
//     the held CV; repeatshift/ratchetshift move it per pulse/ratchet, and
//     transpose/tuning stay live per tick (vibrato input, per minifonion/arpeggio).
//   * DROID triggers are 10 ms, not 1 tick: startofsequence emits a 10 ms window,
//     gatelength = 0 floors each once/all gate to a ~10 ms minimum, and the
//     composemode audition gate opens for the same 10 ms after a CV edit. The
//     manual gives no compose-gate duration ("a short time") — literal reading.
//   * scale-change note memory ("faders remember their original note") is modelled
//     positionally: the raw 0..1 fader position is kept, so a note re-appears when
//     the scale is restored, but the exact original semitone is not separately
//     stored.
//   * probability decided once at step entry (pulse 0), using the engine RNG.
//   * "I Feel Lucky" distributions: the manual describes each op's INTENT and its
//     luckyamount meaning but never pins an exact distribution or bit-for-bit
//     rounding. Literal, property-faithful readings (all draws from the engine RNG,
//     ascending step order, so goldens can pin a seed): candidates keep with
//     probability luckychance (one draw each); values are drawn over the target
//     range implied by luckyamount (continuous ops are uniform; integer ops that
//     round `U()*amount*k` half-weight the two endpoints, as rounding gives 0 and
//     the max only a half-width bin — an accepted literal reading); luckyspread
//     scales each step's distance to the
//     all-step mean by luckyamount*2 (0 collapses to the mean, 0.5 is neutral, 1
//     doubles); luckygateprob treats luckyamount as an inverted floor 1..7;
//     luckyshuffle is a Fisher–Yates permutation of the target steps' full column
//     tuples; luckyties toggles gatepattern between 3 (tie) and 0 (once).
#include "circuit.hpp"
#include "gatereader.hpp"
#include "fadercore.hpp"
#include "notes.hpp"
#include "rng.hpp"
#include "controllerstate.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace droid {

namespace fc = fadercore;

class SeqCore : public Circuit {
    static constexpr float kHigh = kGateHighThreshold;   // 0.1 == 1 V
    static constexpr int kSteps = 32;
    static constexpr int kPresets = 4;

    // Per-step parameter block; assignable so presets are a plain copy.
    struct SeqState {
        float   cvpos[kSteps];       // 0..1 raw fader position (pitch/CV)
        uint8_t randcv[kSteps];      // 0..7 (randomize-CV / accumulator)
        uint8_t gateprob[kSteps];    // 0..7 (7 = top = always)
        uint8_t repeats[kSteps];     // 1..16
        uint8_t gatepat[kSteps];     // 0..3 (once/all/long/tie)
        uint8_t ratchets[kSteps];    // 1..8
        bool    gate[kSteps];
        bool    skip[kSteps];
    };

public:
public:
    void tick(EngineState& s) override {
        if (!inited_) init(s);
        bool selected = isSelected(s);

        // --- dynamic config -------------------------------------------------
        int page       = clampi((int)std::lround(in("page").value(s)), 0, pages() - 1);
        int buttonmode = clampi((int)std::lround(in("buttonmode").value(s)), 0, 3);

        // linktonext: the whole chain reads ONE fadermode off the main instance
        // (linked members leave `fadermode` unwired). `rawFm / 10` picks which
        // chain member owns the physical faders; it edits with `rawFm % 10`, the
        // others release them. A standalone sequencer (no link) keeps its raw
        // fadermode clamped to 0..7 and always owns its faders.
        int rawFm;
        if (chainMain_) rawFm = chainMain_->chainRawFm_;
        else { rawFm = (int)std::lround(in("fadermode").value(s)); chainRawFm_ = rawFm; }
        bool inChain   = chainMain_ != nullptr || linkToNext_;
        int  ownerIdx  = rawFm < 0 ? 0 : rawFm / 10;
        bool faderOwner = !inChain || ownerIdx == chainIndex_;
        int  fadermode = inChain ? clampi(rawFm - 10 * chainIndex_, 0, 7)
                                 : clampi(rawFm, 0, 7);

        // --- presets / clear (always run) -----------------------------------
        bool recall = handlePresets(s);
        if (risingEdge(csPrev_, in("clearskips").value(s)))
            for (int i = 0; i < numsteps_; i++) cur_.skip[i] = false;
        if (risingEdge(crpPrev_, in("clearrepeats").value(s)))
            for (int i = 0; i < numsteps_; i++) cur_.repeats[i] = 1;
        (void)risingEdge(csePrev_, in("clearstartend").value(s));   // interactive: deferred

        // "I Feel Lucky" one-time randomization (also always-run — a trigger fires
        // regardless of selection). A fired op permanently mutates the sequence, so
        // force a motor recall to re-command the faders to the rerolled values (the
        // manual: "you will immediately see them moving around").
        if (applyLucky(s, page, fadermode, buttonmode, faderOwner)) recall = true;

        // this instance shows on the faders only while selected AND the chain
        // fadermode addresses it (a standalone is always its own owner).
        bool showFaders = selected && faderOwner;

        // recall the motors when the visible page/mode changed
        if (page != shownPage_ || fadermode != shownMode_) recall = true;
        shownPage_ = page;
        shownMode_ = fadermode;

        // --- fader + touch editing (only while showing) ---------------------
        if (showFaders) editSurface(s, page, fadermode, buttonmode, recall);
        else wasSelected_ = false;   // re-taking the faders re-commands them (recall)

        // --- transport ------------------------------------------------------
        transport(s);

        // --- step LEDs (panel-only; after transport so playStep_ is fresh) ---
        updateLeds(s, page, buttonmode, showFaders);

        // --- outputs --------------------------------------------------------
        emit(s);
    }

    // The editing surface differs between the two skins: motoquencer drives M4
    // motor faders (absolute position + motor recall + touch plates), encoquencer
    // drives E4 encoders (relative detents + push buttons, no motor/readback).
    // availableLanes = the default lane count when `numfaders` is omitted.
    // setLaneLed = the step LED for a visible lane (M4: the RGB LED below the
    // fader / FaderState.led; E4: the middle-three ring cells below the encoder /
    // EncoderState.stepLed).
    virtual int  availableLanes(EngineState& s) = 0;
    virtual void editSurface(EngineState& s, int page, int fm, int bm, bool recall) = 0;
    virtual void setLaneLed(EngineState& s, int lane, float bright, float color) = 0;

    // --- persistent state (DROIDSTA.BIN contract) ---------------------------
    // The dialed sequence (all per-step parameters) + the 4 presets + slot. The
    // transport (playPos_, period_, gate windows, ...) is runtime dynamics and
    // is NOT saved. Serialized at the fixed 32-step width for a stable length.
    static constexpr size_t kSeqLen = 8 * (size_t)kSteps;
    int stateVersion() const override { return 1; }
    void saveState(StateWriter& w) const override {
        writeSeq(w, cur_);
        for (int p = 0; p < kPresets; p++) writeSeq(w, preset_[p]);
        w.n(prevPreset_);
    }
    void loadState(EngineState& s, int version, const std::vector<double>& in) override {
        if (version != 1 || in.size() != kSeqLen * (kPresets + 1) + 1) return;
        if (!inited_) init(s);
        StateReader r{in};
        readSeq(r, cur_);
        for (int p = 0; p < kPresets; p++) readSeq(r, preset_[p]);
        prevPreset_ = (int)r.n();
        shownPage_ = shownMode_ = -1;   // force a motor/encoder recall next tick
    }

    static void writeSeq(StateWriter& w, const SeqState& st) {
        for (int i = 0; i < kSteps; i++) w.f(st.cvpos[i]);
        for (int i = 0; i < kSteps; i++) w.n(st.randcv[i]);
        for (int i = 0; i < kSteps; i++) w.n(st.gateprob[i]);
        for (int i = 0; i < kSteps; i++) w.n(st.repeats[i]);
        for (int i = 0; i < kSteps; i++) w.n(st.gatepat[i]);
        for (int i = 0; i < kSteps; i++) w.n(st.ratchets[i]);
        for (int i = 0; i < kSteps; i++) w.b(st.gate[i]);
        for (int i = 0; i < kSteps; i++) w.b(st.skip[i]);
    }
    static void readSeq(StateReader& r, SeqState& st) {
        for (int i = 0; i < kSteps; i++) st.cvpos[i]   = (float)r.f();
        for (int i = 0; i < kSteps; i++) st.randcv[i]  = (uint8_t)r.n();
        for (int i = 0; i < kSteps; i++) st.gateprob[i]= (uint8_t)r.n();
        for (int i = 0; i < kSteps; i++) st.repeats[i] = (uint8_t)r.n();
        for (int i = 0; i < kSteps; i++) st.gatepat[i] = (uint8_t)r.n();
        for (int i = 0; i < kSteps; i++) st.ratchets[i]= (uint8_t)r.n();
        for (int i = 0; i < kSteps; i++) st.gate[i]    = r.b();
        for (int i = 0; i < kSteps; i++) st.skip[i]    = r.b();
    }

protected:
    // ---- configuration -----------------------------------------------------
    void init(EngineState& s) {
        long ff = std::lround(in("firstfader").value(s));
        firstFader_ = ff < 1 ? 1 : (int)ff;
        int avail = availableLanes(s);
        long nf = std::lround(in("numfaders").value(s));
        numFaders_ = in("numfaders").connected() && nf > 0 ? (int)nf : (avail > 0 ? avail : 4);
        numFaders_ = clampi(numFaders_, 1, kSteps);
        long ns = std::lround(in("numsteps").value(s));
        numsteps_ = in("numsteps").connected() && ns > 0 ? (int)ns : numFaders_;
        numsteps_ = clampi(numsteps_, 1, kSteps);

        seedState(s, cur_);
        for (int p = 0; p < kPresets; p++) preset_[p] = cur_;
        prevPreset_ = clampi((int)std::lround(in("preset").value(s)), 0, kPresets - 1);

        // linktonext chain resolution. Walk back over the immediately preceding
        // sequencer peers that each `linktonext = 1`; the earliest one is this
        // chain's main and our distance from it is the chain index. `linkToNext_`
        // records whether WE feed a further linked instance.
        linkToNext_ = in("linktonext").value(s) >= kHigh;
        chainMain_ = nullptr;
        chainIndex_ = 0;
        for (SeqCore* p = prevSeqPeer(); p && p->linksForward(s); p = p->prevSeqPeer()) {
            chainMain_ = p;
            chainIndex_++;
        }
        inited_ = true;
    }

    // The sequencer peer immediately before this one in patch order, or nullptr
    // if there is none or it is not a motoquencer/encoquencer.
    SeqCore* prevSeqPeer() const {
        if (!peers_ || peerIndex_ <= 0) return nullptr;
        return asSeq((*peers_)[peerIndex_ - 1]);
    }
    // True iff this instance declares `linktonext = 1` (feeds the next in a chain).
    bool linksForward(EngineState& s) { return in("linktonext").value(s) >= kHigh; }
    static SeqCore* asSeq(Circuit* c) {
        if (!c || !c->def) return nullptr;
        std::string n = c->def->name;
        return (n == "motoquencer" || n == "encoquencer") ? static_cast<SeqCore*>(c)
                                                          : nullptr;
    }

    int pages() const {
        int p = (numsteps_ + numFaders_ - 1) / numFaders_;
        return p < 1 ? 1 : p;
    }

    void seedState(EngineState& s, SeqState& st) {
        bool dg = in("defaultgate").value(s) >= kHigh;
        float base = in("cvbase").value(s), range = in("cvrange").value(s);
        float dc = in("defaultcv").value(s);
        // In notched mode (cvnotches >= 2) defaultcv is a notch index 0..cvnotches-1,
        // NOT a voltage (cvbase/cvrange are ignored — see the cvnotches Inputs row).
        int cn = (int)std::lround(in("cvnotches").value(s));
        float pos;
        if (cn >= 2) {
            int idx = clampi((int)std::lround(dc), 0, cn - 1);
            pos = fc::notchRest(idx, cn);
        } else {
            pos = range > 1e-9f ? clampf((dc - base) / range, 0.0f, 1.0f) : 0.0f;
        }
        for (int i = 0; i < kSteps; i++) {
            st.cvpos[i] = pos; st.randcv[i] = 0; st.gateprob[i] = 7;
            st.repeats[i] = 1; st.gatepat[i] = 0; st.ratchets[i] = 1;
            st.gate[i] = dg; st.skip[i] = false;
        }
    }

    // ---- scale / pitch mapping --------------------------------------------
    NoteSelector selector(EngineState& s) {
        NoteSelector ns;
        ns.setRoot((int)std::lround(in("root").value(s)));
        ns.setDegree((int)std::lround(in("degree").value(s)));
        ns.setHarmonicShift((int)std::lround(in("harmonicshift").value(s)));
        static const char* kSel[7] = {"select1","select3","select5","select7",
                                      "select9","select11","select13"};
        for (int i = 0; i < 7; i++) ns.select[i] = in(kSel[i]).value(s) >= kHigh;
        static const char* kFill[5] = {"selectfill1","selectfill2","selectfill3",
                                       "selectfill4","selectfill5"};
        for (int i = 0; i < 5; i++) ns.fill[i] = in(kFill[i]).value(s) >= kHigh;
        return ns;
    }

    // Absolute semitone list for the pitch fader's notches (quantize 1 / 2).
    std::vector<long> allowedSemis(EngineState& s) {
        float base = in("cvbase").value(s);
        float range = clampf(in("cvrange").value(s), 0.0f, 1.0f);
        long lo = (long)std::ceil((double)base / (double)kSemitoneUnit - 1e-6);
        long hi = (long)std::floor((double)(base + range) / (double)kSemitoneUnit + 1e-6);
        int quant = (int)std::lround(in("quantize").value(s));
        std::vector<long> out;
        if (quant == 2) {
            std::vector<int> pcs = selector(s).selectedNotes();
            for (long sm = lo; sm <= hi; sm++) {
                int pc = (int)(((sm % 12) + 12) % 12);
                for (int a : pcs) if (a == pc) { out.push_back(sm); break; }
            }
        } else {                       // quantize == 1: every semitone
            for (long sm = lo; sm <= hi; sm++) out.push_back(sm);
        }
        return out;
    }

    // Notch count of the current pitch fader (0 = continuous, no dents).
    int pitchNotches(EngineState& s) {
        int cn = (int)std::lround(in("cvnotches").value(s));
        if (cn >= 2) return cn;
        int quant = (int)std::lround(in("quantize").value(s));
        if (quant == 0) return 0;
        int n = (int)allowedSemis(s).size();
        return n < 1 ? 1 : n;
    }

    // Notch count for a fadermode (pitch depends on the scale).
    int notchesFor(EngineState& s, int fm) {
        switch (fm) {
            case 0: return pitchNotches(s);
            case 1: return 8;
            case 2: return 8;
            case 3: return 16;
            case 4: return 4;
            case 5: return 8;
            default: return 2;   // 6 gate, 7 skip
        }
    }

    // The 0..1 rest position of a step's stored value in a given fadermode.
    float storedPos(EngineState& s, int fm, int step) {
        switch (fm) {
            case 0: {
                int cn = (int)std::lround(in("cvnotches").value(s));
                int quant = (int)std::lround(in("quantize").value(s));
                if (cn < 2 && quant == 0) return cur_.cvpos[step];   // continuous
                int N = notchesFor(s, fm);
                int idx = fc::notchIndex(cur_.cvpos[step], N);
                return fc::notchRest(idx, N);
            }
            case 1: return cur_.randcv[step] / 7.0f;
            case 2: return cur_.gateprob[step] / 7.0f;
            case 3: return (cur_.repeats[step] - 1) / 15.0f;
            case 4: return cur_.gatepat[step] / 3.0f;
            case 5: return (cur_.ratchets[step] - 1) / 7.0f;
            case 6: return cur_.gate[step] ? 1.0f : 0.0f;
            default: return cur_.skip[step] ? 1.0f : 0.0f;
        }
    }

    // Apply a user fader position to a step's value in a fadermode.
    // Returns true if the stored value actually changed (drives gate auto-on).
    bool applyEdit(EngineState& s, int fm, int step, float pos, float& snapped) {
        pos = clampf(pos, 0.0f, 1.0f);
        int N = notchesFor(s, fm);
        auto snapIdx = [&](int n) { return fc::notchIndex(pos, n); };
        switch (fm) {
            case 0: {
                int cn = (int)std::lround(in("cvnotches").value(s));
                int quant = (int)std::lround(in("quantize").value(s));
                if (cn < 2 && quant == 0) {           // continuous
                    bool ch = std::fabs(pos - cur_.cvpos[step]) > 1e-6f;
                    cur_.cvpos[step] = pos; snapped = pos; return ch;
                }
                int idx = snapIdx(N);
                float np = fc::notchRest(idx, N);
                bool ch = std::fabs(np - cur_.cvpos[step]) > 1e-6f;
                cur_.cvpos[step] = np; snapped = np; return ch;
            }
            case 1: { int v = snapIdx(8); bool ch = v != cur_.randcv[step];
                      cur_.randcv[step] = (uint8_t)v; snapped = v / 7.0f; return ch; }
            case 2: { int v = snapIdx(8); bool ch = v != cur_.gateprob[step];
                      cur_.gateprob[step] = (uint8_t)v; snapped = v / 7.0f; return ch; }
            case 3: { int v = snapIdx(16); bool ch = (v + 1) != cur_.repeats[step];
                      cur_.repeats[step] = (uint8_t)(v + 1); cur_.skip[step] = false;
                      snapped = v / 15.0f; return ch; }
            case 4: { int v = snapIdx(4); bool ch = v != cur_.gatepat[step];
                      cur_.gatepat[step] = (uint8_t)v; snapped = v / 3.0f; return ch; }
            case 5: { int v = snapIdx(8); bool ch = (v + 1) != cur_.ratchets[step];
                      cur_.ratchets[step] = (uint8_t)(v + 1); snapped = v / 7.0f; return ch; }
            case 6: { bool v = pos >= 0.5f; bool ch = v != cur_.gate[step];
                      cur_.gate[step] = v; snapped = v ? 1.0f : 0.0f; return ch; }
            default: { bool v = pos >= 0.5f; bool ch = v != cur_.skip[step];
                      cur_.skip[step] = v; snapped = v ? 1.0f : 0.0f; return ch; }
        }
    }

    // Push-button edit shared by both skins (M4 touch plate / E4 encoder push):
    // one press toggles/cycles the buttonmode parameter of a step.
    void pressStep(int bm, int step) {
        switch (bm) {
            case 0: cur_.gate[step] = !cur_.gate[step]; break;
            case 2: cur_.gatepat[step] = (cur_.gatepat[step] + 1) & 3; break;
            case 3: cur_.skip[step] = !cur_.skip[step]; break;
            default: break;   // buttonmode 1 (start/end) deferred
        }
    }

    // ---- I Feel Lucky ------------------------------------------------------
    // One-time randomization triggers (manual "I Feel Lucky"). Each trigger, when
    // it rises, permanently rerolls a subset of the dialed steps. `applyLucky`
    // edge-detects all 16 in a fixed order and returns whether any fired (so tick()
    // can force a motor recall). Runs regardless of selection.
    bool applyLucky(EngineState& s, int page, int fm, int bm, bool faderOwner) {
        static const char* kOps[16] = {
            "luckyfaders", "luckybuttons", "luckycvs", "luckycvdrift", "luckyspread",
            "luckyinvert", "luckyrandomizecv", "luckygates", "luckyskips", "luckyties",
            "luckygatepattern", "luckygateprob", "luckyrepeats", "luckyratchets",
            "luckyshuffle", "luckyreverse"};
        bool fired = false;
        for (int op = 0; op < 16; op++) {
            if (!risingEdge(luckyPrev_[op], in(kOps[op]).value(s))) continue;
            fired = true;
            applyLuckyOp(s, op, luckyTargets(s, page), fm, bm, faderOwner);
        }
        return fired;
    }

    // The set of step indices a lucky op affects: the candidate range chosen by
    // `luckyscope`, then each candidate kept with probability `luckychance` (one RNG
    // draw per candidate, ascending step order, so a seeded run is reproducible).
    std::vector<int> luckyTargets(EngineState& s, int page) {
        int scope = (int)std::lround(in("luckyscope").value(s));
        float chance = clampf(in("luckychance").value(s), 0.0f, 1.0f);
        long ss = std::lround(in("startstep").value(s));
        long es = in("endstep").connected() ? std::lround(in("endstep").value(s)) : numsteps_;
        int s0 = clampi((int)ss - 1, 0, numsteps_ - 1);
        int e0 = clampi((int)es - 1, 0, numsteps_ - 1);
        int lo = std::min(s0, e0), hi = std::max(s0, e0);
        int pLo = page * numFaders_, pHi = std::min(pLo + numFaders_ - 1, numsteps_ - 1);
        std::vector<int> out;
        for (int i = 0; i < numsteps_; i++) {
            bool inRange = (i >= lo && i <= hi), inPage = (i >= pLo && i <= pHi);
            bool cand;
            switch (scope) {
                case 1:  cand = true;             break;   // all steps
                case 2:  cand = inRange && inPage; break;   // start..end on this page
                case 3:  cand = inPage;           break;   // whole current page
                default: cand = inRange;          break;   // 0: start..end (default)
            }
            if (cand && randUniform(s.rngState) < chance) out.push_back(i);
        }
        return out;
    }

    // Copy every per-step column from src[si] to dst[di] (used by shuffle/reverse,
    // which move whole step tuples so no attribute is dropped).
    static void copyStep(SeqState& dst, int di, const SeqState& src, int si) {
        dst.cvpos[di]   = src.cvpos[si];   dst.randcv[di]   = src.randcv[si];
        dst.gateprob[di]= src.gateprob[si];dst.repeats[di]  = src.repeats[si];
        dst.gatepat[di] = src.gatepat[si]; dst.ratchets[di] = src.ratchets[si];
        dst.gate[di]    = src.gate[si];    dst.skip[di]     = src.skip[si];
    }

    void luckyReverse(const std::vector<int>& T) {
        SeqState snap = cur_;
        int n = (int)T.size();
        for (int i = 0; i < n; i++) copyStep(cur_, T[i], snap, T[n - 1 - i]);
    }
    void luckyShuffle(EngineState& s, const std::vector<int>& T) {
        int n = (int)T.size();
        if (n < 2) return;
        std::vector<int> order = T;               // forward Fisher–Yates (ascending draws)
        for (int i = 0; i < n - 1; i++) {
            int j = i + (int)(randUniform(s.rngState) * (n - i));
            if (j >= n) j = n - 1;
            std::swap(order[i], order[j]);
        }
        SeqState snap = cur_;
        for (int i = 0; i < n; i++) copyStep(cur_, T[i], snap, order[i]);
    }

    // Apply one lucky operation to its target steps. `amount`/`lvbase` are the
    // luckyamount / luckycvbase inputs; all randomness is drawn ascending from the
    // engine RNG. cvpos/CV work in 0..1 position space (playback maps it to the CV
    // range), matching "within the allowed CV range".
    void applyLuckyOp(EngineState& s, int op, const std::vector<int>& T, int fm, int bm,
                      bool faderOwner) {
        float amount = clampf(in("luckyamount").value(s), 0.0f, 1.0f);
        float lvbase = clampf(in("luckycvbase").value(s), 0.0f, 1.0f);
        auto U = [&] { return randUniform(s.rngState); };
        switch (op) {
            case 0:   // luckyfaders: reroll the currently shown lane (per fadermode).
                // `fm` is only the real shown lane on the fader OWNER. On a chained
                // non-owner instance `fm` is the chain-clamped alias
                // (clampi(rawFm - 10*chainIndex_, 0, 7)) which aliases to lane 0/7 and
                // is not shown anywhere, so luckyfaders no-ops there (it only ever
                // rerolls the one lane visible on the edit surface). luckyTargets
                // above still drew its chance coins, keeping the RNG stream aligned.
                if (!faderOwner) break;
                for (int i : T) {                 // amount caps the max; fm 0 offsets by lvbase
                    float pos = (fm == 0 ? lvbase : 0.0f) + U() * amount, snapped;
                    applyEdit(s, fm, i, clampf(pos, 0.0f, 1.0f), snapped);
                }
                break;
            case 1:   // luckybuttons: reroll the current button lane (per buttonmode).
                for (int i : T) {
                    float u = U();
                    if (bm == 2) cur_.gatepat[i] = (uint8_t)clampi((int)std::lround(u * amount * 3.0f), 0, 3);
                    else if (bm == 3) { if (u < 0.5f) cur_.skip[i] = !cur_.skip[i]; }
                    else if (bm == 0) { if (u < 0.5f) cur_.gate[i] = !cur_.gate[i]; }
                }                                 // bm 1 (start/end) has no stored lane: no-op
                break;
            case 2:   // luckycvs: new CV in lvbase .. lvbase+amount (amount 0 -> cvbase).
                for (int i : T) cur_.cvpos[i] = clampf(lvbase + U() * amount, 0.0f, 1.0f);
                break;
            case 3:   // luckycvdrift: nudge CV +/- amount, staying in range.
                for (int i : T)
                    cur_.cvpos[i] = clampf(cur_.cvpos[i] + (U() * 2.0f - 1.0f) * amount, 0.0f, 1.0f);
                break;
            case 4: {  // luckyspread: scale distance to the all-step mean by amount*2.
                float avg = 0.0f;
                for (int i = 0; i < numsteps_; i++) avg += cur_.cvpos[i];
                avg /= (float)numsteps_;
                float mult = amount * 2.0f;       // 0 collapse, 0.5 neutral, 1 double
                for (int i : T) cur_.cvpos[i] = clampf(avg + (cur_.cvpos[i] - avg) * mult, 0.0f, 1.0f);
                break; }
            case 5:   // luckyinvert: mirror CV within range (amount ignored).
                for (int i : T) cur_.cvpos[i] = 1.0f - cur_.cvpos[i];
                break;
            case 6:   // luckyrandomizecv: randomize-CV value 0..7 (amount caps max).
                for (int i : T)
                    cur_.randcv[i] = (uint8_t)clampi((int)std::lround(U() * amount * 7.0f), 0, 7);
                break;
            case 7:   // luckygates: gate on with probability amount (0 all off, 1 all on).
                for (int i : T) cur_.gate[i] = U() < amount;
                break;
            case 8:   // luckyskips: skip with probability amount.
                for (int i : T) cur_.skip[i] = U() < amount;
                break;
            case 9:   // luckyties: tie (gatepat 3) with probability amount, else once (0).
                for (int i : T) cur_.gatepat[i] = (U() < amount) ? 3 : 0;
                break;
            case 10:  // luckygatepattern: random gate pattern 0..3 (amount reduces set).
                for (int i : T)
                    cur_.gatepat[i] = (uint8_t)clampi((int)std::lround(U() * amount * 3.0f), 0, 3);
                break;
            case 11:  // luckygateprob: random gateprob with amount as an inverted floor:
                for (int i : T) {                 // amount 1 -> always 7, amount 0 -> floor 1.
                    int minv = clampi((int)std::lround(1.0f + amount * 6.0f), 1, 7);
                    int span = 7 - minv;
                    cur_.gateprob[i] = (uint8_t)clampi(minv + (span > 0 ? (int)(U() * (span + 1)) : 0), 1, 7);
                }
                break;
            case 12:  // luckyrepeats: random repeats 1..round(1+amount*15).
                for (int i : T) {
                    int maxr = clampi((int)std::lround(1.0f + amount * 15.0f), 1, 16);
                    cur_.repeats[i] = (uint8_t)clampi(1 + (int)(U() * maxr), 1, maxr);
                }
                break;
            case 13:  // luckyratchets: random ratchets 1..round(1+amount*7).
                for (int i : T) {
                    int maxr = clampi((int)std::lround(1.0f + amount * 7.0f), 1, 8);
                    cur_.ratchets[i] = (uint8_t)clampi(1 + (int)(U() * maxr), 1, maxr);
                }
                break;
            case 14: luckyShuffle(s, T); break;   // luckyshuffle: permute target tuples
            case 15: luckyReverse(T);    break;   // luckyreverse: reverse target tuples
        }
    }

    // ---- step LEDs ---------------------------------------------------------
    // motoquencer.md "LED colors": the LED below each visible step shows the
    // buttonmode state — blue gate (bm 0), green start / red end (bm 1), the
    // gate-pattern colour per step (bm 2; lit on gate-enabled steps, since the
    // colours describe how that step's gate plays), violet skip (bm 3) — and the
    // currently played step is white regardless of buttonmode. encoquencer.md
    // defers to this ("the middle three LEDs below each encoder have the same
    // function as the touch button's LED in the M4"). Colours are DROID colour
    // values (basics.md §5.5, rendered by plugin droidcolor.hpp); white is not in
    // that table, so kLedWhite is a negative sentinel the renderer maps to white.
    // A deselected circuit releases the LEDs (cleared once on the falling edge,
    // then untouched so another overlaid circuit can drive them).
    static constexpr float kLedWhite  = -1.0f;   // sentinel: played step
    static constexpr float kLedCyan   = 0.2f, kLedGreen = 0.4f, kLedYellow = 0.6f,
                           kLedOrange = 0.73f, kLedRed  = 0.8f, kLedPink   = 1.0f,
                           kLedViolet = 1.1f,  kLedBlue = 1.2f;

    void updateLeds(EngineState& s, int page, int bm, bool selected) {
        if (!selected) {
            if (ledsLit_) for (int i = 0; i < numFaders_; i++) setLaneLed(s, i, 0.0f, 0.0f);
            ledsLit_ = false;
            return;
        }
        ledsLit_ = true;
        long ss = std::lround(in("startstep").value(s));
        long es = in("endstep").connected() ? std::lround(in("endstep").value(s)) : numsteps_;
        int start0 = clampi((int)ss - 1, 0, numsteps_ - 1);
        int end0   = clampi((int)es - 1, 0, numsteps_ - 1);
        static constexpr float kPatColor[4] = {kLedCyan, kLedPink, kLedOrange, kLedYellow};
        for (int i = 0; i < numFaders_; i++) {
            int step = page * numFaders_ + i;
            float b = 0.0f, c = 0.0f;
            if (step < numsteps_) {
                switch (bm) {
                    case 0:  if (cur_.gate[step]) { b = 1.0f; c = kLedBlue; } break;
                    case 1:  if (step == start0)  { b = 1.0f; c = kLedGreen; }
                             else if (step == end0) { b = 1.0f; c = kLedRed; } break;
                    case 2:  if (cur_.gate[step]) { b = 1.0f; c = kPatColor[cur_.gatepat[step] & 3]; } break;
                    default: if (cur_.skip[step]) { b = 1.0f; c = kLedViolet; } break;
                }
                if (playStep_ == step) { b = 1.0f; c = kLedWhite; }   // white always wins
            }
            setLaneLed(s, i, b, c);
        }
    }

    // Encoder edit: nudge a step's value in fadermode fm by `detents` notches/units
    // (encoders have no absolute position). Returns true if the value changed.
    bool adjustByDetents(EngineState& s, int fm, int step, long detents) {
        if (detents == 0) return false;
        int N = notchesFor(s, fm);
        auto nudgeIdx = [&](int cur, int hi) {
            long v = cur + detents; if (v < 0) v = 0; if (v > hi) v = hi; return (int)v; };
        switch (fm) {
            case 0: {
                int cn = (int)std::lround(in("cvnotches").value(s));
                int quant = (int)std::lround(in("quantize").value(s));
                if (cn < 2 && quant == 0) {                 // continuous: 1/96 per detent
                    float np = clampf(cur_.cvpos[step] + detents * (1.0f / 96.0f), 0.0f, 1.0f);
                    bool ch = std::fabs(np - cur_.cvpos[step]) > 1e-9f;
                    cur_.cvpos[step] = np; return ch;
                }
                if (N < 2) return false;
                int idx = nudgeIdx(fc::notchIndex(cur_.cvpos[step], N), N - 1);
                float np = fc::notchRest(idx, N);
                bool ch = std::fabs(np - cur_.cvpos[step]) > 1e-9f;
                cur_.cvpos[step] = np; return ch;
            }
            case 1: { int v = nudgeIdx(cur_.randcv[step], 7); bool ch = v != cur_.randcv[step];
                      cur_.randcv[step] = (uint8_t)v; return ch; }
            case 2: { int v = nudgeIdx(cur_.gateprob[step], 7); bool ch = v != cur_.gateprob[step];
                      cur_.gateprob[step] = (uint8_t)v; return ch; }
            case 3: { int v = nudgeIdx(cur_.repeats[step] - 1, 15); bool ch = (v + 1) != cur_.repeats[step];
                      cur_.repeats[step] = (uint8_t)(v + 1); cur_.skip[step] = false; return ch; }
            case 4: { int v = nudgeIdx(cur_.gatepat[step], 3); bool ch = v != cur_.gatepat[step];
                      cur_.gatepat[step] = (uint8_t)v; return ch; }
            case 5: { int v = nudgeIdx(cur_.ratchets[step] - 1, 7); bool ch = (v + 1) != cur_.ratchets[step];
                      cur_.ratchets[step] = (uint8_t)(v + 1); return ch; }
            case 6: { int v = nudgeIdx(cur_.gate[step] ? 1 : 0, 1); bool ch = (bool)v != cur_.gate[step];
                      cur_.gate[step] = v; return ch; }
            default: { int v = nudgeIdx(cur_.skip[step] ? 1 : 0, 1); bool ch = (bool)v != cur_.skip[step];
                      cur_.skip[step] = v; return ch; }
        }
    }

    // ---- transport ---------------------------------------------------------
    // logical step i (0..numsteps-1) -> physical index after shiftsteps.
    int physOf(EngineState& s, int logical) {
        long sh = std::lround(in("shiftsteps").value(s));
        long p = ((long)logical + sh) % numsteps_;
        if (p < 0) p += numsteps_;
        return (int)p;
    }

    // Build the logical play order for one full cycle (range + direction + pingpong).
    std::vector<int> playOrder(EngineState& s) {
        long ss = std::lround(in("startstep").value(s));
        long es = in("endstep").connected() ? std::lround(in("endstep").value(s)) : numsteps_;
        int start0 = clampi((int)ss - 1, 0, numsteps_ - 1);
        int end0   = clampi((int)es - 1, 0, numsteps_ - 1);
        std::vector<int> o;
        if (start0 <= end0) for (int i = start0; i <= end0; i++) o.push_back(i);
        else                for (int i = start0; i >= end0; i--) o.push_back(i);
        if (in("direction").value(s) >= kHigh)
            std::reverse(o.begin(), o.end());
        if (in("pingpong").value(s) >= kHigh && o.size() > 1)
            for (int k = (int)o.size() - 2; k >= 1; k--) o.push_back(o[k]);
        if (o.empty()) o.push_back(0);
        return o;
    }

    void transport(EngineState& s) {
        bool run = in("run").value(s) >= kHigh;
        // Advance the clock/reset edge detectors every tick (even when frozen) so
        // leaving compose/stop does not fire a phantom edge on a still-high input.
        bool clockEdge = clockGate_.risingEdge(in("clock").value(s));
        if (resetGate_.risingEdge(in("reset").value(s))) { resetPending_ = true; acc_ = 0; triggerSos(s); }

        // composemode: the sequencer stops clocking; stepping is driven only by CV
        // edits (see onCvEdited), so ignore clock edges entirely.
        if (in("composemode").value(s) >= kHigh) return;
        if (!run) return;                          // frozen: ignore clock
        if (!clockEdge) return;

        if (in("clock").connected()) {
            if (haveClock_) period_ = double(s.tick - lastClock_);
            lastClock_ = s.tick; haveClock_ = true;
        }

        std::vector<int> order = playOrder(s);
        long autoreset = std::lround(in("autoreset").value(s));

        if (!started_ || resetPending_) {
            playPos_ = 0; pulse_ = 0; turn_ = 1; clocksSinceReset_ = 0;
            started_ = true; resetPending_ = false; triggerSos(s);
            enterStep(s, order, 0);
            return;
        }

        clocksSinceReset_++;
        if (autoreset > 0 && clocksSinceReset_ >= autoreset) {
            playPos_ = 0; pulse_ = 0; turn_ = 1; clocksSinceReset_ = 0;
            advanceAccumulator(s); triggerSos(s);
            enterStep(s, order, 0);
            return;
        }

        int reps = curRepeats(s, order);
        if (pulse_ + 1 < reps) { pulse_++; return; }   // same step, next pulse
        pulse_ = 0;
        bool wrapped = false;
        int next = nextPlayedPos(s, order, playPos_, wrapped);
        playPos_ = next;
        if (wrapped) { turn_++; advanceAccumulator(s); triggerSos(s); }
        enterStep(s, order, playPos_);
    }

    int curRepeats(EngineState& s, const std::vector<int>& order) {
        int phys = physOf(s, order[clampi(playPos_, 0, (int)order.size() - 1)]);
        return cur_.repeats[phys];
    }

    // Advance to the next non-skipped position; sets wrapped if we passed the end.
    int nextPlayedPos(EngineState& s, const std::vector<int>& order, int pos, bool& wrapped) {
        int n = (int)order.size();
        for (int tries = 0; tries < n; tries++) {
            pos++;
            if (pos >= n) { pos = 0; wrapped = true; }
            if (!cur_.skip[physOf(s, order[pos])]) return pos;
        }
        return pos;   // all skipped -> hold (manual: repeats the most recent step)
    }

    void advanceAccumulator(EngineState& s) {
        long range = std::lround(in("accumulatorrange").value(s));
        if (range <= 0) { acc_ = 0; return; }
        if (range > 16) range = 16;
        acc_++;
        if (acc_ > (int)range) acc_ = 0;
    }

    // Latch the step we are entering and precompute its gate windows.
    void enterStep(EngineState& s, const std::vector<int>& order, int pos) {
        int phys = physOf(s, order[clampi(pos, 0, (int)order.size() - 1)]);
        playStep_ = phys;
        playLogical_ = order[clampi(pos, 0, (int)order.size() - 1)];
        latchCvpos_ = cur_.cvpos[phys];
        latchRandcv_ = cur_.randcv[phys];
        plays_ = cur_.gate[phys] && probabilityPlays(s, phys);
        stepStart_ = s.tick;
        pitchCached_ = false;   // recompute the step's raw pitch on entry

        gateWin_.clear();
        if (!plays_) return;
        int reps = cur_.repeats[phys];
        int rat  = cur_.ratchets[phys];
        int gp   = cur_.gatepat[phys];
        double T = period_ > 0.0 ? period_ : 0.0;
        if (T <= 0.0) return;   // period unknown -> fall back to clock-level gate
        double gl = clampf(in("gatelength").value(s), 0.0f, 100.0f);
        if (gp >= 2) {          // long / tie: one gate over the whole step
            uint64_t end = stepStart_ + (uint64_t)std::llround(T * reps);
            gateWin_.push_back({stepStart_, end});
            tie_ = (gp == 3);
            return;
        }
        tie_ = false;
        int pulses = (gp == 0) ? 1 : reps;       // once: only pulse 0; all: each
        double ri = T / (rat > 0 ? rat : 1);
        double len = gl * ri;
        // gatelength = 0 (no steady clock) -> minimal ~10 ms gate ("basically just
        // a trigger"); floor each once/all window to that, never the long/tie ones.
        uint64_t minLen = (uint64_t)trigTicks(s);
        for (int p = 0; p < pulses; p++)
            for (int r = 0; r < rat; r++) {
                double st0 = p * T + r * ri;
                uint64_t a = stepStart_ + (uint64_t)std::llround(st0);
                uint64_t b = stepStart_ + (uint64_t)std::llround(st0 + len);
                if (b < a + minLen) b = a + minLen;
                // A ratchet/repeat subgate must retrigger: never let one subgate
                // reach the next subgate boundary (st0 + ri), or a fast clock's
                // 10 ms trigger floor would fuse them into one gate (no edges).
                if (rat > 1 || pulses > 1) {
                    uint64_t nextB = stepStart_ + (uint64_t)std::llround(st0 + ri);
                    if (b >= nextB) b = (nextB > a + 1) ? nextB - 1 : a + 1;
                }
                gateWin_.push_back({a, b});
            }
    }

    bool probabilityPlays(EngineState& s, int step) {
        int idx = cur_.gateprob[step];        // 0..7, 7 = always
        auto coin = [&](float p) { bool r = randUniform(s.rngState) < p;
                                   lastRandomPos_ = r; return r; };
        switch (idx) {
            case 7: return true;
            case 6: return coin(0.50f);
            case 5: return (turn_ % 2) == 0;              // every even turn
            case 4: return (turn_ % 2) == 1;              // every odd turn
            case 3: return coin(0.25f);
            case 2: return (turn_ % 4) == 0;              // every 4th turn
            case 1: return coin(0.12f);
            default: return lastRandomPos_;               // conditional
        }
    }

    // ---- pitch computation -------------------------------------------------
    // Pitch (engine units) for the currently-playing step at pulse/ratchet.
    float playedPitch(EngineState& s, int pulse, int ratchet) {
        int cn = (int)std::lround(in("cvnotches").value(s));
        bool invert = in("invert").value(s) >= kHigh;
        float base = in("cvbase").value(s);
        float range = clampf(in("cvrange").value(s), 0.0f, 1.0f);
        float pos = clampf(latchCvpos_, 0.0f, 1.0f);
        if (cn >= 2) {
            int idx = fc::notchIndex(pos, cn);
            if (invert) idx = cn - 1 - idx;
            return (float)idx;                       // notched: integer number
        }
        int quant = (int)std::lround(in("quantize").value(s));
        if (quant == 0) {
            float v = invert ? (1.0f - pos) : pos;
            return base + v * range;                 // continuous CV
        }
        std::vector<long> allowed = allowedSemis(s);
        int N = (int)allowed.size();
        if (N < 1) return base;
        int idx = fc::notchIndex(pos, N);
        if (invert) idx = N - 1 - idx;
        long semi = allowed[idx];
        NoteSelector ns = selector(s);
        int sns = (int)std::lround(in("selectnoteshift").value(s));
        int nos = (int)std::lround(in("noteshift").value(s));
        long rs = std::lround(in("repeatshift").value(s)) * (long)pulse;
        long ras = std::lround(in("ratchetshift").value(s)) * (long)ratchet;
        int accShift = accumulatorShift(s);
        semi = ns.shiftNote((int)semi, sns + (int)rs + (int)ras + accShift, nos);
        if (semi > kPitchBorderSemis) semi = kPitchBorderSemis;
        if (semi < -kPitchBorderSemis) semi = -kPitchBorderSemis;
        return (float)((double)semi * (double)kSemitoneUnit);
    }

    // Accumulator note shift (selected-note steps) for the playing step.
    int accumulatorShift(EngineState& s) {
        long range = std::lround(in("accumulatorrange").value(s));
        if (range <= 0) return 0;
        int r = latchRandcv_;
        int factor = 0;
        if (r == 7) factor = 2; else if (r == 6) factor = 1;
        else if (r == 5) factor = -1; else if (r == 4) factor = -2;
        return factor * acc_;
    }

    // ---- outputs -----------------------------------------------------------
    // Apply tuning/transpose (live per tick) to a cached raw pitch. cn>=2 numbers
    // are emitted verbatim (no tuning/transpose).
    float dressPitch(EngineState& s, float raw, int cn) {
        if (cn >= 2) return raw;
        bool tuning = in("tuningmode").value(s) >= kHigh;
        return applyTuning(tuning, in("tuningpitch").value(s), raw, in("transpose").value(s));
    }

    void emit(EngineState& s) {
        bool mute = in("mute").value(s) >= kHigh;
        bool run  = in("run").value(s) >= kHigh;
        int cn = (int)std::lround(in("cvnotches").value(s));
        bool compose = in("composemode").value(s) >= kHigh;

        // composemode: the transport is frozen (see transport()); the CV/gate follow
        // the last edited step so you can audition it. Housekeeping outputs below
        // still reflect the frozen sequencer state.
        if (compose) {
            bool gateHigh = false;
            if (composeActive_) {
                latchCvpos_ = cur_.cvpos[composeStep_];
                latchRandcv_ = cur_.randcv[composeStep_];
                cvHeld_ = dressPitch(s, playedPitch(s, 0, 0), cn);
                gateHigh = (long)s.tick < composeGateUntil_;
            }
            out("cv").set(s, cvHeld_);
            out("gate").set(s, gateHigh ? 1.0f : 0.0f);
            emitHousekeeping(s);
            return;
        }

        // ratchet index within the current pulse (for repeat/ratchet shifts)
        int ratchet = 0;
        if (period_ > 0.0 && playStep_ >= 0) {
            double within = double(s.tick - stepStart_) - pulse_ * period_;
            int rat = cur_.ratchets[playStep_];
            if (rat > 1) {
                ratchet = (int)std::floor(within / (period_ / rat));
                ratchet = clampi(ratchet, 0, rat - 1);
            }
        }

        // CV output (holdcv: update only on played steps unless holdcv == 0).
        // The raw pitch (cvbase/cvrange/scale) is computed ONLY at step entry and at
        // pulse/ratchet boundaries and cached: mid-step changes of cvbase/cvrange/
        // scale must NOT move the CV ("change only takes effect on the next step").
        // repeatshift/ratchetshift legitimately re-derive it per pulse/ratchet.
        // tuning/transpose stay live (vibrato input, per minifonion/arpeggio).
        bool holdcv = in("holdcv").value(s) >= kHigh;
        if (playStep_ >= 0 && (!holdcv || plays_)) {
            if (!pitchCached_ || pulse_ != cachedPulse_ || ratchet != cachedRatchet_) {
                rawPitch_ = playedPitch(s, pulse_, ratchet);
                cachedPulse_ = pulse_; cachedRatchet_ = ratchet; pitchCached_ = true;
            }
            cvHeld_ = dressPitch(s, rawPitch_, cn);
        }
        out("cv").set(s, cvHeld_);

        // gate output
        bool gateHigh = false;
        if (plays_ && run && !mute) {
            if (period_ > 0.0 && !gateWin_.empty()) {
                for (auto& w : gateWin_) if (s.tick >= w.first && s.tick < w.second) gateHigh = true;
            } else {
                gateHigh = in("clock").value(s) >= kHigh;   // period unknown: clock level
            }
        }
        out("gate").set(s, gateHigh ? 1.0f : 0.0f);
        emitHousekeeping(s);
    }

    // startofsequence + the frozen-state status outputs (shared by both emit paths).
    void emitHousekeeping(EngineState& s) {
        // 10 ms trigger window (DROID-standard), not a 1-tick pulse.
        out("startofsequence").set(s, (long)s.tick < sosUntil_ ? 1.0f : 0.0f);
        out("startofpart").set(s, 0.0f);                    // forms deferred
        out("currentstep").set(s, (float)(playStep_ < 0 ? 0 : playStep_));
        out("currentpage").set(s, (float)((playStep_ < 0 ? 0 : playStep_) / numFaders_));
        out("accumulator").set(s, (float)acc_);

        long ss = std::lround(in("startstep").value(s));
        long es = in("endstep").connected() ? std::lround(in("endstep").value(s)) : numsteps_;
        out("startstepout").set(s, (float)clampi((int)ss, 1, numsteps_));
        out("endstepout").set(s, (float)clampi((int)es, 1, numsteps_));
    }

    // ---- presets / select --------------------------------------------------
    bool handlePresets(EngineState& s) {
        bool clearAll = risingEdge(caPrev_, in("clearall").value(s));
        bool clr      = risingEdge(clPrev_, in("clear").value(s));
        bool loadPatched   = in("loadpreset").connected();
        bool savePatched   = in("savepreset").connected();
        bool presetPatched = in("preset").connected();
        bool immediate = presetPatched && !loadPatched && !savePatched;
        bool recall = false;

        if (clearAll) {
            seedState(s, cur_);
            for (int p = 0; p < kPresets; p++) preset_[p] = cur_;
            recall = true;
        } else if (clr) {
            seedState(s, cur_);
            if (immediate) preset_[prevPreset_] = cur_;
            recall = true;
        }
        if (savePatched && risingEdge(spPrev_, in("savepreset").value(s)))
            preset_[presetNum(s, in("savepreset").value(s), presetPatched)] = cur_;
        if (loadPatched && risingEdge(lpPrev_, in("loadpreset").value(s))) {
            cur_ = preset_[presetNum(s, in("loadpreset").value(s), presetPatched)];
            recall = true;
        }
        if (immediate) {
            int c = clampi((int)std::lround(in("preset").value(s)), 0, kPresets - 1);
            if (c != prevPreset_) {
                preset_[prevPreset_] = cur_;
                cur_ = preset_[c];
                prevPreset_ = c;
                recall = true;
            }
        }
        return recall;
    }

    int presetNum(EngineState& s, float trigVal, bool presetPatched) {
        float v = presetPatched ? in("preset").value(s) : trigVal;
        return clampi((int)std::lround(v), 0, kPresets - 1);
    }

    bool isSelected(EngineState& s) {
        if (in("selectat").connected())
            return std::lround(in("select").value(s)) == std::lround(in("selectat").value(s));
        if (in("select").connected())
            return in("select").value(s) >= kHigh;
        return true;
    }

    // ---- edit hooks (called from the skins) --------------------------------
    // composemode: a CV edit (fadermode 0) jumps to that step and auditions it —
    // outputs its CV and opens the gate for a 10 ms window. No-op otherwise.
    void onCvEdited(EngineState& s, int step) {
        if (in("composemode").value(s) < kHigh) return;
        composeActive_ = true;
        composeStep_ = step;
        composeGateUntil_ = (long)s.tick + trigTicks(s);
    }

    // ---- small helpers -----------------------------------------------------
    // DROID-standard 10 ms trigger/minimum-gate window, in ticks (min 1).
    static long trigTicks(EngineState& s) {
        long t = std::lround(0.01 * s.tickRateHz); return t < 1 ? 1 : t;
    }
    void triggerSos(EngineState& s) { sosUntil_ = (long)s.tick + trigTicks(s); }
    static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
    static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
    static bool risingEdge(bool& prev, float v) {
        bool now = v >= kHigh; bool rose = now && !prev; prev = now; return rose;
    }

    // config
    bool inited_ = false;
    int  firstFader_ = 1, numFaders_ = 4, numsteps_ = 4;
    SeqState cur_{};
    SeqState preset_[kPresets]{};
    int  prevPreset_ = 0;

    // editing view
    int  shownPage_ = -1, shownMode_ = -1;
    bool wasSelected_ = false;

    // linktonext chain (resolved once at init). chainMain_ is the chain's first
    // instance (nullptr if this is the main or a standalone); chainIndex_ is our
    // 0-based distance from it; linkToNext_ is whether we feed a further member;
    // chainRawFm_ (published by the main each tick) is the chain-wide fadermode.
    SeqCore* chainMain_ = nullptr;
    int  chainIndex_ = 0;
    bool linkToNext_ = false;
    int  chainRawFm_ = 0;
    std::vector<bool> prevTouch_ = std::vector<bool>(kSteps, false);

    // transport
    GateReader clockGate_, resetGate_;
    bool started_ = false, resetPending_ = false, haveClock_ = false;
    uint64_t lastClock_ = 0; double period_ = 0.0;
    int  playPos_ = 0, pulse_ = 0, turn_ = 1, acc_ = 0;
    long clocksSinceReset_ = 0;
    int  playStep_ = -1, playLogical_ = 0;
    bool ledsLit_ = false;   // we currently drive the step LEDs (clear on deselect)
    float latchCvpos_ = 0.0f; int latchRandcv_ = 0;
    float cvHeld_ = 0.0f;
    bool plays_ = false, tie_ = false, lastRandomPos_ = false;
    long sosUntil_ = 0;                         // startofsequence trigger-window end
    uint64_t stepStart_ = 0;
    std::vector<std::pair<uint64_t, uint64_t>> gateWin_;

    // cached raw pitch: recomputed only at step entry + pulse/ratchet boundaries so
    // mid-step cvbase/cvrange/scale changes cannot drift the held CV.
    bool  pitchCached_ = false;
    int   cachedPulse_ = -1, cachedRatchet_ = -1;
    float rawPitch_ = 0.0f;

    // composemode: last CV-edited step, and the short audition-gate window.
    bool composeActive_ = false;
    int  composeStep_ = 0;
    long composeGateUntil_ = 0;

    // preset trigger edges
    bool caPrev_ = false, clPrev_ = false, spPrev_ = false, lpPrev_ = false;
    bool csPrev_ = false, crpPrev_ = false, csePrev_ = false;
    bool luckyPrev_[16] = {false};              // "I Feel Lucky" trigger edges
};

} // namespace droid
