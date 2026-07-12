#include "ChainModule.hpp"
#include "Layout.hpp"
#include "DroidWidgets.hpp"
#include "droidcolor.hpp"

// DROID G8 gate expander: 8 bidirectional gate jacks. Physical G8 jacks are
// bidirectional; a Rack port is one-directional, so each gate is exposed as an
// input port + an output port pair (approved Rack-idiomatic deviation). Input
// jack j feeds gates[j] upstream (>= 0.75 V reads 1); output jack j emits 5 V
// when its downstream register value >= 0.1. manual/hardware.md §7.
struct DroidG8 : ChainModule {
    enum ParamId { PARAMS_LEN };
    enum InputId { ENUMS(GATE_INPUTS, 8), INPUTS_LEN };
    enum OutputId { ENUMS(GATE_OUTPUTS, 8), OUTPUTS_LEN };
    // RGB per LED: the default gate mirror is red, but an R-register override
    // (R17..R48, manual §5.5) can show any colour from the DROID value table.
    enum LightId { ENUMS(GATE_LIGHTS, 8 * 3), LIGHTS_LEN };

    DroidG8() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        for (int i = 0; i < 8; i++) {
            configInput(GATE_INPUTS + i, string::f("G.%d gate in", i + 1));
            configOutput(GATE_OUTPUTS + i, string::f("G.%d gate out", i + 1));
        }
    }
    void fillUpstream(droid::chain::UpstreamBlock& b) override {
        b.modelId = droid::chain::MG8;
        for (int i = 0; i < 8; i++)   // hardware input threshold: >= 0.75 V reads 1
            b.gates[i] = inputs[GATE_INPUTS + i].getVoltage() >= 0.75f ? 1.f : 0.f;
    }
    void applyDownstream(const droid::chain::DownstreamBlock& b, float sampleTime) override {
        for (int i = 0; i < 8; i++) {
            bool high = b.gates[i] >= 0.1f;          // hardware: >= 0.1 -> 5 V out
            outputs[GATE_OUTPUTS + i].setVoltage(high ? 5.f : 0.f);
            int base = GATE_LIGHTS + i * 3;
            // R-register override (manual §5.5): a driven R LED shows the DROID
            // colour value instead of the red gate mirror. [droid] ledbrightness
            // dims the LIGHT only, never the gate voltage.
            if (b.rLedDriven & (1u << i)) {
                droid::color::RGB c = droid::color::fromValue(b.rLeds[i]);
                lights[base + 0].setBrightnessSmooth(c.r * b.ledBrightness, sampleTime);
                lights[base + 1].setBrightnessSmooth(c.g * b.ledBrightness, sampleTime);
                lights[base + 2].setBrightnessSmooth(c.b * b.ledBrightness, sampleTime);
                continue;
            }
            lights[base + 0].setBrightnessSmooth(high ? b.ledBrightness : 0.f, sampleTime);  // red = gate high
            lights[base + 1].setBrightnessSmooth(0.f, sampleTime);
            lights[base + 2].setBrightnessSmooth(0.f, sampleTime);
        }
    }
    void process(const ProcessArgs& args) override { relay(args.sampleTime); }
};

struct DroidG8Widget : VcvoidModuleWidget {
    DroidG8Widget(DroidG8* module) {
        setModule(module);
        const auto* L = droid::layout::find("g8");
        box.size = Vec(L->hp * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
        addChild(dw::ImagePanel::create("res/faceplates/g8.png", L->hp, "G8"));
        // Layout -> render px through the faceplate art (dw::ArtMap): the
        // PNG is 461x2915 px and stretched to the module box.
        dw::ArtMap A{461.f, 2915.f, box.size};
        // Physical G8 jacks are bidirectional; each gate is exposed here as an
        // input port + an output port at the same faceplate jack center
        // (deviation noted in commit message). Both are OpaqueWidget
        // PortWidgets, and Rack hit-tests topmost-first: a full-size output
        // (added last) would consume every click and leave the input beneath
        // it mouse-unreachable — you could not patch a cable INTO a G8 gate
        // (review finding: stacked OpaqueWidgets shadow). Fix: give the pair
        // SPLIT hit-boxes that tile the original 2 HP jack — the input takes
        // the upper half, the output the lower half — so each has its own
        // clickable region. Port IDs and the visual jack center are unchanged,
        // so every saved patch keeps working. DroidPortHalf redraws its hover
        // ring around the true jack center so the split is invisible on-panel.
        const float halfH = dw::hpPx(1.0f);   // half of the 2 HP jack hit-box
        for (int i = 0; i < 8; i++) {
            Vec c = A.vec(L->pos('G', i + 1)).plus(A.off(dw::kJackArtDx, dw::kJackArtDy));
            auto* in = createInputCentered<dw::DroidPortHalf>(c, module, DroidG8::GATE_INPUTS + i);
            in->box.size.y = halfH;
            in->box.pos.y = c.y - halfH;   // upper half of the jack
            in->jackCenterY = halfH;       // jack center = this box's bottom edge
            addInput(in);
            auto* out = createOutputCentered<dw::DroidPortHalf>(c, module, DroidG8::GATE_OUTPUTS + i);
            out->box.size.y = halfH;
            out->box.pos.y = c.y;          // lower half of the jack
            out->jackCenterY = 0.f;        // jack center = this box's top edge
            addOutput(out);
        }
        for (int i = 0; i < 8; i++) {
            auto* l = createLightCentered<dw::SquareLight<RedGreenBlueLight>>(
                A.vec(L->pos('R', i + 1)), module, DroidG8::GATE_LIGHTS + i * 3);
            l->box.size = Vec(dw::hpPx(2.1f) * 0.8f, dw::hpPx(2.1f) * 0.8f);
            l->box.pos = A.vec(L->pos('R', i + 1)).minus(l->box.size.div(2));
            addChild(l);
        }
    }
};

Model* modelDroidG8 = createModel<DroidG8, DroidG8Widget>("g8");
