#pragma once
// UAT bridge singleton — localhost HTTP control surface for headless Rack UAT.
// HTTP thread parses and dispatches; Rack state is touched only via
// engineMutex-guarded reads, thread-safe Engine calls, or the UI queue
// drained by BridgeWidget. Env-gated: VCVOID_UAT_BRIDGE.
#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>
#include "httpcore.hpp"

struct DroidMasterBase;
namespace rack { struct Context; }

namespace uat {

class Bridge {
public:
    static Bridge* instance();
    static void start();               // no-op unless env var set
    void ensureWidget();               // UI thread; idempotent

    void registerMaster(DroidMasterBase* m);
    void unregisterMaster(DroidMasterBase* m);

    void runOnUi(std::function<void()> fn);
    void drainUi();                    // BridgeWidget::step() only
    void expireHolds();                // BridgeWidget::step() only; UI thread
    bool uiAttached() const { return uiAttached_.load(); }
    void setUiAttached() { uiAttached_.store(true); }

    // Rack's APP macro is a THREAD-LOCAL contextGet(); plugin init() runs
    // before Rack even creates its Context (boot order: plugins 0.09s,
    // engine 0.18s), so the context is captured lazily from the first
    // UI-thread caller and applied on the HTTP thread per-request.
    void noteContext(rack::Context* c) { rackCtx_.store(c); }
    rack::Context* rackContext() const { return rackCtx_.load(); }

    std::string dispatch(const Request& req);

private:
    Bridge() = default;
    void listenLoop();

    // Route handlers (grown by later tasks; all return a JSON body string
    // and set *code). Implemented in Bridge.cpp.
    std::string handlePing(int* code);
    std::string handleMasterStatus(DroidMasterBase* m, int* code);
    std::string handleMasterRegisters(DroidMasterBase* m, const Request& req, int* code);
    std::string handleMasterPatch(DroidMasterBase* m, const Request& req, int* code);
    std::string handleMasterReload(DroidMasterBase* m, int* code);
    std::string handleMasterResetState(DroidMasterBase* m, int* code);
    std::string handleParams(const Request& req, int* code);
    std::string handleProbe(const Request& req, int* code);
    DroidMasterBase* findMaster(int64_t id);

    // Task 10: MIDI pickers + LED/fader read-back.
    std::string handleMidiPorts(DroidMasterBase* m, int* code);
    std::string handleMidiPortSet(DroidMasterBase* m, const Request& req, int* code);
    std::string handleFaders(DroidMasterBase* m, int* code);
    std::string handleLeds(DroidMasterBase* m, int* code);

    // Task 11: Rack lifecycle -- save/quit/sample-rate, plus per-master tick
    // rate. All via uiCall() (patch::Manager/Window/settings are UI-thread
    // state, same reasoning as the generic rack ops above).
    std::string handleRackSave(int* code);
    std::string handleRackQuit(int* code);
    std::string handleRackSampleRate(const Request& req, int* code);
    std::string handleMasterTickRate(DroidMasterBase* m, const Request& req, int* code);

    // Issue #3: CPU cost query + per-circuit profiling toggle.
    std::string handleMasterCpu(DroidMasterBase* m, int* code);
    std::string handleMasterCpuProfiling(DroidMasterBase* m, const Request& req, int* code);

    // Generic rack ops (Task 8): module/cable list/add/delete/move. All six
    // route through the uiCall() helper in Bridge.cpp (RackWidget/ModuleWidget/
    // CableWidget are UI-thread widgets, not Share-locked like Engine).
    std::string handleModulesList(int* code);
    std::string handleModulesAdd(const Request& req, int* code);
    std::string handleModulesDelete(int64_t id, int* code);
    std::string handleModulesMove(int64_t id, const Request& req, int* code);
    std::string handleCablesList(int* code);
    std::string handleCablesAdd(const Request& req, int* code);
    std::string handleCablesDelete(int64_t id, int* code);

    std::thread listener_;
    std::atomic<bool> uiAttached_{false};
    std::atomic<rack::Context*> rackCtx_{nullptr};
    std::mutex uiMutex_;
    std::queue<std::function<void()>> uiQueue_;
    std::mutex mastersMutex_;
    std::vector<DroidMasterBase*> masters_;

    // Longpress-emulation holds: setParamValue'd now, restored to 0 at
    // deadline. moduleId (not Module*) since the module may be deleted
    // before expiry; re-resolved via APP->engine->getModule() at restore
    // time. Guarded by holdsMutex_; expired in expireHolds() (UI thread,
    // called from BridgeWidget::step()).
    struct Hold {
        int64_t moduleId;
        int paramId;
        std::chrono::steady_clock::time_point deadline;
    };
    std::mutex holdsMutex_;
    std::vector<Hold> holds_;
};

} // namespace uat
