#include "ChainModule.hpp"
#include "DroidWidgets.hpp"

// DROID S10 controller: 10 switches (S<c>.1-10). S.1-S.2 are 8-position
// rotaries reading integers 0-7; S.3-S.10 are 3-position toggles reading
// 0/1/2 (down/center/up). manual/hardware.md §6.7. Rack switches snap
// discretely, which automatically gives the manual's skip-intermediate
// debounce behavior.
struct DroidS10 : ChainModule {
    enum ParamId { ENUMS(ROTARY_PARAMS, 2), ENUMS(TOGGLE_PARAMS, 8), PARAMS_LEN };
    enum InputId { INPUTS_LEN };
    enum OutputId { OUTPUTS_LEN };
    enum LightId { LIGHTS_LEN };

    DroidS10() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        for (int i = 0; i < 2; i++) {
            configSwitch(ROTARY_PARAMS + i, 0.f, 7.f, 0.f, string::f("S%d (rotary)", i + 1),
                         {"0", "1", "2", "3", "4", "5", "6", "7"});
        }
        for (int i = 0; i < 8; i++)
            configSwitch(TOGGLE_PARAMS + i, 0.f, 2.f, 1.f, string::f("S%d", i + 3),
                         {"down", "center", "up"});
    }

    void fillUpstream(droid::chain::UpstreamBlock& b) override {
        b.modelId = droid::chain::MS10;
        for (int i = 0; i < 2; i++) b.switches[i] = params[ROTARY_PARAMS + i].getValue();
        for (int i = 0; i < 8; i++) b.switches[2 + i] = params[TOGGLE_PARAMS + i].getValue();
    }

    void applyDownstream(const droid::chain::DownstreamBlock&, float) override {}   // no LEDs

    void process(const ProcessArgs& args) override { relay(args.sampleTime); }
};

// The rotaries read discrete integer steps (configSwitch, 8 labeled values),
// so the widget must snap like RoundBlackSnapKnob — dw::DroidKnob alone only
// matches continuous pots. The rotation range matches the faceplate's printed
// tick marks, measured off s10.png: the 8 ticks sit at -91°..+121° from 12
// o'clock (30.3° apart, and NOT symmetric) — DroidKnob's default ±149° pot
// sweep pointed past the "0" and "7" marks at the ends (UAT round 4).
struct DroidKnobSnap : dw::DroidKnob {
    DroidKnobSnap() {
        snap = true;
        minAngle = -91.f * (float)M_PI / 180.f;
        maxAngle = 121.f * (float)M_PI / 180.f;
    }
};

struct DroidS10Widget : VcvoidModuleWidget {
    DroidS10Widget(DroidS10* module) {
        setModule(module);
        // Layout -> render px through the faceplate art (dw::ArtMap): the
        // PNG is 577x2915 px and stretched to the module box.
        dw::ArtMap A = dw::setupPanel(this, "s10", "S10", 577.f, 2915.f);
        const auto* L = droid::layout::find("s10");
        for (int i = 0; i < 2; i++) {  // S1-S2: rotaries, discrete 0-7 steps
            auto* k = createParamCentered<DroidKnobSnap>(
                A.vec(L->pos('S', i + 1)), module, DroidS10::ROTARY_PARAMS + i);
            addParam(k);
        }
        for (int i = 0; i < 8; i++)    // S3-S10: 3-way toggles
            addParam(createParamCentered<dw::DroidToggle>(
                A.vec(L->pos('S', i + 3)), module, DroidS10::TOGGLE_PARAMS + i));
    }
};

Model* modelDroidS10 = createModel<DroidS10, DroidS10Widget>("s10");
