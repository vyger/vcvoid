#include "ChainModule.hpp"
#include "EncoderWidgets.hpp"
#include "DroidWidgets.hpp"

// DROID E4 controller: 4 endless encoders (E<c>.1-4) with integrated push
// (B<c>.1-4) and a 32-LED value ring per encoder (L<c>.1-4). manual/hardware.md.
//
// The master (Task 2) drives the chain: it reads detentCount[n-1] and diffs it
// against its last-seen value to synthesize turn events (turnEncoder), reads the
// buttons bit for pushEncoder + the B register, and writes ring[n-1] (the value
// dot) plus leds[n-1] (the white L-register overlay). So this module publishes a
// MONOTONIC detent counter per encoder (only ever ++/-- by the widget, never
// reset) and a push level; and stores the downstream ring/led for the widget.
struct DroidE4 : ChainModule {
    enum ParamId { PARAMS_LEN };
    enum InputId { INPUTS_LEN };
    enum OutputId { OUTPUTS_LEN };
    enum LightId { LIGHTS_LEN };

    uint32_t detent[4] = {};   // monotonic per-encoder detent counters (widget ++/--)
    // Per-encoder click/turn/push classifier (EncoderGesture.hpp): the widget
    // feeds it press/move/release on the UI thread; process() steps its
    // timers and fillUpstream publishes its level() as the push bit.
    vcvoid::EncoderGesture gest[4];
    // Select-gated ring image (issue #15): flags bit0 active / bit1 bipolar /
    // bit2 fill / bit3 legacy encoquencer gauge; value, DROID colours, and the
    // select-gated white `led`-param overlay — see chain.hpp DownstreamBlock.
    uint8_t ringFlags[4] = {};
    float ringValue[4] = {};
    float ringColor[4] = {};
    float ringNegColor[4] = {};
    float ringOverlay[4] = {};
    float ringLed[4] = {};     // last downstream L-register white overlay 0..1
    float stepLed[4] = {};     // encoquencer step LED (middle-three bottom cells): brightness
    float stepLedColor[4] = {};// ... DROID colour value (<0 = white played-step marker)

    DroidE4() { config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN); }

    void fillUpstream(droid::chain::UpstreamBlock& b) override {
        b.modelId = droid::chain::ME4;
        for (int i = 0; i < 4; i++) b.detentCount[i] = detent[i];
        b.buttons = 0;
        for (int i = 0; i < 4; i++)
            if (gest[i].level()) b.buttons |= (1u << i);
    }

    void applyDownstream(const droid::chain::DownstreamBlock& b, float) override {
        for (int i = 0; i < 4; i++) {
            ringFlags[i]    = b.ringFlags[i];
            ringValue[i]    = b.ringValue[i];
            ringColor[i]    = b.ringColor[i];
            ringNegColor[i] = b.ringNegColor[i];
            ringOverlay[i]  = b.ringOverlay[i];
            ringLed[i]      = b.leds[i];
            stepLed[i]      = b.encStepLed[i];
            stepLedColor[i] = b.encStepLedColor[i];
        }
    }

    void process(const ProcessArgs& args) override {
        for (auto& g : gest) g.step(args.sampleTime);
        relay(args.sampleTime);
    }
};

// Value-ring display bound to one encoder's ring[i] (+ L-register overlay).
// Draws the SQUARE 32-LED ring baked into the E4 faceplate art; `half` (centre
// to a side's LED-centre row) and `cell` (LED square side) are measured from
// res/faceplates/e4.png and set by DroidE4Widget so the drawn LEDs land exactly
// on the art cells.
struct E4RingWidget : Widget {
    DroidE4* module = nullptr;
    int idx = 0;
    float half = 0.f;   // centre -> side LED-centre row (px)
    float cell = 0.f;   // LED square side (px)
    void draw(const DrawArgs& args) override {
        RingDrawState rs;
        if (module) {
            uint8_t f = module->ringFlags[idx];
            rs.active      = f & 1;
            rs.bipolar     = f & 2;
            rs.fill        = f & 4;
            rs.legacyGauge = f & 8;
            rs.value       = module->ringValue[idx];
            rs.color       = module->ringColor[idx];
            rs.negColor    = module->ringNegColor[idx];
            rs.overlay     = module->ringOverlay[idx];
            rs.lOverlay    = module->ringLed[idx];
            rs.stepLed     = module->stepLed[idx];
            rs.stepLedColor = module->stepLedColor[idx];
        }
        drawEncoderRingSquare(args.vg, box.size.div(2), half, cell, rs);
    }
};

struct DroidE4Widget : VcvoidModuleWidget {
    DroidE4Widget(DroidE4* module) {
        setModule(module);
        dw::setupPanel(this, "e4", "E4", 692.f, 2915.f);   // ArtMap unused: geometry below is hand-scaled (sx/sy)

        // Geometry measured directly from res/faceplates/e4.png (692x2915 px;
        // the panel image fills the module box, so art px -> Rack px is just the
        // box/image ratio below). Per encoder the faceplate bakes a SQUARE ring
        // of 32 small square LEDs (9 per side, corners shared) around a glossy
        // black knob with the numeral 1-4 printed inside the ring's top-left.
        // Encoder 1 values, replicated down the panel at the encoder pitch:
        //   LED-grid centre  = (345.9, 508.4) art px, side-row half-span 250.6,
        //                      LED cell side 44 px
        //   knob (cap) circle= centre (348.0, 502.5); dark core 324 px across,
        //                      but the rim highlight and the baked pointer
        //                      wedge tip reach ~350 px — the drawn cap uses 350
        //                      so no static print peeks out (UAT round 2)
        //   encoder pitch    = 632.75 art px  (Layout's 5.490 HP row spacing)
        const float sx = box.size.x / 692.f;    // art px -> Rack px (x)
        const float sy = box.size.y / 2915.f;   // art px -> Rack px (y); ~= sx
        const float artRingCx = 345.9f, artRingCy0 = 508.4f;
        const float artKnobCx = 348.0f, artKnobCy0 = 502.5f;
        const float artPitchY = 632.75f;
        const float artHalf   = 250.6f;   // ring centre -> a side's LED-centre row
        const float artCell   = 44.f;     // LED square side
        const float artKnobD  = 350.f;    // knob cap diameter incl. rim highlight
        for (int i = 0; i < 4; i++) {
            // Square value ring: LEDs land exactly on the baked art cells.
            float rcx  = artRingCx * sx;
            float rcy  = (artRingCy0 + i * artPitchY) * sy;
            float half = artHalf * sx;   // sx ~= sy (uniform panel scale)
            float cell = artCell * sx;
            float side = 2.f * half + cell;
            auto* ring = new E4RingWidget;
            ring->module = module;
            ring->idx = i;
            ring->half = half;
            ring->cell = cell;
            ring->box.size = Vec(side, side);
            ring->box.pos  = Vec(rcx - side / 2.f, rcy - side / 2.f);
            addChild(ring);
            // Endless encoder on top: cap sized/placed to overlay the art knob
            // exactly (smaller than before, so the printed numerals stay visible).
            float kcx = artKnobCx * sx;
            float kcy = (artKnobCy0 + i * artPitchY) * sy;
            float kd  = artKnobD * sx;
            auto* enc = new DroidEndlessEncoder;
            enc->box.size = Vec(kd, kd);
            enc->box.pos  = Vec(kcx - kd / 2.f, kcy - kd / 2.f);
            if (module) {
                enc->detentCount = &module->detent[i];
                enc->gesture = &module->gest[i];
            }
            addChild(enc);
        }
    }
};

Model* modelDroidE4 = createModel<DroidE4, DroidE4Widget>("e4");
