#pragma once
#include "plugin.hpp"
#include "src/engine.hpp"   // droid::Engine (via -I../engine)
#include "src/patchstate.hpp"   // issue #42: per-patch circuit-state store
#include "ChainModule.hpp"  // droid::chain protocol + ChainModule::isChain{Left,Right}Neighbor
#include "Layout.hpp"
#include "RegisterLabels.hpp"   // issue #26: patch labels -> tooltips + panel chips
#include "MasterStatus.hpp"     // issue #46: the visible error state (Rack-free model)
#include "StatusRing.hpp"       // issue #46: the module halo
#include "uatbridge/Bridge.hpp"   // forward-declares Rack types only; safe here
#include "AdaptiveRate.hpp"
#include "BuildInfo.hpp"
#include <osdialog.h>
#include <sys/stat.h>
#include <fstream>
#include <sstream>
#include <mutex>
#include <atomic>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <ctime>

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
    // Persistent circuit state (DROIDSTA.BIN contract, hardware.md §11.1),
    // associated with the PATCH rather than with the module (issue #42). The
    // hardware keeps one state blob per SD card and reloads it into whatever
    // patch is on that card; its escape hatch for "different state for a
    // different patch" is a second card, which a Rack module does not have. So a
    // master keeps a store of snapshots keyed by the patch's structural
    // fingerprint, and a load either restores this patch's own snapshot,
    // migrates one from an earlier revision of the same file (or `# STATE:`
    // tag), or starts fresh — never silently inherits an unrelated patch's
    // state. Unbounded by design: nothing is ever evicted.
    //
    // `currentPatchId` is the identity the live engine's state belongs to (what
    // its snapshot is stored under); `legacyState` holds the single blob read
    // from a Rack patch saved before this store existed, consumed by the next
    // load; `forceFreshState` is the one-shot "Reset circuit state" flag.
    // All touched only under engineMutex or on UI-thread paths that hold it.
    droid::PatchStateStore stateStore;
    droid::PatchIdentity currentPatchId;
    droid::StateSnapshot legacyState;
    bool forceFreshState = false;
    // One-line provenance for the status menu / UAT status: "state: restored
    // (saved 12 Sep 11:02)", "state: migrated from …", "state: fresh". Empty
    // when no patch is loaded. engineMutex, like patchStatus.
    std::string stateStatus;
    // Register labels from the patch's header comments (issue #26).
    // `sharedLabels` is written by whichever thread loads the patch (UI or the
    // UAT bridge's HTTP thread) under engineMutex; `labelGen` is bumped after
    // it, and the widgets copy it out on their own step(). `registerLabels` is
    // this master's own UI-thread view — the widgets read it while drawing.
    droid::PatchLabels sharedLabels;              // engineMutex
    std::atomic<uint32_t> labelGen{0};
    vcvoid::labels::ModuleLabels registerLabels;  // UI thread only
    float targetHz = 6000.f;
    // Timing mode (issue #3). Adaptive (default for new masters) derives
    // targetHz from the loaded patch's RAM footprint (AdaptiveRate.hpp) on
    // every load; Fixed behaves as before. Old saves carry no timingMode key
    // and load as Fixed at their saved targetHz (see dataFromJson).
    enum class TimingMode { Adaptive, Fixed };
    TimingMode timingMode = TimingMode::Adaptive;
    float adaptiveHz = vcvoid::kMaxAdaptiveHz;   // last computed; UI thread
    int divider = 8;
    float effectiveRate = 6000.f;   // sampleRate / divider; the rate the engine runs at
    int frameCounter = 0;
    // Relay clock (issue #32): bumped on every tick frame we write downstream, so
    // the controller chain can tell "the master produced something new" from "the
    // same frame again" without knowing our tick rate. Audio thread only.
    uint32_t chainTickSeq = 0;
    // Set on the engine thread by onSampleRateChange; consumed by the widget's
    // step() on the UI thread, which is the only thread allowed to reload.
    std::atomic<bool> timingDirty{false};
    // Experimental (#13): load patches over the hardware limits (RAM budget,
    // 64 000-byte size cap — the latter measured the way the master measures
    // it, with abbreviated parameter names, see droid::deployedPatchSize and
    // issue #41) — the limit errors downgrade to warnings. Persisted;
    // read on whatever thread calls loadPatchFile (bool torn read tolerable,
    // same as the timing fields).
    bool ignoreHwMemoryLimits = false;
    // Experimental (#12): allow vcvoid-only circuits (e.g. trigseq) to load.
    // Off by default so a patch built here stays hardware-compatible; same
    // threading note as ignoreHwMemoryLimits above.
    bool allowExperimentalCircuits = false;

    // --- visible error state (issue #46) ---------------------------------
    // A master that refuses to run used to look exactly like one that is
    // running: the only trace was a line in the context menu, which nobody
    // opens until they already suspect something. These carry the verdict out
    // to the panel — the ring around the module, the MASTER's blink code, the
    // hover tooltip and the menu card.
    //
    // The MODEL is Rack-free and unit-tested (MasterStatus.hpp): everything
    // here is plumbing. `patchUnreadable` is the one piece of state the load
    // path did not already record — loadPatchFile returns early when the file
    // cannot be opened, leaving the PREVIOUS engine running, so "the file I am
    // pointed at is gone" is not visible in lastResult at all.
    bool patchUnreadable = false;        // engineMutex, like patchStatus
    // Set by anything that can change the verdict (a load, a chain
    // revalidation); consumed by the widget's step(), which republishes the
    // lock-free halves below. Starting true makes a fresh module publish once.
    std::atomic<bool> statusDirty{true};
    // The state enum, for the ring. Read every frame by the widget's
    // drawLayer(); written only by publishStatus(), i.e. only on a UI frame —
    // anything that has just changed the verdict off the UI thread reports
    // currentState() instead (see below).
    std::atomic<int> uiState{(int) vcvoid::status::State::NoPatch};
    // What the MASTER's 4x4 matrix should do, and the blink-code colours when
    // that is Blink. Read by the AUDIO thread (DroidMaster::process), hence
    // atomics rather than the plain struct: 16 relaxed loads per tick frame is
    // nothing, and it makes the hand-off race-free instead of
    // "torn read costs one frame". Each entry is 0x00RRGGBB; 0 = dark.
    std::atomic<int> matrixMode{(int) vcvoid::status::Matrix::Dark};
    std::atomic<uint32_t> matrixBlink[16] = {};
    // The one-line status ("LOAD ERROR · line 99 — Unknown register 'O9' …"),
    // UI thread only. Shown as the hover tooltip and appended to the matrix
    // LEDs' light descriptions, so hovering the blinking code decodes it.
    std::string statusLine;

    // Everything the status model needs, copied out under the lock. Cheap
    // enough to call on a menu open or a hover; not called per frame.
    vcvoid::status::Report statusReport() {
        vcvoid::status::Report r;
        {
            std::lock_guard<std::mutex> lock(engineMutex);
            r.havePatch = !patchPath.empty();
            r.fileUnreadable = patchUnreadable;
            r.loadOk = (engine != nullptr) && lastResult.ok;
            r.errorCount = (int) lastResult.errors.size();
            if (!lastResult.errors.empty()) {
                r.errorLine = lastResult.errors[0].line;
                r.errorCode = lastResult.errors[0].code;
                r.errorMessage = lastResult.errors[0].message;
            }
            if (r.fileUnreadable) {
                r.errorCount = 1;
                r.errorLine = 0;
                r.errorMessage = patchStatus;   // "cannot open <path>"
            }
            r.warningCount = (int) lastResult.warnings.size();
            if (!lastResult.warnings.empty()) r.warningMessage = lastResult.warnings[0];
            r.fileName = patchPath.empty() ? std::string() : system::getFilename(patchPath);
        }
        // chainError is UI-thread-only (written by the widget's step()), so it
        // is deliberately read outside the lock, like the menu already does.
        r.chainError = chainError;
        return r;
    }

    // Recompute the verdict and publish the parts other threads read. UI thread
    // only — it writes std::strings and LightInfo descriptions that Rack's
    // tooltips read while drawing.
    void publishStatus() {
        vcvoid::status::Status s = vcvoid::status::evaluate(statusReport());
        for (int i = 0; i < 16; i++) {
            const vcvoid::status::RGB& c = s.blink.led[i];
            uint32_t packed = (uint32_t(rack::math::clamp(c.r, 0.f, 1.f) * 255.f + 0.5f) << 16)
                            | (uint32_t(rack::math::clamp(c.g, 0.f, 1.f) * 255.f + 0.5f) << 8)
                            |  uint32_t(rack::math::clamp(c.b, 0.f, 1.f) * 255.f + 0.5f);
            matrixBlink[i].store(packed, std::memory_order_relaxed);
        }
        // Release AFTER the colours: an audio thread that sees Blink is
        // guaranteed to see the pattern that goes with it.
        matrixMode.store((int) s.matrix, std::memory_order_release);
        uiState.store((int) s.state, std::memory_order_release);
        // Wrapped: the tooltip (and the matrix LEDs' descriptions, which append
        // it) are sized by Rack to their widest line, and a single-line
        // "Circuit '…' is experimental (…)" ran the tooltip off the window.
        statusLine = vcvoid::status::wrapText(vcvoid::status::oneLine(s));
        applyOwnLabels();   // re-stamp the LED descriptions with the new line
    }

    // The last PUBLISHED state. Lock-free and cheap, for the per-frame readers
    // (the ring's drawLayer) — but it only moves when the widget's step()
    // consumes statusDirty, i.e. on the next UI frame.
    vcvoid::status::State statusState() const {
        return (vcvoid::status::State) uiState.load(std::memory_order_acquire);
    }

    // The state as of RIGHT NOW, recomputed instead of read back from the last
    // publish. A caller that has just CHANGED the verdict and has to report it
    // in the same breath must use this: the UAT bridge loads a patch straight
    // from its HTTP thread (loadPatchFile is engine-only and engineMutex-
    // guarded), so at the moment it serialises its reply the UI thread has not
    // run a frame yet and statusState() still holds the pre-load verdict —
    // loading a broken patch over a running one answered "running". Same lock
    // discipline as every other caller: statusReport() takes engineMutex
    // itself, and reads the UI-thread-only chainError outside it exactly as
    // the bridge's status handler and the context menu already do (a chain
    // revalidation the load just armed lands on the next UI frame either way).
    vcvoid::status::State currentState() {
        return vcvoid::status::evaluate(statusReport()).state;
    }

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
    // Per-motor-fader last-seen PANEL position, indexed by 0-based GLOBAL fader
    // number, and whether a value has been seen at all. The fader feed below
    // pushes a position into the engine only when it DIFFERS from this baseline
    // — i.e. only when the user (or the widget's motor animation) actually moved
    // the panel. A frame-by-frame echo of an unchanged position is not user
    // movement, and while a fader is held (motor off, panel frozen) that echo
    // used to overwrite a `clear`/preset/startvalue recall one tick after the
    // engine commanded it — issue #45. Same threading/reset rules as lastDetent.
    float lastFaderPos[droid::chain::kMaxChainModules *
                       droid::chain::kMaxFadersPerModule] = {};
    bool lastFaderPosSeen[droid::chain::kMaxChainModules *
                          droid::chain::kMaxFadersPerModule] = {};
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
    std::mutex probeMutex_;                 // guards probePort_/probeIsOutput_/probeBuf_/probeCount_/probeRateHz_
    int probePort_ = 0;
    bool probeIsOutput_ = true;
    std::vector<float> probeBuf_;
    size_t probeCount_ = 0;
    // True audio-thread sample rate at capture time (args.sampleRate),
    // recorded alongside every sample write below. Lets the bridge derive
    // frame-exact timestamps (i / probeRateHz_) instead of reconstructing a
    // grid from HTTP-thread wall-clock elapsed time, which runs ~10% slow
    // once arm/disarm/re-resolution overhead is folded in (issue #5).
    double probeRateHz_ = 0.0;

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
    // Disarm and return the collected samples plus the true audio-thread
    // sample rate they were captured at (out param, Hz; 0 if no frame ever
    // ran while armed). Only the first probeCount_ samples are valid data;
    // the rest of the preallocated buffer is discarded here.
    std::vector<float> disarmProbe(double* rateHzOut = nullptr) {
        std::lock_guard<std::mutex> lk(probeMutex_);
        probeArmed_.store(false, std::memory_order_relaxed);
        std::vector<float> out(probeBuf_.begin(), probeBuf_.begin() + probeCount_);
        probeBuf_.clear();
        probeBuf_.shrink_to_fit();
        if (rateHzOut) *rateHzOut = probeRateHz_;
        return out;
    }

    // --- UAT bridge signal watch (M6 follow-up: engine registers / cables) ---
    // Armed/collected by the HTTP thread (uat::Bridge::handleWatch*) via
    // armWatch()/collectWatch(); sampled once per ENGINE TICK right after
    // engine->tick() below, inside the engineMutex hold — so a 1-tick trigger
    // pulse (e.g. an internal cable like _T1_P1_SAVE) is caught by
    // construction, which HTTP-side register polling cannot guarantee.
    // Stats are O(1) per signal regardless of window length (no sample
    // buffer), so a watch left armed by a vanished client is harmless.
    // Lock order: the audio thread holds engineMutex then takes watchMutex_;
    // HTTP-side arm/collect take watchMutex_ alone (never inside->outside).
    // Edge = rising crossing of 0.1 with re-arm below 0.05 (engine units:
    // 1 V / 0.5 V — the /probe Schmitt scaled by 10), starting DISARMED so a
    // window opening mid-pulse doesn't count the in-progress pulse.
    struct WatchStat {
        std::string name;       // as armed: register, "F<n>", or "_CABLE"
        float min = 0.f, max = 0.f, last = 0.f;
        double sum = 0.0;
        uint64_t ticks = 0;     // samples taken (0 -> engine never ticked)
        // Time spent >= 0.1, integrated per tick at the tick rate CURRENT at
        // each sample — a mid-window tick-rate change (adaptive mode, POST
        // tick-rate) can't misscale the total the way a ticks-x-final-rate
        // conversion would.
        double highSecs = 0.0;
        int edges = 0;
        bool edgeArmed = false;
    };
    static constexpr size_t kWatchMaxSignals = 16;
    std::mutex watchMutex_;     // guards watch_/watchTickRate_ (tick vs arm/collect)
    std::atomic<bool> watchArmed_{false};
    std::vector<WatchStat> watch_;
    float watchTickRate_ = 0.f; // effectiveRate at last sample (reported as tickRateHz)

    // Install a watch (replaces any armed one). Caller (bridge HTTP thread)
    // validates the names against the live engine FIRST, under engineMutex.
    // The vector (with its string allocations) is built OUTSIDE the lock and
    // swapped in, so a re-arm over a live watch never blocks the audio
    // thread on HTTP-thread heap work.
    void armWatch(const std::vector<std::string>& names) {
        std::vector<WatchStat> fresh;
        fresh.reserve(names.size());
        for (const auto& n : names) {
            WatchStat w;
            w.name = n;
            fresh.push_back(std::move(w));
        }
        std::lock_guard<std::mutex> lk(watchMutex_);
        watch_.swap(fresh);
        watchTickRate_ = 0.f;
        watchArmed_.store(!watch_.empty(), std::memory_order_release);
    }
    // Disarm and collect the accumulated stats. Returns false (leaving *out
    // untouched) if no watch was armed — checked under the same lock as the
    // collect so two concurrent collectors can't both read one arm.
    bool collectWatch(std::vector<WatchStat>* out, float* tickRateOut = nullptr) {
        std::lock_guard<std::mutex> lk(watchMutex_);
        if (!watchArmed_.load(std::memory_order_relaxed))
            return false;
        watchArmed_.store(false, std::memory_order_relaxed);
        if (tickRateOut) *tickRateOut = watchTickRate_;
        out->clear();
        out->swap(watch_);
        return true;
    }
    // Drop any armed watch. Called (under engineMutex) wherever the engine
    // is swapped or destroyed: the arm-time name validation only holds for
    // the engine it ran against — a stale cable name would silently read 0.0
    // from the new engine every tick (Engine::getValue's documented unknown-
    // name behavior), which is indistinguishable from "signal never fired".
    // Collecting after a mid-watch patch load now 409s instead.
    void disarmWatch() {
        std::lock_guard<std::mutex> lk(watchMutex_);
        watch_.clear();
        watchArmed_.store(false, std::memory_order_relaxed);
    }
    bool watchArmed() const { return watchArmed_.load(std::memory_order_acquire); }

    // --- tick-cost stats (issue #3 CPU query) ---
    // Audio-thread accumulation over ~1 s epochs, published as atomics the
    // bridge HTTP thread reads lock-free. Epoch counters are audio-thread
    // only; statValid is dropped on patch load so a fresh engine never shows
    // the previous patch's numbers.
    double epochSumUs_ = 0.0, epochMaxUs_ = 0.0;
    int epochTicks_ = 0;
    std::atomic<float> statAvgTickUs{0.f}, statMaxTickUs{0.f};
    std::atomic<bool> statValid{false};

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

    // The one status line that says where this patch's circuit state came from
    // (issue #42). Deliberately short — it sits directly under the patch status
    // in the module menu and is echoed by the UAT bridge's /master/status.
    static std::string formatStateStatus(droid::StateOrigin origin, int64_t savedAt,
                                         const std::string& migratedFrom) {
        switch (origin) {
            case droid::StateOrigin::Restored: {
                if (savedAt <= 0) return "state: restored";
                std::time_t t = (std::time_t) savedAt;
                char buf[32] = {};
                std::tm tmv{};
#ifdef ARCH_WIN
                localtime_s(&tmv, &t);
#else
                localtime_r(&t, &tmv);
#endif
                if (!std::strftime(buf, sizeof(buf), "%d %b %H:%M", &tmv))
                    return "state: restored";
                return std::string("state: restored (saved ") + buf + ")";
            }
            case droid::StateOrigin::Migrated:
                return migratedFrom.empty()
                    ? "state: migrated from a previous version of this patch"
                    : "state: migrated from previous version of " + migratedFrom;
            case droid::StateOrigin::Fresh:
            default:
                return "state: fresh";
        }
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
                // #46: the previous engine keeps running, so nothing in
                // lastResult says the file we are pointed at has gone. This
                // flag is what turns the ring red and flashes the hardware's
                // "patch not found" code.
                patchUnreadable = true;
                statusDirty.store(true);
                return;
            }
            std::stringstream ss; ss << f.rdbuf(); text = ss.str();
        }
        droid::LoadOptions lopts;
        lopts.ignoreMemoryLimits = ignoreHwMemoryLimits;
        lopts.allowExperimental = allowExperimentalCircuits;
        auto fresh = std::make_unique<droid::Engine>(
            masterType_, effectiveRate);
        droid::LoadResult r = fresh->load(text, lopts);
        if (r.ok) {
            adaptiveHz = vcvoid::adaptiveTickHz(r.ramUsed);
            if (timingMode == TimingMode::Adaptive) {
                targetHz = adaptiveHz;
                updateDivider(APP->engine->getSampleRate());
                // The engine derives all timing from its constructor rate, so
                // if the adaptive rate moved the divider, rebuild at the true
                // effective rate (load cost only; patch loads are rare).
                // This makes loadPatchFile another writer of divider/effectiveRate,
                // from whatever thread loads the patch — including the UAT
                // bridge's HTTP-thread POST /master/patch path — covered by the
                // same plan-acknowledged torn-read tolerance as onSampleRateChange.
                // Invariant: this reload is of the IDENTICAL text, and load
                // success is tick-rate-independent by construction, so r stays
                // ok here. The failure branch below is belt-and-braces should
                // that invariant ever break.
                if (effectiveRate != fresh->tickRateHz()) {
                    fresh = std::make_unique<droid::Engine>(masterType_, effectiveRate);
                    r = fresh->load(text, lopts);
                }
            }
        }
        // Register labels come from the RAW text: they are comments, which the
        // engine parser strips, and they are published even when the patch
        // fails to load — the labels are still what the file says, and a
        // half-broken patch is exactly when knowing what a jack was meant to be
        // helps most.
        droid::PatchLabels labels = droid::parseRegisterLabels(text);
        // Structural identity of the incoming patch (issue #42) — computed from
        // the same raw text, off the lock, before anything is swapped.
        droid::PatchIdentity newId = droid::patchIdentity(text, path);

        std::lock_guard<std::mutex> lock(engineMutex);
        // Every engine swap/drop invalidates an armed signal watch: its names
        // were validated against the OLD engine (see disarmWatch). Lock order
        // engineMutex -> watchMutex_ matches the audio thread's.
        disarmWatch();
        lastResult = r;
        patchPath = path;
        patchUnreadable = false;   // we read it; whatever is wrong is in `r` now
        sharedLabels = std::move(labels);
        // Publish last: a widget that sees the new generation must find the
        // labels already in place.
        labelGen.fetch_add(1, std::memory_order_release);
        // Circuit-state transfer (hardware.md §11.1: "when you press the button
        // for loading a new patch, the states are saved immediately"). The
        // OUTGOING engine's state is parked under the OUTGOING patch's own
        // fingerprint — that is what makes going back and forth between two
        // patches in one master non-destructive. Done BEFORE the r.ok branch:
        // the live engine is dropped either way, and a load that turns out to
        // fail must not cost the previous patch its dialled-in state.
        int64_t now = (int64_t)std::time(nullptr);
        if (engine && !currentPatchId.empty())
            stateStore.store(currentPatchId, engine->saveState(), now);
        if (r.ok) {
            droid::StateOrigin origin = droid::StateOrigin::Fresh;
            int64_t restoredAt = 0;
            std::string migratedFrom;
            if (forceFreshState) {
                // "Reset circuit state" / the UAT bridge's reset-state: this one
                // load must not pick up a migration source either.
                forceFreshState = false;
                legacyState = droid::StateSnapshot();
            } else if (droid::StateLookup lk = stateStore.lookup(newId);
                       lk.source) {
                origin = lk.origin;
                restoredAt = lk.source->savedAt;
                if (origin == droid::StateOrigin::Restored) {
                    fresh->restoreState(lk.source->state);
                } else {
                    migratedFrom = lk.source->title.empty()
                        ? system::getFilename(lk.source->path) : lk.source->title;
                    fresh->migrateState(lk.source->state);
                }
                legacyState = droid::StateSnapshot();
            } else if (!legacyState.empty()) {
                // A Rack patch saved before the store existed carries a single
                // blob that, under the old contract, IS this patch's state.
                // Adopt it as such; it is stored under newId just below.
                origin = droid::StateOrigin::Restored;
                fresh->restoreState(legacyState);
                legacyState = droid::StateSnapshot();
            }
            currentPatchId = newId;
            stateStatus = formatStateStatus(origin, restoredAt, migratedFrom);
            engine = std::move(fresh);
            // Capture the result immediately for anything but a plain restore:
            // a migrated patch must keep its migrated state under the NEW
            // fingerprint (the source entry stays untouched, so the older
            // revision of the file keeps its own state), and a fresh patch gets
            // an entry so its path/tag lineage exists from the first load.
            if (origin != droid::StateOrigin::Restored)
                stateStore.store(currentPatchId, engine->saveState(), now);
            statValid.store(false);   // stale epoch: previous patch's cost
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
            // Nothing ran, so nothing has state. The store is untouched: the
            // last good revision of this file keeps its snapshot, ready to
            // migrate in once the patch parses again.
            stateStatus.clear();
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
        statusDirty.store(true);   // #46: ring / blink code / tooltip / menu card
    }

    // UAT bridge (M9): fresh-boot the currently loaded patch without recreating
    // the module — the F5 recreate-the-module dance, minus the recreation.
    // loadPatchFile() alone is NOT enough: when a live engine exists it
    // snapshots the engine's CURRENT (dialed) state and parks it under the
    // current patch's fingerprint, where the reload would find it again (the
    // hot-reload transfer, by design — see loadPatchFile above). So to really
    // wipe state we drop the live engine first, drop THIS patch's stored
    // snapshot, and arm forceFreshState so the reload cannot migrate one in
    // from an older revision of the same file either. The fresh engine then
    // starts from its circuits' startvalues, same as a cold Rack-reopen with no
    // saved state.
    //
    // Scope is the CURRENT patch only: every other patch's snapshot in this
    // master's store is left alone, exactly as pulling one SD card leaves the
    // others alone.
    void resetCircuitState() {
        std::string path;
        {
            std::lock_guard<std::mutex> lock(engineMutex);
            path = patchPath;
            engine.reset();
            disarmWatch();   // engine dropped: armed names no longer resolvable
            stateStore.erase(currentPatchId.fingerprint);
            legacyState = droid::StateSnapshot();
            forceFreshState = true;
        }
        if (!path.empty())
            loadPatchFile(path);
    }

    // Push the current registerLabels onto this master's own jacks (issue #26).
    // UI thread only — these are std::strings Rack's tooltips read while
    // drawing. MASTER overrides to add its 4x4 LED matrix.
    virtual void applyOwnLabels() {
        using namespace vcvoid::labels;
        applyPortBank(this, Port::INPUT, 0, numIns_, 'I', registerLabels, "I%d");
        applyPortBank(this, Port::OUTPUT, 0, numOuts_, 'O', registerLabels, "O%d");
        applyPortBank(this, Port::OUTPUT, numOuts_, numGateOuts_, 'G',
                      registerLabels, "G%d");
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
                probeRateHz_ = args.sampleRate;
                probeBuf_[probeCount_++] = probeIsOutput_
                    ? outputs[probePort_].getVoltage()
                    : inputs[probePort_].getVoltage();
            }
        }

        // Saturating increment: while a tick is overdue (contended below) the
        // counter parks at `divider` instead of growing without bound, so a
        // pathological contention streak can't overflow it.
        frameCounter = std::min(frameCounter + 1, divider);
        if (frameCounter < divider) return;
        std::unique_lock<std::mutex> lock(engineMutex, std::try_to_lock);
        if (!lock.owns_lock()) return;   // overdue: retry the try-lock next frame
        // Reset only once the lock is held: a contended frame delays the tick
        // by ≤1 audio frame instead of dropping a whole divider cycle (#7).
        frameCounter = 0;

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
                for (auto& seen : lastFaderPosSeen) seen = false;   // ...and fader baselines
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
                    droid::RegId gr = droid::g8Register(g8, j, masterType_);
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
            // moveFader is TOUCH-GATED *and* CHANGE-GATED: an untouched position
            // echo — the widget animating toward the motorTarget — must never
            // register as user movement, and neither must a held fader's
            // unchanged position, repeated every frame while the motor is off
            // under the finger. Re-pushing it undid a `clear`/preset/startvalue
            // recall the tick after the engine commanded the motor, which is
            // what broke the `button = _T` / `clear = _T` toggle trick (#45);
            // the engine's fadercore RecallHold guards the same window from the
            // other side. The PLATE (plateTouch, TOUCH_PARAMS only — never a drag)
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
                float pos = up.block[i].faderPos[f - 1];
                bool posChanged = !lastFaderPosSeen[fad] || pos != lastFaderPos[fad];
                lastFaderPos[fad] = pos;
                lastFaderPosSeen[fad] = true;
                if (touched && posChanged)
                    engine->moveFader(g, pos);
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
        auto tickT0 = std::chrono::steady_clock::now();
        engine->tick();
        double tickUs = std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - tickT0).count();
        epochSumUs_ += tickUs;
        if (tickUs > epochMaxUs_) epochMaxUs_ = tickUs;
        if (++epochTicks_ >= std::max(1, (int)effectiveRate)) {
            statAvgTickUs.store((float)(epochSumUs_ / epochTicks_));
            statMaxTickUs.store((float)epochMaxUs_);
            statValid.store(true);
            epochSumUs_ = epochMaxUs_ = 0.0;
            epochTicks_ = 0;
        }

        // UAT signal watch: sample armed engine signals once per tick, fresh
        // off engine->tick() above (engine is non-null on this path). Lock-free
        // armed check first so the common (unarmed) case costs one atomic load.
        if (watchArmed_.load(std::memory_order_acquire)) {
            std::lock_guard<std::mutex> wlk(watchMutex_);
            if (watchArmed_.load(std::memory_order_relaxed)) {
                watchTickRate_ = effectiveRate;
                for (WatchStat& w : watch_) {
                    float v = engine->getValue(w.name);
                    if (w.ticks == 0) w.min = w.max = v;
                    w.min = std::min(w.min, v);
                    w.max = std::max(w.max, v);
                    w.last = v;
                    w.sum += v;
                    w.ticks++;
                    if (v >= 0.1f && effectiveRate > 0.f)
                        w.highSecs += 1.0 / effectiveRate;
                    if (w.edgeArmed && v >= 0.1f) {
                        w.edgeArmed = false;
                        w.edges++;
                    } else if (!w.edgeArmed && v < 0.05f) {
                        w.edgeArmed = true;
                    }
                }
            }
        }

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
            down->tickSeq = ++chainTickSeq;   // relay clock: this frame is new (issue #32)
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
                        droid::RegId gr = droid::g8Register(g8d, j, masterType_);
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
                    // Select-gated ring image (issue #15): flags/value/colours
                    // straight from the engine's RingDisplay.
                    droid::RingDisplay rd = engine->encoderRingInfo(g);
                    b.ringFlags[n - 1] = (rd.active  ? 1u : 0u)
                                       | (rd.bipolar ? 2u : 0u)
                                       | (rd.fill    ? 4u : 0u)
                                       | (rd.style == 1 ? 8u : 0u);
                    b.ringValue[n - 1]    = rd.value;
                    b.ringColor[n - 1]    = rd.color;
                    b.ringNegColor[n - 1] = rd.negColor;
                    b.ringOverlay[n - 1]  = rd.overlay;
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
                        // The header is cut to kDb8eHeaderChars, hard, no
                        // ellipsis — measured on hardware (issue #19: a cable
                        // named _FILTER_RESONANCE_AMOUNT shows as
                        // "FILTER_RESONANCE_"). Truncation belongs here and not
                        // in the engine: the engine's text table holds the real
                        // name, and how much of it fits is a property of the
                        // screen, like the font sizing beside it.
                        copyDisplayText(b.dispHeader,
                            engine->textForNumber(float(ds->headerText)).substr(
                                0, droid::chain::kDb8eHeaderChars));
                        b.dispIsText = ds->isText ? 1 : 0;
                        if (ds->isText)
                            copyDisplayText(b.dispText, engine->textForNumber(float(ds->bodyText)));
                        b.dispActive     = ds->active ? 1 : 0;
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
            if (!e.signature.empty())
                json_object_set_new(obj, "sig", json_string(e.signature.c_str()));
            json_t* vals = json_array();
            // Integral values are written as JSON integers rather than reals.
            // Most of a snapshot is flags, step counts and indices, and jansson
            // renders a real at 17 significant digits ("0.0", "1.0", and worse
            // for anything inexact); the store is unbounded, so this is free
            // size back. json_number_value on the read side accepts both, so
            // older saves and newer ones parse identically.
            for (double d : e.values) {
                if (d == std::floor(d) && std::fabs(d) < 9.007199254740992e15)
                    json_array_append_new(vals, json_integer((json_int_t)d));
                else
                    json_array_append_new(vals, json_real(d));
            }
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
            // Absent in saves written before issue #42: an empty signature is a
            // circuit with no migration identity, which simply falls back to the
            // positional (hardware) rule — the pre-#42 behaviour.
            if (json_t* j = json_object_get(obj, "sig"))
                if (const char* s = json_string_value(j)) cs.signature = s;
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

    // The per-patch snapshot store (issue #42). One JSON object per stored
    // patch: its structural fingerprint, the file it came from, its title, its
    // optional `# STATE:` tag, when it was saved, and the snapshot itself.
    // Unbounded — the array is as long as the number of distinct patches this
    // master has run, and nothing is ever dropped to make room.
    static json_t* storeToJson(const droid::PatchStateStore& store) {
        json_t* arr = json_array();
        for (const auto& e : store.entries()) {
            json_t* obj = json_object();
            json_object_set_new(obj, "fp", json_string(e.fingerprint.c_str()));
            json_object_set_new(obj, "path", json_string(e.path.c_str()));
            if (!e.title.empty())
                json_object_set_new(obj, "title", json_string(e.title.c_str()));
            if (!e.tag.empty())
                json_object_set_new(obj, "tag", json_string(e.tag.c_str()));
            json_object_set_new(obj, "savedAt", json_integer((json_int_t)e.savedAt));
            json_object_set_new(obj, "state", snapshotToJson(e.state));
            json_array_append_new(arr, obj);
        }
        return arr;
    }
    static droid::PatchStateStore storeFromJson(json_t* arr) {
        droid::PatchStateStore store;
        if (!json_is_array(arr)) return store;
        size_t i;
        json_t* obj;
        json_array_foreach(arr, i, obj) {
            if (!json_is_object(obj)) continue;
            droid::StoredSnapshot e;
            if (json_t* j = json_object_get(obj, "fp"))
                if (const char* s = json_string_value(j)) e.fingerprint = s;
            if (e.fingerprint.empty()) continue;
            if (json_t* j = json_object_get(obj, "path"))
                if (const char* s = json_string_value(j)) e.path = s;
            if (json_t* j = json_object_get(obj, "title"))
                if (const char* s = json_string_value(j)) e.title = s;
            if (json_t* j = json_object_get(obj, "tag"))
                if (const char* s = json_string_value(j)) e.tag = s;
            if (json_t* j = json_object_get(obj, "savedAt"))
                e.savedAt = (int64_t)json_integer_value(j);
            if (json_t* j = json_object_get(obj, "state"))
                e.state = snapshotFromJson(j);
            store.entries().push_back(std::move(e));
        }
        return store;
    }

    json_t* dataToJson() override {
        json_t* root = json_object();
        // Rack calls dataToJson on autosave; snapshot the LIVE engine now (under
        // the engine lock) so the saved state matches what is currently dialed
        // in. patchPath is copied out under the same lock — the HTTP bridge
        // thread can be writing it concurrently via loadPatchFile.
        std::string path;
        droid::StateSnapshot current;
        droid::PatchStateStore storeCopy;
        {
            std::lock_guard<std::mutex> lock(engineMutex);
            path = patchPath;
            // Same cadence as before, now filed under the loaded patch's own
            // fingerprint instead of into one module-wide slot.
            if (engine && !currentPatchId.empty())
                stateStore.store(currentPatchId, engine->saveState(),
                                 (int64_t)std::time(nullptr));
            if (const droid::StoredSnapshot* e =
                    stateStore.find(currentPatchId.fingerprint))
                current = e->state;
            // Copy out, then serialise OUTSIDE the lock: the audio thread takes
            // engineMutex every tick frame, and the store is unbounded, so
            // building the whole JSON tree under it would be a dropout waiting
            // for a big enough store.
            storeCopy = stateStore;
        }
        json_object_set_new(root, "patchPath", json_string(path.c_str()));
        json_object_set_new(root, "targetHz", json_real(targetHz));
        json_object_set_new(root, "timingMode",
            json_string(timingMode == TimingMode::Adaptive ? "adaptive" : "fixed"));
        json_object_set_new(root, "ignoreHwMemoryLimits",
            json_boolean(ignoreHwMemoryLimits));
        json_object_set_new(root, "allowExperimentalCircuits",
            json_boolean(allowExperimentalCircuits));
        json_object_set_new(root, "showRegisterLabels",
            json_boolean(registerLabels.show));
        json_object_set_new(root, "circuitStateStore", storeToJson(storeCopy));
        // Kept for DOWNGRADE compatibility: a vcvoid build from before issue #42
        // reads only this key, and finds exactly what it used to — the currently
        // loaded patch's state. Newer builds prefer circuitStateStore and only
        // fall back to this when the store key is absent.
        json_object_set_new(root, "circuitState", snapshotToJson(current));
        return root;
    }
    void dataFromJson(json_t* root) override {
        // json_number_value accepts both JSON int and real; clamp above 0 so a
        // corrupt/zero value can't divide-by-zero in updateDivider.
        if (json_t* j = json_object_get(root, "targetHz"))
            targetHz = std::fmax(1.f, (float)json_number_value(j));
        updateDivider(APP->engine->getSampleRate());
        // Saves written before timingMode existed carry only targetHz: honor
        // them as Fixed rather than silently switching to Adaptive, so old
        // racks keep byte-for-byte their old timing.
        timingMode = TimingMode::Fixed;
        if (json_t* j = json_object_get(root, "timingMode"))
            if (const char* s = json_string_value(j))
                if (std::strcmp(s, "adaptive") == 0)
                    timingMode = TimingMode::Adaptive;
        // Restore BEFORE the patch load below: an over-budget patch saved with
        // the limits ignored must reload the same way on Rack reopen.
        if (json_t* j = json_object_get(root, "ignoreHwMemoryLimits"))
            ignoreHwMemoryLimits = json_boolean_value(j);
        // Likewise for experimental circuits: a patch using trigseq must reload
        // on Rack reopen instead of failing with the gate error (#12).
        if (json_t* j = json_object_get(root, "allowExperimentalCircuits"))
            allowExperimentalCircuits = json_boolean_value(j);
        if (json_t* j = json_object_get(root, "showRegisterLabels"))
            registerLabels.show = json_boolean_value(j);
        // Load the saved circuit state BEFORE the patch load below, so that
        // loadPatchFile (with no live engine yet) restores it into the fresh
        // engine — the Rack-reopen mirror of the hot-reload transfer.
        //
        // Two shapes are accepted (issue #42). The store is authoritative when
        // present; a Rack patch saved by an older build carries only a single
        // `circuitState` blob, which under the old contract IS the state of
        // whatever patch `patchPath` names — so it is held as legacyState and
        // adopted by the load below under that patch's fingerprint. One reopen
        // converts an old save to the new shape with no state lost.
        if (json_t* j = json_object_get(root, "circuitStateStore")) {
            stateStore = storeFromJson(j);
        } else if (json_t* j2 = json_object_get(root, "circuitState")) {
            legacyState = snapshotFromJson(j2);
        }
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

// --- context-menu error card (issue #46) ---------------------------------
// The top of a master's menu used to be one grey `patchStatus` line. It is the
// place someone lands when the panel's ring has told them something is wrong,
// so it became the card that actually explains and fixes it: a coloured title,
// the message, the offending line quoted from the file, where the patch and its
// circuit state came from, and the three actions worth having right there.

// The card's title row: a coloured square in the state's ring colour, then the
// title. Not a MenuItem — there is nothing to click, and a disabled MenuItem
// greys exactly the text that has to stand out.
struct StatusTitleLabel : ui::MenuLabel {
    NVGcolor color = nvgRGB(0x80, 0x80, 0x80);
    static constexpr float kIndent = 16.f;

    void step() override {
        MenuLabel::step();
        box.size.x += kIndent;
    }
    void draw(const DrawArgs& args) override {
        float s = 8.f;
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 6.f, (box.size.y - s) / 2.f, s, s);
        nvgFillColor(args.vg, color);
        nvgFill(args.vg);
        bndMenuLabel(args.vg, kIndent, 0.f, box.size.x - kIndent, box.size.y,
                     -1, text.c_str());
    }
};

// One line of the patch file, quoted with its number. Monospaced on purpose:
// the whole point of showing the line is that the reader can see WHERE in it
// the problem is, and a proportional font moves the column under them.
struct StatusCodeLabel : ui::MenuLabel {
    int line = 0;
    static constexpr float kFontSize = 12.f;

    void step() override {
        MenuLabel::step();
        // MenuLabel sizes for the theme font; the mono font is wider per
        // character, so measure it here or the menu clips the line.
        box.size.x = std::max(box.size.x, 40.f + kFontSize * 0.62f
                              * float(text.size() + 5));
    }
    void draw(const DrawArgs& args) override {
        std::shared_ptr<rack::Font> font = APP->window->loadFont(
            rack::asset::system("res/fonts/ShareTechMono-Regular.ttf"));
        if (!font || font->handle < 0) {   // no font: fall back, never crash
            MenuLabel::draw(args);
            return;
        }
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 6.f, 1.f, box.size.x - 12.f, box.size.y - 2.f, 2.f);
        nvgFillColor(args.vg, nvgRGB(0x10, 0x10, 0x10));
        nvgFill(args.vg);
        nvgFontFaceId(args.vg, font->handle);
        nvgFontSize(args.vg, kFontSize);
        nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        float y = box.size.y / 2.f;
        std::string num = string::f("%d", line);
        nvgFillColor(args.vg, nvgRGB(0x6f, 0x6f, 0x6f));
        nvgText(args.vg, 12.f, y, num.c_str(), NULL);
        nvgFillColor(args.vg, nvgRGB(0xd8, 0xd8, 0xd8));
        nvgText(args.vg, 12.f + kFontSize * 0.62f * 5.f, y, text.c_str(), NULL);
    }
};

// Read one 1-based line out of a patch file, trimmed of trailing whitespace and
// clipped to the card's column (status::kWrapWidth) so a pathological line
// cannot stretch the menu off the screen. Clipped rather than wrapped: the
// quote is one numbered line of the file and must stay one line.
// Returns "" when the file or the line is not there — the card then simply
// omits the quote, which is also what happens for a whole-patch error.
inline std::string readPatchLine(const std::string& path, int line) {
    if (path.empty() || line <= 0) return std::string();
    std::ifstream f(path);
    if (!f) return std::string();
    std::string s;
    for (int i = 0; i < line; i++)
        if (!std::getline(f, s)) return std::string();
    while (!s.empty() && (s.back() == '\r' || s.back() == ' ' || s.back() == '\t'))
        s.pop_back();
    if (s.size() > vcvoid::status::kWrapWidth)
        s = s.substr(0, vcvoid::status::kWrapWidth - 3) + "...";
    return s;
}

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
    // Register-label distribution (issue #26): what we last published, so the
    // string work only runs when the patch or the physical chain changed.
    uint32_t lastLabelGen = 0;
    std::vector<Module*> lastLabelChain;
    bool labelsPublished = false;
    // #46: the hover tooltip for the master's error state. Owned here and
    // parented to the scene (Rack's own convention for tooltips — a tooltip
    // inside the module would be clipped by, and scroll with, the rack).
    ui::Tooltip* statusTooltip = nullptr;

    ~DroidMasterBaseWidget() override { destroyStatusTooltip(); }

    // --- the module halo (issue #46) -------------------------------------
    // Layer 1 is Rack's LED pass: it runs after every module's panel (so a
    // neighbour cannot paint over the ring) and it is not clipped to the module
    // box (so the glow can bleed over the rails like a bright edge LED).
    void drawLayer(const DrawArgs& args, int layer) override {
        ModuleWidget::drawLayer(args, layer);
        if (layer != 1) return;
        // Framebuffer passes are the module browser's preview and Rack's own
        // screenshot mode (`Rack -t`, which tools/panelshots.sh drives). Rack's
        // LightWidget::drawHalo bows out of those the same way, and it keeps
        // the panel-shot baselines in tests/panel-baseline a picture of the
        // PANEL rather than of whatever state a scratch module happened to be
        // in.
        if (args.fb) return;
        DroidMasterBase* m = getModule<DroidMasterBase>();
        if (!m) return;   // browser preview: no module, no state to report
        vcvoid::status::RGB c;
        if (!vcvoid::status::ringColor(m->statusState(), c)) return;   // running: no ring
        dw::drawStatusRing(args.vg, box.size, dw::toNVG(c));
    }

    // --- the hover tooltip (issue #46) ------------------------------------
    // Rack delivers Enter/Leave to the DEEPEST widget under the pointer, and
    // every control on a master (ports, matrix LEDs) consumes hover itself, so
    // this fires exactly over empty faceplate — the jacks and LEDs keep their
    // own tooltips, which carry the same text (see applyOwnLabels).
    void onEnter(const EnterEvent& e) override {
        ModuleWidget::onEnter(e);
        createStatusTooltip();
    }
    void onLeave(const LeaveEvent& e) override {
        ModuleWidget::onLeave(e);
        destroyStatusTooltip();
    }

    void createStatusTooltip() {
        if (statusTooltip || !settings::tooltips) return;
        DroidMasterBase* m = getModule<DroidMasterBase>();
        if (!m || m->statusLine.empty()) return;   // running: nothing to say
        auto* tt = new ui::Tooltip;
        tt->text = m->statusLine;
        APP->scene->addChild(tt);
        statusTooltip = tt;
    }
    void destroyStatusTooltip() {
        if (!statusTooltip) return;
        APP->scene->removeChild(statusTooltip);
        delete statusTooltip;
        statusTooltip = nullptr;
    }
    // The state can change under a resting pointer (a chain error clears the
    // moment the missing controller is plugged in, a watched file reloads), so
    // a visible tooltip follows it instead of going stale until the next hover.
    void refreshStatusTooltip() {
        DroidMasterBase* m = getModule<DroidMasterBase>();
        if (!statusTooltip) {
            // A master that goes wrong under a resting pointer had nothing to
            // say when the pointer arrived, so there is no tooltip to update —
            // make one now rather than wait for a re-hover.
            if (APP->event->hoveredWidget == this) createStatusTooltip();
            return;
        }
        if (!m || m->statusLine.empty()) destroyStatusTooltip();
        else statusTooltip->text = m->statusLine;
    }

    // Hand the patch's register labels to this master and to every module on
    // its chain, numbering them exactly as the chain protocol does. UI thread:
    // these end up in std::strings that Rack's tooltips read while drawing.
    void updateRegisterLabels(DroidMasterBase* m) {
        std::vector<Module*> chain;
        for (Module* mod = m->rightExpander.module;
             mod && ChainModule::isChainRightNeighbor(mod);
             mod = mod->rightExpander.module)
            chain.push_back(mod);
        // The "Show register labels" toggle belongs to the SYSTEM, not to one
        // module: a master and its chain are one instrument. The master owns
        // the flag (and persists it); every module on the chain mirrors it.
        // Re-pushed every frame — it is a handful of bool writes, and it means
        // the toggle takes effect without waiting for a patch or chain change.
        for (Module* mod : chain)
            static_cast<ChainModule*>(mod)->registerLabels.show =
                m->registerLabels.show;

        uint32_t gen = m->labelGen.load(std::memory_order_acquire);
        if (labelsPublished && gen == lastLabelGen && chain == lastLabelChain)
            return;
        lastLabelGen = gen;
        lastLabelChain = chain;
        labelsPublished = true;

        droid::PatchLabels labels;
        bool havePatch = false;
        {
            std::lock_guard<std::mutex> lock(m->engineMutex);
            labels = m->sharedLabels;
            havePatch = !m->patchPath.empty();
        }
        m->registerLabels.patch = labels;
        m->registerLabels.active = havePatch;
        // A MASTER18's own G1..G4 are written "G1.n": the Forge numbers gate
        // registers per expander even for the master's built-in gates, and
        // rewrites a bare "G3" to G1.3 (modules/module.cpp:207). Harmless on
        // the MASTER, which has no gate jacks.
        m->registerLabels.gateG8 = 1;
        m->applyOwnLabels();

        unsigned controller = 0, g8 = 0;
        for (Module* mod : chain) {
            // isChainRightNeighbor() already established the type.
            auto* cm = static_cast<ChainModule*>(mod);
            auto& ml = cm->registerLabels;
            ml.patch = labels;
            ml.active = havePatch;
            ml.controller = ml.gateG8 = ml.gateOffset = ml.rOffset = 0;
            droid::chain::ModelId id = cm->chainModel();
            if (droid::chain::isControllerModel(id)) {
                ml.controller = ++controller;
            } else if (id == droid::chain::MG8) {
                // Gates are G<n>.1-8; the RGB LED banks continue after the
                // master's own R1..R16, eight per G8.
                ml.gateG8 = ++g8;
                ml.rOffset = 16 + 8 * (g8 - 1);
            } else if (id == droid::chain::MX7) {
                ml.gateOffset = 8;    // the X7's gates are G9..G12
                ml.rOffset = 48;      // and its LEDs R49..R56
            }
            cm->applyOwnLabels();
        }
    }

    void step() override {
        ModuleWidget::step();
        // Widget ctors run before the module is added to APP->scene, so
        // ensureWidget()'s APP->scene guard would no-op there; step() is the
        // first point guaranteed to run with the scene attached. Idempotent.
        if (auto* b = uat::Bridge::instance()) b->ensureWidget();
        DroidMasterBase* m = getModule<DroidMasterBase>();
        if (!m) return;                            // no module in browser preview
        updateRegisterLabels(m);
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
            m->statusDirty.store(true);   // the chain verdict just moved (#46)
        }
        // #46: republish the ring state / blink code / tooltip line whenever
        // the load result or the chain verdict changed. Gated on the flag so
        // the common case is one atomic exchange per frame, not a lock plus a
        // handful of string builds.
        if (m->statusDirty.exchange(false)) {
            m->publishStatus();
            refreshStatusTooltip();
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
        appendMasterMenu(menu);
        appendBuildInfoMenu(menu);
    }

    // The error card (issue #46): the top of the master's menu, and the one
    // place that both explains a refused patch and offers the fixes. Built from
    // the same status model the ring and the tooltip use, so the three can
    // never say different things.
    void appendStatusCard(Menu* menu, DroidMasterBase* m,
                          const std::string& patchPath,
                          const std::string& stateStatus, unsigned ramUsed,
                          const std::vector<std::string>& declared,
                          const std::vector<std::string>& physical) {
        vcvoid::status::Report rep = m->statusReport();
        vcvoid::status::Status s = vcvoid::status::evaluate(rep);
        std::string fileName = patchPath.empty() ? std::string()
                                                 : system::getFilename(patchPath);
        // Rack sizes a menu to its widest child, so every sentence the card
        // shows goes in one wrapped line at a time (issue #46 review): one
        // 190-character error message used to make this menu 1900 px wide.
        // The title row is exempt on purpose — it is generated ("LOAD ERROR ·
        // line 99", "Running with 3 warnings"), never free text, and the
        // coloured square has to sit on the same row as its words.
        auto addWrapped = [&menu](const std::string& text) {
            for (const std::string& l : vcvoid::status::wrapLines(text))
                menu->addChild(createMenuLabel(l));
        };

        if (!s.title.empty()) {
            auto* title = new StatusTitleLabel;
            title->text = s.title;
            vcvoid::status::RGB c;
            if (vcvoid::status::ringColor(s.state, c))
                title->color = dw::toNVG(c);
            menu->addChild(title);
        }
        if (!s.message.empty())
            addWrapped(s.message);
        // A chain error's fix is "plug in what the patch asks for", so spell out
        // both sides rather than only the slot that differs.
        if (s.state == vcvoid::status::State::ChainError) {
            auto list = [](const std::vector<std::string>& v) {
                if (v.empty()) return std::string("nothing");
                std::string out;
                for (size_t i = 0; i < v.size(); i++) out += (i ? ", " : "") + v[i];
                return out;
            };
            addWrapped("patch declares: " + list(declared));
            addWrapped("chain has: " + list(physical));
        }
        // The offending line, quoted from the file. Line errors only: a
        // whole-patch error (too big, out of memory, a register used only as an
        // input) has no single line to point at.
        std::string code = readPatchLine(patchPath, s.line);
        if (!code.empty()) {
            auto* cl = new StatusCodeLabel;
            cl->line = s.line;
            cl->text = code;
            menu->addChild(cl);
        }
        // Where the patch and its circuit state came from (issue #42), on one
        // row: it answers the same "it loaded but does nothing" question the
        // rest of the card answers, so it belongs with it.
        // The file name is elided in the middle rather than wrapped: a name is
        // recognised by its two ends, and a row that starts mid-word reads as a
        // different file.
        std::string shortName = vcvoid::status::elideMiddle(fileName, 32);
        if (!fileName.empty()) {
            std::string line = shortName;
            if (ramUsed) line += string::f(" · %u bytes RAM", ramUsed);
            if (!stateStatus.empty()) line += " · " + stateStatus;
            addWrapped(line);
        }

        menu->addChild(createMenuItem("Reload patch", "", [m, patchPath]() {
            if (!patchPath.empty()) m->loadPatchFile(patchPath);
        }, patchPath.empty()));
        if (!patchPath.empty()) {
            // Hands the file to the platform's default application for .ini —
            // a text editor, on every platform anyone has set one up on. There
            // is no portable way to ask it to jump to a line (every editor
            // spells that differently, and Rack gives a plugin no editor
            // preference to read), so the line rides along as the item's right
            // text and the reader types it into their own Go-to-line.
            std::string label = "Open " + shortName + " in editor";
            std::string right = s.line > 0 ? string::f("line %d", s.line) : "";
            menu->addChild(createMenuItem(label, right, [patchPath]() {
                system::openBrowser(patchPath);
            }));
        }
    }

    // The master-common menu body, separate from appendContextMenu so
    // DroidMaster18Widget can insert its MIDI submenus between this and the
    // trailing build-info line.
    void appendMasterMenu(Menu* menu) {
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
        std::string stateStatus, patchPath, chainLine;
        std::vector<std::string> declared, physical;
        unsigned ramUsed = 0;
        bool midiWarn = false;
        {
            std::lock_guard<std::mutex> lock(m->engineMutex);
            stateStatus = m->stateStatus;
            patchPath = m->patchPath;
            if (m->engine && m->lastResult.ok) ramUsed = m->lastResult.ramUsed;
            // #46: a chain error names the offending slot; the card also shows
            // the whole expected-vs-found pair, which is what actually tells
            // someone what to plug in.
            if (m->engine) declared = m->engine->declaredControllers();
            physical = m->chainPhysical;
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
        appendStatusCard(menu, m, patchPath, stateStatus, ramUsed, declared, physical);
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("chain: " + chainLine));
        if (midiWarn)
            menu->addChild(createMenuLabel("patch uses MIDI but no X7 detected"));
        menu->addChild(createMenuItem("Load DROID patch…", "", [m]() {
            char* path = osdialog_file(OSDIALOG_OPEN, nullptr, nullptr, nullptr);
            if (path) { m->loadPatchFile(path); free(path); }
        }));
        // Mirrors the Forge's View -> Show register labels (F3), but per module
        // rather than per application, since a Rack patch can hold several
        // independent DROID systems.
        menu->addChild(createBoolPtrMenuItem("Show register labels", "",
                                             &m->registerLabels.show));
        menu->addChild(createIndexSubmenuItem("Tick rate",
            {string::f("Adaptive (currently %.1f kHz)", m->adaptiveHz / 1000.f),
             "2 kHz", "4 kHz", "6 kHz (hardware-typical)", "8 kHz"},
            [m]() {
                return m->timingMode == DroidMasterBase::TimingMode::Adaptive
                    ? (size_t)0 : m->tickRateIndex() + 1;
            },
            [m](size_t i) {
                if (i == 0) {
                    m->timingMode = DroidMasterBase::TimingMode::Adaptive;
                    m->targetHz = m->adaptiveHz;
                } else {
                    m->timingMode = DroidMasterBase::TimingMode::Fixed;
                    m->targetHz = DroidMasterBase::kTickRates[i - 1];
                }
                m->applyTiming(APP->engine->getSampleRate());
            }));
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Experimental"));
        menu->addChild(createBoolMenuItem("Ignore memory limits", "",
            [m]() { return m->ignoreHwMemoryLimits; },
            [m](bool v) {
                m->ignoreHwMemoryLimits = v;
                // Re-evaluate immediately: an over-budget patch either loads
                // now or goes back to a hard load error.
                std::string path;
                {
                    std::lock_guard<std::mutex> lock(m->engineMutex);
                    path = m->patchPath;
                }
                if (!path.empty()) m->loadPatchFile(path);
            }));
        menu->addChild(createBoolMenuItem("Allow experimental circuits", "",
            [m]() { return m->allowExperimentalCircuits; },
            [m](bool v) {
                m->allowExperimentalCircuits = v;
                // Re-evaluate immediately, like the memory-limits item: a patch
                // using an experimental circuit either loads now or goes back
                // to the gate's load error.
                std::string path;
                {
                    std::lock_guard<std::mutex> lock(m->engineMutex);
                    path = m->patchPath;
                }
                if (!path.empty()) m->loadPatchFile(path);
            }));
    }
};
