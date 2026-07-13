#include "Bridge.hpp"
#include "BridgeWidget.hpp"
#include "../MasterBase.hpp"
#include "src/registers.hpp"   // droid::RegId, parseRegisterName, parseFaderName, canonicalize
#include <rack.hpp>
#include <patch.hpp>   // rack::patch::Manager -- context.hpp only forward-declares it
#include <algorithm>
#include <arpa/inet.h>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <future>
#include <jansson.h>
#include <memory>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

#ifndef VCVOID_GIT_HASH
#define VCVOID_GIT_HASH "unknown"
#endif

namespace uat {

static Bridge* g_bridge = nullptr;
Bridge* Bridge::instance() { return g_bridge; }

// Register-name parser: thin reuse wrapper over the engine's own
// droid::parseRegisterName (engine/src/registers.cpp) — no hand-rolled
// tokenizing. Only plain/dotted register forms ("O1", "L1.1", ...); cable
// ("_NAME") and fader ("F<n>") handles are dispatched separately below since
// they are not RegId-addressable.
static bool parseRegId(const std::string& name, droid::RegId* out) {
    auto r = droid::parseRegisterName(name);
    if (!r) return false;
    *out = *r;
    return true;
}

// Consolidates the repeated "dump to compact JSON, free the C string, decref
// the json_t" idiom used by every handler that serializes a response body:
// json_dumps(JSON_COMPACT) can return null on failure, so that case is
// substituted with "{}" rather than propagated, matching every call site this
// replaces. Always takes ownership of (decrefs) j.
static std::string dumpAndFree(json_t* j) {
    char* s = json_dumps(j, JSON_COMPACT);
    std::string out = s ? s : "{}";
    free(s);
    json_decref(j);
    return out;
}

// Consolidates the repeated "parse a JSON request body, 400 on failure" idiom
// used by every POST handler that takes a JSON body. Callers still own the
// exact 400 error string they return on a null result (kept per-site so
// existing response text is unchanged).
static json_t* parseJsonBody(const std::string& body, int* code) {
    json_error_t err;
    json_t* root = json_loads(body.c_str(), 0, &err);
    if (!root) *code = 400;
    return root;
}

// Consolidates the repeated single-field json_pack("{s:s}", "error", msg)
// idiom used by uiCall() closures to build a one-field error object.
static json_t* jerr(const char* msg) {
    return json_pack("{s:s}", "error", msg);
}

void Bridge::start() {
    const char* env = std::getenv("VCVOID_UAT_BRIDGE");
    if (!env || !*env || g_bridge) return;
    g_bridge = new Bridge();
    // Rack's APP macro resolves a THREAD-LOCAL context (context.hpp: "You
    // must set the context when preparing each thread if the code uses the
    // APP macro in that thread"). It cannot be captured here: plugin init()
    // runs before Rack creates its Context (boot log: plugins at 0.09s,
    // engine at 0.18s), so contextGet() is still null then. Instead
    // installBridgeWidget (UI thread) notes the live context and listenLoop
    // applies it before dispatching each request.
    g_bridge->listener_ = std::thread([] { g_bridge->listenLoop(); });
    g_bridge->listener_.detach();     // lives for the process; sockets close on exit
}

void Bridge::ensureWidget() {
    installBridgeWidget();
}

void Bridge::runOnUi(std::function<void()> fn) {
    std::lock_guard<std::mutex> lk(uiMutex_);
    uiQueue_.push(std::move(fn));
}

void Bridge::drainUi() {
    for (;;) {
        std::function<void()> fn;
        {
            std::lock_guard<std::mutex> lk(uiMutex_);
            if (uiQueue_.empty()) return;
            fn = std::move(uiQueue_.front());
            uiQueue_.pop();
        }
        fn();
    }
}

void Bridge::registerMaster(DroidMasterBase* m) {
    std::lock_guard<std::mutex> lk(mastersMutex_);
    masters_.push_back(m);
}
void Bridge::unregisterMaster(DroidMasterBase* m) {
    std::lock_guard<std::mutex> lk(mastersMutex_);
    masters_.erase(std::remove(masters_.begin(), masters_.end(), m), masters_.end());
}

DroidMasterBase* Bridge::findMaster(int64_t id) {
    std::lock_guard<std::mutex> lk(mastersMutex_);
    for (auto* m : masters_)
        if (m->id == id) return m;
    return nullptr;
}

// patchPath/patchStatus/chainPhysical/x7Present/chainError are all touched by
// process()/step() under engineMutex (see MasterBase.hpp field comments —
// chainError's writes in step() are inside the same lock even though it's
// documented "UI thread only"), so a snapshot copy under that lock is a safe
// cross-thread read from the HTTP thread.
std::string Bridge::handleMasterStatus(DroidMasterBase* m, int* code) {
    *code = 200;
    std::string patchPath, patchStatus, chainError;
    std::vector<std::string> chain;
    bool x7 = false;
    bool midiWarn = false;
    {
        std::lock_guard<std::mutex> lk(m->engineMutex);
        patchPath = m->patchPath;
        patchStatus = m->patchStatus;
        chain = m->chainPhysical;
        x7 = m->x7Present;
        chainError = m->chainError;
        // Same diagnostic the context menu computes (MasterBase.hpp
        // appendContextMenu / ISSUE-3): a MIDI patch with no reachable MIDI
        // hardware runs silently. Reuse the engine's own predicates under the
        // same lock the menu takes.
        midiWarn = m->engine && m->engine->patchUsesMidi() && !m->engine->midiAvailable();
    }
    // timingMode/targetHz/effectiveRate/adaptiveHz are UI-thread fields
    // written without engineMutex elsewhere (menu callback, dataFromJson,
    // applyTiming's divider computation) -- same torn-read tolerance the
    // divider already accepts (see onSampleRateChange comment). Read them
    // outside the lock above.
    DroidMasterBase::TimingMode timingMode = m->timingMode;
    float targetHz = m->targetHz;
    float effectiveRate = m->effectiveRate;
    float adaptiveHz = m->adaptiveHz;
    json_t* o = json_object();
    json_object_set_new(o, "patchPath", json_string(patchPath.c_str()));
    json_object_set_new(o, "statusLine", json_string(patchStatus.c_str()));
    json_t* arr = json_array();
    if (x7) json_array_append_new(arr, json_string("x7"));
    for (auto& c : chain) json_array_append_new(arr, json_string(c.c_str()));
    json_object_set_new(o, "chain", arr);
    json_object_set_new(o, "x7Present", json_boolean(x7));
    json_object_set_new(o, "chainError", json_string(chainError.c_str()));
    json_object_set_new(o, "midiWarning", json_boolean(midiWarn));
    json_object_set_new(o, "timingMode",
        json_string(timingMode == DroidMasterBase::TimingMode::Adaptive
                        ? "adaptive" : "fixed"));
    json_object_set_new(o, "targetHz", json_real(targetHz));
    json_object_set_new(o, "effectiveRate", json_real(effectiveRate));
    json_object_set_new(o, "adaptiveHz", json_real(adaptiveHz));
    return dumpAndFree(o);
}

// ids= is a comma-separated list of register/cable/fader handles. Cables
// ("_NAME") and faders ("F<n>") route through Engine::getValue (the same
// dispatch the golden-test harness uses, engine.cpp:242); plain registers go
// through parseRegId + canonicalize + getRegister, the fast typed path the
// Rack chain adapter itself uses. An unresolved plain-register name is a 400
// (the offending name is echoed); an unresolved cable/fader name has no
// existence check exposed by Engine (cableIndex_ is private) so it reads back
// 0.0, matching Engine's own unreachable-cable fallback in makeOperand.
std::string Bridge::handleMasterRegisters(DroidMasterBase* m, const Request& req, int* code) {
    auto it = req.query.find("ids");
    if (it == req.query.end() || it->second.empty()) {
        *code = 400;
        return "{\"error\":\"missing ids\"}";
    }
    std::vector<std::string> names;
    size_t pos = 0;
    const std::string& s = it->second;
    while (pos <= s.size()) {
        size_t comma = s.find(',', pos);
        names.push_back(s.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos));
        if (comma == std::string::npos) break;
        pos = comma + 1;
    }

    std::lock_guard<std::mutex> lk(m->engineMutex);
    if (!m->engine) {
        *code = 400;
        return "{\"error\":\"no patch loaded\"}";
    }
    json_t* o = json_object();
    for (auto& name : names) {
        if (name.empty()) continue;
        float v;
        if (name[0] == '_' || droid::parseFaderName(name)) {
            v = m->engine->getValue(name);
        } else {
            droid::RegId r;
            if (!parseRegId(name, &r)) {
                json_decref(o);
                *code = 400;
                json_t* e = json_object();
                json_object_set_new(e, "error", json_string("unknown register"));
                json_object_set_new(e, "name", json_string(name.c_str()));
                return dumpAndFree(e);
            }
            v = m->engine->getRegister(droid::canonicalize(r, m->masterType_));
        }
        json_object_set_new(o, name.c_str(), json_real(v));
    }
    *code = 200;
    return dumpAndFree(o);
}

// loadPatchFile (MasterBase.hpp) does disk I/O + engine rebuild and guards all
// of its engine/patchPath/patchStatus writes — including the file-not-found
// early return — with engineMutex; it never touches widgets or dialogs. That
// makes it the "engine-only" case the task brief calls out: call it directly
// from the HTTP thread, the same bare call the context-menu actions and the
// mtime poller already make.
std::string Bridge::handleMasterPatch(DroidMasterBase* m, const Request& req, int* code) {
    json_t* root = parseJsonBody(req.body, code);
    if (!root) return "{\"error\":\"invalid JSON body\"}";
    json_t* jpath = json_object_get(root, "path");
    if (!jpath || !json_is_string(jpath)) {
        json_decref(root);
        *code = 400;
        return "{\"error\":\"missing path\"}";
    }
    std::string path = json_string_value(jpath);
    json_decref(root);
    if (path.empty() || path[0] != '/') {
        *code = 400;
        return "{\"error\":\"path must be absolute\"}";
    }
    m->loadPatchFile(path);
    return handleMasterStatus(m, code);
}

std::string Bridge::handleMasterReload(DroidMasterBase* m, int* code) {
    std::string path;
    {
        std::lock_guard<std::mutex> lk(m->engineMutex);
        path = m->patchPath;
    }
    if (path.empty()) {
        *code = 400;
        return "{\"error\":\"no patch loaded\"}";
    }
    m->loadPatchFile(path);
    return handleMasterStatus(m, code);
}

// resetCircuitState() (MasterBase.hpp) drops the live engine and clears
// lastSnapshot under engineMutex, then reloads the current patch — fresh-boot
// semantics (all stateful circuits re-seed from startvalues) without
// recreating the module. Same "no patch loaded" 400 as reload.
std::string Bridge::handleMasterResetState(DroidMasterBase* m, int* code) {
    std::string path;
    {
        std::lock_guard<std::mutex> lk(m->engineMutex);
        path = m->patchPath;
    }
    if (path.empty()) {
        *code = 400;
        return "{\"error\":\"no patch loaded\"}";
    }
    m->resetCircuitState();
    return handleMasterStatus(m, code);
}

// setParamValue/getParamValue (rack-sdk include/engine/Engine.hpp) are the
// only two methods in the Params section with no "Share-locks."/"Exclusively
// locks." doc comment — every other Engine method in that header is
// annotated one way or the other. Treated as NOT confirmed thread-safe off
// the UI thread, so the actual set is queued via runOnUi and applied by
// BridgeWidget::step() (UI thread) rather than called here on the HTTP
// thread. getModule() *is* documented "Share-locks.", so the existence
// check below is a direct call.
std::string Bridge::handleParams(const Request& req, int* code) {
    json_t* root = parseJsonBody(req.body, code);
    if (!root) return "{\"error\":\"invalid JSON body\"}";
    json_t* jModuleId = json_object_get(root, "moduleId");
    json_t* jParamId = json_object_get(root, "paramId");
    json_t* jValue = json_object_get(root, "value");
    json_t* jHoldMs = json_object_get(root, "holdMs");
    if (!jModuleId || !json_is_number(jModuleId) || !jParamId || !json_is_number(jParamId)) {
        json_decref(root);
        *code = 400;
        return "{\"error\":\"missing or non-numeric moduleId/paramId\"}";
    }
    int64_t moduleId = (int64_t)json_number_value(jModuleId);
    int paramId = (int)json_number_value(jParamId);
    float value = (jValue && json_is_number(jValue)) ? (float)json_number_value(jValue) : 0.f;
    int holdMs = (jHoldMs && json_is_number(jHoldMs)) ? (int)json_number_value(jHoldMs) : 0;
    json_decref(root);

    // Reject a negative paramId at parse time; the upper bound is checked once
    // the module is resolved below (params.size() is per-module).
    if (paramId < 0) {
        *code = 400;
        return "{\"error\":\"paramId must be non-negative\"}";
    }

    // The set is queued onto the UI thread (setParamValue), so — like the
    // generic rack ops in uiCall() — it silently no-ops if no BridgeWidget has
    // attached the drain hook yet. Gate the same way rather than returning 200
    // on a set that will never run.
    if (!uiAttached()) {
        *code = 503;
        return "{\"error\":\"ui bridge not attached; add any vcvoid module or launch from the runbook template\"}";
    }

    // getModule() is documented Share-locks. (Engine.hpp), so resolving the
    // module and reading params.size() here on the HTTP thread is safe — do it
    // to reject an out-of-range paramId with a 400 up front rather than letting
    // the queued closure index module->params unchecked (a UI-thread crash on a
    // curl typo).
    rack::engine::Module* mod = APP->engine->getModule(moduleId);
    if (!mod) {
        *code = 404;
        return "{\"error\":\"no such module\"}";
    }
    if (paramId >= (int)mod->params.size()) {
        *code = 400;
        return "{\"error\":\"paramId out of range\"}";
    }

    runOnUi([moduleId, paramId, value] {
        // Re-resolve on the UI thread and re-check bounds defensively: the
        // module can be deleted (and a param count can only be trusted for the
        // module we re-fetch) between the HTTP-thread check above and this run.
        if (rack::engine::Module* m = APP->engine->getModule(moduleId))
            if (paramId >= 0 && paramId < (int)m->params.size())
                APP->engine->setParamValue(m, paramId, value);
    });

    if (holdMs > 0) {
        std::lock_guard<std::mutex> lk(holdsMutex_);
        holds_.push_back({moduleId, paramId,
            std::chrono::steady_clock::now() + std::chrono::milliseconds(holdMs)});
    }

    *code = 200;
    json_t* o = json_object();
    json_object_set_new(o, "ok", json_boolean(true));
    if (holdMs > 0) json_object_set_new(o, "holdMs", json_integer(holdMs));
    return dumpAndFree(o);
}

// Shared stats over a sampled voltage series (M6 /probe). Edge = rising
// crossing of 1.0 V with 0.5 V hysteresis (re-arms once the signal drops back
// below 0.5 V) — a standard Schmitt-trigger debounce so a noisy/ramping
// signal near the threshold does not multi-count. Per-sample timestamps are
// reconstructed as a uniform grid over the measured wall-clock elapsedMs
// (samples are taken at a roughly constant cadence in both backends, so this
// is a good approximation without threading real timestamps through the hot
// per-frame probe write). 0 or 1 edges -> periodStddevMs 0 (no interval to
// measure; documented rather than null so JSON consumers need no null-check).
struct ProbeStats {
    float min = 0.f, max = 0.f, avg = 0.f;
    int edges = 0;
    double periodStddevMs = 0.0;
    double sampleRateHz = 0.0;
};

static ProbeStats computeProbeStats(const std::vector<float>& samples, double elapsedMs) {
    ProbeStats st;
    if (samples.empty()) return st;
    double msPerSample = samples.size() > 1 ? elapsedMs / (double)(samples.size() - 1) : 0.0;
    double sum = 0.0;
    bool armed = true;   // re-armed (below 0.5V or start): a rise past 1.0V counts as an edge
    std::vector<double> edgeMs;
    st.min = st.max = samples[0];
    for (size_t i = 0; i < samples.size(); i++) {
        float v = samples[i];
        st.min = std::min(st.min, v);
        st.max = std::max(st.max, v);
        sum += v;
        if (armed && v >= 1.0f) {
            armed = false;
            st.edges++;
            edgeMs.push_back(i * msPerSample);
        } else if (!armed && v < 0.5f) {
            armed = true;
        }
    }
    st.avg = (float)(sum / samples.size());
    if (edgeMs.size() >= 2) {
        std::vector<double> periods;
        for (size_t i = 1; i < edgeMs.size(); i++) periods.push_back(edgeMs[i] - edgeMs[i - 1]);
        double mean = 0.0;
        for (double p : periods) mean += p;
        mean /= periods.size();
        double var = 0.0;
        for (double p : periods) var += (p - mean) * (p - mean);
        var /= periods.size();
        st.periodStddevMs = std::sqrt(var);
    }
    st.sampleRateHz = elapsedMs > 0.0 ? samples.size() / (elapsedMs / 1000.0) : 0.0;
    return st;
}

static json_t* probeStatsToJson(const ProbeStats& st) {
    json_t* o = json_object();
    json_object_set_new(o, "min", json_real(st.min));
    json_object_set_new(o, "max", json_real(st.max));
    json_object_set_new(o, "avg", json_real(st.avg));
    json_object_set_new(o, "edges", json_integer(st.edges));
    json_object_set_new(o, "periodStddevMs", json_real(st.periodStddevMs));
    json_object_set_new(o, "sampleRateHz", json_real(st.sampleRateHz));
    return o;
}

// GET /probe?moduleId=&portId=&kind=out|in&ms=500 — blocks the HTTP thread
// for (clamped) ms while sampling a port's voltage, then returns
// min/max/avg/edges/periodStddevMs/sampleRateHz. Single-threaded accept loop
// means this blocks other bridge requests for the probe window — acceptable
// at UAT scale (one client, short probes).
//
// Two backends:
//  - Registered vcvoid master: arms the ring buffer in DroidMasterBase
//    (armProbe/disarmProbe, MasterBase.hpp), which process() fills every
//    audio frame under probeMutex_. Best timing fidelity (audio-rate).
//  - Any other module: sampled right here on the HTTP thread, once per
//    millisecond, via APP->engine->getModule(id). getModule() itself
//    share-locks (documented thread-safe); the getVoltage() that follows is
//    a plain `float` member read on Port with no lock — a benign torn read
//    (the value is a single IEEE-754 float written atomically-in-practice by
//    the engine thread; no partial-value tearing is possible on this
//    platform's float stores). Good to roughly >=100 Hz signals given the
//    1 ms sampling grid (finer than a Schmitt-debounced 5 Hz gate square,
//    coarser than audio-rate).
std::string Bridge::handleProbe(const Request& req, int* code) {
    auto qget = [&](const char* k) -> std::string {
        auto it = req.query.find(k);
        return it == req.query.end() ? std::string() : it->second;
    };
    std::string moduleIdStr = qget("moduleId");
    std::string portIdStr = qget("portId");
    std::string kind = qget("kind");
    std::string msStr = qget("ms");
    if (moduleIdStr.empty() || portIdStr.empty() || (kind != "out" && kind != "in")) {
        *code = 400;
        return "{\"error\":\"missing/invalid moduleId, portId, or kind (out|in)\"}";
    }
    char* end = nullptr;
    int64_t moduleId = std::strtoll(moduleIdStr.c_str(), &end, 10);
    if (end == moduleIdStr.c_str() || *end != '\0') {
        *code = 400;
        return "{\"error\":\"invalid moduleId\"}";
    }
    int portId = (int)std::strtol(portIdStr.c_str(), &end, 10);
    if (end == portIdStr.c_str() || *end != '\0' || portId < 0) {
        *code = 400;
        return "{\"error\":\"invalid portId\"}";
    }
    int ms = 500;
    if (!msStr.empty()) {
        ms = (int)std::strtol(msStr.c_str(), &end, 10);
        if (end == msStr.c_str() || *end != '\0') {
            *code = 400;
            return "{\"error\":\"invalid ms\"}";
        }
    }
    ms = std::max(10, std::min(5000, ms));
    bool wantOut = (kind == "out");

    ProbeStats stats;
    auto t0 = std::chrono::steady_clock::now();

    if (DroidMasterBase* m = findMaster(moduleId)) {
        size_t n = wantOut ? m->outputs.size() : m->inputs.size();
        if ((size_t)portId >= n) {
            *code = 400;
            return "{\"error\":\"portId out of range\"}";
        }
        m->armProbe(portId, wantOut);
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        // Re-resolve after the sample window: the master can be deleted
        // mid-probe (user right-click Delete; DELETE /modules later), and its
        // destructor unregisters it — so only touch it again if it is still
        // registered AND is the same pointer we armed. If gone, there is
        // nothing to disarm (buffer and armed flag died with the module).
        // Mirrors the foreign backend's per-iteration re-resolution below.
        DroidMasterBase* m2 = findMaster(moduleId);
        if (m2 != m) {
            *code = 404;
            return "{\"error\":\"module removed during probe\"}";
        }
        std::vector<float> samples = m2->disarmProbe();
        double elapsedMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - t0).count();
        stats = computeProbeStats(samples, elapsedMs);
    } else {
        // Foreign backend needs APP (thread-local context) — not yet applied
        // if no vcvoid widget has stepped (installBridgeWidget notes it).
        if (!rack::contextGet()) {
            *code = 503;
            return "{\"error\":\"rack context not yet available; add a vcvoid module\"}";
        }
        rack::engine::Module* mod = APP->engine->getModule(moduleId);
        if (!mod) {
            *code = 404;
            return "{\"error\":\"no such module\"}";
        }
        size_t n = wantOut ? mod->outputs.size() : mod->inputs.size();
        if ((size_t)portId >= n) {
            *code = 400;
            return "{\"error\":\"portId out of range\"}";
        }
        std::vector<float> samples;
        samples.reserve((size_t)ms + 1);
        // Re-fetch the module every iteration: it can be deleted mid-probe
        // (e.g. the UAT script deletes it) — if so, stop early and report
        // stats over whatever was collected rather than dereferencing a
        // dangling pointer.
        for (int i = 0; i < ms; i++) {
            rack::engine::Module* mm = APP->engine->getModule(moduleId);
            if (!mm) break;
            samples.push_back(wantOut ? mm->outputs[portId].getVoltage()
                                       : mm->inputs[portId].getVoltage());
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        double elapsedMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - t0).count();
        stats = computeProbeStats(samples, elapsedMs);
    }

    *code = 200;
    json_t* o = probeStatsToJson(stats);
    return dumpAndFree(o);
}

// uiCall: run fn on the UI thread and block the HTTP thread on a promise/
// future until it completes or 5s elapse. fn signature: json_t*(int* code)
// -- fn sets *code and returns an owned json_t*, which uiCall dumps and
// frees. Every generic rack op below goes through this, including the pure
// reads (GET /modules, GET /cables): unlike the Params handler's
// setParamValue/getParamValue (documented Share-locks. in Engine.hpp, hence
// callable straight from the HTTP thread), RackWidget/ModuleWidget/
// CableWidget carry no such thread-safety annotation anywhere in the SDK --
// they are plain UI widgets walked and mutated by the render thread every
// frame, so touching them off that thread is unsafe regardless of whether the
// call reads or writes.
template <class F>
static std::string uiCall(F fn, int* code) {
    static const char* kUnattached =
        "{\"error\":\"ui bridge not attached; add any vcvoid module or launch from the runbook template\"}";
    Bridge* b = Bridge::instance();
    if (!b || !b->uiAttached()) {
        *code = 503;
        return kUnattached;
    }
    struct Result { int code; std::string body; };
    auto promise = std::make_shared<std::promise<Result>>();
    std::future<Result> fut = promise->get_future();
    b->runOnUi([fn, promise] {
        int c = 200;
        json_t* j = fn(&c);
        std::string body = dumpAndFree(j);
        promise->set_value(Result{c, body});
    });
    if (fut.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
        // The queued lambda still holds a copy of `promise` and will run
        // eventually (BridgeWidget::step() never stops draining), so its
        // set_value() call is safe even though we give up waiting here --
        // it just writes into a future nobody reads anymore.
        *code = 503;
        return kUnattached;
    }
    Result r = fut.get();
    *code = r.code;
    return r.body;
}

// Reverse of droid::chain::modelName: the chain-adapter code only exposes
// name-from-id (chain.hpp), and the bridge needs id-from-name to call
// defaultLedFromButton/defaultLedFromPot (which take a ModelId) while walking
// chainPhysical (a list of names). Only 9 controller models exist, so a linear
// scan over MP2B8..MDB8E is simplest — no new engine API needed for one bridge
// endpoint's benefit.
static droid::chain::ModelId modelIdFromName(const std::string& name) {
    for (uint8_t i = droid::chain::MP2B8; i <= droid::chain::MDB8E; i++) {
        auto id = (droid::chain::ModelId)i;
        if (name == droid::chain::modelName(id)) return id;
    }
    return droid::chain::None;
}

// Collect every MIDI port reachable from master `m`: its own native ports
// (empty on MASTER16, six on MASTER18 — DroidMasterBase::midiPorts()) plus, if
// an X7 currently sits at the chain head, the X7's four ports. The X7 is
// always the master's immediate right-expander neighbour when present (manual:
// "always first in the controller chain"), so rightExpander.module IS the X7
// module itself — no chain walk needed. Reached via ChainModule::midiPorts()
// (virtual), so this file never needs DroidX7's definition (private to
// X7.cpp). UI-thread only (rack::midi::Port, like ModuleWidget, carries no
// thread-safety annotation) — callers must be inside uiCall().
//
// x7Present is written by process() under engineMutex (MasterBase.hpp field
// comments; handleMasterStatus locks for the same read), so snapshot it —
// and the rightExpander.module pointer that only matters when it is true —
// under that lock, then RELEASE before the dynamic_cast/midiPorts() port
// collection: nothing below needs the engine lock, and holding it across
// the callers' subsequent MIDI driver enumeration would contend with the
// audio thread for no benefit.
static std::vector<ChainModule::MidiPortRef> collectMidiPorts(DroidMasterBase* m) {
    rack::engine::Module* x7Module = nullptr;
    {
        std::lock_guard<std::mutex> lk(m->engineMutex);
        if (m->x7Present)
            x7Module = m->rightExpander.module;
    }
    std::vector<ChainModule::MidiPortRef> refs = m->midiPorts();
    if (auto* x7 = dynamic_cast<ChainModule*>(x7Module)) {
        auto x7refs = x7->midiPorts();
        refs.insert(refs.end(), x7refs.begin(), x7refs.end());
    }
    return refs;
}

// Enumerate every device available for `direction` ("in"/"out") across every
// registered MIDI driver -- the same driver/device walk Rack's own MIDI menu
// (rack::app::appendMidiMenu, used by the masters'/X7's context menus) builds
// its choice lists from, just flattened into one list instead of a submenu
// tree. Driver::get{Input,Output}DeviceIds()/get{Input,Output}DeviceName()
// (midi.hpp) are the low-level calls the Port-level menu widgets ultimately
// call too.
static json_t* enumerateMidiDevices(const std::string& direction) {
    json_t* arr = json_array();
    for (int driverId : rack::midi::getDriverIds()) {
        rack::midi::Driver* drv = rack::midi::getDriver(driverId);
        if (!drv) continue;
        std::vector<int> ids = (direction == "in") ? drv->getInputDeviceIds() : drv->getOutputDeviceIds();
        for (int devId : ids) {
            std::string name = (direction == "in") ? drv->getInputDeviceName(devId)
                                                     : drv->getOutputDeviceName(devId);
            json_t* d = json_object();
            json_object_set_new(d, "driverId", json_integer(driverId));
            json_object_set_new(d, "deviceId", json_integer(devId));
            json_object_set_new(d, "name", json_string(name.c_str()));
            json_array_append_new(arr, d);
        }
    }
    return arr;
}

// GET /master/{id}/midi-ports -> [{port,direction,name,currentDevice,devices:[...]}].
// Empty array for a MASTER16 with no X7 attached (no MIDI hardware reachable) —
// same "empty, not an error" shape as GET /faders with no M4, per the task
// brief's documented choice for a master with no applicable ports.
std::string Bridge::handleMidiPorts(DroidMasterBase* m, int* code) {
    // Capture the master's id, not the raw pointer: uiCall() blocks the HTTP
    // thread up to 5s on the UI queue, during which the module can be deleted
    // (its dtor unregisters it). Re-resolve via findMaster inside the closure
    // (same pattern as the /probe re-resolution) and 404 if it is gone.
    int64_t id = m->id;
    return uiCall([this, id](int* c) -> json_t* {
        DroidMasterBase* m = findMaster(id);
        if (!m) {
            *c = 404;
            return jerr("master removed");
        }
        *c = 200;
        json_t* arr = json_array();
        for (auto& ref : collectMidiPorts(m)) {
            json_t* o = json_object();
            json_object_set_new(o, "port", json_string(ref.key.c_str()));
            json_object_set_new(o, "direction", json_string(ref.direction.c_str()));
            json_object_set_new(o, "name", json_string((ref.key + " " + ref.direction).c_str()));
            int curDevice = ref.port->getDeviceId();
            std::string curName = curDevice >= 0 ? ref.port->getDeviceName(curDevice) : std::string();
            json_object_set_new(o, "currentDevice", json_string(curName.c_str()));
            json_object_set_new(o, "devices", enumerateMidiDevices(ref.direction));
            json_array_append_new(arr, o);
        }
        return arr;
    }, code);
}

// POST /master/{id}/midi-port {port,direction,deviceName} -> {ok,driverId,
// deviceId,name} or 404 {error,candidates:[names...]}. deviceName matches by
// substring against every enumerated device's name for that direction (first
// hit wins, driver-id then device-id order) so a UAT script can say "IAC" and
// not need the exact vendor-qualified string. Device switching goes through
// uiCall — Rack's midi::Port::setDriverId/setDeviceId are not documented
// thread-safe (same reasoning as the generic rack ops in Task 8).
std::string Bridge::handleMidiPortSet(DroidMasterBase* m, const Request& req, int* code) {
    json_t* root = parseJsonBody(req.body, code);
    if (!root) return "{\"error\":\"invalid JSON body\"}";
    json_t* jPort = json_object_get(root, "port");
    json_t* jDir = json_object_get(root, "direction");
    json_t* jDev = json_object_get(root, "deviceName");
    if (!jPort || !json_is_string(jPort) || !jDir || !json_is_string(jDir) ||
        !jDev || !json_is_string(jDev)) {
        json_decref(root);
        *code = 400;
        return "{\"error\":\"missing/invalid port, direction, or deviceName\"}";
    }
    std::string portKey = json_string_value(jPort);
    std::string direction = json_string_value(jDir);
    std::string wantName = json_string_value(jDev);
    json_decref(root);
    if (direction != "in" && direction != "out") {
        *code = 400;
        return "{\"error\":\"direction must be \\\"in\\\" or \\\"out\\\"\"}";
    }

    int64_t id = m->id;
    return uiCall([this, id, portKey, direction, wantName](int* c) -> json_t* {
        DroidMasterBase* m = findMaster(id);
        if (!m) {
            *c = 404;
            return jerr("master removed");
        }
        rack::midi::Port* port = nullptr;
        for (auto& ref : collectMidiPorts(m))
            if (ref.key == portKey && ref.direction == direction) { port = ref.port; break; }
        if (!port) {
            *c = 404;
            return json_pack("{s:s, s:s}", "error", "no such midi port on this master", "port", portKey.c_str());
        }
        for (int driverId : rack::midi::getDriverIds()) {
            rack::midi::Driver* drv = rack::midi::getDriver(driverId);
            if (!drv) continue;
            std::vector<int> ids = (direction == "in") ? drv->getInputDeviceIds() : drv->getOutputDeviceIds();
            for (int devId : ids) {
                std::string name = (direction == "in") ? drv->getInputDeviceName(devId)
                                                         : drv->getOutputDeviceName(devId);
                if (name.find(wantName) != std::string::npos) {
                    port->setDriverId(driverId);
                    port->setDeviceId(devId);
                    *c = 200;
                    return json_pack("{s:b, s:i, s:i, s:s}", "ok", 1,
                                      "driverId", driverId, "deviceId", devId, "name", name.c_str());
                }
            }
        }
        *c = 404;
        json_t* e = json_object();
        json_object_set_new(e, "error", json_string("no device name matched"));
        json_object_set_new(e, "candidates", enumerateMidiDevices(direction));
        return e;
    }, code);
}

// GET /master/{id}/faders -> [{global,motorTarget,notches,led,ledColor}, ...]
// over the chain's M4s, in chain order (global fader numbers assigned the same
// way DroidMasterBase::process() does: walk chainPhysical, skip surplus
// controllers past declaredControllers().size(), count faders only on models
// that have them). Engine reads are engineMutex-guarded, plain register-style
// access straight from the HTTP thread -- the same pattern GET
// /master/{id}/registers already uses, not a UI-thread op, so no uiCall.
// Empty array (not an error) when no patch is loaded or the chain has no M4,
// matching the brief's documented choice.
std::string Bridge::handleFaders(DroidMasterBase* m, int* code) {
    std::lock_guard<std::mutex> lk(m->engineMutex);
    json_t* arr = json_array();
    if (m->engine) {
        int ctrl = 0;
        int fad = 0;
        size_t declaredCount = m->engine->declaredControllers().size();
        for (auto& name : m->chainPhysical) {
            ctrl++;
            if ((size_t)ctrl > declaredCount) continue;   // surplus controller: engine tracks no state for it
            const droid::ControllerModel* cm = droid::findControllerModel(name);
            if (!cm) continue;
            for (uint8_t f = 0; f < cm->faders; f++) {
                int g = ++fad;
                json_t* o = json_object();
                json_object_set_new(o, "global", json_integer(g));
                json_object_set_new(o, "motorTarget", json_real(m->engine->faderMotorTarget(g)));
                json_object_set_new(o, "notches", json_integer(m->engine->faderNotches(g)));
                json_object_set_new(o, "led", json_real(m->engine->faderLed(g)));
                json_object_set_new(o, "ledColor", json_real(m->engine->faderLedColor(g)));
                json_array_append_new(arr, o);
            }
        }
    }
    *code = 200;
    return dumpAndFree(arr);
}

// GET /master/{id}/leds -> {matrix:[16 x {r,g,b}], buttons:{"L<ctrl>.<n>":0..1}}.
// matrix is present only for masters with one (MASTER16;
// DroidMasterBase::matrixLedColor() returns false on MASTER18 -- the field is
// simply omitted, documented deviation for the shape-per-master-type case the
// brief calls out). matrix values are read straight off Module::lights[] (the
// exact smoothed brightness DroidMaster::process() just wrote and the panel
// widget renders), not recomputed -- no engineMutex needed, same benign-float-
// read precedent as the M6 probe's foreign-module Port::getVoltage() reads.
// buttons mirrors DroidMasterBase::process()'s downstream L-register readback
// (registerDriven -> R value; else defaultLedFromButton/Pot -> B/P fallback)
// exactly, walking chainPhysical/declaredControllers() the same way GET
// /master/{id}/faders does; engineMutex-guarded plain engine reads, no uiCall.
std::string Bridge::handleLeds(DroidMasterBase* m, int* code) {
    json_t* root = json_object();
    json_t* matrix = json_array();
    bool hasMatrix = false;
    for (int i = 0; i < 16; i++) {
        float r = 0.f, g = 0.f, b = 0.f;
        if (!m->matrixLedColor(i, r, g, b)) continue;
        hasMatrix = true;
        json_t* c = json_object();
        json_object_set_new(c, "r", json_real(r));
        json_object_set_new(c, "g", json_real(g));
        json_object_set_new(c, "b", json_real(b));
        json_array_append_new(matrix, c);
    }
    if (hasMatrix)
        json_object_set_new(root, "matrix", matrix);
    else
        json_decref(matrix);

    json_t* buttons = json_object();
    {
        std::lock_guard<std::mutex> lk(m->engineMutex);
        if (m->engine) {
            int ctrl = 0;
            size_t declaredCount = m->engine->declaredControllers().size();
            for (auto& name : m->chainPhysical) {
                ctrl++;
                if ((size_t)ctrl > declaredCount) continue;   // surplus: no engine state
                const droid::ControllerModel* cm = droid::findControllerModel(name);
                if (!cm) continue;
                droid::chain::ModelId id = modelIdFromName(name);
                for (uint8_t k = 1; k <= cm->leds; k++) {
                    droid::RegId l{'L', (uint8_t)ctrl, k};
                    float v = 0.f;
                    if (m->engine->registerDriven(l))
                        v = std::fabs(m->engine->getRegister(l));
                    else if (droid::chain::defaultLedFromButton(id))
                        v = m->engine->getRegister({'B', (uint8_t)ctrl, k});
                    else if (droid::chain::defaultLedFromPot(id))
                        v = m->engine->getRegister({'P', (uint8_t)ctrl, k});
                    std::string key = "L" + std::to_string(ctrl) + "." + std::to_string(k);
                    json_object_set_new(buttons, key.c_str(), json_real(v));
                }
            }
        }
    }
    json_object_set_new(root, "buttons", buttons);

    *code = 200;
    return dumpAndFree(root);
}

// GET /modules -- list every module currently in the rack (id, plugin/model
// slug, px position, px width). RackWidget::getModules() (RackWidget.hpp)
// walks the module container; ModuleWidget::model (ModuleWidget.hpp) is not
// owned by the widget so plugin/slug are read straight off it.
std::string Bridge::handleModulesList(int* code) {
    return uiCall([](int* c) -> json_t* {
        *c = 200;
        json_t* arr = json_array();
        for (auto* mw : APP->scene->rack->getModules()) {
            rack::engine::Module* m = mw->module;
            json_t* o = json_object();
            json_object_set_new(o, "id", json_integer(m ? m->id : -1));
            json_object_set_new(o, "plugin", json_string(
                (mw->model && mw->model->plugin) ? mw->model->plugin->slug.c_str() : ""));
            json_object_set_new(o, "slug", json_string(mw->model ? mw->model->slug.c_str() : ""));
            json_object_set_new(o, "x", json_real(mw->box.pos.x));
            json_object_set_new(o, "y", json_real(mw->box.pos.y));
            json_object_set_new(o, "width", json_real(mw->box.size.x));
            json_array_append_new(arr, o);
        }
        return arr;
    }, code);
}

// POST /modules {plugin,slug,x,y} -> {"id":...}. Sequence mirrors Rack's own
// module-browser add: Model::createModule() builds the engine::Module,
// Engine::addModule assigns its id, Model::createModuleWidget(module)
// builds the ModuleWidget (adopting the module), RackWidget::addModule
// attaches the widget. The explicit Engine::addModule is REQUIRED despite
// RackWidget.hpp's doc claim that addModule(widget) "adds it to the Engine"
// -- verified live on Rack 2.6.4 that it does not (module->id stayed -1,
// leaving a zombie widget whose delete crashes in ~ModuleWidget's
// unconditional Engine::removeModule). setModulePosForce then places it at
// the exact requested px position (same "force" placement used by
// /modules/{id}/move, so initial placement and move share behavior).
std::string Bridge::handleModulesAdd(const Request& req, int* code) {
    json_t* root = parseJsonBody(req.body, code);
    if (!root) return "{\"error\":\"invalid JSON body\"}";
    json_t* jPlugin = json_object_get(root, "plugin");
    json_t* jSlug = json_object_get(root, "slug");
    json_t* jX = json_object_get(root, "x");
    json_t* jY = json_object_get(root, "y");
    if (!jPlugin || !json_is_string(jPlugin) || !jSlug || !json_is_string(jSlug) ||
        !jX || !json_is_number(jX) || !jY || !json_is_number(jY)) {
        json_decref(root);
        *code = 400;
        return "{\"error\":\"missing/invalid plugin, slug, x, or y\"}";
    }
    std::string plugin = json_string_value(jPlugin);
    std::string slug = json_string_value(jSlug);
    float x = (float)json_number_value(jX);
    float y = (float)json_number_value(jY);
    json_decref(root);

    return uiCall([plugin, slug, x, y](int* c) -> json_t* {
        rack::plugin::Model* model = rack::plugin::getModel(plugin, slug);
        if (!model) {
            *c = 404;
            return json_pack("{s:s, s:s, s:s}", "error", "unknown plugin/model",
                              "plugin", plugin.c_str(), "slug", slug.c_str());
        }
        rack::engine::Module* module = model->createModule();
        if (!module) {
            *c = 500;
            return jerr("failed to create module");
        }
        APP->engine->addModule(module);   // assigns module->id (see header note)
        rack::app::ModuleWidget* widget = model->createModuleWidget(module);
        if (!widget) {
            APP->engine->removeModule(module);
            delete module;   // never adopted by a widget -- ours to free
            *c = 500;
            return jerr("failed to create module widget");
        }
        APP->scene->rack->addModule(widget);
        APP->scene->rack->setModulePosForce(widget, rack::math::Vec(x, y));
        *c = 200;
        return json_pack("{s:I}", "id", (json_int_t)(widget->module ? widget->module->id : -1));
    }, code);
}

// DELETE /modules/{id}. ModuleWidget::removeAction() (ModuleWidget.hpp:
// "Deletes `this`") is the same call the panel's own right-click Delete
// (and Backspace/Delete key) makes -- it detaches cables, removes the
// engine::Module, removes the widget from the rack, and frees it, all as one
// canonical sequence, so there is nothing left for this handler to mirror
// manually (no separate RackWidget::removeModule + Engine::removeModule +
// delete widget dance to get in the wrong order).
std::string Bridge::handleModulesDelete(int64_t id, int* code) {
    return uiCall([id](int* c) -> json_t* {
        rack::app::ModuleWidget* widget = APP->scene->rack->getModule(id);
        if (!widget) {
            *c = 404;
            return jerr("no such module");
        }
        // A widget whose module never entered the engine (id -1) crashes in
        // ~ModuleWidget's unconditional Engine::removeModule; refuse it.
        if (widget->module && widget->module->id < 0) {
            *c = 500;
            return jerr("module not engine-added (zombie widget); restart Rack to clear it");
        }
        widget->removeAction();
        *c = 200;
        return json_pack("{s:b}", "ok", 1);
    }, code);
}

// POST /modules/{id}/move {x,y} -> {"x":..,"y":..} (actual final px pos).
// RackWidget::setModulePosForce (RackWidget.hpp: "Moves a module to a
// position, pushing other modules in the same row to the left or right, as
// needed.") -- unlike removeModule/addModule this does NOT delete/recreate
// the widget or its Module, so all module-internal Rack ParamQuantity/
// session state (e.g. a pot/button/preset circuit's persisted value) survives
// the move untouched; only box.pos changes. Response echoes widget->box.pos
// after the call (Force always honors the requested position exactly, but
// reading it back rather than echoing the request keeps this handler correct
// even if that guarantee ever changes).
std::string Bridge::handleModulesMove(int64_t id, const Request& req, int* code) {
    json_t* root = parseJsonBody(req.body, code);
    if (!root) return "{\"error\":\"invalid JSON body\"}";
    json_t* jX = json_object_get(root, "x");
    json_t* jY = json_object_get(root, "y");
    if (!jX || !json_is_number(jX) || !jY || !json_is_number(jY)) {
        json_decref(root);
        *code = 400;
        return "{\"error\":\"missing/invalid x or y\"}";
    }
    float x = (float)json_number_value(jX);
    float y = (float)json_number_value(jY);
    json_decref(root);

    return uiCall([id, x, y](int* c) -> json_t* {
        rack::app::ModuleWidget* widget = APP->scene->rack->getModule(id);
        if (!widget) {
            *c = 404;
            return jerr("no such module");
        }
        APP->scene->rack->setModulePosForce(widget, rack::math::Vec(x, y));
        *c = 200;
        return json_pack("{s:f, s:f}", "x", (double)widget->box.pos.x, "y", (double)widget->box.pos.y);
    }, code);
}

// GET /cables -- list every complete/incomplete cable via the Engine (not the
// widget layer): Engine::getCableIds()/getCable() (Engine.hpp) are the source
// of truth CableWidget mirrors, and give inputModule/outputModule ids without
// walking widget children.
std::string Bridge::handleCablesList(int* code) {
    return uiCall([](int* c) -> json_t* {
        *c = 200;
        json_t* arr = json_array();
        for (int64_t id : APP->engine->getCableIds()) {
            rack::engine::Cable* cab = APP->engine->getCable(id);
            if (!cab) continue;
            json_t* o = json_object();
            json_object_set_new(o, "id", json_integer(cab->id));
            json_object_set_new(o, "outputModuleId", json_integer(cab->outputModule ? cab->outputModule->id : -1));
            json_object_set_new(o, "outputId", json_integer(cab->outputId));
            json_object_set_new(o, "inputModuleId", json_integer(cab->inputModule ? cab->inputModule->id : -1));
            json_object_set_new(o, "inputId", json_integer(cab->inputId));
            json_array_append_new(arr, o);
        }
        return arr;
    }, code);
}

// POST /cables {outputModuleId,outputId,inputModuleId,inputId} -> {"id":...}.
// engine::Cable (Cable.hpp) is a plain struct with no factory; the Forge/NH
// pattern (and CableWidget.hpp's own doc comment: "Cable must already be
// added to the Engine" before CableWidget::setCable "Adopts ownership") is:
// build the Cable, APP->engine->addCable() it (assigns ->id), THEN build a
// CableWidget and setCable() it (which resolves the two PortWidgets from the
// already-registered engine module ids) and hand it to
// RackWidget::addCable() ("Adds a cable and adopts ownership.") so it draws
// and participates in the UI same as a hand-patched cable.
std::string Bridge::handleCablesAdd(const Request& req, int* code) {
    json_t* root = parseJsonBody(req.body, code);
    if (!root) return "{\"error\":\"invalid JSON body\"}";
    json_t* jOutMod = json_object_get(root, "outputModuleId");
    json_t* jOutId = json_object_get(root, "outputId");
    json_t* jInMod = json_object_get(root, "inputModuleId");
    json_t* jInId = json_object_get(root, "inputId");
    if (!jOutMod || !json_is_number(jOutMod) || !jOutId || !json_is_number(jOutId) ||
        !jInMod || !json_is_number(jInMod) || !jInId || !json_is_number(jInId)) {
        json_decref(root);
        *code = 400;
        return "{\"error\":\"missing/invalid outputModuleId, outputId, inputModuleId, or inputId\"}";
    }
    int64_t outputModuleId = (int64_t)json_number_value(jOutMod);
    int outputId = (int)json_number_value(jOutId);
    int64_t inputModuleId = (int64_t)json_number_value(jInMod);
    int inputId = (int)json_number_value(jInId);
    json_decref(root);

    return uiCall([outputModuleId, outputId, inputModuleId, inputId](int* c) -> json_t* {
        rack::engine::Module* outMod = APP->engine->getModule(outputModuleId);
        rack::engine::Module* inMod = APP->engine->getModule(inputModuleId);
        if (!outMod || !inMod) {
            *c = 404;
            return jerr("no such module");
        }
        if (outputId < 0 || (size_t)outputId >= outMod->outputs.size() ||
            inputId < 0 || (size_t)inputId >= inMod->inputs.size()) {
            *c = 400;
            return jerr("portId out of range");
        }
        rack::engine::Cable* cable = new rack::engine::Cable();
        cable->outputModule = outMod;
        cable->outputId = outputId;
        cable->inputModule = inMod;
        cable->inputId = inputId;
        APP->engine->addCable(cable);
        rack::app::CableWidget* cw = new rack::app::CableWidget();
        cw->setCable(cable);
        APP->scene->rack->addCable(cw);
        *c = 200;
        return json_pack("{s:I}", "id", (json_int_t)cable->id);
    }, code);
}

// DELETE /cables/{id}. RackWidget::removeCable(cw) (RackWidget.hpp: "Removes
// cable and releases ownership to caller") detaches the widget from the rack
// and hands the CableWidget (and its owned engine::Cable) back to us; unlike
// ModuleWidget::removeAction() there is no single "delete this" convenience
// on CableWidget, so the Engine-side detach is made explicit and defensive
// (hasCable() guard) rather than assumed to have already happened via some
// undocumented onRemove side effect, then the widget (which owns and frees
// the Cable in its destructor) is deleted.
std::string Bridge::handleCablesDelete(int64_t id, int* code) {
    return uiCall([id](int* c) -> json_t* {
        rack::app::CableWidget* cw = APP->scene->rack->getCable(id);
        if (!cw) {
            *c = 404;
            return jerr("no such cable");
        }
        APP->scene->rack->removeCable(cw);
        rack::engine::Cable* cable = cw->cable;
        if (cable && APP->engine->hasCable(cable))
            APP->engine->removeCable(cable);
        delete cw;
        *c = 200;
        return json_pack("{s:b}", "ok", 1);
    }, code);
}

// POST /rack/save -- saves the currently loaded patch in place.
// rack::patch::Manager::save(path) (patch.hpp: "Saves the patch and nothing
// else.") is the same call Rack's own Ctrl+S/menu Save item makes (it just
// also handles the Save-As-if-unsaved branching Rack does with a file dialog,
// which is irrelevant here: the UAT runbook template always launches with a
// patch path already set). 400 if APP->patch->path is empty (nothing to save
// to -- matches the brief's documented case: "the runbook template always has
// one" so this is a defensive guard, not an expected path). save() is not
// documented to throw, but patch I/O can still fail (permissions, disk full),
// so the call is wrapped in try/catch(rack::Exception&) -- the base exception
// type declared in common.hpp that Rack's own dialogs catch around patch I/O
// -- to turn a failure into a 500 with the message rather than crashing the
// UI thread inside a queued uiCall closure. Via uiCall: Manager is UI-thread
// state, same reasoning as the generic rack ops (RackWidget/ModuleWidget
// etc.) above -- no Share-locks./Exclusively-locks. annotation anywhere on
// patch::Manager.
std::string Bridge::handleRackSave(int* code) {
    return uiCall([](int* c) -> json_t* {
        if (APP->patch->path.empty()) {
            *c = 400;
            return jerr("no patch path (unsaved patch)");
        }
        try {
            APP->patch->save(APP->patch->path);
        } catch (rack::Exception& e) {
            *c = 500;
            return jerr(e.what());
        }
        *c = 200;
        return json_pack("{s:b, s:s}", "ok", 1, "path", APP->patch->path.c_str());
    }, code);
}

// POST /rack/quit -- requests a graceful shutdown.
// window::Window::close() (window/Window.hpp: "Request Window to be closed
// after rendering the current frame.") only sets the GLFW should-close flag;
// it does not itself exit the process. Standalone Rack's main loop
// (`while (!glfwWindowShouldClose(win)) { ...render frame... }`, not part of
// the SDK headers but implied by the doc comment's "after rendering the
// current frame" wording) notices the flag on its NEXT iteration, finishes
// that frame, then falls out of the loop and runs its normal shutdown path
// (settings/autosave, window/audio teardown, process exit) -- so quitting is
// asynchronous relative to this call by at least one more frame (typically
// several ms at 60+ fps).
//
// Ordering: this handler runs inside uiCall(), which blocks the HTTP thread
// on a promise until the queued closure finishes -- and close() returning is
// the entire closure body, so the promise is fulfilled (and *code/body
// composed) immediately, well before the main thread even reaches the "fall
// out of the loop" step above, let alone process exit. Control then returns
// to listenLoop() on the HTTP thread, which writes the HTTP response over the
// already-open loopback socket and closes it -- a local write() on an
// established TCP connection is a fast, synchronous kernel buffer copy, not
// something that waits on the peer, so it completes and the response reaches
// the client (e.g. curl) well within the frame-or-more of headroom before the
// process actually exits. No response-torn-off race in practice.
std::string Bridge::handleRackQuit(int* code) {
    return uiCall([](int* c) -> json_t* {
        APP->window->close();
        *c = 200;
        return json_pack("{s:b}", "ok", 1);
    }, code);
}

// POST /rack/sample-rate {hz} -- mirrors Rack's Engine menu "Sample rate"
// picker. rack::engine::Engine::setSampleRate() (Engine.hpp) is PRIVATE
// (rack.hpp: `#define PRIVATE __attribute__((deprecated(...)))` on clang,
// `__attribute__((error(...)))` elsewhere) -- calling it from a plugin either
// hard-fails the build or trips a -Wdeprecated-declarations warning under
// this project's -Wall -Wextra, which would break the warning-clean build
// requirement. The public, non-deprecated lever the menu actually turns is
// the global rack::settings::sampleRate (settings.hpp: "extern float
// sampleRate;"); setting it is exactly what the menu callback does, and the
// engine picks it up itself (Engine::setSuggestedSampleRate's doc comment --
// "Sets the sample rate if the sample rate in the settings is 'Auto'" --
// confirms settings::sampleRate, not a direct Engine call, is the
// plugin-facing source of truth the engine consults). Validated against the
// fixed set Rack's own sample-rate menu offers (44100/48000/88200/96000/
// 176400/192000); anything else is a 400. Via uiCall: settings:: globals are
// touched by UI-thread menu code with no documented thread-safety contract.
std::string Bridge::handleRackSampleRate(const Request& req, int* code) {
    json_t* root = parseJsonBody(req.body, code);
    if (!root) return "{\"error\":\"invalid JSON body\"}";
    json_t* jHz = json_object_get(root, "hz");
    if (!jHz || !json_is_number(jHz)) {
        json_decref(root);
        *code = 400;
        return "{\"error\":\"missing/invalid hz\"}";
    }
    double hz = json_number_value(jHz);
    json_decref(root);
    static const double kValid[] = {44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0};
    bool ok = false;
    for (double v : kValid) if (hz == v) { ok = true; break; }
    if (!ok) {
        *code = 400;
        return "{\"error\":\"hz must be one of 44100, 48000, 88200, 96000, 176400, 192000\"}";
    }

    return uiCall([hz](int* c) -> json_t* {
        rack::settings::sampleRate = (float)hz;
        *c = 200;
        return json_pack("{s:b, s:f}", "ok", 1, "hz", hz);
    }, code);
}

// POST /master/{id}/tick-rate {hz} | {mode:"adaptive"} -- mode-aware
// counterpart of the master's own "Tick rate" context-menu item
// (MasterBase.hpp createIndexSubmenuItem, ~line 998-1010), which offers both
// an "Adaptive" entry (timingMode = Adaptive; targetHz = adaptiveHz) and the
// four fixed-hz entries (timingMode = Fixed; targetHz = kTickRates[i]),
// always followed by applyTiming(). This handler mirrors both branches rather
// than only the fixed one: {"hz":...} implies Fixed at that hz; {"mode":
// "adaptive"} implies Adaptive at the master's current adaptiveHz. Getting
// this right matters because loadPatchFile() (MasterBase.hpp) overwrites
// targetHz from adaptiveHz whenever timingMode == Adaptive -- a handler that
// only set targetHz without also setting timingMode to Fixed would be a
// silent no-op on an Adaptive master (the very bug this handler fixes).
// timingMode/targetHz are plain fields written unlocked by that menu callback
// (UI thread) and by dataToJson/dataFromJson (also UI thread, MasterBase.hpp
// ~line 760/768) -- never under engineMutex -- so this handler matches that
// precedent exactly rather than inventing new locking. applyTiming() itself
// takes engineMutex internally (to snapshot patchPath) before calling
// loadPatchFile(), which is the same "engine-only, HTTP-thread-safe" call
// already used bare by handleMasterReload/handleMasterResetState -- but since
// the outer set of timingMode/targetHz must run on the UI thread (matching
// the menu item, not merely for engineMutex reasons), the whole sequence goes
// through uiCall. hz must be exactly one of the four hardware tick rates
// (2000/4000/6000/8000); anything else -- and any mode other than the literal
// string "adaptive" -- is 400 (unlike the menu's index-based picker, an
// arbitrary HTTP hz value has no "nearest" fallback to silently round to).
// Returns the updated status JSON, matching the convention already used by
// reload/reset-state.
std::string Bridge::handleMasterTickRate(DroidMasterBase* m, const Request& req, int* code) {
    json_t* root = parseJsonBody(req.body, code);
    if (!root) return "{\"error\":\"invalid JSON body\"}";
    static const char* kBadRequest =
        "{\"error\":\"hz must be one of 2000, 4000, 6000, 8000, or mode must be \\\"adaptive\\\"\"}";
    json_t* jMode = json_object_get(root, "mode");
    bool adaptive = false;
    double hz = 0.0;
    if (jMode) {
        if (!json_is_string(jMode) || std::string(json_string_value(jMode)) != "adaptive") {
            json_decref(root);
            *code = 400;
            return kBadRequest;
        }
        adaptive = true;
    } else {
        json_t* jHz = json_object_get(root, "hz");
        if (!jHz || !json_is_number(jHz)) {
            json_decref(root);
            *code = 400;
            return kBadRequest;
        }
        hz = json_number_value(jHz);
        bool valid = false;
        for (float v : DroidMasterBase::kTickRates) if (hz == (double)v) { valid = true; break; }
        if (!valid) {
            json_decref(root);
            *code = 400;
            return kBadRequest;
        }
    }
    json_decref(root);

    // The timingMode/targetHz write + applyTiming() call must happen on the
    // UI thread (matching the menu item), but handleMasterStatus() itself is
    // the ordinary engineMutex-guarded, HTTP-thread-safe read already used
    // bare by handleMasterPatch/handleMasterReload/handleMasterResetState
    // above -- so only the mutation goes through uiCall, and the status
    // snapshot is taken afterward on the HTTP thread, same two-step shape as
    // those three handlers (loadPatchFile(); return
    // handleMasterStatus(m, code);). Capture the id, not the raw pointer:
    // uiCall() blocks up to 5s during which the master can be deleted.
    // Re-resolve inside the closure (and again before the status read below,
    // which also dereferences the master).
    int64_t id = m->id;
    int uiCode = 200;
    std::string uiBody = uiCall([this, id, hz, adaptive](int* c) -> json_t* {
        DroidMasterBase* m = findMaster(id);
        if (!m) {
            *c = 404;
            return jerr("master removed");
        }
        if (adaptive) {
            m->timingMode = DroidMasterBase::TimingMode::Adaptive;
            m->targetHz = m->adaptiveHz;
        } else {
            m->timingMode = DroidMasterBase::TimingMode::Fixed;
            m->targetHz = (float)hz;
        }
        m->applyTiming(APP->engine->getSampleRate());
        *c = 200;
        return json_pack("{s:b}", "ok", 1);
    }, &uiCode);
    if (uiCode != 200) {
        *code = uiCode;
        return uiBody;
    }
    DroidMasterBase* m2 = findMaster(id);
    if (!m2) {
        *code = 404;
        return "{\"error\":\"master removed\"}";
    }
    return handleMasterStatus(m2, code);
}

void Bridge::expireHolds() {
    auto now = std::chrono::steady_clock::now();
    std::vector<Hold> expired;
    {
        std::lock_guard<std::mutex> lk(holdsMutex_);
        auto it = std::remove_if(holds_.begin(), holds_.end(), [&](const Hold& h) {
            return h.deadline <= now;
        });
        expired.assign(it, holds_.end());
        holds_.erase(it, holds_.end());
    }
    for (auto& h : expired) {
        // Re-resolve and bounds-check before restoring: the module may have
        // been deleted, or (defensively) a stale paramId must never index
        // module->params unchecked. Skip silently on a miss — nothing to do.
        if (rack::engine::Module* m = APP->engine->getModule(h.moduleId))
            if (h.paramId >= 0 && h.paramId < (int)m->params.size())
                APP->engine->setParamValue(m, h.paramId, 0.f);
    }
}

std::string Bridge::handlePing(int* code) {
    *code = 200;
    json_t* o = json_object();
    json_object_set_new(o, "bridgeVersion", json_integer(1));
    json_object_set_new(o, "gitHash", json_string(VCVOID_GIT_HASH));
    return dumpAndFree(o);
}

std::string Bridge::dispatch(const Request& req) {
    int code = 404;
    std::string body = "{\"error\":\"no such route\"}";
    auto parts = splitPath(req.path);
    if (req.method == "GET" && req.path == "/ping")
        body = handlePing(&code);
    else if (req.method == "POST" && req.path == "/params")
        body = handleParams(req, &code);
    else if (req.method == "GET" && req.path == "/probe")
        body = handleProbe(req, &code);
    else if (parts.size() == 3 && parts[0] == "master") {
        int64_t id = std::strtoll(parts[1].c_str(), nullptr, 10);
        DroidMasterBase* m = findMaster(id);
        if (!m) { code = 404; body = "{\"error\":\"no such master\"}"; }
        else if (req.method == "GET" && parts[2] == "status")
            body = handleMasterStatus(m, &code);
        else if (req.method == "GET" && parts[2] == "registers")
            body = handleMasterRegisters(m, req, &code);
        else if (req.method == "POST" && parts[2] == "patch")
            body = handleMasterPatch(m, req, &code);
        else if (req.method == "POST" && parts[2] == "reload")
            body = handleMasterReload(m, &code);
        else if (req.method == "POST" && parts[2] == "reset-state")
            body = handleMasterResetState(m, &code);
        else if (req.method == "GET" && parts[2] == "midi-ports")
            body = handleMidiPorts(m, &code);
        else if (req.method == "POST" && parts[2] == "midi-port")
            body = handleMidiPortSet(m, req, &code);
        else if (req.method == "GET" && parts[2] == "faders")
            body = handleFaders(m, &code);
        else if (req.method == "GET" && parts[2] == "leds")
            body = handleLeds(m, &code);
        else if (req.method == "POST" && parts[2] == "tick-rate")
            body = handleMasterTickRate(m, req, &code);
    }
    else if (req.method == "POST" && req.path == "/rack/save")
        body = handleRackSave(&code);
    else if (req.method == "POST" && req.path == "/rack/quit")
        body = handleRackQuit(&code);
    else if (req.method == "POST" && req.path == "/rack/sample-rate")
        body = handleRackSampleRate(req, &code);
    else if (req.method == "GET" && req.path == "/modules")
        body = handleModulesList(&code);
    else if (req.method == "POST" && req.path == "/modules")
        body = handleModulesAdd(req, &code);
    else if (req.method == "DELETE" && parts.size() == 2 && parts[0] == "modules") {
        int64_t id = std::strtoll(parts[1].c_str(), nullptr, 10);
        body = handleModulesDelete(id, &code);
    }
    else if (req.method == "POST" && parts.size() == 3 && parts[0] == "modules" && parts[2] == "move") {
        int64_t id = std::strtoll(parts[1].c_str(), nullptr, 10);
        body = handleModulesMove(id, req, &code);
    }
    else if (req.method == "GET" && req.path == "/cables")
        body = handleCablesList(&code);
    else if (req.method == "POST" && req.path == "/cables")
        body = handleCablesAdd(req, &code);
    else if (req.method == "DELETE" && parts.size() == 2 && parts[0] == "cables") {
        int64_t id = std::strtoll(parts[1].c_str(), nullptr, 10);
        body = handleCablesDelete(id, &code);
    }
    // Later tasks extend routing here (parts-based).
    return makeResponse(code, body);
}

void Bridge::listenLoop() {
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(2601);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (bind(srv, (sockaddr*)&addr, sizeof addr) != 0 || listen(srv, 8) != 0) {
        close(srv);
        return;
    }
    bool ctxApplied = false;
    for (;;) {
        int cli = accept(srv, nullptr, nullptr);
        if (cli < 0) continue;
        // Apply Rack's thread-local context once it exists (noted by
        // installBridgeWidget on the UI thread) — plugin init spawned this
        // thread before Rack created its Context, so APP-> uses on this
        // thread segfault until contextSet runs here.
        if (!ctxApplied) {
            if (rack::Context* c = rackCtx_.load()) {
                rack::contextSet(c);
                ctxApplied = true;
            }
        }
        std::string raw;
        char buf[8192];
        // Single read is fine for our tiny bodies; loop until headers+body seen.
        for (;;) {
            ssize_t n = read(cli, buf, sizeof buf);
            if (n <= 0) break;
            raw.append(buf, n);
            size_t he = raw.find("\r\n\r\n");
            if (he == std::string::npos) continue;
            size_t cl = 0;
            size_t p = raw.find("Content-Length:");
            if (p != std::string::npos) cl = std::strtoul(raw.c_str() + p + 15, nullptr, 10);
            if (raw.size() >= he + 4 + cl) break;
        }
        Request req;
        std::string resp = parseRequest(raw, req)
            ? dispatch(req)
            : makeResponse(400, "{\"error\":\"bad request\"}");
        (void)write(cli, resp.data(), resp.size());
        close(cli);
    }
}

} // namespace uat
