#include "MasterBase.hpp"
#include "DroidWidgets.hpp"
#include "droidcolor.hpp"

// DROID MASTER (Master16): 8 CV inputs + 8 CV outputs, 4x4 LED matrix. All the
// engine hosting, patch loading, controller-chain + X7/MIDI feed, and process()
// live in DroidMasterBase; this subclass only pins the master type / I/O counts
// and owns the LED-matrix light ids (the base never touches lights[]).
struct DroidMaster : DroidMasterBase {
    enum ParamId { PARAMS_LEN };
    enum InputId { ENUMS(IN_INPUTS, 8), INPUTS_LEN };
    enum OutputId { ENUMS(OUT_OUTPUTS, 8), OUTPUTS_LEN };
    // RedGreenBlueLight uses 3 light ids per LED, so the 4x4 matrix needs 16 * 3.
    enum LightId { ENUMS(MATRIX_LIGHTS, 16 * 3), LIGHTS_LEN };

    DroidMaster() : DroidMasterBase(droid::MasterType::Master16, 8, 8, 0, 16 * 3) {}

    // The 4x4 LED matrix mirrors the eight input and eight output jacks, exactly
    // like the hardware MASTER ("a 4 x 4 multicolor LED matrix displaying the
    // state of the inputs and outputs" — manual basics.md ch. 6). Layout 'R' is
    // row-major (index = row*4 + col): rows 0-1 (i = 0..7) are inputs I1..I8,
    // rows 2-3 (i = 8..15) are outputs O1..O8. Brightness tracks |voltage|/10 V;
    // a positive jack glows red, a negative one blue. The manual doesn't pin down
    // the master's colours, so we adopt the warm=positive / cool=negative
    // convention (RedGreenBlueLight channel order: red 0, green 1, blue 2). The
    // base owns the engine tick + jack I/O; only this subclass touches lights[].
    //
    // A patch may OVERRIDE any of the 16 LEDs via the R registers R1..R16
    // (manual/basics.md §5.5: R1..R8 = the input rows, R9..R16 = the output rows,
    // row-major — the same order as index i). When R<i+1> is driven the LED shows
    // the register value through the DROID colour table (droidcolor.hpp) instead
    // of the jack mirror; 0 = dark. ledbrightness dims both cases.
    // Target RGB per matrix LED, refreshed on tick frames only (below).
    float ledTarget_[16][3] = {};

    void process(const ProcessArgs& args) override {
        DroidMasterBase::process(args);
        // [droid] ledbrightness dims the master's matrix LEDs (manual: "the 24
        // LEDs of the master and the G8"); jack voltages are unaffected.
        //
        // Targets are recomputed only on tick frames (frameCounter just
        // wrapped in the base): R registers only change on engine ticks, and
        // the per-LED registerDriven hash lookup 16x per audio sample was pure
        // waste. Jack mirrors are sampled at tick rate too — the smoothing
        // below runs per sample, and tick rate (>= ~2 kHz) is far above any
        // visible LED rate.
        if (frameCounter == 0) {
            float lb = engine ? engine->ledBrightness() : 1.f;
            for (int i = 0; i < 16; i++) {
                droid::RegId rr{'R', 0, uint8_t(i + 1)};
                if (engine && engine->registerDriven(rr)) {
                    droid::color::RGB c = droid::color::fromValue(engine->getRegister(rr));
                    ledTarget_[i][0] = c.r * lb;
                    ledTarget_[i][1] = c.g * lb;
                    ledTarget_[i][2] = c.b * lb;
                    continue;
                }
                float v = i < 8 ? inputs[IN_INPUTS + i].getVoltage()
                                : outputs[OUT_OUTPUTS + (i - 8)].getVoltage();
                float mag = std::fmin(std::fabs(v) / 10.f, 1.f) * lb;
                ledTarget_[i][0] = v > 0.f ? mag : 0.f;   // red  = positive
                ledTarget_[i][1] = 0.f;                   // green unused
                ledTarget_[i][2] = v < 0.f ? mag : 0.f;   // blue = negative
            }
        }
        for (int i = 0; i < 16; i++) {
            int base = MATRIX_LIGHTS + 3 * i;
            for (int c = 0; c < 3; c++)
                lights[base + c].setBrightnessSmooth(ledTarget_[i][c], args.sampleTime);
        }
    }

    // UAT bridge (M10): read back the smoothed light value process() just wrote
    // above, rather than recomputing the R-override/jack-mirror logic a second
    // time — this is exactly what the panel widget renders (setBrightnessSmooth
    // this frame -> getBrightness() next read), so there is nothing to duplicate
    // or drift out of sync.
    bool matrixLedColor(int i, float& r, float& g, float& b) override {
        if (i < 0 || i >= 16) return false;
        int base = MATRIX_LIGHTS + 3 * i;
        r = lights[base + 0].getBrightness();
        g = lights[base + 1].getBrightness();
        b = lights[base + 2].getBrightness();
        return true;
    }
};

struct DroidMasterWidget : DroidMasterBaseWidget {
    DroidMasterWidget(DroidMaster* module) {
        setModule(module);
        const auto* L = droid::layout::find("master");
        box.size = Vec(L->hp * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
        addChild(dw::ImagePanel::create("res/faceplates/master.png", L->hp, "MASTER"));
        // Layout -> render px through the faceplate art (dw::ArtMap): the
        // PNG is 928x2917 px and stretched to the module box.
        dw::ArtMap A{928.f, 2917.f, box.size};
        for (int i = 0; i < 16; i++) {
            auto* l = createLightCentered<dw::SquareLight<RedGreenBlueLight>>(
                A.vec(L->pos('R', i + 1)), module, DroidMaster::MATRIX_LIGHTS + 3 * i);
            l->box.size = Vec(dw::hpPx(2.1f) * 0.8f, dw::hpPx(2.1f) * 0.8f);
            l->box.pos = A.vec(L->pos('R', i + 1)).minus(l->box.size.div(2));
            addChild(l);
        }
        for (int i = 0; i < 8; i++)
            addInput(createInputCentered<dw::DroidPort>(
                A.vec(L->pos('I', i + 1)).plus(A.off(dw::kJackArtDx, dw::kJackArtDy)),
                module, DroidMaster::IN_INPUTS + i));
        for (int i = 0; i < 8; i++)
            addOutput(createOutputCentered<dw::DroidPort>(
                A.vec(L->pos('O', i + 1)).plus(A.off(dw::kJackArtDx, dw::kJackArtDy)),
                module, DroidMaster::OUT_OUTPUTS + i));
    }
};

Model* modelDroidMaster = createModel<DroidMaster, DroidMasterWidget>("master");
