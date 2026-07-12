#include "ChainModule.hpp"
#include "DroidWidgets.hpp"

// DROID P4B2 controller: 4 pots (P<c>.1-4), 2 momentary buttons (B<c>.1-2),
// 2 button LEDs (L<c>.1-2). manual/hardware.md §6.5.
struct DroidP4B2 : ChainModule {
    enum ParamId { ENUMS(POT_PARAMS, 4), ENUMS(BUTTON_PARAMS, 2), PARAMS_LEN };
    enum InputId { INPUTS_LEN };
    enum OutputId { OUTPUTS_LEN };
    enum LightId { ENUMS(BUTTON_LIGHTS, 2), LIGHTS_LEN };

    DroidP4B2() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        for (int i = 0; i < 4; i++)
            configParam(POT_PARAMS + i, 0.f, 1.f, 0.f, string::f("P%d", i + 1));
        for (int i = 0; i < 2; i++)
            configButton(BUTTON_PARAMS + i, string::f("B%d", i + 1));
    }

    void fillUpstream(droid::chain::UpstreamBlock& b) override {
        b.modelId = droid::chain::MP4B2;
        for (int i = 0; i < 4; i++) b.pots[i] = params[POT_PARAMS + i].getValue();
        b.buttons = packButtonParams(BUTTON_PARAMS, 2);
    }

    void applyDownstream(const droid::chain::DownstreamBlock& b, float sampleTime) override {
        applyLedBank(BUTTON_LIGHTS, 2, b.leds, sampleTime);
    }

    void process(const ProcessArgs& args) override { relay(args.sampleTime); }
};

struct DroidP4B2Widget : VcvoidModuleWidget {
    DroidP4B2Widget(DroidP4B2* module) {
        setModule(module);
        // Layout -> render px through the faceplate art (dw::ArtMap): the
        // PNG is 577x2915 px and stretched to the module box.
        dw::ArtMap A = dw::setupPanel(this, "p4b2", "P4B2", 577.f, 2915.f);
        const auto* L = droid::layout::find("p4b2");
        for (int i = 0; i < 4; i++) {
            auto* k = createParamCentered<dw::DroidKnob>(
                A.vec(L->pos('P', i + 1)), module, DroidP4B2::POT_PARAMS + i);
            addParam(k);
        }
        for (int i = 0; i < 2; i++) {
            auto* b = createParamCentered<dw::DroidButton>(
                A.vec(L->pos('B', i + 1)).plus(A.off(-2.9f, 2.9f)),  // baked-button centroid offset
                module, DroidP4B2::BUTTON_PARAMS + i);
            b->lightId = DroidP4B2::BUTTON_LIGHTS + i;
            addParam(b);
        }
    }
};

Model* modelDroidP4B2 = createModel<DroidP4B2, DroidP4B2Widget>("p4b2");
