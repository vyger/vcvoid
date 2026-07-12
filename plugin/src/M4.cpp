#include "ChainModule.hpp"
#include "droidcolor.hpp"
#include "DroidWidgets.hpp"
#include <cmath>

// DROID M4 controller: 4 motorised faders (P<c>.1-4), each with a touch plate
// and an RGB LED below it. manual/hardware.md (M4 motor faders).
//
// The master (Task 2) drives the chain: it reads faderPos[n-1] and the
// faderTouch bit per fader to know whether the user is currently holding the
// fader (a motoquencer/motorfader must ignore commanded motor moves as user
// input). It writes motorTarget[n-1] (where the circuit wants the motor), the
// notch count, and the fader LED brightness (faderLed) + DROID colour value
// (faderLedColor). So this module publishes physical fader position + touch
// bits upstream, and stores the downstream motor target / LED for the widget.
//
// The animated fader widget (below) mirrors real hardware: while the user drags
// a fader the touch bit is set and the param follows the mouse; when released
// the motor eases the param toward motorTarget with a ~80 ms time constant.
// The hardware touch plate is a real momentary button here (TOUCH_PARAMS):
// holding it raises the fader's touch bit exactly like holding the fader, and
// its RGB LED (TOUCH_LIGHTS) shows the circuit-driven faderLed colour. The
// plate press is ALSO published separately (plateTouch): it is the step/B
// button surface, and a fader drag must never register as one — on hardware
// the plate sits below the fader, so moving a fader doesn't touch it.
struct DroidM4 : ChainModule {
    enum ParamId { ENUMS(FADER_PARAMS, 4), ENUMS(TOUCH_PARAMS, 4), PARAMS_LEN };
    enum InputId { INPUTS_LEN };
    enum OutputId { OUTPUTS_LEN };
    enum LightId { ENUMS(TOUCH_LIGHTS, 4 * 3), LIGHTS_LEN };

    bool touch[4] = {};          // widget sets true for the duration of a drag
    float motorTarget[4] = {};   // downstream: where the circuit wants the motor
    uint8_t notches[4] = {};     // downstream: notch count (0 = smooth)
    float ledB[4] = {};          // downstream: fader-LED brightness 0..1
    float ledC[4] = {};          // downstream: fader-LED DROID colour value 0..1

    DroidM4() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        for (int i = 0; i < 4; i++)
            configParam(FADER_PARAMS + i, 0.f, 1.f, 0.f, string::f("Fader %d", i + 1));
        for (int i = 0; i < 4; i++)
            configButton(TOUCH_PARAMS + i, string::f("Touch plate %d", i + 1));
    }

    // Touched (grabbed) = holding the fader cap (drag) OR pressing the
    // touch-plate button; gates edit acceptance and stops the motor.
    bool isTouched(int i) {
        return touch[i] || isPlatePressed(i);
    }
    bool isPlatePressed(int i) {
        return params[TOUCH_PARAMS + i].getValue() > 0.5f;
    }

    void fillUpstream(droid::chain::UpstreamBlock& b) override {
        b.modelId = droid::chain::MM4;
        for (int i = 0; i < 4; i++) b.faderPos[i] = params[FADER_PARAMS + i].getValue();
        b.faderTouch = 0;
        b.plateTouch = 0;
        for (int i = 0; i < 4; i++) {
            if (isTouched(i)) b.faderTouch |= (1u << i);
            if (isPlatePressed(i)) b.plateTouch |= (1u << i);
        }
    }

    void applyDownstream(const droid::chain::DownstreamBlock& b, float) override {
        for (int i = 0; i < 4; i++) {
            motorTarget[i] = b.motorTarget[i];
            notches[i]     = b.notches[i];
            ledB[i]        = b.faderLed[i];
            ledC[i]        = b.faderLedColor[i];
        }
    }

    void process(const ProcessArgs& args) override {
        relay(args.sampleTime);
        for (int i = 0; i < 4; i++) {
            droid::color::RGB c = droid::color::fromValue(ledC[i]);
            lights[TOUCH_LIGHTS + i * 3 + 0].setBrightness(c.r * ledB[i]);
            lights[TOUCH_LIGHTS + i * 3 + 1].setBrightness(c.g * ledB[i]);
            lights[TOUCH_LIGHTS + i * 3 + 2].setBrightness(c.b * ledB[i]);
        }
    }
};

// Geometry of the motor fader, measured off the faceplate art (m4.png,
// plugin/res/faceplates/m4.png) with a python strip scan — see the M4 fader
// UAT task. All four columns are identical. Values in millimetres; Rack px come
// from mm2px() so they track the panel, not the art's own DPI.
//   - slot (dark track in the art) top edge: y = 25.39 mm
//   - slot bottom (cap resting at value 0): y = 102.62 mm  -> span 77.23 mm
//   - white ribbed cap as drawn: 11.4 mm wide x 21.73 mm tall
// The cap TOP travels slotTop..(slotBot-capH); i.e. cap centre 36.26..91.76 mm
// for value 1..0. The slot's x centre is the layout 'P' x (parity-checked);
// only the y is driven by these measurements (the layout 'P' y is the cap's
// resting centre ~93 mm, which if used to *centre* the travel box would push
// the whole slot too low — the UAT "faders shifted down" bug).
static constexpr float kSlotTopMm = 25.39f;   // cap top at value 1
[[maybe_unused]] static constexpr float kSlotSpanMm = 77.23f;  // slot travel height = SVG box height
// Cap width: the baked caps are 11.5 mm wide, but column 4's is printed
// ~0.4 mm left of its slot centre (art x-centres 201.5/605/1008.5/1401.5 px vs
// slots at 202/605/1008/1411), so the drawn cap is 12.6 mm to fully mask the
// baked cap in every column (UAT round 2: a sliver of column 4's baked cap
// peeked out on the left).
static constexpr float kCapWmm = 12.6f;       // white cap width (drawn)
static constexpr float kCapHmm = 21.73f;      // white cap height

// One motor fader: a bounded 0..1 slider (VCVSlider is an SvgSlider, itself a
// Knob/ParamWidget with a bound ParamQuantity — so unlike the endless encoder a
// stock slider works). The visible element is a hand-drawn white ribbed cap
// (draw() below) that exactly overlays the art's slot and moves with the param;
// the SVG background is transparent (the slot itself is baked into the art).
// We override the drag hooks to raise/lower the touch bit for the drag's whole
// duration (matching the master's touch-gate: Task 2 only treats a fader as
// user-moved while its touch bit is set), and step() to ease the param toward
// the module's motorTarget when NOT touched.
struct DroidM4Fader : VCVSlider {
    DroidM4* module = nullptr;
    int idx = 0;

    DroidM4Fader() {
        // Transparent background SVG sized to the measured slot travel box
        // (mm2px(11.4) x mm2px(77.23)); this sets our box.size. The invisible
        // SvgSlider handle is given the CAP's real size so the centred handle
        // endpoints map value 0 -> cap resting at the slot bottom and value 1
        // -> cap at the slot top. We read minHandlePos/maxHandlePos in draw()
        // to place the drawn cap; the stock handle SVG is never shown.
        setBackgroundSvg(Svg::load(asset::plugin(pluginInstance, "res/M4FaderRail.svg")));
        handle->box.size = mm2px(math::Vec(kCapWmm, kCapHmm));
        float w = box.size.x, h = box.size.y, ch = mm2px(kCapHmm);
        setHandlePosCentered(math::Vec(w / 2.f, h - ch / 2.f),   // value 0: bottom
                             math::Vec(w / 2.f, ch / 2.f));       // value 1: top
    }

    // Draw the moveable white ribbed cap (with a black centre indicator line)
    // matching the M4 hardware fader cap in the faceplate art. We paint only the
    // cap — the transparent background/handle SVGs contribute nothing — so we do
    // NOT chain to VCVSlider::draw() (which would render the framebuffered stock
    // widgets underneath). Cap top-left is the SvgSlider handle position for the
    // current param value, interpolated between minHandlePos (value 0, bottom)
    // and maxHandlePos (value 1, top).
    void draw(const DrawArgs& args) override {
        NVGcontext* vg = args.vg;
        float v = 0.f;
        if (engine::ParamQuantity* pq = getParamQuantity())
            v = math::clamp(pq->getScaledValue(), 0.f, 1.f);
        math::Vec p = minHandlePos.crossfade(maxHandlePos, v);  // cap top-left
        float w = handle->box.size.x, h = handle->box.size.y;
        float x = p.x, y = p.y;
        float r = mm2px(1.0f);
        // body
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x, y, w, h, r);
        nvgFillColor(vg, nvgRGB(0xec, 0xec, 0xec));
        nvgFill(vg);
        // left->right shading for a rounded plastic look
        NVGpaint grad = nvgLinearGradient(vg, x, y, x + w, y,
            nvgRGBA(0xff, 0xff, 0xff, 0x50), nvgRGBA(0x00, 0x00, 0x00, 0x38));
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x, y, w, h, r);
        nvgFillPaint(vg, grad);
        nvgFill(vg);
        // horizontal ribs (grip grooves)
        nvgStrokeWidth(vg, 1.0f);
        nvgStrokeColor(vg, nvgRGBA(0x40, 0x40, 0x40, 0x70));
        const int ribs = 7;
        for (int i = 1; i <= ribs; i++) {
            float ry = y + h * (float)i / (ribs + 1);
            nvgBeginPath(vg);
            nvgMoveTo(vg, x + w * 0.15f, ry);
            nvgLineTo(vg, x + w * 0.85f, ry);
            nvgStroke(vg);
        }
        // black centre indicator line (drawn last, over the middle rib)
        float lh = mm2px(0.7f);
        nvgBeginPath(vg);
        nvgRect(vg, x + w * 0.06f, y + h / 2.f - lh / 2.f, w * 0.88f, lh);
        nvgFillColor(vg, nvgRGB(0x0a, 0x0a, 0x0a));
        nvgFill(vg);
        // outer border
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x + 0.5f, y + 0.5f, w - 1.f, h - 1.f, r);
        nvgStrokeWidth(vg, 1.0f);
        nvgStrokeColor(vg, nvgRGB(0x5a, 0x5a, 0x5a));
        nvgStroke(vg);
    }

    void onDragStart(const DragStartEvent& e) override {
        if (module) module->touch[idx] = true;
        VCVSlider::onDragStart(e);
    }
    void onDragEnd(const DragEndEvent& e) override {
        if (module) module->touch[idx] = false;
        VCVSlider::onDragEnd(e);
    }

    void step() override {
        // Motor animation: when the user is NOT holding the fader, ease the
        // param toward the circuit-commanded target with a ~80 ms time constant.
        if (module && !module->isTouched(idx)) {
            if (engine::ParamQuantity* pq = getParamQuantity()) {
                double dt = APP->window->getLastFrameDuration();   // seconds
                if (dt > 0.0 && std::isfinite(dt)) {
                    float cur    = pq->getValue();
                    float target = module->motorTarget[idx];
                    // The exponential ease is asymptotic and never exactly equals
                    // the target, so snap once within epsilon — otherwise the
                    // fader glides forever through denormal values (e.g. 1.23e-22)
                    // before "reaching" 0 (issue 2d).
                    if (std::fabs(target - cur) < 1e-4f) {
                        pq->setValue(target);
                    } else {
                        float k = 1.f - std::exp(-(float)dt / 0.08f);
                        pq->setValue(cur + (target - cur) * k);
                    }
                }
            }
        }
        VCVSlider::step();   // let the slider update its handle from the param
    }
};

struct DroidM4Widget : VcvoidModuleWidget {
    DroidM4Widget(DroidM4* module) {
        setModule(module);
        const auto* L = droid::layout::find("m4");
        box.size = Vec(L->hp * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
        addChild(dw::ImagePanel::create("res/faceplates/m4.png", L->hp, "M4"));
        for (int i = 0; i < 4; i++) {
            // Motor fader. The travel box is placed to overlay the measured art
            // slot exactly: x centred on the layout 'P' column (parity-checked),
            // top edge at the slot top (25.39 mm). We do NOT centre on the layout
            // 'P' y — that is the cap's resting centre and would sink the whole
            // travel below the art (the UAT "faders shifted down" bug).
            auto* fader = createParam<DroidM4Fader>(
                Vec(0, 0), module, DroidM4::FADER_PARAMS + i);
            fader->module = module;
            fader->idx = i;
            float colX = dw::hpVec(L->pos('P', i + 1)).x;
            fader->box.pos = Vec(colX - fader->box.size.x / 2.f, mm2px(kSlotTopMm));
            addParam(fader);
            // Touch plate: standard momentary light-button at the layout's L
            // position (the faceplate art is blank here — the baked DMMDM icon
            // is gone). Its RGB light shows the circuit-driven faderLed colour.
            addParam(createLightParamCentered<VCVLightBezel<MediumSimpleLight<RedGreenBlueLight>>>(
                dw::hpVec(L->pos('L', i + 1)), module,
                DroidM4::TOUCH_PARAMS + i, DroidM4::TOUCH_LIGHTS + i * 3));
        }
    }
};

Model* modelDroidM4 = createModel<DroidM4, DroidM4Widget>("m4");
