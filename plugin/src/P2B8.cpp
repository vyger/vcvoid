#include "ChainModule.hpp"
#include "DroidWidgets.hpp"

// DROID P2B8 controller: 2 pots (P<c>.1-2), 8 momentary buttons (B<c>.1-8),
// 8 button LEDs (L<c>.1-8). manual/hardware.md §6.4.
struct DroidP2B8 : ChainModule {
    enum ParamId { ENUMS(POT_PARAMS, 2), ENUMS(BUTTON_PARAMS, 8), PARAMS_LEN };
    enum InputId { INPUTS_LEN };
    enum OutputId { OUTPUTS_LEN };
    enum LightId { ENUMS(BUTTON_LIGHTS, 8), LIGHTS_LEN };

    DroidP2B8() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        for (int i = 0; i < 2; i++)
            configParam(POT_PARAMS + i, 0.f, 1.f, 0.f, string::f("P%d", i + 1));
        for (int i = 0; i < 8; i++)
            configButton(BUTTON_PARAMS + i, string::f("B%d", i + 1));
    }

    void fillUpstream(droid::chain::UpstreamBlock& b) override {
        b.modelId = droid::chain::MP2B8;
        for (int i = 0; i < 2; i++) b.pots[i] = params[POT_PARAMS + i].getValue();
        b.buttons = packButtonParams(BUTTON_PARAMS, 8);
    }

    void applyDownstream(const droid::chain::DownstreamBlock& b, float sampleTime) override {
        applyLedBank(BUTTON_LIGHTS, 8, b.leds, sampleTime);
    }

    void process(const ProcessArgs& args) override { relay(args.sampleTime); }
};

struct DroidP2B8Widget : VcvoidModuleWidget {
    DroidP2B8Widget(DroidP2B8* module) {
        setModule(module);
        // Layout -> render px through the faceplate art (dw::ArtMap): the
        // PNG is 577x2915 px and stretched to the module box.
        dw::ArtMap A = dw::setupPanel(this, "p2b8", "P2B8", 577.f, 2915.f);
        const auto* L = droid::layout::find("p2b8");
        for (int i = 0; i < 2; i++) {
            auto* k = createParamCentered<dw::DroidKnob>(
                A.vec(L->pos('P', i + 1)), module, DroidP2B8::POT_PARAMS + i);
            addParam(k);
        }
        for (int i = 0; i < 8; i++) {
            auto* b = createParamCentered<dw::DroidButton>(
                A.vec(L->pos('B', i + 1)).plus(A.off(-2.8f, 1.9f)),  // baked-button centroid offset
                module, DroidP2B8::BUTTON_PARAMS + i);
            b->lightId = DroidP2B8::BUTTON_LIGHTS + i;
            addParam(b);
        }
    }
};

Model* modelDroidP2B8 = createModel<DroidP2B8, DroidP2B8Widget>("p2b8");
