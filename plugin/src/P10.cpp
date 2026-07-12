#include "ChainModule.hpp"
#include "DroidWidgets.hpp"

// DROID P10 controller: 10 pots (P<c>.1-10), no buttons/LEDs/switches.
// manual/hardware.md §6.6.
struct DroidP10 : ChainModule {
    enum ParamId { ENUMS(POT_PARAMS, 10), PARAMS_LEN };
    enum InputId { INPUTS_LEN };
    enum OutputId { OUTPUTS_LEN };
    enum LightId { LIGHTS_LEN };

    DroidP10() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        for (int i = 0; i < 10; i++)
            configParam(POT_PARAMS + i, 0.f, 1.f, 0.f, string::f("P%d", i + 1));
    }

    void fillUpstream(droid::chain::UpstreamBlock& b) override {
        b.modelId = droid::chain::MP10;
        for (int i = 0; i < 10; i++) b.pots[i] = params[POT_PARAMS + i].getValue();
    }

    void applyDownstream(const droid::chain::DownstreamBlock&, float) override {}   // no LEDs

    void process(const ProcessArgs& args) override { relay(args.sampleTime); }
};

struct DroidP10Widget : VcvoidModuleWidget {
    DroidP10Widget(DroidP10* module) {
        setModule(module);
        // Layout -> render px through the faceplate art (dw::ArtMap): the
        // PNG is 576x2915 px and stretched to the module box.
        dw::ArtMap A = dw::setupPanel(this, "p10", "P10", 576.f, 2915.f);
        const auto* L = droid::layout::find("p10");
        for (int i = 0; i < 2; i++) {
            auto* k = createParamCentered<dw::DroidKnob>(
                A.vec(L->pos('P', i + 1)), module, DroidP10::POT_PARAMS + i);
            addParam(k);
        }
        for (int i = 0; i < 8; i++) {
            auto* k = createParamCentered<dw::DroidKnobSmall>(
                A.vec(L->pos('P', i + 3)), module, DroidP10::POT_PARAMS + 2 + i);
            k->capDiaHP = 1.38f;   // art-measured full cap inside the printed gold ring (160 px)
            addParam(k);
        }
    }
};

Model* modelDroidP10 = createModel<DroidP10, DroidP10Widget>("p10");
