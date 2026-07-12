#pragma once
#include "plugin.hpp"
#include "src/chain.hpp"
#include <midi.hpp>
#include <string>
#include <vector>

// Base for all DROID chain expander modules (controllers + G8). Owns the
// receiver-side double buffers Rack requires and relays chain messages one hop
// per frame in each direction:
//   upstream  (controls, toward master): prepend my block, write the LEFT
//             neighbour's rightExpander producer buffer;
//   downstream(LEDs, away from master): consume block[0] of my leftExpander
//             consumer buffer, relay the rest to the RIGHT neighbour.
// Latency is one frame per hop per direction — well inside the engine tick
// divider (>= 6 frames at 48 kHz / 8 kHz target).
struct ChainModule : Module {
    ChainModule() {
        leftExpander.producerMessage  = new droid::chain::DownstreamMessage;
        leftExpander.consumerMessage  = new droid::chain::DownstreamMessage;
        rightExpander.producerMessage = new droid::chain::UpstreamMessage;
        rightExpander.consumerMessage = new droid::chain::UpstreamMessage;
    }
    ~ChainModule() override {
        delete (droid::chain::DownstreamMessage*) leftExpander.producerMessage;
        delete (droid::chain::DownstreamMessage*) leftExpander.consumerMessage;
        delete (droid::chain::UpstreamMessage*)  rightExpander.producerMessage;
        delete (droid::chain::UpstreamMessage*)  rightExpander.consumerMessage;
    }

    virtual void fillUpstream(droid::chain::UpstreamBlock& b) = 0;
    virtual void applyDownstream(const droid::chain::DownstreamBlock& b, float sampleTime) = 0;

  protected:
    // Packs `n` momentary-button params (starting at `firstParamId`) into a
    // bitmask, LSB = first button — the shape every button-bank controller
    // (P2B8, P4B2, B32, DB8E's face buttons) sends upstream in fillUpstream().
    uint32_t packButtonParams(int firstParamId, int n) {   // Param::getValue() is non-const
        uint32_t bits = 0;
        for (int i = 0; i < n; ++i)
            if (params[firstParamId + i].getValue() > 0.5f) bits |= (1u << i);
        return bits;
    }

    // Smooths `n` downstream LED values (starting at `values[0]`) into `n`
    // lights (starting at `firstLightId`) — the shape every LED-bank
    // controller (P2B8, P4B2, P8S8) uses in applyDownstream().
    void applyLedBank(int firstLightId, int n, const float* values, float sampleTime) {
        for (int i = 0; i < n; ++i)
            lights[firstLightId + i].setBrightnessSmooth(values[i], sampleTime);
    }

  public:

    // UAT bridge (M10): a chain module's native MIDI ports, if any (only the X7
    // carries MIDI on the controller chain). key is the bridge's port handle
    // ("x7usb"/"x7trs"); direction "in"/"out". Base returns none; DroidX7
    // overrides. Lets the HTTP bridge reach the X7's rack::midi::Port objects
    // through the base-class pointer it already has (m->rightExpander.module)
    // without needing DroidX7's definition (private to X7.cpp).
    struct MidiPortRef { std::string key; std::string direction; rack::midi::Port* port; };
    virtual std::vector<MidiPortRef> midiPorts() { return {}; }

    // The chain is side-asymmetric, so neighbour validity is too. DroidMaster
    // allocates only rightExpander buffers (no left-face producer/consumer), so
    // it can be a valid LEFT neighbour only — a master to my right is not part
    // of my chain and must never be written to.
    static bool isChainLeftNeighbor(Module* m);   // valid as MY left neighbour: master or a controller
    static bool isChainRightNeighbor(Module* m);  // valid as MY right neighbour: controllers only, never the master

    void relay(float sampleTime) {
        using namespace droid::chain;
        // ---- upstream: my controls + everything from my right, to my left --
        UpstreamMessage incoming;                    // zero-count if chain ends here
        if (rightExpander.module && isChainRightNeighbor(rightExpander.module)) {
            // Count-bounded copy: only the live blocks (not all 21) move per
            // frame. `incoming` is default-constructed (count=0, tail zeroed) and
            // prependUpstream reads only block[0..count-1], so this is behavior-
            // preserving while avoiding a ~2.7 KB full-struct copy every frame.
            const auto* src = (const UpstreamMessage*) rightExpander.consumerMessage;
            incoming.count = std::min<uint8_t>(src->count, kMaxChainModules);
            for (uint8_t k = 0; k < incoming.count; k++) incoming.block[k] = src->block[k];
        }
        UpstreamBlock mine;
        fillUpstream(mine);
        if (leftExpander.module && isChainLeftNeighbor(leftExpander.module)) {
            // Participants allocate this producer in their constructors; the null
            // guard protects against a future left neighbour that does not.
            if (auto* dst = (UpstreamMessage*) leftExpander.module->rightExpander.producerMessage) {
                // `mine`/`incoming` are distinct storage from `*dst` (own stack + our
                // consumer vs the neighbour's producer), so prependUpstream's no-alias
                // precondition on (in, out) holds.
                prependUpstream(mine, incoming, *dst);
                leftExpander.module->rightExpander.requestMessageFlip();
            }
        }
        // ---- downstream: my LEDs from block[0], relay the rest rightward ---
        DownstreamBlock forMe;
        DownstreamMessage rest;
        if (leftExpander.module && isChainLeftNeighbor(leftExpander.module))
            shiftDownstream(*(DownstreamMessage*) leftExpander.consumerMessage, forMe, rest);
        else
            forMe = DownstreamBlock{};   // left neighbour invalid: dark LEDs, don't freeze at last state
        applyDownstream(forMe, sampleTime);
        if (rightExpander.module && isChainRightNeighbor(rightExpander.module)) {
            // Participants allocate this producer in their constructors; the null
            // guard protects against a future right neighbour that does not.
            if (auto* dst = (DownstreamMessage*) rightExpander.module->leftExpander.producerMessage) {
                *dst = rest;
                rightExpander.module->leftExpander.requestMessageFlip();
            }
        }
    }
};
