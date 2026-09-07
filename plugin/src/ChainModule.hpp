#pragma once
#include "plugin.hpp"
#include "src/chain.hpp"
#include "RegisterLabels.hpp"   // issue #26
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

    // Which DROID module this is. Stamped into every upstream block by relay(),
    // so fillUpstream() never sets modelId itself and the two can't disagree;
    // the master's widget also reads it to number the chain when distributing
    // register labels (issue #26).
    virtual droid::chain::ModelId chainModel() const = 0;

    // ---- register labels (issue #26) ------------------------------------
    // UI-thread only. The master's widget walks the chain, fills this in with
    // the patch's labels plus THIS module's controller/expander number, and
    // calls applyOwnLabels(); the panel overlay draws from it. `show` is a
    // MIRROR of the master's flag, re-pushed every frame — a DROID system is
    // one instrument, so its labels turn on and off together. Do not persist
    // it here; the master owns it.
    vcvoid::labels::ModuleLabels registerLabels;
    virtual void applyOwnLabels() {}

    // True when a master sits at the head of the chain to my left. A module
    // dragged off the chain keeps whatever labels it was given until it notices
    // this, so its widget clears them (see VcvoidModuleWidget::step).
    bool onMasterChain();

    // The label state of the master at the head of my chain, or null when I am
    // not on one. This is where the "Show register labels" toggle lives, so
    // right-clicking any module of a system flips the whole system.
    vcvoid::labels::ModuleLabels* chainMasterLabels();

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
        Module* left  = leftExpander.module;
        Module* right = rightExpander.module;
        const bool haveLeft  = left  && isChainLeftNeighbor(left);
        const bool haveRight = right && isChainRightNeighbor(right);

        // ---- upstream: my controls + everything from my right, to my left --
        UpstreamBlock mine;
        fillUpstream(mine);
        mine.modelId = chainModel();
        if (haveLeft) {
            // Participants allocate this producer in their constructors; the null
            // guard protects against a future left neighbour that does not.
            if (auto* dst = (UpstreamMessage*) left->rightExpander.producerMessage) {
                // Prepend STRAIGHT from my consumer into the neighbour's producer.
                // Staging through a local UpstreamMessage would value-initialize
                // all 21 blocks (5.7 KB) every audio frame; prependUpstream copies
                // only block[0..count-1] and clamps an untrusted count itself, so
                // the temporary bought nothing (issue #32). `mine` and my consumer
                // are distinct storage from the neighbour's producer, so
                // prependUpstream's no-alias precondition on (in, out) holds.
                static const UpstreamMessage kEmptyChain;   // count 0: chain ends to my right
                prependUpstream(mine,
                                haveRight ? *(const UpstreamMessage*) rightExpander.consumerMessage
                                          : kEmptyChain,
                                *dst);
                left->rightExpander.requestMessageFlip();
            }
        }
        // ---- downstream: my LEDs from block[0], relay the rest rightward ---
        // Participants allocate this producer in their constructors; the null
        // guard protects against a future right neighbour that does not.
        DownstreamMessage* dst = haveRight
            ? (DownstreamMessage*) right->leftExpander.producerMessage
            : nullptr;
        DownstreamBlock forMe;
        if (haveLeft) {
            // Shift STRAIGHT into the neighbour's producer — again distinct
            // storage from my own consumer. A null `dst` means the chain ends
            // here, and shiftDownstream then skips building the tail entirely
            // rather than filling an 11 KB local we would throw away.
            shiftDownstream(*(const DownstreamMessage*) leftExpander.consumerMessage,
                            forMe, dst);
        } else {
            forMe = DownstreamBlock{};   // left neighbour invalid: dark LEDs, don't freeze at last state
            if (dst) dst->count = 0;     // ... and the same darkness propagates rightward
        }
        applyDownstream(forMe, sampleTime);
        if (dst) right->leftExpander.requestMessageFlip();
    }
};
