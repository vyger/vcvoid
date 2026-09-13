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
//   * the interactive start/end range (buttonmode 1): a plate/push touch sets the
//     END, a second finger on another step while the first is held sets the
//     START, and the two override slots beat the startstep / endstep inputs until
//     clear / clearall / clearstartend drops them. `setendstep` sets the end "as
//     if you had manually changed it". Every consumer of the range — the play
//     order, the green/red LEDs, luckyscope and startstepout / endstepout — reads
//     it through rangeStart0() / rangeEnd0() on the range OWNER, which for a
//     linked member is the chain main. The override is runtime state: not saved,
//     not part of a preset.
//   * `doublerange`: copies the played range's steps (all per-step columns) into
//     the second half, on this instance AND every linked member, and moves the
//     end there as a manual override. Ignored on a member (the main does it for
//     the whole chain) and a no-op when the range is already at maximum.
//   * per-step timing: repeats (step duration), ratchets (sub-clock), gate
//     patterns (once/all/long/tie), repeatshift / ratchetshift, gatelength,
//     holdcv.
//   * gate probability: the full musical table (always / random 50-25-12% /
//     even / odd / every-4th / conditional) with a per-turn counter + engine RNG.
//   * pitch accumulator (accumulatorrange) — the four accumulator fader
//     positions (idx 4..7 of randomize-CV) shifting the note per turn.
//   * outputs: cv, gate, startofsequence, currentstep, currentpage, accumulator,
//     startstepout, endstepout.
//   * `linktonext` multi-track linking: the FADER and BUTTON/LED editing
//     surfaces are shared across the chain, each addressed independently by the
//     main's fadermode resp. buttonmode — `mode / 10 == chain index`, editing
//     with the local `mode % 10` — so only the addressed instance drives the
//     faders (resp. reads the step buttons and lights the LEDs) and the others
//     release that surface (the boot page shows the addressed instance's steps,
//     not a collision of all of them). luckyshuffle / luckyreverse fired on the
//     main apply the identical permutation to every member, and the four
//     step-order lucky ops (luckyskips, luckyrepeats, luckyshuffle,
//     luckyreverse) are ignored when read on a member (motoquencer.md:704-705).
//     The members' TRANSPORT is remote-controlled from the chain main: each
//     member plays the main's step number (so the main's shiftsteps, play order,
//     repeats and skips carry over, and the member's own are ignored) while its
//     own lane supplies CV, gate, probability, gate pattern and ratchets. `holdcv`
//     on a member takes the extra value 2 (sync the CV to its own gate instead of
//     the main's). A member ignores its own clock/reset/run-order inputs entirely.
//     A member's GEOMETRY (firstfader / numfaders / numsteps) is inherited from
//     the chain main whenever the member leaves that input unpatched — the chain
//     shares one fader bank and one step position, and the Forge's own MFPS
//     generator emits linked lanes with none of the three while the main sets
//     numfaders / numsteps (#34). Stating any of them on a member still wins.
//   * select/selectat overlay, 4 presets, clear / clearall / clearskips /
//     clearrepeats, defaultcv (a notch index when cvnotches >= 2) / defaultgate.
//   * `bulkedit`: while it is high, a user edit — fader, encoder or button —
//     stamps every step to the RIGHT of the edited one with the same value,
//     across pages, up to numsteps. The button lanes copy the pressed step's
//     RESULTING value instead of re-applying the toggle; buttonmode 1 (start/end)
//     is exempt; machine writers (luckyfaders and friends) never stamp.
//   * composemode: while high the transport ignores clock edges; a CV edit
//     (fadermode 0) jumps to that step, outputs its CV and opens a short gate.
//   * "I Feel Lucky": all 16 one-time-randomization triggers (luckyfaders,
//     luckybuttons, luckycvs, luckycvdrift, luckyspread, luckyinvert,
//     luckyrandomizecv, luckygates, luckyskips, luckyties, luckygatepattern,
//     luckygateprob, luckyrepeats, luckyratchets, luckyshuffle, luckyreverse) with
//     luckychance / luckyscope / luckyamount / luckycvbase. Each trigger permanently
//     mutates the dialed sequence and re-commands the motors so the reroll shows.
//   * `constantlength` 1 / 2 — length compensation at the edit sites: a repeats
//     (level 1) or skip (level 2) edit is paid for by the following steps of the
//     start..end range. See the block comment at compensateLength().
//   * song `form` (A / AAAB / AABB / ABAC / AAABAAAC / AB / AAB) with the
//     `startofpart` trigger output: the range window is cut into parts AFTER
//     start/end, the parts always run forward, direction/pingpong apply inside
//     each part, and a wrap (accumulator + startofsequence) marks the end of the
//     complete form, not of a part. A chain member follows the main's form.
//
// DEFERRED (documented, NOT implemented — every one is either a live-performance
// convenience the manual frames as advanced, an interactive gesture with no
// headless analog, or panel-only):
//   * movement `pattern` 1..7 (two-forward-one-back etc.) — pattern 0 (linear)
//     only. It belongs inside stepThrough(), where a form part is walked.
//   * `metricsaver` — the polymetric clock snap-back (read but inert). Unlike
//     `constantlength` (implemented, see below) it needs a running count of the
//     clock cycles since the last external reset plus a rule for re-entering the
//     grid, which the manual only sketches.
//   * keyboard recording: keyboardcv/keyboardgate/keyboardmode/recordmode/
//     recordsilence.
//   * copy / paste / pastefaders / pastebuttons / stepcopy. (Note the
//     shared-button rule for stepcopy + doublerange on one button — doublerange
//     then fires on the RELEASE, if no step was touched meanwhile — lands with
//     stepcopy; see the TODO at the doublerange edge detector.)
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
//   * song forms, three readings the manual leaves open: (a) a window that does
//     not divide evenly gives the EXTRA steps to the EARLIER parts, so A is the
//     longer part ("or else your parts won't have equal size (which on the other
//     hand could be funny anyway)" is all the manual offers); (b) `startofpart`
//     fires on the first part of a form too, coinciding with startofsequence —
//     a part boundary is a part boundary — and never fires at form = 0; (c) the
//     parts are INDEPENDENT WINDOWS with respect to skips, so a skipped step
//     shortens only the part entry it sits in, and a part whose steps are all
//     skipped is passed over without a startofpart.
//   * DROID triggers are 10 ms, not 1 tick: startofsequence emits a 10 ms window,
//     gatelength = 0 floors each once/all gate to a ~10 ms minimum, and the
//     composemode audition gate opens for the same 10 ms after a CV edit. The
//     manual gives no compose-gate duration ("a short time") — literal reading.
//   * scale-change note memory ("faders remember their original note") is modelled
//     positionally: the raw 0..1 fader position is kept, so a note re-appears when
//     the scale is restored, but the exact original semitone is not separately
//     stored.
//   * probability decided once at step entry (pulse 0), using the engine RNG.
//   * the start/end gesture with two plates going down in the SAME engine tick:
//     the manual only describes the sequential gesture ("first setting an end
//     step and *holding* that button"). Both skins walk their lanes ascending, so
//     the lower lane becomes the end/anchor and the higher lane the start.
//   * `setendstep` at boot: "whenever this number changes" leaves the FIRST
//     observed value undefined. It is latched silently, so a constant setendstep
//     — or a buttongroup sitting at its start value — never shrinks the sequence.
//   * `doublerange` when the doubled range does not fit (a 5-step range of 8):
//     the manual only rules out a range already at maximum. Copy as many steps as
//     fit and clamp the end to numsteps; the alternative (no-op unless 2L fits)
//     would make doublerange silently dead on most odd ranges.
//   * `bulkedit` (see bulkStamp): "at the right of the modified step" is read as
//     every higher step number up to numsteps — the whole track, not just the
//     played range. The pitch edit's gate auto-on stays with the fader that
//     really MOVED (a stamp copies the addressed lane and nothing else, so one
//     held button cannot switch the whole track's gates on), and buttonmode 1
//     (start/end) is exempt because a range gesture has no per-step value.
//   * `bulkedit` + `constantlength` together (see the note at compensateLength):
//     a bulk stamp BYPASSES the compensation. While bulkedit is high, a repeats
//     or skip edit and the stamp it lays to the right are honoured verbatim —
//     every step to the right takes the value, no step to the left moves, and the
//     track length changes by the full amount. The manual never combines the two
//     inputs, and composing them literally is incoherent (each stamped write
//     would compensate itself out of the steps the same gesture is writing).
//     Single-step edits (bulkedit low) and the machine writers are unaffected.
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

        // linktonext: the whole chain reads ONE fadermode and ONE buttonmode off
        // the main instance (linked members leave both unwired; motoquencer.md
        // "add 10 to fadermode or buttonmode"). `raw / 10` picks which chain
        // member owns the physical faders (resp. the step buttons + LEDs); it
        // edits with `raw % 10`, the others release that surface. The two are
        // independent: fadermode 11 + buttonmode 0 shows the linked member's
        // faders and the main's buttons. A standalone sequencer (no link) keeps
        // its raw modes clamped and always owns both surfaces.
        bool inChain = chainMain_ != nullptr || linkToNext_;
        if (!chainMain_) {
            chainRawFm_ = (int)std::lround(in("fadermode").value(s));
            chainRawBm_ = (int)std::lround(in("buttonmode").value(s));
        }
        const SeqCore* main = chainMain_ ? chainMain_ : this;
        bool faderOwner, buttonOwner;
        int  fadermode  = resolveChainMode(main->chainRawFm_, inChain, 7, faderOwner);
        int  buttonmode = resolveChainMode(main->chainRawBm_, inChain, 3, buttonOwner);

        // --- presets / clear (always run) -----------------------------------
        bool recall = handlePresets(s);
        if (risingEdge(csPrev_, in("clearskips").value(s)))
            for (int i = 0; i < numsteps_; i++) cur_.skip[i] = false;
        if (risingEdge(crpPrev_, in("clearrepeats").value(s)))
            for (int i = 0; i < numsteps_; i++) cur_.repeats[i] = 1;
        handleStartEndInputs(s);
        // The chain main rewrote our steps this tick (doublerange): re-command
        // the motors so the copied values show. Picked up here because the main
        // always ticks before its members.
        if (pendingRecall_) { pendingRecall_ = false; recall = true; }
        // `doublerange` (also always-run — MFPS fires it from a track button,
        // not from the fader selection). TODO: when `stepcopy` lands, the
        // shared-button rule (motoquencer.md §"Copy & paste single steps") moves
        // this to the FALLING edge whenever `stepcopy` is connected too.
        bool drFired = risingEdge(drPrev_, in("doublerange").value(s));
        // A chain member ignores its own `doublerange`: the main copies every
        // member's steps as well, and the Forge's MFPS generator wires the input
        // on every lane of a track — a member that fired too would double an
        // already-doubled range. (Same rule as the step-order lucky ops.)
        if (drFired && !chainMain_ && doubleRange(s)) recall = true;

        // "I Feel Lucky" one-time randomization (also always-run — a trigger fires
        // regardless of selection). A fired op permanently mutates the sequence, so
        // force a motor recall to re-command the faders to the rerolled values (the
        // manual: "you will immediately see them moving around").
        if (applyLucky(s, page, fadermode, buttonmode, faderOwner, buttonOwner)) recall = true;
        // A chain member mirrors the MAIN's step rearrangements (luckyshuffle /
        // luckyreverse): "the exact same rearrangement of steps will happen at
        // the linked sequencers" (motoquencer.md). Pull-style like the transport
        // epochs: the main published the permutation this tick (it ticks first).
        if (chainMain_ && chainMain_->orderEpoch_ != seenOrderEpoch_) {
            seenOrderEpoch_ = chainMain_->orderEpoch_;
            permuteSteps(chainMain_->orderT_, chainMain_->orderSrc_);
            recall = true;
        }

        // this instance shows on the faders (resp. buttons + LEDs) only while
        // selected AND the chain fadermode (resp. buttonmode) addresses it (a
        // standalone is always its own owner).
        bool showFaders  = selected && faderOwner;
        bool showButtons = selected && buttonOwner;
        // A plate release is only observed while we read the plates, so a
        // start/end gesture anchor held across a deselect (or across a loss of
        // button ownership in a chain) would silently turn the NEXT single
        // press into a start press. Drop it.
        if (!showButtons) seAnchorLane_ = -1;

        // `bulkedit` is a plain level with no gesture: sample it once here, just
        // before the edit surface runs, and every edit made this tick stamps (or
        // does not stamp) the steps to its right accordingly. bulkStampFrom_ is
        // the per-tick record of what was stamped (see markStamped).
        bulkEdit_ = in("bulkedit").value(s) >= kHigh;
        bulkStampFrom_ = -1;

        // recall the motors when the visible page/mode changed
        if (page != shownPage_ || fadermode != shownMode_) recall = true;
        shownPage_ = page;
        shownMode_ = fadermode;

        // --- fader + touch editing (only while showing) ---------------------
        if (showFaders || showButtons)
            editSurface(s, page, fadermode, buttonmode, recall, showFaders, showButtons);
        if (!showFaders) wasSelected_ = false;   // re-taking the faders re-commands them (recall)

        // --- transport ------------------------------------------------------
        transport(s);

        // --- step LEDs (panel-only; after transport so playStep_ is fresh) ---
        updateLeds(s, page, buttonmode, showButtons);

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
    // `faders` / `buttons` say which halves of the surface this instance owns
    // this tick (a chained member can own one without the other, see tick()).
    virtual void editSurface(EngineState& s, int page, int fm, int bm, bool recall,
                             bool faders, bool buttons) = 0;
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
        // The main precedes us in patch order, so it normally ticked (and
        // inited) first; make sure of it before reading its geometry.
        if (chainMain_ && !chainMain_->inited_) chainMain_->init(s);

        // Geometry. A chain member edits the SAME physical faders as the main
        // and plays the main's step, so an unset `firstfader` / `numfaders` /
        // `numsteps` inherits from the chain main rather than falling back to
        // the per-circuit default (fader 1, the whole M4 bank) (#34): the
        // Forge's MFPS generator writes linked lanes without any of the three,
        // and with a bank-sized single page a member's `page` input would clamp
        // to 0 and pages 2+ of that lane could never be reached on hardware.
        long ff = std::lround(in("firstfader").value(s));
        if (in("firstfader").connected() && ff >= 1) firstFader_ = (int)ff;
        else if (chainMain_)                         firstFader_ = chainMain_->firstFader_;
        else                                         firstFader_ = ff < 1 ? 1 : (int)ff;
        int avail = availableLanes(s);
        long nf = std::lround(in("numfaders").value(s));
        if (in("numfaders").connected() && nf > 0) numFaders_ = (int)nf;
        else if (chainMain_)                       numFaders_ = chainMain_->numFaders_;
        else                                       numFaders_ = avail > 0 ? avail : 4;
        numFaders_ = clampi(numFaders_, 1, kSteps);
        long ns = std::lround(in("numsteps").value(s));
        if (in("numsteps").connected() && ns > 0) numsteps_ = (int)ns;
        else if (chainMain_)                      numsteps_ = chainMain_->numsteps_;
        else                                      numsteps_ = numFaders_;
        numsteps_ = clampi(numsteps_, 1, kSteps);

        seedState(s, cur_);
        for (int p = 0; p < kPresets; p++) preset_[p] = cur_;
        prevPreset_ = clampi((int)std::lround(in("preset").value(s)), 0, kPresets - 1);
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

    // ---- constantlength ----------------------------------------------------
    // motoquencer.md:922. Level 1: "every change in the *repeats* of a step is
    // compensated by changing the repeats in the following steps. E.g. if you
    // increase the number of repeats from 4 to 5 in step 3 [...] the repeats in
    // step 4 are reduced by 1. If they are already 1, step 5 is tried an so on,
    // until it wrap around to step 1." Level 2: "also the *skip* setting of steps
    // is honored and modified in order to keep the length constant. A skipped step
    // essentially has the length 0 (or 0 repeats). The componsation is now done not
    // only when the repeats are changed but also when skip is switched on or off on
    // a step. All the compensation is only active with the range that is set with
    // the start and end step."
    //
    // It hooks the EDIT sites, never the transport: the length is rebalanced the
    // moment a repeats or skip edit lands, and the sequencer then plays whatever it
    // finds, so the step that is currently running reads its (possibly rewritten)
    // repeat count at the next clock edge exactly as it does after a direct edit.
    //
    // Bulk operations are deliberately outside the feature: clearrepeats /
    // clearskips / luckyrepeats / luckyskips / presets / doublerange all rewrite
    // the whole sequence at once, and the manual frames the compensation as the
    // answer to "a change in the repeats of a step".
    //
    // SPEC-GAP: a `bulkedit` gesture is one of those bulk operations. While
    // bulkedit is high, a repeats or skip edit AND the stamp it lays on every step
    // to its right bypass compensation entirely (BulkBypass below). The manual
    // never puts the two features in the same sentence, and composing them
    // literally is incoherent: each stamped write would compensate itself out of
    // the steps the same gesture is writing, so a CTRL move of the repeats fader
    // leaves a lane that is neither the value asked for nor a constant length.
    // "All faders to the right take this value" is an explicit whole-track
    // gesture, so it is honoured verbatim: every step to the right gets the value,
    // no step to the LEFT moves, and the track length changes by the full amount.
    // A single-step edit (bulkedit low) still compensates exactly as before, and
    // so do the machine writers, which never run inside the bypass.
    //
    // SPEC-GAPs (the manual is silent; deterministic readings):
    //   * A step's LENGTH is `skip ? 0 : repeats` at BOTH levels, so the skip a
    //     repeats edit automatically clears (motoquencer.md:183) is part of the
    //     measured change even at level 1. The levels differ in what may be
    //     MODIFIED: level 1 never touches a skip, so a skipped step has no
    //     capacity and the forward search steps over it.
    //   * A change the range cannot absorb is still applied: compensate as far as
    //     the slack goes and drop the remainder. The manual only promises the
    //     feature "*tries* to keep the actual length constant", and refusing or
    //     snapping back the edit would make a motor fader fight the hand on it.
    //   * Level 2 may UN-skip a step to buy length, the mirror of skipping one to
    //     spend it. A skipped candidate is un-skipped only when its whole repeat
    //     count fits in what is still owed, so the gesture is the exact inverse.
    //   * The level is read off the chain MAIN, like the range: the Forge's MFPS
    //     generator wires `constantlength` on a track's main lane only.
    //
    // The length in clock pulses one step contributes to the played range.
    int stepLength(int step) const {
        step = clampi(step, 0, kSteps - 1);
        return cur_.skip[step] ? 0 : cur_.repeats[step];
    }

    // 0 = off, 1 = repeats only, 2 = repeats + skips.
    int constantLength(EngineState& s) {
        SeqCore* o = rangeOwner();
        return clampi((int)std::lround(o->in("constantlength").value(s)), 0, 2);
    }

    // Scope guard around ONE user-edit gesture: while `bulkedit` is high it turns
    // the compensation off for the whole gesture — the edited step's own write and
    // every step the stamp then rewrites. Off (a plain single-step edit) it is a
    // no-op, and it is never entered by the machine writers (lucky*, clear*,
    // presets, doublerange), which reach setLaneValue directly.
    struct BulkBypass {
        SeqCore& o;
        bool saved;
        explicit BulkBypass(SeqCore& c) : o(c), saved(c.bulkBypass_) {
            if (c.bulkEdit_) c.bulkBypass_ = true;
        }
        ~BulkBypass() { o.bulkBypass_ = saved; }
    };

    // Absorb the length change an edit to `step` just made into the OTHER steps of
    // the range. `before` is that step's length before the edit; `skipEdit` says the
    // edit itself was a skip toggle (only level 2 compensates those). Returns true
    // if any other step changed.
    bool compensateLength(EngineState& s, int step, int before, bool skipEdit) {
        if (bulkBypass_) return false;          // see the SPEC-GAP note above
        int level = constantLength(s);
        if (level == 0) return false;
        if (skipEdit && level < 2) return false;
        int owed = stepLength(step) - before;   // > 0: too long now, take it back
        if (owed == 0) return false;
        int a = rangeStart0(s), b = rangeEnd0(s);
        int lo = a < b ? a : b, hi = a < b ? b : a;
        if (step < lo || step > hi) return false;   // only inside start..end
        int n = hi - lo + 1;
        bool changed = false;
        for (int k = 1; k < n && owed != 0; k++) {
            int i = lo + (step - lo + k) % n;        // forward, wrapping in-range
            if (owed > 0) {                          // spend: shorten the others
                if (cur_.skip[i]) continue;          // already length 0
                int cap = level >= 2 ? cur_.repeats[i] : cur_.repeats[i] - 1;
                int take = owed < cap ? owed : cap;
                if (take <= 0) continue;
                if (take == (int)cur_.repeats[i]) cur_.skip[i] = true;   // level 2
                else cur_.repeats[i] = (uint8_t)(cur_.repeats[i] - take);
                owed -= take;
            } else {                                 // buy: lengthen the others
                int want = -owed;
                if (cur_.skip[i]) {
                    if (level < 2 || (int)cur_.repeats[i] > want) continue;
                    cur_.skip[i] = false;            // un-skip: exactly repeats back
                    owed += cur_.repeats[i];
                } else {
                    int cap = 16 - (int)cur_.repeats[i];
                    int take = want < cap ? want : cap;
                    if (take <= 0) continue;
                    cur_.repeats[i] = (uint8_t)(cur_.repeats[i] + take);
                    owed += take;
                }
            }
            changed = true;
            refreshLane(s, i);
        }
        return changed;
    }

    // A compensated step's value moved without the user touching its handle, so the
    // surface has to be re-commanded — on an M4 the untouched physical fader would
    // otherwise be read back next pass and silently undo the compensation. Default
    // no-op: an E4's encoders are relative and have no position to fight.
    virtual void refreshLane(EngineState& s, int step) { (void)s; (void)step; }

    // Write a step's value in a fadermode from a raw 0..1 fader position, snapping
    // it to that lane's notch grid. `snapped` receives the rest position of the
    // value actually stored (what the motor is commanded to); the return value
    // says whether the stored value really changed.
    //
    // This is the plain column write, shared by every writer of a lane: the user
    // edit (applyEdit, which adds the interaction semantics on top) and the
    // machine-driven rerolls of luckyfaders.
    bool setLaneValue(EngineState& s, int fm, int step, float pos, float& snapped) {
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
                      int was = stepLength(step);
                      cur_.repeats[step] = (uint8_t)(v + 1);
                      if (ch) { cur_.skip[step] = false;   // only on a real CHANGE
                                compensateLength(s, step, was, false); }
                      snapped = v / 15.0f; return ch; }
            case 4: { int v = snapIdx(4); bool ch = v != cur_.gatepat[step];
                      cur_.gatepat[step] = (uint8_t)v; snapped = v / 3.0f; return ch; }
            case 5: { int v = snapIdx(8); bool ch = (v + 1) != cur_.ratchets[step];
                      cur_.ratchets[step] = (uint8_t)(v + 1); snapped = v / 7.0f; return ch; }
            case 6: { bool v = pos >= 0.5f; bool ch = v != cur_.gate[step];
                      cur_.gate[step] = v; snapped = v ? 1.0f : 0.0f; return ch; }
            default: { bool v = pos >= 0.5f; bool ch = v != cur_.skip[step];
                      int was = stepLength(step);
                      cur_.skip[step] = v; snapped = v ? 1.0f : 0.0f;
                      if (ch) compensateLength(s, step, was, true);
                      return ch; }
        }
    }

    // Apply a USER fader position to a step's value in a fadermode (the edit
    // entry point the skins call for a fader that moved).
    // Returns true if the stored value actually changed (drives gate auto-on).
    bool applyEdit(EngineState& s, int fm, int step, float pos, float& snapped) {
        BulkBypass bypass(*this);            // constantlength is off under bulkedit
        bool changed = setLaneValue(s, fm, step, pos, snapped);
        if (changed) bulkStamp(s, fm, step);
        return changed;
    }

    // ---- bulkedit ----------------------------------------------------------
    // motoquencer.md `bulkedit`: "if you move one fader (or encoder) or
    // touch/press one button, all other faders (or encoders) *at the right of the
    // modified step* will move along to the same value – even in the steps that
    // are currently on another page".
    //
    // A plain level (no gesture), sampled once per tick in tick() and read by the
    // edit entry points below. "At the right" = every higher step number up to
    // `numsteps` — the whole track, not just the played range, and never
    // leftwards. Only a USER edit stamps: luckyfaders and the other machine
    // writers go through setLaneValue and draw their own value per step.
    //
    // Only the addressed lane is copied. The pitch edit's gate auto-on stays with
    // the fader that really moved (SPEC-GAP: the manual describes the OTHER faders
    // "moving along to the same value" and nothing else — a single CTRL move must
    // not switch the whole track's gates on).
    //
    // In a `linktonext` chain the stamp is local by construction: only the
    // instance that owns the edited surface runs editSurface, so it stamps its own
    // steps over its own (inherited) geometry and the other lanes of the track
    // keep their values.
    void bulkStamp(EngineState& s, int fm, int from) {
        if (!bulkEdit_ || from + 1 >= numsteps_) return;
        float pos = storedPos(s, fm, from), snapped;
        for (int i = from + 1; i < numsteps_; i++) setLaneValue(s, fm, i, pos, snapped);
        markStamped(from + 1);
    }

    // The button-lane half. "The same value" is the RESULTING value of the pressed
    // step (gate on/off, gate pattern, skip), not the toggle: re-toggling every
    // step to the right would flip the ones that already agree with the edit,
    // which is the opposite of what the manual asks for.
    //
    // buttonmode 1 (start/end) is EXEMPT — it is a two-finger range gesture, not a
    // per-step value, so there is nothing to copy (SPEC-GAP: the manual's bulkedit
    // row does not mention it; it talks about a value the buttons to the right can
    // take).
    void bulkStampButton(int bm, int from) {
        if (!bulkEdit_ || from + 1 >= numsteps_) return;
        for (int i = from + 1; i < numsteps_; i++) {
            switch (bm) {
                case 0: cur_.gate[i]    = cur_.gate[from];    break;
                case 2: cur_.gatepat[i] = cur_.gatepat[from]; break;
                case 3: cur_.skip[i]    = cur_.skip[from];    break;
                default: return;                              // 1 = start/end: exempt
            }
        }
        markStamped(from + 1);
    }

    // Remember the leftmost step the stamp rewrote this tick. The skins walk their
    // lanes ascending, so a stamped lane to the RIGHT of the edited one is still
    // ahead of them in this very tick: without this it would be read back from its
    // own (unmoved) physical fader and the fresh value lost again. bulkStamped()
    // makes those lanes take the motorized-recall branch instead.
    void markStamped(int step) {
        if (bulkStampFrom_ < 0 || step < bulkStampFrom_) bulkStampFrom_ = step;
    }
    // True while a lane the bulk stamp rewrote is still to be visited this tick.
    bool bulkStamped(int step) const {
        return bulkStampFrom_ >= 0 && step >= bulkStampFrom_;
    }

    // Push-button edit shared by both skins (M4 touch plate / E4 encoder push).
    // BOTH edges are reported, not just the press: buttonmode 1 (start/end) is a
    // two-finger gesture whose first finger stays down as the anchor, so the
    // release is what ends it. `lane` is the physical plate index within this
    // instance's lanes; `step` is the step it currently addresses (page-mapped).
    void plateEdge(EngineState& s, int bm, int lane, int step, bool pressed) {
        BulkBypass bypass(*this);            // constantlength is off under bulkedit
        if (!pressed) {
            if (seAnchorLane_ == lane) seAnchorLane_ = -1;
            return;
        }
        switch (bm) {
            case 0: cur_.gate[step] = !cur_.gate[step]; break;
            case 1: startEndPress(lane, step); break;
            case 2: cur_.gatepat[step] = (cur_.gatepat[step] + 1) & 3; break;
            // A skip toggled here is a length change like any other, so it is
            // compensated at constantlength level 2 (see compensateLength) —
            // except under bulkedit, where the whole gesture bypasses it.
            case 3: { int was = stepLength(step);
                      cur_.skip[step] = !cur_.skip[step];
                      compensateLength(s, step, was, true); break; }
            default: break;
        }
        bulkStampButton(bm, step);          // `bulkedit`, see bulkStampButton
    }

    // buttonmode 1, motoquencer.md §"Start and end": "Touching a button changes
    // the *end* step. You can set the start step by first setting an end step and
    // *holding* that button and then – with a second finger – press another step.
    // This will set the start step."
    //
    // So a lone press only ever moves the END; the START is reachable only while
    // another plate is still held. The two override slots are independent — a
    // single touch does not reset the start, which is what makes "clearstartend
    // resets the end step to its default" coherent. The anchor is a physical
    // finger and so lives on the instance that owns the buttons, while the range
    // lives on the chain main (see rangeOwner): a gesture performed on a linked
    // member (buttonmode 1x) edits the main's range, since a member has no play
    // order of its own.
    //
    // SPEC-GAP: two plates going down in the SAME engine tick. The manual only
    // describes the sequential gesture. Both skins walk their lanes ascending, so
    // the lower lane becomes the end/anchor and the higher lane the start.
    void startEndPress(int lane, int step) {
        SeqCore* o = rangeOwner();
        if (seAnchorLane_ >= 0 && seAnchorLane_ != lane) {
            o->manualStart0_ = step;            // second finger -> START
        } else {
            o->manualEnd0_ = step;              // single touch -> END
            seAnchorLane_ = lane;               // ...and becomes the anchor
        }
    }

    // `clearstartend` and `setendstep`, the non-gestural halves of the same
    // feature. A linked member ignores both (motoquencer.md:677 — the range is
    // the main's), but its edge latches are still advanced so nothing fires late.
    void handleStartEndInputs(EngineState& s) {
        bool cleared = risingEdge(csePrev_, in("clearstartend").value(s));
        if (chainMain_) return;                 // a member has no range of its own
        // "A trigger here clears the manual settings of the start and end step",
        // i.e. both slots fall back to the startstep / endstep inputs.
        if (cleared) manualStart0_ = manualEnd0_ = -1;
        if (!in("setendstep").connected()) return;
        // "As soon as you send a different number than 0, the end step is set to
        // that value as if you had manually changed it [...] The input value 0
        // does not change the end step."
        // SPEC-GAP: the manual never defines the FIRST observed value. Latch it
        // silently, so a constant setendstep — or a buttongroup sitting at its
        // start value at boot — cannot shrink the sequence behind your back.
        int v = (int)std::lround(in("setendstep").value(s));
        if (!setEndSeen_) { setEndSeen_ = true; prevSetEnd_ = v; return; }
        if (v == prevSetEnd_) return;
        prevSetEnd_ = v;
        if (v != 0) manualEnd0_ = clampi(v - 1, 0, numsteps_ - 1);
    }

    // The next instance of THIS chain in patch order, or nullptr at its end.
    SeqCore* nextChainMember() {
        SeqCore* p = asSeq(nextPeer());
        return (p && p->chainMain_ == rangeOwner()) ? p : nullptr;
    }

    // `doublerange` — motoquencer.md §"Doubling the range": "A trigger here
    // doubles the current playing range and copies the contents of the previous
    // range to the second half of the new range. This only works if the playing
    // range (start/stop) is not at maximum." The end "is set to step 16 (and
    // counts as manually modified)", i.e. it becomes an interactive override
    // exactly like a plate press; the START is left alone, so a range 5..8
    // doubles to 5..12.
    //
    // What is copied is "the contents of the previous range" — and copying a
    // step copies "always *all* aspects of the step" (§"Copy & paste"), which is
    // every per-step column, i.e. copyStep. Linked sequencers are handled with
    // it ("If you have linked sequencers, those will automatically be handled as
    // well"), over the same step indices, since the whole chain plays one step
    // number.
    //
    // SPEC-GAP: the manual only rules out a range already at maximum, leaving a
    // range that does not have room for a full second copy undefined (5 steps of
    // 8). Literal reading: copy as many as fit and clamp the end to numsteps —
    // the alternative (no-op unless 2L fits) would make doublerange silently
    // dead on most odd ranges. Returns whether anything happened.
    bool doubleRange(EngineState& s) {
        int a = rangeStart0(s), b = rangeEnd0(s);
        int hi = numsteps_ - 1;
        int dir = (b >= a) ? 1 : -1;                 // a reversed range doubles backwards
        if (dir > 0 ? (b >= hi) : (b <= 0)) return false;   // already at maximum
        int len  = (dir > 0 ? b - a : a - b) + 1;
        int room = dir > 0 ? hi - b : b;             // steps left beyond the end
        int fit  = len < room ? len : room;
        for (int k = 0; k < fit; k++) {
            int src = a + dir * k, dst = b + dir * (k + 1);
            copyStep(cur_, dst, cur_, src);
            for (SeqCore* m = nextChainMember(); m; m = m->nextChainMember())
                copyStep(m->cur_, dst, m->cur_, src);
        }
        manualEnd0_ = clampi(b + dir * fit, 0, hi);  // "counts as manually modified"
        for (SeqCore* m = nextChainMember(); m; m = m->nextChainMember())
            m->pendingRecall_ = true;
        return true;
    }

    // ---- the played range (startstep / endstep) ----------------------------
    // ONE accessor pair for every consumer of the range — the play order, the
    // buttonmode-1 LEDs, `luckyscope` and the startstepout / endstepout
    // outputs — so they cannot drift apart.
    //
    // The range belongs to the chain MAIN. A linked member "does not react to
    // clock, reset, startstep, endstep, form, direction, pingpong, pattern,
    // autoreset, shiftsteps [...] Instead the current step number of the linked
    // sequencer will always be the same as the step number of the main
    // sequencer" (motoquencer.md:677) — so its own startstep / endstep inputs
    // are ignored everywhere, not just in the transport, and the range it
    // reports and scopes lucky ops to is the one it actually plays.
    SeqCore* rangeOwner() { return chainMain_ ? chainMain_ : this; }

    // 0-based, clamped into this instance's step count. An interactive override
    // (the buttonmode-1 gesture, setendstep, doublerange) wins over the
    // startstep / endstep inputs until a clear / clearall / clearstartend drops
    // it: "the manual settings override the inputs startstep and endstep until
    // you do a clear or clearstartend" (motoquencer.md). The two slots are
    // independent, so a manual end can sit over an input-driven start.
    int rangeStart0(EngineState& s) {
        SeqCore* o = rangeOwner();
        if (o->manualStart0_ >= 0) return clampi(o->manualStart0_, 0, numsteps_ - 1);
        return clampi((int)std::lround(o->in("startstep").value(s)) - 1, 0, numsteps_ - 1);
    }
    int rangeEnd0(EngineState& s) {
        SeqCore* o = rangeOwner();
        if (o->manualEnd0_ >= 0) return clampi(o->manualEnd0_, 0, numsteps_ - 1);
        long es = o->in("endstep").connected()
                ? std::lround(o->in("endstep").value(s)) : (long)o->numsteps_;
        return clampi((int)es - 1, 0, numsteps_ - 1);
    }

    // ---- I Feel Lucky ------------------------------------------------------
    // One-time randomization triggers (manual "I Feel Lucky"). Each trigger, when
    // it rises, permanently rerolls a subset of the dialed steps. `applyLucky`
    // edge-detects all 16 in a fixed order and returns whether any fired (so tick()
    // can force a motor recall). Runs regardless of selection.
    // A chain member's raw mode (shared off the main) maps to its own lane as
    // `raw - 10 * chainIndex_`; it owns the surface iff `raw / 10` is its index.
    int resolveChainMode(int raw, bool inChain, int hi, bool& owner) const {
        if (!inChain) { owner = true; return clampi(raw, 0, hi); }
        int ownerIdx = raw < 0 ? 0 : raw / 10;
        owner = ownerIdx == chainIndex_;
        return clampi(raw - 10 * chainIndex_, 0, hi);
    }

    bool applyLucky(EngineState& s, int page, int fm, int bm, bool faderOwner, bool buttonOwner) {
        static const char* kOps[16] = {
            "luckyfaders", "luckybuttons", "luckycvs", "luckycvdrift", "luckyspread",
            "luckyinvert", "luckyrandomizecv", "luckygates", "luckyskips", "luckyties",
            "luckygatepattern", "luckygateprob", "luckyrepeats", "luckyratchets",
            "luckyshuffle", "luckyreverse"};
        bool fired = false;
        for (int op = 0; op < 16; op++) {
            if (!risingEdge(luckyPrev_[op], in(kOps[op]).value(s))) continue;
            // On a chain member the four step-ORDER ops are ignored: skips and
            // repeats come from the main's play order, and shuffle/reverse are
            // mirrored from the main (motoquencer.md: "with the exception of
            // luckyskips, luckyrepeats, luckyshuffle and luckyreverse").
            if (chainMain_ && (op == 8 || op == 12 || op == 14 || op == 15)) continue;
            fired = true;
            applyLuckyOp(s, op, luckyTargets(s, page), fm, bm, faderOwner, buttonOwner);
        }
        return fired;
    }

    // The set of step indices a lucky op affects: the candidate range chosen by
    // `luckyscope`, then each candidate kept with probability `luckychance` (one RNG
    // draw per candidate, ascending step order, so a seeded run is reproducible).
    std::vector<int> luckyTargets(EngineState& s, int page) {
        int scope = (int)std::lround(in("luckyscope").value(s));
        float chance = clampf(in("luckychance").value(s), 0.0f, 1.0f);
        int s0 = rangeStart0(s), e0 = rangeEnd0(s);
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

    // Move whole step tuples: step T[i] takes the tuple that was at src[i]. The
    // permutation is also published for chain members (orderEpoch_, see tick()).
    void permuteSteps(const std::vector<int>& T, const std::vector<int>& src) {
        SeqState snap = cur_;
        int n = (int)T.size();
        for (int i = 0; i < n; i++) copyStep(cur_, T[i], snap, src[i]);
    }
    void publishOrder(const std::vector<int>& T, const std::vector<int>& src) {
        orderT_ = T; orderSrc_ = src; orderEpoch_++;
    }
    void luckyReverse(const std::vector<int>& T) {
        std::vector<int> src(T.rbegin(), T.rend());
        permuteSteps(T, src);
        publishOrder(T, src);
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
        permuteSteps(T, order);
        publishOrder(T, order);
    }

    // Apply one lucky operation to its target steps. `amount`/`lvbase` are the
    // luckyamount / luckycvbase inputs; all randomness is drawn ascending from the
    // engine RNG. cvpos/CV work in 0..1 position space (playback maps it to the CV
    // range), matching "within the allowed CV range".
    void applyLuckyOp(EngineState& s, int op, const std::vector<int>& T, int fm, int bm,
                      bool faderOwner, bool buttonOwner) {
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
                // Each target gets its OWN draw, so this is the plain column write
                // (setLaneValue), not the user-edit entry point.
                for (int i : T) {                 // amount caps the max; fm 0 offsets by lvbase
                    float pos = (fm == 0 ? lvbase : 0.0f) + U() * amount, snapped;
                    setLaneValue(s, fm, i, clampf(pos, 0.0f, 1.0f), snapped);
                }
                break;
            case 1:   // luckybuttons: reroll the current button lane (per buttonmode).
                // Same ownership rule as luckyfaders: `bm` is only the real shown
                // lane on the button OWNER; elsewhere it is the chain-clamped alias.
                if (!buttonOwner) break;
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
        int start0 = rangeStart0(s), end0 = rangeEnd0(s);
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
    // The USER-edit entry point of the E4 skin, so a change bulk-stamps the steps
    // to the right (see bulkStamp) exactly like a fader move does.
    bool adjustByDetents(EngineState& s, int fm, int step, long detents) {
        BulkBypass bypass(*this);            // constantlength is off under bulkedit
        bool changed = nudgeLaneValue(s, fm, step, detents);
        if (changed) bulkStamp(s, fm, step);
        return changed;
    }

    // The plain relative column write behind adjustByDetents (the counterpart of
    // setLaneValue for encoders).
    bool nudgeLaneValue(EngineState& s, int fm, int step, long detents) {
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
                      int was = stepLength(step);
                      cur_.repeats[step] = (uint8_t)(v + 1);
                      if (ch) { cur_.skip[step] = false;   // only on a real CHANGE
                                compensateLength(s, step, was, false); }
                      return ch; }
            case 4: { int v = nudgeIdx(cur_.gatepat[step], 3); bool ch = v != cur_.gatepat[step];
                      cur_.gatepat[step] = (uint8_t)v; return ch; }
            case 5: { int v = nudgeIdx(cur_.ratchets[step] - 1, 7); bool ch = (v + 1) != cur_.ratchets[step];
                      cur_.ratchets[step] = (uint8_t)(v + 1); return ch; }
            case 6: { int v = nudgeIdx(cur_.gate[step] ? 1 : 0, 1); bool ch = (bool)v != cur_.gate[step];
                      cur_.gate[step] = v; return ch; }
            default: { int v = nudgeIdx(cur_.skip[step] ? 1 : 0, 1); bool ch = (bool)v != cur_.skip[step];
                      int was = stepLength(step);
                      cur_.skip[step] = v;
                      if (ch) compensateLength(s, step, was, true);
                      return ch; }
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

    // ---- play order --------------------------------------------------------
    // The logical play order for one full cycle is built in layers, so that each
    // step-order feature owns exactly one of them:
    //
    //   1. rangeWindow()  — the steps between start and end, in range order.
    //   2. formSequence() — that window cut into parts (A/B/C), played in the
    //                       order the `form` input names.
    //   3. stepThrough()  — ONE part walked per direction + pingpong.
    //
    // playOrder() stitches the layers together and hands the transport a flat
    // list of logical step indices plus, per position, which part of the form
    // that position belongs to.
    struct PlayOrder {
        std::vector<int> steps;    // logical step index at each play position
        std::vector<int> part;     // the form part entry each position belongs to
        bool formed = false;       // false when form = 0 — no parts, no startofpart
    };

    // Layer 1: the steps between start and end, in range order. A reversed range
    // (startstep after endstep) is legal and "reverses the playing order", so the
    // window itself may descend.
    std::vector<int> rangeWindow(EngineState& s) {
        int start0 = rangeStart0(s), end0 = rangeEnd0(s);
        std::vector<int> w;
        if (start0 <= end0) for (int i = start0; i <= end0; i++) w.push_back(i);
        else                for (int i = start0; i >= end0; i--) w.push_back(i);
        return w;
    }

    // Layer 3: walk ONE window — a form part, or the whole range when there is no
    // form — according to direction and pingpong, appending to `outSteps`.
    //
    // The order built here is exactly what a movement `pattern` (issue #54)
    // re-walks: the manual's patterns move "according to `direction` and
    // `pingpong`", so "one step forward" means the next element of the sequence
    // below. The walk itself cannot live here — patterns 5..7 are random and must
    // draw once per played step rather than once per playOrder() rebuild — so it
    // sits in nextPlayedPos(), confined to one part run (see runBounds).
    void stepThrough(EngineState& s, std::vector<int> part, std::vector<int>& outSteps) {
        if (part.empty()) return;
        if (in("direction").value(s) >= kHigh)
            std::reverse(part.begin(), part.end());
        if (in("pingpong").value(s) >= kHigh && part.size() > 1)
            for (int k = (int)part.size() - 2; k >= 1; k--) part.push_back(part[k]);
        outSteps.insert(outSteps.end(), part.begin(), part.end());
    }

    // Layer 2: the song form. `form` "allows you to slice your steps into two or
    // three parts and create musical song forms like AAAB or ABAC". The letters
    // of each form, as part indices:
    //   0 = A (off), 1 = AAAB, 2 = AABB, 3 = ABAC, 4 = AAABAAAC, 5 = AB, 6 = AAB.
    // Note form 5 (AB) is NOT the same as form 0: the parts are walked
    // separately, which shows the moment direction / pingpong are in play.
    static const std::vector<int>& formSequence(int form) {
        static const std::vector<int> kForms[7] = {
            {0},                          // 0  A
            {0, 0, 0, 1},                 // 1  AAAB
            {0, 0, 1, 1},                 // 2  AABB
            {0, 1, 0, 2},                 // 3  ABAC
            {0, 0, 0, 1, 0, 0, 0, 2},     // 4  AAABAAAC
            {0, 1},                       // 5  AB
            {0, 0, 1},                    // 6  AAB
        };
        return kForms[clampi(form, 0, 6)];
    }

    PlayOrder playOrder(EngineState& s) {
        PlayOrder o;
        std::vector<int> win = rangeWindow(s);
        int form = clampi((int)std::lround(in("form").value(s)), 0, 6);
        if (form == 0) {                          // no parts at all
            stepThrough(s, win, o.steps);
            if (o.steps.empty()) o.steps.push_back(0);
            o.part.assign(o.steps.size(), 0);
            return o;
        }
        o.formed = true;
        const std::vector<int>& seq = formSequence(form);
        int parts = 0;
        for (int letter : seq) if (letter + 1 > parts) parts = letter + 1;
        int n = (int)win.size();
        // SPEC-GAP: the manual only shrugs at a window that does not divide
        // evenly ("or else your parts won't have equal size (which on the other
        // hand could be funny anyway)"). We give the EXTRA steps to the EARLIER
        // parts, so A is the longer one — ceiling boundaries. A part can come out
        // empty when the window is shorter than the part count; it is then simply
        // not played, and contributes no part entry.
        auto bound = [&](int i) { return (i * n + parts - 1) / parts; };
        int entry = 0;
        for (int letter : seq) {
            int a = bound(letter), b = bound(letter + 1);
            size_t before = o.steps.size();
            stepThrough(s, std::vector<int>(win.begin() + a, win.begin() + b), o.steps);
            if (o.steps.size() == before) continue;      // empty part: passed over
            // Every ENTRY into a part is its own index, so re-entering A (AABB,
            // ABAC) still reads as a new part and retriggers `startofpart`.
            o.part.resize(o.steps.size(), entry++);
        }
        if (o.steps.empty()) { o.steps.push_back(0); o.part.assign(1, 0); }
        return o;
    }

    void transport(EngineState& s) {
        if (chainMain_) { transportLinked(s); return; }
        bool run = in("run").value(s) >= kHigh;
        // Advance the clock/reset edge detectors every tick (even when frozen) so
        // leaving compose/stop does not fire a phantom edge on a still-high input.
        bool clockEdge = clockGate_.risingEdge(in("clock").value(s));
        if (resetGate_.risingEdge(in("reset").value(s))) {
            resetPending_ = true; acc_ = 0; triggerSos(s);
            accEpoch_++; accEvtReset_ = true;       // published to the chain members
        }

        // composemode: the sequencer stops clocking; stepping is driven only by CV
        // edits (see onCvEdited), so ignore clock edges entirely.
        if (in("composemode").value(s) >= kHigh) return;
        if (!run) return;                          // frozen: ignore clock
        if (!clockEdge) return;

        if (in("clock").connected()) {
            if (haveClock_) period_ = double(s.tick - lastClock_);
            lastClock_ = s.tick; haveClock_ = true;
        }

        PlayOrder order = playOrder(s);
        long autoreset = std::lround(in("autoreset").value(s));

        if (!started_ || resetPending_) {
            playPos_ = 0; pulse_ = 0; turn_ = 1; clocksSinceReset_ = 0;
            started_ = true; resetPending_ = false; triggerSos(s);
            enterStep(s, order.steps, 0);
            enterPart(s, order, 0, true);
            return;
        }

        clocksSinceReset_++;
        if (autoreset > 0 && clocksSinceReset_ >= autoreset) {
            playPos_ = 0; pulse_ = 0; turn_ = 1; clocksSinceReset_ = 0;
            advanceAccumulator(s); triggerSos(s);
            enterStep(s, order.steps, 0);
            enterPart(s, order, 0, true);
            return;
        }

        int reps = curRepeats(s, order.steps);
        if (pulse_ + 1 < reps) { pulse_++; return; }   // same step, next pulse
        pulse_ = 0;
        bool wrapped = false;
        int next = nextPlayedPos(s, order, playPos_, wrapped);
        playPos_ = next;
        // A wrap is the end of the COMPLETE form, not of a part: "if you enable a
        // form like AAAB, the accumulator is increased at the end of the complete
        // form", and startofsequence marks the same instant.
        if (wrapped) { turn_++; advanceAccumulator(s); triggerSos(s); }
        enterStep(s, order.steps, playPos_);
        enterPart(s, order, playPos_, wrapped);
    }

    // A linktonext chain member is REMOTE CONTROLLED: it ignores its own clock /
    // reset / startstep / endstep / direction / pingpong / autoreset / shiftsteps
    // (motoquencer.md:677) and simply plays whatever step the chain main is on —
    // including the main's repeats and skips, whose per-step settings on the
    // member are ignored. Its own lane still decides CV, gate, gate probability,
    // gate pattern and ratchets. The main always ticks first (the chain is
    // resolved along patch order), so its transport state is fresh here.
    void transportLinked(EngineState& s) {
        SeqCore* m = chainMain_;
        period_    = m->period_;
        pulse_     = m->pulse_;
        turn_      = m->turn_;
        playPos_   = m->playPos_;
        // The accumulator is per-instance (its own accumulatorrange), but it turns
        // on the MAIN's wraps and zeroes on the main's resets.
        if (m->accEpoch_ != seenAccEpoch_) {
            seenAccEpoch_ = m->accEpoch_;
            if (m->accEvtReset_) acc_ = 0; else advanceAccumulator(s);
        }
        if (m->sosEpoch_ != seenSosEpoch_) { seenSosEpoch_ = m->sosEpoch_; triggerSos(s); }
        if (m->sopEpoch_ != seenSopEpoch_) { seenSopEpoch_ = m->sopEpoch_; triggerSop(s); }
        if (m->stepEpoch_ != seenStepEpoch_) {
            seenStepEpoch_ = m->stepEpoch_;
            started_ = true;
            enterStepAt(s, m->playStep_, m->playLogical_, m->curStepRepeats());
        }
    }

    // Repeats of the step this instance is currently playing (1 before the first
    // step entry) — read by the chain members, which inherit the main's repeats.
    int curStepRepeats() const {
        return playStep_ < 0 ? 1 : cur_.repeats[clampi(playStep_, 0, kSteps - 1)];
    }

    int curRepeats(EngineState& s, const std::vector<int>& order) {
        int phys = physOf(s, order[clampi(playPos_, 0, (int)order.size() - 1)]);
        return cur_.repeats[phys];
    }

    // The contiguous run of play positions that belong to the same form-part
    // ENTRY as `pos`, as the inclusive bounds [lo, hi]. playOrder() lays the part
    // entries down back to back, so a part entry is always one contiguous run;
    // without a form there is a single run covering the whole order.
    //
    // This is the arena a movement pattern is confined to (issue #54): the manual
    // says stepping modifications "are always applied within each individual
    // part", so "forward" means the next position of THIS run and leaving the run
    // forwards is what ends a cycle.
    void runBounds(const PlayOrder& o, int pos, int& lo, int& hi) const {
        int n = (int)o.steps.size();
        if (!o.formed || (int)o.part.size() != n) { lo = 0; hi = n - 1; return; }
        int at = clampi(pos, 0, n - 1), p = o.part[at];
        lo = at; while (lo > 0     && o.part[lo - 1] == p) lo--;
        hi = at; while (hi < n - 1 && o.part[hi + 1] == p) hi++;
    }

    // Leave the run ending at `hi` forwards: the first position of the next run,
    // or position 0 with `wrapped` set when that was the last run of the form.
    static int enterNextRun(const PlayOrder& o, int hi, bool& wrapped) {
        int q = hi + 1;
        if (q >= (int)o.steps.size()) { q = 0; wrapped = true; }
        return q;
    }

    // Advance to the next non-skipped position; sets wrapped if we passed the end
    // of the whole order (= the end of the complete form).
    int nextPlayedPos(EngineState& s, const PlayOrder& o, int pos, bool& wrapped) {
        const std::vector<int>& order = o.steps;
        int n = (int)order.size();
        for (int tries = 0; tries < n; tries++) {
            int lo, hi; runBounds(o, pos, lo, hi);
            pos = (pos >= lo && pos < hi) ? pos + 1 : enterNextRun(o, hi, wrapped);
            if (!cur_.skip[physOf(s, order[pos])]) return pos;
        }
        return pos;   // all skipped -> hold (manual: repeats the most recent step)
    }

    // `startofpart` "outputs a trigger whenever a form part starts again". It
    // fires on every ENTRY into a part — including the first part of the form,
    // where it coincides with startofsequence (SPEC-GAP: the manual does not say
    // whether the first one counts, and a part boundary is a part boundary) — and
    // never at all when form = 0, where there are no parts to start.
    // `restart` forces it on a reset / autoreset / wrap, where the part index may
    // be unchanged (a one-part form) but the part genuinely started again.
    void enterPart(EngineState& s, const PlayOrder& o, int pos, bool restart) {
        int p = (pos >= 0 && pos < (int)o.part.size()) ? o.part[pos] : 0;
        bool changed = restart || p != curPart_;
        curPart_ = p;
        if (o.formed && changed) triggerSop(s);
    }

    void advanceAccumulator(EngineState& s) {
        accEpoch_++; accEvtReset_ = false;      // published to the chain members
        long range = std::lround(in("accumulatorrange").value(s));
        if (range <= 0) { acc_ = 0; return; }
        if (range > 16) range = 16;
        acc_++;
        if (acc_ > (int)range) acc_ = 0;
    }

    // Latch the step we are entering and precompute its gate windows.
    void enterStep(EngineState& s, const std::vector<int>& order, int pos) {
        int logical = order[clampi(pos, 0, (int)order.size() - 1)];
        int phys = physOf(s, logical);
        enterStepAt(s, phys, logical, cur_.repeats[clampi(phys, 0, kSteps - 1)]);
    }

    // Enter physical step `phys` (logical index `logical`) lasting `reps` clock
    // pulses. Split out of enterStep so a chain member can be handed the MAIN's
    // step number and repeat count while still latching its own lane's values.
    void enterStepAt(EngineState& s, int phys, int logical, int reps) {
        phys = clampi(phys, 0, kSteps - 1);
        playStep_ = phys;
        playLogical_ = logical;
        latchCvpos_ = cur_.cvpos[phys];
        latchRandcv_ = cur_.randcv[phys];
        plays_ = cur_.gate[phys] && probabilityPlays(s, phys);
        stepStart_ = s.tick;
        pitchCached_ = false;   // recompute the step's raw pitch on entry
        stepEpoch_++;           // published to the chain members

        gateWin_.clear();
        if (!plays_) return;
        if (reps < 1) reps = 1;
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
        // holdcv 0 = update on every step; 1 = only on steps that play a gate. On a
        // chain member the manual adds value 2: 1 syncs the CV to the MAIN's gate
        // (the default), 2 to the member's own gate (motoquencer.md:690).
        float hold = in("holdcv").value(s);
        bool cvUpdates = hold < kHigh ? true                        // 0: every step
                       : (chainMain_ && hold < 1.5f) ? chainMain_->plays_
                       : plays_;
        if (playStep_ >= 0 && cvUpdates) {
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
                gateHigh = chainClock(s) >= kHigh;          // period unknown: clock level
            }
        }
        out("gate").set(s, gateHigh ? 1.0f : 0.0f);
        emitHousekeeping(s);
    }

    // startofsequence + the frozen-state status outputs (shared by both emit paths).
    void emitHousekeeping(EngineState& s) {
        // 10 ms trigger window (DROID-standard), not a 1-tick pulse.
        out("startofsequence").set(s, (long)s.tick < sosUntil_ ? 1.0f : 0.0f);
        out("startofpart").set(s, (long)s.tick < sopUntil_ ? 1.0f : 0.0f);
        out("currentstep").set(s, (float)(playStep_ < 0 ? 0 : playStep_));
        out("currentpage").set(s, (float)((playStep_ < 0 ? 0 : playStep_) / numFaders_));
        out("accumulator").set(s, (float)acc_);

        out("startstepout").set(s, (float)(rangeStart0(s) + 1));   // 1-based
        out("endstepout").set(s, (float)(rangeEnd0(s) + 1));
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
        // An interactive start/end override does not survive a clear: "They are
        // reactived if you clear everything" (the startstep / endstep rows).
        if (clearAll || clr) manualStart0_ = manualEnd0_ = -1;
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
    // The clock level driving this instance — a chain member has no clock of its
    // own, so it borrows the main's (used only for the gate fallback before the
    // clock period is known).
    float chainClock(EngineState& s) {
        return (chainMain_ ? chainMain_ : this)->in("clock").value(s);
    }
    void triggerSos(EngineState& s) {
        sosUntil_ = (long)s.tick + trigTicks(s);
        sosEpoch_++;                                // published to the chain members
    }
    void triggerSop(EngineState& s) {
        sopUntil_ = (long)s.tick + trigTicks(s);
        sopEpoch_++;                                // published to the chain members
    }
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

    // `bulkedit`, sampled once per tick before the edit surface runs, and the
    // leftmost step this tick's stamp rewrote (-1 = none). Both are per-tick
    // scratch, not state: nothing about a bulk edit survives the tick except the
    // step values it wrote.
    bool bulkEdit_ = false;
    int  bulkStampFrom_ = -1;
    // True while a bulkedit gesture is in flight (the user edit plus the stamp it
    // produces): constantlength compensation is off for its whole span. See the
    // BulkBypass block comment at compensateLength().
    bool bulkBypass_ = false;

    // Interactive start/end (motoquencer.md §"Start and end"). Two INDEPENDENT
    // override slots, 0-based, -1 = not overridden (follow the startstep /
    // endstep inputs): a plate touch, setendstep and doublerange override only
    // the END; the two-finger gesture also overrides the START. They live on the
    // range owner (the chain main) and are dropped by clear / clearall /
    // clearstartend. NOT persisted: the manual calls them "temporarily
    // modified", so like playPos_ they are runtime state and stateVersion()
    // stays 1.
    int  manualStart0_ = -1, manualEnd0_ = -1;
    // The plate that set the end and is still held — the anchor for the second
    // finger. Identified by LANE, not step, so flipping pages mid-gesture keeps
    // it. Lives on the instance that owns the buttons (it is a finger).
    int  seAnchorLane_ = -1;
    // setendstep change detector; the first observed value is latched silently.
    bool setEndSeen_ = false;
    int  prevSetEnd_ = 0;
    bool drPrev_ = false;          // doublerange trigger edge
    // Set on a chain member by the main when doublerange rewrote its steps, so
    // the member re-commands its motors when it ticks (later the same tick).
    bool pendingRecall_ = false;

    // linktonext chain (resolved once at init). chainMain_ is the chain's first
    // instance (nullptr if this is the main or a standalone); chainIndex_ is our
    // 0-based distance from it; linkToNext_ is whether we feed a further member;
    // chainRawFm_ / chainRawBm_ (published by the main each tick) are the
    // chain-wide fadermode / buttonmode.
    SeqCore* chainMain_ = nullptr;
    int  chainIndex_ = 0;
    bool linkToNext_ = false;
    int  chainRawFm_ = 0;
    int  chainRawBm_ = 0;   // likewise the chain-wide buttonmode
    // Transport events published by the chain main and mirrored by its members
    // (see transportLinked). Monotonic counters rather than one-tick flags, so a
    // member cannot miss one; accEvtReset_ says whether the latest accumulator
    // event was a reset (zero it) or a wrap (advance it by our own range).
    uint64_t stepEpoch_ = 0, accEpoch_ = 0, sosEpoch_ = 0, sopEpoch_ = 0;
    // Last luckyshuffle / luckyreverse permutation published by the main
    // (targets + the source step each target took), mirrored by the members.
    uint64_t orderEpoch_ = 0;
    std::vector<int> orderT_, orderSrc_;
    bool     accEvtReset_ = false;
    uint64_t seenStepEpoch_ = 0, seenAccEpoch_ = 0, seenSosEpoch_ = 0, seenOrderEpoch_ = 0;
    uint64_t seenSopEpoch_ = 0;
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
    long sopUntil_ = 0;                         // startofpart trigger-window end
    int  curPart_ = -1;                         // form part entry being played
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
