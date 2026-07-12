#include "ChainModule.hpp"
#include "DroidWidgets.hpp"

// DROID P8S8 controller: 8 sliders (P<c>.1-8) with LEDs (L<c>.1-8), and 8
// three-way toggles (S<c>.1-8) reading 0/1/2 (down/center/up).
// manual/hardware.md §6.8. The unwritten-L default — LED shows the slider
// position — is produced master-side by defaultLedFromPot; the module just
// renders leds[].
struct DroidP8S8 : ChainModule {
    enum ParamId { ENUMS(SLIDER_PARAMS, 8), ENUMS(TOGGLE_PARAMS, 8), PARAMS_LEN };
    enum InputId { INPUTS_LEN };
    enum OutputId { OUTPUTS_LEN };
    enum LightId { ENUMS(SLIDER_LIGHTS, 8), LIGHTS_LEN };

    DroidP8S8() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        for (int i = 0; i < 8; i++)
            configParam(SLIDER_PARAMS + i, 0.f, 1.f, 0.f, string::f("P%d", i + 1));
        for (int i = 0; i < 8; i++)
            configSwitch(TOGGLE_PARAMS + i, 0.f, 2.f, 1.f, string::f("S%d", i + 1),
                         {"down", "center", "up"});
    }

    void fillUpstream(droid::chain::UpstreamBlock& b) override {
        b.modelId = droid::chain::MP8S8;
        for (int i = 0; i < 8; i++) b.pots[i] = params[SLIDER_PARAMS + i].getValue();
        for (int i = 0; i < 8; i++) b.switches[i] = params[TOGGLE_PARAMS + i].getValue();
    }

    void applyDownstream(const droid::chain::DownstreamBlock& b, float sampleTime) override {
        applyLedBank(SLIDER_LIGHTS, 8, b.leds, sampleTime);
    }

    void process(const ProcessArgs& args) override { relay(args.sampleTime); }
};

struct DroidP8S8Widget : VcvoidModuleWidget {
    DroidP8S8Widget(DroidP8S8* module) {
        setModule(module);
        // Layout -> render px through the faceplate art (dw::ArtMap): the
        // PNG is 923x2915 px and stretched to the module box.
        dw::ArtMap A = dw::setupPanel(this, "p8s8", "P8S8", 923.f, 2915.f);
        const auto* L = droid::layout::find("p8s8");
        // Sliders (P1-P8): real vertical faders. dw::DroidSlider repaints the
        // baked black slot and draws the moving orange cap over it, with the L
        // LED rendered inside the cap (brightness = leds[i]) — so the separate
        // SmallLight widget is gone; the LED tracks the cap as on the hardware.
        //
        // Slot travel measured off the art (p8s8.png): the slots span y
        // 403..990 px (row 1) and 1307..1894 px (row 2) — 587 px = 25.8 mm of
        // travel. The Layout 'P' y is the CAP'S RESTING CENTRE at the slot
        // bottom (art 934/1837 px), NOT the slot middle, so centring the box
        // on it truncated the travel to the bottom half (UAT round 3: P at
        // 1.0 only reached mid-slot). The box is therefore placed from the
        // measured slot top; only its x centre comes from the layout.
        const float slotTopArt[2] = {403.f, 1307.f};
        const float slotHArt = 587.f;
        for (int i = 0; i < 8; i++) {
            auto* s = createParamCentered<dw::DroidSlider>(
                A.vec(L->pos('P', i + 1)), module, DroidP8S8::SLIDER_PARAMS + i);
            float xc = s->box.pos.x + s->box.size.x / 2.f;  // keep art-mapped x
            s->box.size.y = A.y(slotHArt);
            s->box.pos = Vec(xc - s->box.size.x / 2.f, A.y(slotTopArt[i / 4]));
            s->lightId = DroidP8S8::SLIDER_LIGHTS + i;
            addParam(s);
        }
        // Toggles (S1-S8): 3-way (down/center/up), 2.7 HP per Layout.hpp
        // (dw::DroidToggle defaults to 2.0 HP) — resize then recenter.
        for (int i = 0; i < 8; i++) {
            Vec center = A.vec(L->pos('S', i + 1));
            auto* t = createParamCentered<dw::DroidToggle>(
                center, module, DroidP8S8::TOGGLE_PARAMS + i);
            t->box.size = Vec(dw::hpPx(2.7f), dw::hpPx(2.7f));
            t->box.pos = center.minus(t->box.size.div(2));
            addParam(t);
        }
    }
};

Model* modelDroidP8S8 = createModel<DroidP8S8, DroidP8S8Widget>("p8s8");
