#include "ChainModule.hpp"
#include "DroidWidgets.hpp"

// DROID BLING: 1 HP decorative blind panel. Participates in the expander chain
// as a pure pass-through so a BLING between two controllers (or master and
// controller) does not break the chain: no block added or consumed. Gating on
// each side is asymmetric and mirrors ChainModule::relay() exactly — each
// direction's write is gated ONLY on the destination neighbour being valid,
// never on both. When the source-side neighbour is missing/invalid, Bling
// still writes to a valid destination, but with default-constructed (empty/
// dark) content rather than freezing the destination's buffer at its last
// value. Concretely: a BLING at the right end of the chain (no right
// neighbour) still reports an EMPTY upstream chain to its left neighbour,
// instead of leaving that neighbour's stale consumerMessage untouched (which
// would otherwise look like a phantom controller that never goes away).
struct DroidBling : ChainModule {
    DroidBling() { config(0, 0, 0, 0); }

    // unused — relay() is bypassed entirely
    void fillUpstream(droid::chain::UpstreamBlock&) override {}
    void applyDownstream(const droid::chain::DownstreamBlock&, float) override {}

    void process(const ProcessArgs&) override {
        using namespace droid::chain;
        // upstream: right -> left, gated on the LEFT (destination) neighbour only
        if (leftExpander.module && isChainLeftNeighbor(leftExpander.module)) {
            if (auto* dst = (UpstreamMessage*) leftExpander.module->rightExpander.producerMessage) {
                UpstreamMessage incoming;   // zero-count if chain ends here (no right neighbour)
                if (rightExpander.module && isChainRightNeighbor(rightExpander.module)) {
                    // Count-bounded copy: only the live blocks move, mirroring
                    // relay()'s upstream copy (and avoiding a full-struct copy).
                    const auto* src = (const UpstreamMessage*) rightExpander.consumerMessage;
                    incoming.count = std::min<uint8_t>(src->count, kMaxChainModules);
                    for (uint8_t k = 0; k < incoming.count; k++) incoming.block[k] = src->block[k];
                }
                *dst = incoming;
                leftExpander.module->rightExpander.requestMessageFlip();
            }
        }
        // downstream: left -> right, gated on the RIGHT (destination) neighbour only
        if (rightExpander.module && isChainRightNeighbor(rightExpander.module)) {
            if (auto* dst = (DownstreamMessage*) rightExpander.module->leftExpander.producerMessage) {
                // Left neighbour invalid: dark/empty, don't freeze at last state
                // (matches relay()'s downstream fallback).
                *dst = (leftExpander.module && isChainLeftNeighbor(leftExpander.module))
                           ? *(const DownstreamMessage*) leftExpander.consumerMessage
                           : DownstreamMessage{};
                rightExpander.module->leftExpander.requestMessageFlip();
            }
        }
    }
};

struct DroidBlingWidget : VcvoidModuleWidget {
    DroidBlingWidget(DroidBling* module) {
        setModule(module);
        const auto* L = droid::layout::find("bling");
        box.size = Vec(L->hp * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
        addChild(dw::ImagePanel::create("res/faceplates/bling.png", L->hp, "BLING"));
    }
};

Model* modelDroidBling = createModel<DroidBling, DroidBlingWidget>("bling");
