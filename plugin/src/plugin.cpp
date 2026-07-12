#include "plugin.hpp"
#include "ChainModule.hpp"
#include "uatbridge/Bridge.hpp"
Plugin* pluginInstance;
void init(Plugin* p) {
    pluginInstance = p;
    p->addModel(modelDroidMaster);
    p->addModel(modelDroidMaster18);
    p->addModel(modelDroidP2B8);
    p->addModel(modelDroidP4B2);
    p->addModel(modelDroidP10);
    p->addModel(modelDroidS10);
    p->addModel(modelDroidP8S8);
    p->addModel(modelDroidB32);
    p->addModel(modelDroidE4);
    p->addModel(modelDroidM4);
    p->addModel(modelDroidG8);
    p->addModel(modelDroidDB8E);
    p->addModel(modelDroidX7);
    p->addModel(modelDroidBling);
    uat::Bridge::start();
}

// Modules that relay the chain protocol as a controller (never the master).
static bool isChainController(Module* m) {
    return m && (m->model == modelDroidP2B8
              || m->model == modelDroidP4B2
              || m->model == modelDroidP10
              || m->model == modelDroidS10
              || m->model == modelDroidP8S8
              || m->model == modelDroidB32
              || m->model == modelDroidE4
              || m->model == modelDroidM4
              || m->model == modelDroidG8
              || m->model == modelDroidDB8E
              || m->model == modelDroidX7
              || m->model == modelDroidBling);
}

bool ChainModule::isChainLeftNeighbor(Module* m) {
    // My left neighbour may be the master (which consumes my upstream) or an
    // upstream controller relaying further left.
    return m && (m->model == modelDroidMaster
              || m->model == modelDroidMaster18
              || isChainController(m));
}

bool ChainModule::isChainRightNeighbor(Module* m) {
    // My right neighbour must be a controller: a master to my right belongs to a
    // different chain (and has no left-face buffers to write).
    return isChainController(m);
}
