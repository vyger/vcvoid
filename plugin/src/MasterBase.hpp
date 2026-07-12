#pragma once
#include "plugin.hpp"
#include "src/engine.hpp"   // droid::Engine (via -I../engine)
#include "ChainModule.hpp"  // droid::chain protocol + ChainModule::isChain{Left,Right}Neighbor
#include "Layout.hpp"
#include "uatbridge/Bridge.hpp"   // forward-declares Rack types only; safe here
#include <osdialog.h>
#include <sys/stat.h>
#include <fstream>
#include <sstream>
#include <mutex>
#include <atomic>
#include <cmath>
#include <cstring>
#include <algorithm>

// Copy a symbolic display string into a fixed NUL-terminated field, truncating
// to fit (the buffer's last byte always stays NUL). Used for the DB8E screen's
// header/body text (DownstreamBlock.dispHeader/dispText). The caller zeroes the
// whole block first, so the tail past the copied bytes is already 0.
template <size_t N>
static void copyDisplayText(char (&dst)[N], const std::string& src) {
    size_t n = std::min(src.size(), N - 1);
    if (n) std::memcpy(dst, src.data(), n);
    dst[n] = '\0';
}

// Shared engine host for the DROID masters (MASTER / MASTER18). Holds the engine,
// patch loading, controller-chain + X7/MIDI feed, and process(). Parameterized by
// master type and I/O counts; the LED matrix (Master16 only) lives in the subclass
// widget, and MASTER18's gate I/O rides the processExtraIO() hook. A concrete
// master subclass supplies its own InputId/OutputId/LightId enums (input IDs from
// 0, CV-output IDs from 0, gate-output IDs after the CV outs) and passes matching
// counts so its register wiring lines up with the base's plain-int indexing.
struct DroidMasterBase : Module {
    std::unique_ptr<droid::Engine> engine;
    std::mutex engineMutex;
    std::string patchPath;
    std::string patchStatus = "no patch loaded";
    droid::LoadResult lastResult;
    // Persistent circuit state (DROIDSTA.BIN contract, hardware.md §11.1). Kept
    // across a patch hot-reload (snapshot the old engine, restore into the new)
    // and across a Rack save/reopen (serialized into dataToJson, restored after
    // dataFromJson's patch load). Touched only under engineMutex or on the UI
    // thread paths that also hold it.
    droid::StateSnapshot lastSnapshot;
    float targetHz = 6000.f;
    int divider = 8;
    float effectiveRate = 6000.f;   // sampleRate / divider; the rate the engine runs at
    int frameCounter = 0;
    // Set on the engine thread by onSampleRateChange; consumed by the widget's
    // step() on the UI thread, which is the only thread allowed to reload.
    std::atomic<bool> timingDirty{false};

    // Master type + I/O geometry (set once by the subclass constructor).
    droid::MasterType masterType_;
    int numIns_;
    int numOuts_;
    int numGateOuts_;

    // --- controller chain (M4) ---
    // Physical models seen on the right expander chain, master-nearest first.
    // process() writes it under engineMutex; the widget's step() reads it under
    // a lock_guard to revalidate. Distinct from declaredControllers() (the patch).
    std::vector<std::string> chainPhysical;   // engineMutex
    bool chainOk = true;                      // engineMutex; process() ticks only when true
    std::string chainError;                   // UI thread only (widget step()/menu)
    std::atomic<bool> chainDirty{false};      // audio/any -> UI: revalidate request
    // ISSUE-5: chainOk demotion is debounced so a transient chain shrink during
    // Rack's expander re-enumeration (hot-plug) does not pause the engine for a
    // frame. chainForce marks a fresh patch (re)load, which must demote at once
    // (no debounce) so a wrong chain at load errors immediately. Both touched
    // only on the UI thread (step()), except chainForce set from loadPatchFile.
    droid::chain::ChainOkDebounce chainDebounce;   // UI thread only (step())
    std::atomic<bool> chainForce{false};
    // Per-encoder last-seen detent count, indexed by 0-based GLOBAL encoder number
    // (chain-order, matching ControllerState::configure()). The upstream feed diffs
    // each block's monotonic detentCount against this (detentDelta) to derive
    // movement, then stores the new value here. Touched ONLY inside process() under
    // engineMutex (like the rest of the feed) — no separate synchronisation needed.
    // Reset when the physical chain changes so a re-plugged encoder cannot emit a
    // spurious jump against a stale baseline.
    uint32_t lastDetent[droid::chain::kMaxChainModules *
                        droid::chain::kMaxEncodersPerModule] = {};
    // --- X7 expander / MIDI feed (M5) ---
    // controllerModels() skips the X7, so X7 presence/placement is tracked here
    // separately. All touched ONLY inside process()/step() under engineMutex,
    // like the chain fields above. x7Present = an X7 currently sits at the chain
    // head (block[0]); chainX7Error = x7ChainError(up) ("" or a misplaced/dup
    // message), folded into chainError during revalidation. lastMidiTotalUp is the
    // per-port baseline for the upstream sliding-window contract (chain.hpp
    // MidiFrame): diffed against the X7's monotonic totals each tick frame to
    // consume exactly the not-yet-seen events; RESYNCED to the current totals on
    // any chain/X7 change so an attached, streaming X7 never has consumed events
    // replayed and a re-attached X7 never reprocesses stale ones. midiSeqDown is
    // the downstream frame counter, bumped only when events are written and kept
    // MONOTONIC across chain changes (never reset) so the X7 reader can never see
    // a repeated seq value.
    bool x7Present = false;
    std::string chainX7Error;
    uint32_t lastMidiTotalUp[droid::chain::kChainMidiPorts] = {};
    uint32_t midiSeqDown = 0;
    // Raw upstream chain signature (modelId list) from the last tick frame.
    // chainPhysical, x7 presence/placement, and the revalidation triggers are
    // all pure functions of this list, so process() skips their per-tick
    // string/vector work while it is unchanged. -1 count = never evaluated.
    uint8_t lastChainIds[droid::chain::kMaxChainModules] = {};
    // ControllerModel per chain slot (nullptr for G8/X7/non-controllers),
    // resolved once under the rawChanged gate below — findControllerModel is a
    // name-keyed linear scan, pure per-tick waste when the chain hasn't moved.
    const droid::ControllerModel* chainModels_[droid::chain::kMaxChainModules] = {};
    int lastChainCount = -1;

    // --- UAT bridge port probe (M6) ---
    // Armed/disarmed by the HTTP thread (uat::Bridge::handleProbe) via
    // armProbe()/disarmProbe(); sampled every audio frame in process(), at the
    // TOP of the function (before the tick-divider early-return) so the sample
    // rate tracks args.sampleRate, not the (much slower) engine tick rate —
    // needed for edge-timing fidelity on fast signals. probeMutex_ is a
    // separate, tiny mutex (not engineMutex) held only for the single-float
    // write below and for arm/disarm/readout, so probing never contends with
    // engine reloads. probeArmed_ is checked lock-free first so the common
    // (unarmed) case costs one atomic load per frame.
    //
    // Cap: probeBuf_ is sized once per arm() to kProbeMaxSamples (1,000,000
    // floats, ~4 MB) — enough for the clamped 5 s max probe window at a
    // 200 kHz ceiling, comfortably above Rack's practical sample-rate range
    // (44.1 kHz-192 kHz). A pathologically high sample rate just stops
    // filling the buffer once full (probeCount_ < probeBuf_.size() guard);
    // it does not grow or wrap.
    static constexpr size_t kProbeMaxSamples = 1'000'000;
    std::atomic<bool> probeArmed_{false};
    std::mutex probeMutex_;                 // guards probePort_/probeIsOutput_/probeBuf_/probeCount_
    int probePort_ = 0;
    bool probeIsOutput_ = true;
    std::vector<float> probeBuf_;
    size_t probeCount_ = 0;

    // Arm a probe on output/input port `port` (Rack port index, 0-based).
    // Called from the HTTP thread; must run before the caller starts timing
    // the probe window.
    void armProbe(int port, bool isOutput) {
        std::lock_guard<std::mutex> lk(probeMutex_);
        probePort_ = port;
        probeIsOutput_ = isOutput;
        probeBuf_.assign(kProbeMaxSamples, 0.f);
        probeCount_ = 0;
        probeArmed_.store(true, std::memory_order_release);
    }
    // Disarm and return the collected samples (only the first probeCount_ are
    // valid data; the rest of the preallocated buffer is discarded here).
    std::vector<float> disarmProbe() {
        std::lock_guard<std::mutex> lk(probeMutex_);
        probeArmed_.store(false, std::memory_order_relaxed);
        std::vector<float> out(probeBuf_.begin(), probeBuf_.begin() + probeCount_);
        probeBuf_.clear();
        probeBuf_.shrink_to_fit();
        return out;
    }

    // UAT bridge (M10): this master's own native MIDI ports (MASTER18's
    // usb/trs1/trs2 in+out). Base (MASTER16) has none. Not engineMutex-guarded
    // (rack::midi::Port is UI-thread state, like ModuleWidget) — callers reach
    // this through uiCall().
    virtual std::vector<ChainModule::MidiPortRef> midiPorts() { return {}; }

    // UAT bridge (M10): read back the i'th (0..15) 4x4 matrix LED colour as
    // currently displayed, i.e. the SAME Module::lights[] the widget renders
    // (DroidMaster::process() computes it every frame: R-register override via
    // droid::color::fromValue, else the jack-voltage mirror). Returns false for
    // masters with no matrix (MASTER18) or i out of range. lights[].getBrightness()
    // is a plain float member read, benign off the audio thread like the M6
    // probe's Port::getVoltage() reads — no engineMutex needed.
    virtual bool matrixLedColor(int i, float& r, float& g, float& b) { (void)i; (void)r; (void)g; (void)b; return false; }

protected:
    DroidMasterBase(droid::MasterType type, int numIns, int numOuts,
                    int numGateOuts, int numLights)
        : masterType_(type), numIns_(numIns), numOuts_(numOuts),
          numGateOuts_(numGateOuts) {
        config(0, numIns_, numOuts_ + numGateOuts_, numLights);
        for (int i = 0; i < numIns_; i++)
            configInput(i, string::f("I%d", i + 1));
        for (int i = 0; i < numOuts_; i++)
            configOutput(i, string::f("O%d", i + 1));
        for (int i = 0; i < numGateOuts_; i++)
            configOutput(numOuts_ + i, string::f("G%d", i + 1));
        updateDivider(APP->engine->getSampleRate());
        // Master has no left chain face; only the rightExpander carries upstream
        // controls in / relays downstream LEDs out. Freed in the destructor.
        rightExpander.producerMessage = new droid::chain::UpstreamMessage;
        rightExpander.consumerMessage = new droid::chain::UpstreamMessage;
        if (auto* b = uat::Bridge::instance()) b->registerMaster(this);
    }

public:
    ~DroidMasterBase() override {
        if (auto* b = uat::Bridge::instance()) b->unregisterMaster(this);
        delete (droid::chain::UpstreamMessage*) rightExpander.producerMessage;
        delete (droid::chain::UpstreamMessage*) rightExpander.consumerMessage;
    }

    // A controller was added/removed/moved on the chain: request revalidation.
    void onExpanderChange(const ExpanderChangeEvent& e) override {
        chainDirty.store(true);
    }

    // Recompute the tick divider and the effective control rate the engine runs
    // at. The engine derives all timing from its constructor tick rate, so the
    // effective rate (not the requested targetHz) is what a fresh Engine is built
    // with. Pure: does not touch the engine — callers reload if timing changed.
    void updateDivider(float sampleRate) {
        divider = std::max(1, (int)std::round(sampleRate / targetHz));
        effectiveRate = sampleRate / divider;
    }

    // Recompute the divider for the given sample rate and, if a patch is loaded,
    // rebuild the engine at the new effective rate. The engine takes its tick
    // rate only in its constructor (no setter), so both a sample-rate change and
    // a tick-rate menu change funnel through here to reload. Hardware reloads on
    // such changes too.
    void applyTiming(float sampleRate) {
        updateDivider(sampleRate);
        // Copy patchPath out under the lock before calling loadPatchFile — it
        // takes the same (non-recursive) engineMutex itself, and the HTTP
        // bridge thread can be writing patchPath concurrently.
        std::string path;
        {
            std::lock_guard<std::mutex> lock(engineMutex);
            path = patchPath;
        }
        if (!path.empty())
            loadPatchFile(path);
    }

    // Engine-thread callback: must not reload here (loadPatchFile does disk I/O
    // and writes the UI-thread-only patchPath/patchStatus/lastResult). Update
    // the divider directly — it writes only ints/floats, whose torn read in
    // process() is plan-acknowledged tolerable — so timing is correct even when
    // no widget pumps step(). Any patch reload is deferred to the UI thread.
    void onSampleRateChange(const SampleRateChangeEvent& e) override {
        updateDivider(e.sampleRate);
        timingDirty.store(true);
    }

    // The four tick-rate menu choices, in order.
    static constexpr float kTickRates[4] = {2000.f, 4000.f, 6000.f, 8000.f};

    // Index of the current targetHz among kTickRates (nearest match; default 6 kHz).
    size_t tickRateIndex() const {
        size_t best = 2;   // 6 kHz
        float bestDiff = std::fabs(targetHz - kTickRates[2]);
        for (size_t i = 0; i < 4; i++) {
            float d = std::fabs(targetHz - kTickRates[i]);
            if (d < bestDiff) { bestDiff = d; best = i; }
        }
        return best;
    }

    void loadPatchFile(const std::string& path) {
        std::string text;
        {   // read whole file
            std::ifstream f(path, std::ios::binary);
            if (!f) {
                // Persist the path even on failure so it round-trips through
                // Rack save/load and the mtime poller / Reload item can recover
                // once the file reappears. Engine state stays untouched. Locked
                // like every other patchPath/patchStatus write: the UAT HTTP
                // thread can call loadPatchFile() concurrently with the UI
                // thread, whose reads of these fields are also lock-guarded.
                std::lock_guard<std::mutex> lock(engineMutex);
                patchPath = path;
                patchStatus = "cannot open " + path;
                return;
            }
            std::stringstream ss; ss << f.rdbuf(); text = ss.str();
        }
        auto fresh = std::make_unique<droid::Engine>(
            masterType_, effectiveRate);
        droid::LoadResult r = fresh->load(text);
        std::lock_guard<std::mutex> lock(engineMutex);
        lastResult = r;
        patchPath = path;
        if (r.ok) {
            // Circuit-state transfer (hardware.md §11.1: "when you press the
            // button for loading a new patch, the states are saved immediately").
            // A live engine (edit-and-save hot reload) is snapshotted here so the
            // dialed state survives the swap; on a first load from a Rack reopen
            // the engine is null and lastSnapshot came from dataFromJson.
            if (engine)
                lastSnapshot = engine->saveState();
            if (!lastSnapshot.empty())
                fresh->restoreState(lastSnapshot);
            engine = std::move(fresh);
            // A reload builds a FRESH Engine whose state_.midi.x7 defaults false;
            // the engine's own keepX7 preserve only covers an in-place load() on
            // the same engine, so it can't carry presence across this swap. X7
            // presence is a property of the physical chain, not the patch, so
            // re-assert the last-known chain state into the new engine — otherwise
            // midiin/midiout/midithrough stay gated off (s.midi.x7 == false) until
            // the chain next changes, even though an X7 is attached.
            engine->setX7Present(x7Present);
            // "SD card" = the folder holding the loaded droid.ini: midifileplayer's
            // midi<N>.mid files are read from there on demand (engine thread —
            // matches the hardware's brief SD stall on track load).
            std::string dir = system::getDirectory(path);
            engine->setFileProvider([dir](int num, std::vector<uint8_t>& bytes) {
                std::ifstream mf(dir + "/midi" + std::to_string(num) + ".mid",
                                 std::ios::binary);
                if (!mf) return false;
                bytes.assign(std::istreambuf_iterator<char>(mf),
                             std::istreambuf_iterator<char>());
                return true;
            });
            patchStatus = system::getFilename(path) +
                string::f(" — ok, %u bytes RAM", r.ramUsed);
            if (!r.warnings.empty()) {
                patchStatus += string::f(" — %d warning(s)", (int)r.warnings.size());
                for (const auto& w : r.warnings)
                    WARN("vcvoid: patch warning: %s", w.c_str());
            }
        } else {
            engine.reset();   // hardware stops on a bad patch; so do we
            if (!r.errors.empty())
                patchStatus = string::f("LOAD ERROR line %d: %s",
                    r.errors[0].line, r.errors[0].message.c_str());
            else
                patchStatus = "LOAD ERROR: load failed";
            WARN("vcvoid: %s", patchStatus.c_str());
        }
        // A fresh patch declares its own controller chain; revalidate it against
        // whatever is physically connected (ok/bad regardless of load result).
        // Force an immediate (undebounced) verdict: a wrong chain at load must
        // error at once, not after the ISSUE-5 hot-plug tolerance window.
        chainForce.store(true);
        chainDirty.store(true);
    }

    // UAT bridge (M9): fresh-boot the currently loaded patch without recreating
    // the module — the F5 recreate-the-module dance, minus the recreation.
    // loadPatchFile() alone is NOT enough: when a live engine exists it
    // snapshots the engine's CURRENT (dialed) state into lastSnapshot and
    // restores that right back into the fresh engine (the hot-reload transfer,
    // by design — see loadPatchFile above). So to really wipe state we must
    // drop the live engine BEFORE calling loadPatchFile: with engine == nullptr
    // and lastSnapshot cleared, loadPatchFile's "if (engine) lastSnapshot =
    // engine->saveState()" is skipped and the fresh engine starts from its
    // circuits' startvalues, same as a cold Rack-reopen with no saved state.
    void resetCircuitState() {
        std::string path;
        {
            std::lock_guard<std::mutex> lock(engineMutex);
            path = patchPath;
            engine.reset();
            lastSnapshot = droid::StateSnapshot();
        }
        if (!path.empty())
            loadPatchFile(path);
    }

    // MASTER18 gate/extra I/O hook, called at the end of process() right after the
    // CV-output write-back. Default: nothing (MASTER16 has no extra I/O).
    virtual void processExtraIO() {}

    // Value fed into engine register Ii+1 for a patched input jack, sampled on
    // tick frames just before engine->tick(). MASTER's I1-I8 are continuous CV
    // (voltage/10); MASTER18 overrides this to binarize its I1/I2 gate inputs.
    // (Non-const: rack::engine::Input::getVoltage() is non-const.)
    virtual float inputRegisterValue(int i) {
        return inputs[i].getVoltage() / 10.f;
    }

    void process(const ProcessArgs& args) override {
        // UAT probe sample: every audio frame, ahead of the tick-divider gate
        // below, so a probe's timing resolution is the audio rate, not the
        // (divided-down) engine tick rate. outputs[]/inputs[] hold their last
        // written voltage between engine ticks, so sampling here on
        // non-tick frames still reads the current value.
        if (probeArmed_.load(std::memory_order_acquire)) {
            std::lock_guard<std::mutex> lk(probeMutex_);
            if (probeArmed_.load(std::memory_order_relaxed) && probeCount_ < probeBuf_.size()) {
                probeBuf_[probeCount_++] = probeIsOutput_
                    ? outputs[probePort_].getVoltage()
                    : inputs[probePort_].getVoltage();
            }
        }

        if (++frameCounter < divider) return;
        frameCounter = 0;
        std::unique_lock<std::mutex> lock(engineMutex, std::try_to_lock);
        if (!lock.owns_lock()) return;   // contended: retry next tick frame

        using namespace droid::chain;
        // ---- chain upstream: controls from the expander chain -------------
        // NOTE: the physical-chain tracking below (chainPhysical / x7Present /
        // chainX7Error) runs even with NO engine loaded, so the context menu's
        // "chain:" line reflects the real controller chain before any patch is
        // loaded (UAT F1). Only engine-dependent work (setX7Present, MIDI
        // draining, and everything past the `if (!engine) return` below) is
        // gated on a live engine; validateChain (declared-vs-physical) stays
        // engine-gated in the widget's step().
        // kEmptyChain has static storage, so binding `up` to a const ref is safe.
        static const UpstreamMessage kEmptyChain;
        bool haveChain = rightExpander.module && ChainModule::isChainRightNeighbor(rightExpander.module);
        const UpstreamMessage& up = haveChain
            ? *(const UpstreamMessage*) rightExpander.consumerMessage
            : kEmptyChain;
        // Track the physical chain and flag a revalidation on any change. This
        // must run BEFORE the chainOk gate so a fixed chain can clear the flag.
        // Gated on the raw modelId signature: everything below (chainPhysical,
        // X7 presence/placement, the revalidation triggers) is a pure function
        // of that list, so an unchanged signature means nothing to do — and the
        // per-tick vector<string>/string building this avoids showed up hot in
        // audio-thread profiles.
        int upCount = std::min<int>(up.count, kMaxChainModules);
        bool rawChanged = (upCount != lastChainCount);
        for (int i = 0; !rawChanged && i < upCount; i++)
            rawChanged = up.block[i].modelId != lastChainIds[i];
        if (rawChanged) {
            lastChainCount = upCount;
            for (int i = 0; i < upCount; i++) lastChainIds[i] = up.block[i].modelId;
            for (int i = 0; i < kMaxChainModules; i++) {
                ModelId mid = ModelId(i < upCount ? up.block[i].modelId : 0);
                chainModels_[i] = isControllerModel(mid)
                    ? droid::findControllerModel(modelName(mid)) : nullptr;
            }
            std::vector<std::string> models = controllerModels(up);
            bool chainChanged = (models != chainPhysical);
            if (chainChanged) {
                chainPhysical = models;
                for (auto& d : lastDetent) d = 0;   // chain changed: drop stale detent baselines
            }
            // X7 presence + placement (controllerModels() skips the X7). x7Now = an
            // X7 at the chain head; x7err catches a misplaced/duplicate X7. On any
            // chain OR X7 change: flag revalidation, tell the engine, resync the
            // upstream seq baseline, and flush stale out-queues so a re-attached X7
            // never replays events queued before the change (the lastDetent-style
            // reset pattern).
            bool x7Now = up.count > 0 && ModelId(up.block[0].modelId) == MX7;
            std::string x7err = x7ChainError(up);
            if (chainChanged || x7Now != x7Present || x7err != chainX7Error) {
                chainDirty.store(true);
                x7Present = x7Now;
                chainX7Error = x7err;
                if (engine) engine->setX7Present(x7Now);
                // RESYNC the baselines to the CURRENT totals — don't zero them. If
                // the X7 stays attached and streaming while an unrelated controller
                // is hot-plugged, zeroing would make the pump below see a huge
                // positive diff on this very frame (chainOk only flips in step()
                // next UI frame) and replay the whole window into the engine —
                // duplicate note-on/off, possible stuck note. A genuinely fresh X7
                // starts its totals at 0, so resync behaves identically to zeroing
                // on a re-attach.
                for (int port = 0; port < droid::chain::kChainMidiPorts; port++)
                    lastMidiTotalUp[port] = x7Now ? up.block[0].midi.total[port] : 0;
                if (engine) {
                    droid::MidiEvent tmp;
                    for (int p = 0; p < droid::kNumMidiPorts; p++)
                        while (engine->drainMidiOut((droid::MidiPort) p, tmp)) {}
                }
            }
        }
        // With no patch loaded there is nothing to run — but chainPhysical /
        // x7Present are now current (tracked above), so the menu is correct.
        if (!engine) return;
        // chainOk may lag the physical chain / a fresh patch by ~one UI frame (revalidation runs in step() off chainDirty); self-correcting, outputs just hold one frame longer.
        if (!chainOk) return;   // mismatch: engine paused until revalidation (outputs hold)

        uint8_t ctrl = 0;
        uint8_t g8 = 0;
        // Running 0-based GLOBAL encoder / fader indices. ControllerState::configure()
        // assigns globals by walking the DECLARED controllers in chain order (E4->4
        // encoders, DB8E->1; M4->4 faders; x7 excluded). The chainOk gate above
        // guarantees the physical chain prefix-matches the declared list, so these
        // running counters — advanced only for non-surplus controllers, per that same
        // per-model element count, in chain order — land on the engine's globals. The
        // downstream loop advances an identical pair (enc2/fad2) so readbacks align.
        int enc = 0;
        int fad = 0;
        for (int i = 0; i < up.count && i < kMaxChainModules; i++) {
            ModelId id = ModelId(up.block[i].modelId);
            if (id == MG8) {
                if (++g8 > 4) continue;                          // hardware max, extras ignored
                for (uint8_t j = 1; j <= 8; j++) {
                    droid::RegId gr = droid::canonicalize({'G', g8, j}, masterType_);
                    if (!engine->registerDriven(gr))             // input jack ONLY if patch doesn't drive it
                        engine->setRegister(gr, up.block[i].gates[j - 1]);
                }
                continue;
            }
            if (!isControllerModel(id)) continue;   // non-G8 non-controller: skip
            ctrl++;
            const droid::ControllerModel* cm = chainModels_[i];   // cached under rawChanged
            if (!cm || ctrl > engine->declaredControllers().size()) continue;   // surplus: idle
            for (uint8_t k = 1; k <= cm->pots; k++)
                engine->setRegister({'P', ctrl, k}, up.block[i].pots[k - 1]);
            // Button (B) feed: generic for every model EXCEPT M4, whose B registers are
            // the touch plates — fed once from faderTouch in the fader loop below so
            // there is a single B source (a double-write would fight itself).
            if (id != MM4)
                for (uint8_t k = 1; k <= cm->buttons; k++)
                    engine->setRegister({'B', ctrl, k}, (up.block[i].buttons >> (k - 1)) & 1u ? 1.f : 0.f);
            for (uint8_t k = 1; k <= cm->switches; k++)
                engine->setRegister({'S', ctrl, k}, up.block[i].switches[k - 1]);
            // --- encoders (E4: 4, DB8E: 1; cm->encoders is 0 for other models) ----
            // Addressed by NAME "E<ctrl>.<num>": Engine::turnEncoder/pushEncoder parse
            // it and resolve via the chain lookup to the same global `enc` tracks.
            // detentCount is a wrapping monotonic per-encoder counter; detentDelta
            // recovers the signed movement since last-seen so nothing is lost to the
            // tick divider / relay latency. Push is a SEPARATE surface from B (the
            // encoder circuit reads EncoderState.pushed, NOT the B register): on the
            // E4 the push IS button n (bit n-1, also fed to B above); the DB8E's
            // encoder push is bit 8, which the generic B loop above now feeds to
            // register B<c>.9 (db8e buttons = 9, Forge parity) — the same
            // push-is-a-B-register model as the E4.
            for (uint8_t n = 1; n <= cm->encoders; n++) {
                int32_t d = droid::chain::detentDelta(up.block[i].detentCount[n - 1], lastDetent[enc]);
                lastDetent[enc] = up.block[i].detentCount[n - 1];
                droid::RegId er{'E', ctrl, n};
                if (d) engine->turnEncoder(er, d);
                int pushBit = (id == MDB8E) ? 8 : (n - 1);
                engine->pushEncoder(er, (up.block[i].buttons >> pushBit) & 1u);
                enc++;
            }
            // --- motor faders (M4: 4; cm->faders is 0 for other models) -----------
            // Addressed by GLOBAL number "F<g>" (faders have no register form).
            // moveFader is TOUCH-GATED: an untouched position echo — the widget
            // animating toward the motorTarget — must never register as user
            // movement. The PLATE (plateTouch, TOUCH_PARAMS only — never a drag)
            // drives the plate button B<ctrl>.<f> here (M4's single B source,
            // skipped in the generic loop above) and the engine's plate surface
            // (motoquencer step buttons, `button` outputs). A drag counts as
            // touched but not as a plate press — see chain.hpp.
            for (uint8_t f = 1; f <= cm->faders; f++) {
                int g = fad + 1;
                bool touched = (up.block[i].faderTouch >> (f - 1)) & 1u;
                bool plate = (up.block[i].plateTouch >> (f - 1)) & 1u;
                engine->touchFader(g, touched);
                engine->pressFaderPlate(g, plate);
                if (touched)
                    engine->moveFader(g, up.block[i].faderPos[f - 1]);
                engine->setRegister({'B', ctrl, f}, plate ? 1.f : 0.f);
                fad++;
            }
        }

        // ---- chain upstream: MIDI from the X7 (sliding-window contract) ----
        // The X7 publishes a persistent window of the most recent events per
        // port plus a monotonic total (chain.hpp MidiFrame); diff the total
        // against our baseline and consume exactly the unseen window tail, so
        // events arriving between tick frames are never lost (ISSUE-2). Only
        // the head block carries meaningful MIDI, and x7Present (kept in sync
        // with the raw chain signature above) guarantees it.
        if (x7Present) {
            const MidiFrame& mf = up.block[0].midi;
            for (int port = 0; port < droid::chain::kChainMidiPorts; port++) {
                int first, n;
                int32_t lost = consumeUpstreamMidi(mf, port, lastMidiTotalUp[port], first, n);
                if (lost > 0)
                    WARN("vcvoid: x7 upstream MIDI window overflow, port %d: %d event(s) lost", port, lost);
                for (int k = first; k < first + n; k++)
                    engine->sendMidiIn(x7PhysicalPort(port), mf.ev[port][k]);
                lastMidiTotalUp[port] = mf.total[port];
            }
        }

        // I/O registers are canonical as-is ('I'/'O' with ctrl 0; canonicalize
        // only remaps G), so the plain-RegId fast path is safe here.
        for (int i = 0; i < numIns_; i++) {
            bool patched = inputs[i].isConnected();
            engine->setInputPatched(i + 1, patched);
            if (patched)
                engine->setRegister({'I', 0, uint8_t(i + 1)},
                                    inputRegisterValue(i));
        }
        engine->tick();

        // ---- chain downstream: LED states to the expander chain -----------
        // Only written on tick frames — that is the sampling contract. One block
        // per physical module (modelId preserved even for skipped G8 blocks) so
        // relays stay position-aligned.
        // Participants allocate this producer in their constructors; the null
        // guard protects against a future right neighbour that does not.
        if (auto* down = haveChain
                ? (DownstreamMessage*) rightExpander.module->leftExpander.producerMessage
                : nullptr) {
            down->count = 0;
            uint8_t c2 = 0;
            uint8_t g8d = 0;
            // Same running globals as the upstream loop (chain order), advanced on the
            // same non-surplus controllers by the same per-model counts, so a block's
            // encoder/fader/display readback addresses the engine global its upstream
            // feed drove. db8e counts DB8Es only (1-based, matching displayState()).
            int enc2 = 0;
            int fad2 = 0;
            int db8e = 0;
            for (int i = 0; i < up.count && i < kMaxChainModules && down->count < kMaxChainModules; i++) {
                ModelId id = ModelId(up.block[i].modelId);
                DownstreamBlock& b = down->block[down->count++];
                b = DownstreamBlock{};
                b.modelId = id;
                b.ledBrightness = engine->ledBrightness();   // [droid]; G8-only consumer
                if (id == MG8) {
                    if (++g8d > 4) continue;
                    for (uint8_t j = 1; j <= 8; j++) {
                        droid::RegId gr = droid::canonicalize({'G', g8d, j}, masterType_);
                        if (engine->registerDriven(gr))          // driven register == output jack
                            b.gates[j - 1] = engine->getRegister(gr);
                        // undriven stays 0 -> output jack low
                        // R-register LED override (manual §5.5): the first G8's
                        // eight LEDs are R17..R24, the second's R25..R32, etc.
                        // (R17 + (g8-1)*8 .. +7). A driven R register shows its
                        // colour value on the LED instead of the gate mirror.
                        droid::RegId rr{'R', 0, uint8_t(16 + (g8d - 1) * 8 + j)};
                        if (engine->registerDriven(rr)) {
                            b.rLedDriven |= (1u << (j - 1));
                            b.rLeds[j - 1] = engine->getRegister(rr);
                        }
                    }
                    continue;
                }
                if (id == MX7) {
                    // Downstream MIDI: drain the engine out-queues into this frame,
                    // capped per port at kMidiEventsPerFrame. Overflow stays queued
                    // for the next tick frame (the engine queue holds 64) and flags
                    // dropped[port]. There is no queue-peek seam, so a full frame is
                    // treated as an overflow diagnostic — this over-counts by 1 only
                    // when the queue held exactly kMidiEventsPerFrame events.
                    bool wrote = false;
                    for (int port = 0; port < droid::chain::kChainMidiPorts; port++) {
                        uint8_t cnt = 0;
                        droid::MidiEvent ev;
                        while (cnt < kMidiEventsPerFrame &&
                               engine->drainMidiOut(x7PhysicalPort(port), ev)) {
                            b.midi.ev[port][cnt++] = ev;
                            wrote = true;
                        }
                        b.midi.count[port] = cnt;
                        if (cnt == kMidiEventsPerFrame) b.midi.dropped[port]++;
                    }
                    // Bump the frame seq ONLY when events were written, so the X7
                    // dedupes empty frames; monotonic (never reset) so a reset can
                    // never collide with the reader's last-seen value.
                    if (wrote) b.midi.seq = ++midiSeqDown;
                    // X7 module gate outs G9–G12 (ctrl 0 on the MASTER — canonicalize
                    // only remaps G1..G8). Driven-only, like the G8 output-jack case.
                    for (int j = 0; j < 4; j++) {
                        droid::RegId gr = droid::canonicalize({'G', 0, uint8_t(9 + j)},
                                                              masterType_);
                        if (engine->registerDriven(gr))
                            b.gates[j] = engine->getRegister(gr);
                    }
                    // R-register LED override (manual §5.5): the X7's 2x4 LED
                    // matrix is R49..R56. A driven R register shows its colour
                    // value on that LED instead of the SD/USB/TRS + gate default
                    // (row-major: R49..R52 the status row, R53..R56 the gate row —
                    // see DroidX7::applyDownstream).
                    for (int j = 0; j < 8; j++) {
                        droid::RegId rr{'R', 0, uint8_t(48 + 1 + j)};
                        if (engine->registerDriven(rr)) {
                            b.rLedDriven |= (1u << j);
                            b.rLeds[j] = engine->getRegister(rr);
                        }
                    }
                    continue;
                }
                if (!isControllerModel(id)) continue;   // non-G8 non-controller: skip
                c2++;
                const droid::ControllerModel* cm = chainModels_[i];   // cached under rawChanged
                if (!cm || c2 > engine->declaredControllers().size()) continue;   // surplus: cleared
                for (uint8_t k = 1; k <= cm->leds; k++) {
                    droid::RegId l{'L', c2, k};
                    if (engine->registerDriven(l))
                        b.leds[k - 1] = std::fabs(engine->getRegister(l));   // hardware: negative == positive
                    else if (defaultLedFromButton(id))
                        b.leds[k - 1] = engine->getRegister({'B', c2, k});
                    else if (defaultLedFromPot(id))
                        b.leds[k - 1] = engine->getRegister({'P', c2, k});
                }
                // --- encoder value rings (E4: 4, DB8E: 1) --------------------
                // ring = EncoderState.ringDisplay (the value dot); the L-register
                // white overlay rides leds[] above as on every model. encStepLed
                // is the encoquencer step LED (middle-three bottom ring cells).
                // enc2+1 is the 1-based global matching the upstream `enc`.
                for (uint8_t n = 1; n <= cm->encoders; n++) {
                    int g = ++enc2;
                    b.ring[n - 1]            = engine->encoderRing(g);
                    b.encStepLed[n - 1]      = engine->encoderStepLed(g);
                    b.encStepLedColor[n - 1] = engine->encoderStepLedColor(g);
                }
                // --- motor fader readbacks (M4: 4) ---------------------------
                for (uint8_t f = 1; f <= cm->faders; f++) {
                    int g = ++fad2;
                    b.motorTarget[f - 1] = engine->faderMotorTarget(g);
                    b.notches[f - 1]     = (uint8_t) engine->faderNotches(g);
                    b.faderLed[f - 1]    = engine->faderLed(g);
                    // Colour is the motorfader's `ledcolor` (FaderState.ledColor).
                    // There is NO per-fader R-register override in the engine: R
                    // registers are master-global R1..R56 (ctrl==0), never
                    // R<ctrl>.<f> (validRegisterOnMaster16 rejects R with ctrl!=0,
                    // and no circuit binds it), so registerDriven({'R',ctrl,f}) is
                    // permanently false and the spec-§2 R override is a dead branch
                    // — omitted. See task-2 report Step-1 finding on the R feed.
                    b.faderLedColor[f - 1] = engine->faderLedColor(g);
                }
                // --- DB8E symbolic screen (1 per DB8E, chain order) ----------
                // Mirrors tests/runner/main.cpp evalExpectDisplay: header/body via
                // textForNumber; value+numbermode when !isText. An inactive display
                // has headerText/bodyText 0 -> "" and value 0 (block was zeroed).
                if (id == MDB8E) {
                    if (const droid::DisplayState* ds = engine->displayState(++db8e)) {
                        copyDisplayText(b.dispHeader, engine->textForNumber(float(ds->headerText)));
                        b.dispIsText = ds->isText ? 1 : 0;
                        if (ds->isText)
                            copyDisplayText(b.dispText, engine->textForNumber(float(ds->bodyText)));
                        b.dispValue      = ds->value;
                        b.dispNumbermode = ds->numbermode;
                        b.dispFontsize   = ds->fontsize;
                    }
                }
            }
            rightExpander.module->leftExpander.requestMessageFlip();
        }

        for (int i = 0; i < numOuts_; i++)
            outputs[i].setVoltage(
                engine->getRegister({'O', 0, uint8_t(i + 1)}) * 10.f);

        processExtraIO();   // MASTER18 gate I/O (no-op on MASTER16)
    }

    // --- circuit-state persistence (DROIDSTA.BIN contract) <-> JSON ----------
    // Serialized as a plain JSON array (not base64) so a saved Rack patch stays
    // diffable. Each entry: {type, ord, ver, v:[...]}.
    static json_t* snapshotToJson(const droid::StateSnapshot& snap) {
        json_t* arr = json_array();
        for (const auto& e : snap.entries) {
            json_t* obj = json_object();
            json_object_set_new(obj, "type", json_string(e.type.c_str()));
            json_object_set_new(obj, "ord", json_integer(e.ordinal));
            json_object_set_new(obj, "ver", json_integer(e.version));
            json_t* vals = json_array();
            for (double d : e.values) json_array_append_new(vals, json_real(d));
            json_object_set_new(obj, "v", vals);
            json_array_append_new(arr, obj);
        }
        return arr;
    }
    static droid::StateSnapshot snapshotFromJson(json_t* arr) {
        droid::StateSnapshot snap;
        if (!json_is_array(arr)) return snap;
        size_t i;
        json_t* obj;
        json_array_foreach(arr, i, obj) {
            if (!json_is_object(obj)) continue;
            droid::CircuitState cs;
            if (json_t* j = json_object_get(obj, "type"))
                if (const char* s = json_string_value(j)) cs.type = s;
            if (cs.type.empty()) continue;
            if (json_t* j = json_object_get(obj, "ord")) cs.ordinal = (int)json_integer_value(j);
            if (json_t* j = json_object_get(obj, "ver")) cs.version = (int)json_integer_value(j);
            if (json_t* vals = json_object_get(obj, "v")) {
                if (json_is_array(vals)) {
                    size_t k; json_t* d;
                    json_array_foreach(vals, k, d) cs.values.push_back(json_number_value(d));
                }
            }
            snap.entries.push_back(std::move(cs));
        }
        return snap;
    }

    json_t* dataToJson() override {
        json_t* root = json_object();
        // Rack calls dataToJson on autosave; snapshot the LIVE engine now (under
        // the engine lock) so the saved state matches what is currently dialed
        // in. patchPath is copied out under the same lock — the HTTP bridge
        // thread can be writing it concurrently via loadPatchFile.
        std::string path;
        {
            std::lock_guard<std::mutex> lock(engineMutex);
            path = patchPath;
            if (engine) lastSnapshot = engine->saveState();
        }
        json_object_set_new(root, "patchPath", json_string(path.c_str()));
        json_object_set_new(root, "targetHz", json_real(targetHz));
        json_object_set_new(root, "circuitState", snapshotToJson(lastSnapshot));
        return root;
    }
    void dataFromJson(json_t* root) override {
        // json_number_value accepts both JSON int and real; clamp above 0 so a
        // corrupt/zero value can't divide-by-zero in updateDivider.
        if (json_t* j = json_object_get(root, "targetHz"))
            targetHz = std::fmax(1.f, (float)json_number_value(j));
        updateDivider(APP->engine->getSampleRate());
        // Load the saved circuit state BEFORE the patch load below, so that
        // loadPatchFile (with no live engine yet) restores it into the fresh
        // engine — the Rack-reopen mirror of the hot-reload transfer.
        if (json_t* j = json_object_get(root, "circuitState"))
            lastSnapshot = snapshotFromJson(j);
        // json_string_value returns NULL for a non-string node; dereferencing
        // that via std::string is UB, so guard the accessor.
        if (json_t* j = json_object_get(root, "patchPath")) {
            if (const char* s = json_string_value(j)) {
                std::string p = s;
                if (!p.empty()) loadPatchFile(p);
            }
        }
    }
};

// Shared master widget behaviour: patch-load menu, hot reload, and chain
// revalidation. Both MASTER and MASTER18 derive this; each subclass constructor
// adds its own panel image, ports, and (MASTER only) LED matrix. Operates on the
// common DroidMasterBase via getModule<DroidMasterBase>(), so both masters get
// identical Load / Reload / Tick-rate menus and the same revalidation pump.
struct DroidMasterBaseWidget : ModuleWidget {
    // Hot-reload state. lastMtime is only meaningful for mtimePath; whenever the
    // module's patchPath changes (e.g. the user loads a new file) we reset so a
    // stale mtime from the previous file can't trigger a spurious reload.
    float mtimePollTimer = 0.f;
    double lastMtime = 0.0;
    std::string mtimePath;
    // ISSUE-5: while the chainOk debounce is holding a still-invalid chain, keep
    // revalidating every frame so the tolerance window advances in real time.
    bool chainRevalPending = false;

    void step() override {
        ModuleWidget::step();
        // Widget ctors run before the module is added to APP->scene, so
        // ensureWidget()'s APP->scene guard would no-op there; step() is the
        // first point guaranteed to run with the scene attached. Idempotent.
        if (auto* b = uat::Bridge::instance()) b->ensureWidget();
        DroidMasterBase* m = getModule<DroidMasterBase>();
        if (!m) return;                            // no module in browser preview
        // UI-thread reload for a pending sample-rate change. If no widget ever
        // pumps step() (rare headless/library use), the flag stays set and the
        // engine keeps its previous rate until a frame runs — acceptable.
        if (m->timingDirty.exchange(false))
            m->applyTiming(APP->engine->getSampleRate());
        // UI-thread chain revalidation: recompute chainError/chainOk against the
        // current patch's declared controllers whenever the chain or patch changed.
        // chainRevalPending keeps us revalidating every frame while the ISSUE-5
        // debounce is holding a still-invalid chain, so the tolerance window
        // advances in real time (a static wrong chain still errors within it).
        bool force = m->chainForce.exchange(false);
        if (m->chainDirty.exchange(false) || chainRevalPending || force) {
            std::lock_guard<std::mutex> lock(m->engineMutex);
            if (m->engine) {
                m->chainError = droid::chain::validateChain(
                    m->engine->declaredControllers(), m->chainPhysical);
                // A misplaced/duplicate X7 is a chain error too (controllerModels()
                // skips the X7, so validateChain can't see it). chainX7Error is set
                // in process() under this same lock.
                if (m->chainError.empty())
                    m->chainError = m->chainX7Error;
                auto r = m->chainDebounce.update(m->chainOk, m->chainError.empty(), force);
                m->chainOk = r.ok;
                chainRevalPending = r.pending;
            } else {
                m->chainError.clear();
                m->chainOk = true;
                m->chainDebounce.invalidFrames = 0;
                chainRevalPending = false;
            }
        }
        // Copy patchPath out under the lock — the HTTP bridge thread can be
        // writing it concurrently via loadPatchFile.
        std::string patchPath;
        {
            std::lock_guard<std::mutex> lock(m->engineMutex);
            patchPath = m->patchPath;
        }
        if (patchPath.empty()) return;
        if (patchPath != mtimePath) {              // path changed: restart tracking
            mtimePath = patchPath;
            lastMtime = 0.0;
        }
        mtimePollTimer += APP->window->getLastFrameDuration();
        if (mtimePollTimer < 1.f) return;          // stat at most once per second
        mtimePollTimer = 0.f;
        struct stat st;
        if (stat(patchPath.c_str(), &st) == 0) {
            double mt = (double)st.st_mtime;
            // Only reload once we have a baseline mtime for this file, so the
            // first poll after a (re)load never re-triggers.
            if (lastMtime != 0.0 && mt != lastMtime)
                m->loadPatchFile(patchPath);
            lastMtime = mt;
        }
    }

    void appendContextMenu(Menu* menu) override {
        DroidMasterBase* m = getModule<DroidMasterBase>();
        menu->addChild(new MenuSeparator);
        // Chain status: physical contents plus any validation error. chainPhysical,
        // x7Present, patchPath and patchStatus are all engineMutex-guarded (the
        // latter two because the HTTP bridge thread can load a patch concurrently);
        // chainError is UI-thread-only so it needs no lock. controllerModels() skips
        // the X7, so it is tracked separately (x7Present) — surface it here
        // (ISSUE-4) at its chain position (always the head, nearest the master) so
        // the line confirms X7 presence. Copy everything out before building the
        // menu so the lock isn't held across menu construction.
        std::string patchStatus, patchPath, chainLine;
        bool midiWarn = false;
        {
            std::lock_guard<std::mutex> lock(m->engineMutex);
            patchStatus = m->patchStatus;
            patchPath = m->patchPath;
            std::vector<std::string> parts;
            if (m->x7Present) parts.push_back("x7");
            for (auto& c : m->chainPhysical) parts.push_back(c);
            if (parts.empty()) {
                chainLine = "master only";
            } else {
                for (size_t i = 0; i < parts.size(); i++)
                    chainLine += (i ? ", " : "") + parts[i];
            }
            // ISSUE-3: a MIDI patch with no reachable MIDI hardware (no X7 on a
            // MASTER, and not a MASTER18) runs silently — the MIDI circuits gate
            // off. Flag it so the failure is diagnosable from the UI.
            midiWarn = m->engine && m->engine->patchUsesMidi() && !m->engine->midiAvailable();
        }
        menu->addChild(createMenuLabel(patchStatus));
        menu->addChild(createMenuLabel("chain: " + chainLine));
        if (!m->chainError.empty())
            menu->addChild(createMenuLabel("CHAIN ERROR: " + m->chainError));
        if (midiWarn)
            menu->addChild(createMenuLabel("patch uses MIDI but no X7 detected"));
        menu->addChild(createMenuItem("Load DROID patch…", "", [m]() {
            char* path = osdialog_file(OSDIALOG_OPEN, nullptr, nullptr, nullptr);
            if (path) { m->loadPatchFile(path); free(path); }
        }));
        menu->addChild(createMenuItem("Reload patch", "", [m, patchPath]() {
            if (!patchPath.empty()) m->loadPatchFile(patchPath);
        }, patchPath.empty()));
        menu->addChild(createIndexSubmenuItem("Tick rate",
            {"2 kHz", "4 kHz", "6 kHz (hardware-typical)", "8 kHz"},
            [m]() { return m->tickRateIndex(); },
            [m](size_t i) {
                m->targetHz = DroidMasterBase::kTickRates[i];
                m->applyTiming(APP->engine->getSampleRate());
            }));
    }
};
