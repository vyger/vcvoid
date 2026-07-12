#include "ChainModule.hpp"
#include "DroidWidgets.hpp"

// DROID B32 controller: 32 momentary buttons (B<c>.1-32) with 32 LEDs
// (L<c>.1-32) in a 4x8 grid. Hardware renders LEDs at only four brightness
// levels (off/low/medium/full) — a fast-serial-link design choice — so this
// module quantizes each incoming brightness before display.
// manual/hardware.md §6.9, manual/basics.md §5.5.
struct DroidB32 : ChainModule {
    enum ParamId { ENUMS(BUTTON_PARAMS, 32), PARAMS_LEN };
    enum InputId { INPUTS_LEN };
    enum OutputId { OUTPUTS_LEN };
    enum LightId { ENUMS(BUTTON_LIGHTS, 32), LIGHTS_LEN };

    DroidB32() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        for (int i = 0; i < 32; i++)
            configButton(BUTTON_PARAMS + i, string::f("B%d", i + 1));
    }

    // Quantize 0..1 brightness to the four hardware levels {0, 1/3, 2/3, 1}.
    // Sanity check (nearest of 3 equal steps): 0.1*3=0.3->round 0 -> 0;
    // 0.2*3=0.6->round 1 -> 1/3; 0.6*3=1.8->round 2 -> 2/3; 0.9*3=2.7->round 3 -> 1.
    static float quantizeLed(float v) {
        return std::round(math::clamp(v, 0.f, 1.f) * 3.f) / 3.f;
    }

    void fillUpstream(droid::chain::UpstreamBlock& b) override {
        b.modelId = droid::chain::MB32;
        b.buttons = packButtonParams(BUTTON_PARAMS, 32);   // bit 31 (button 32) fits in uint32_t buttons
    }

    void applyDownstream(const droid::chain::DownstreamBlock& b, float sampleTime) override {
        for (int i = 0; i < 32; i++)
            lights[BUTTON_LIGHTS + i].setBrightnessSmooth(quantizeLed(b.leds[i]), sampleTime);
    }

    void process(const ProcessArgs& args) override { relay(args.sampleTime); }
};

struct DroidB32Widget : VcvoidModuleWidget {
    DroidB32Widget(DroidB32* module) {
        setModule(module);
        // Layout -> render px through the faceplate art (dw::ArtMap): the
        // PNG is 1153x2915 px and stretched to the module box.
        dw::ArtMap A = dw::setupPanel(this, "b32", "B32", 1153.f, 2915.f);
        const auto* L = droid::layout::find("b32");
        for (int i = 0; i < 32; i++) {
            auto* b = createParamCentered<dw::DroidButton>(
                A.vec(L->pos('B', i + 1)).plus(A.off(-2.7f, 0.1f)),  // baked-button centroid offset
                module, DroidB32::BUTTON_PARAMS + i);
            b->lightId = DroidB32::BUTTON_LIGHTS + i;
            addParam(b);
        }
    }
};

Model* modelDroidB32 = createModel<DroidB32, DroidB32Widget>("b32");
