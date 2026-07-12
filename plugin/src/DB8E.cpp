#include "ChainModule.hpp"
#include "EncoderWidgets.hpp"
#include "DroidWidgets.hpp"
#include <cstring>

// DROID DB8E controller: 8 momentary buttons (B<c>.1-8) with 8 button LEDs
// (L<c>.1-8), one endless encoder (E<c>.1) whose integrated push is B<c>.9 and
// whose 32-LED value ring is L<c>.9 (the ring's white overlay), and a 128x64
// OLED display driven by the master's display circuit. manual/hardware.md §6.12
// + Forge moduledb8e.cpp (numRegisters BUTTON=9, LED=9, ENC=1).
//
// Master contract (Task 2): the master reads the 8 face buttons on `buttons`
// bits 0-7 and the ENCODER PUSH on bit 8 (DroidMaster.cpp: pushBit = 8 for
// MDB8E; bit 8 also lands in register B<c>.9 via the generic B feed now that
// db8e buttons = 9). It diffs detentCount[0] into turn events, writes ring[0]
// (value dot) + leds[0..8] (L1.1-8 = the 8 button LEDs, L1.9 = leds[8] = the
// ring's white overlay), and fills the disp* fields (header/body as
// NUL-terminated ASCII via textForNumber; value+numbermode when !isText). This
// module publishes a monotonic detent counter + push level, and mirrors the
// downstream disp* content for the OLED widget's draw().
struct DroidDB8E : ChainModule {
    enum ParamId { ENUMS(BUTTON_PARAMS, 8), PARAMS_LEN };
    enum InputId { INPUTS_LEN };
    enum OutputId { OUTPUTS_LEN };
    enum LightId { ENUMS(BUTTON_LIGHTS, 8), LIGHTS_LEN };

    uint32_t detent = 0;   // monotonic encoder detent counter (widget ++/--)
    bool push = false;     // encoder push level (widget sets on press)
    float ring = 0.f;      // last downstream ring value 0..1 (widget draws)
    float ringLed = 0.f;   // last downstream L-register white overlay 0..1

    // OLED content mirrored from the downstream disp* fields (fixed-size, no heap).
    char dispHeader[24] = {};
    char dispText[24] = {};
    float dispValue = 0.f;
    uint8_t dispNumbermode = 0, dispFontsize = 0, dispIsText = 0;
    bool dispActive = false;

    DroidDB8E() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        for (int i = 0; i < 8; i++)
            configButton(BUTTON_PARAMS + i, string::f("B%d", i + 1));
    }

    void fillUpstream(droid::chain::UpstreamBlock& b) override {
        b.modelId = droid::chain::MDB8E;
        b.detentCount[0] = detent;
        b.buttons = packButtonParams(BUTTON_PARAMS, 8);   // 8 face buttons -> bits 0-7
        if (push) b.buttons |= (1u << 8);                 // encoder push -> bit 8 (Task 2 contract)
    }

    void applyDownstream(const droid::chain::DownstreamBlock& b, float sampleTime) override {
        for (int i = 0; i < 8; i++)                       // L1.1-L1.8 = the 8 button LEDs
            lights[BUTTON_LIGHTS + i].setBrightnessSmooth(b.leds[i], sampleTime);
        ring = b.ring[0];
        ringLed = b.leds[8];                              // L1.9 = the encoder ring overlay (un-shared from button 1)
        std::memcpy(dispHeader, b.dispHeader, sizeof dispHeader);
        std::memcpy(dispText, b.dispText, sizeof dispText);
        dispHeader[sizeof dispHeader - 1] = '\0';         // defensive NUL guards
        dispText[sizeof dispText - 1] = '\0';
        dispValue = b.dispValue;
        dispNumbermode = b.dispNumbermode;
        dispFontsize = b.dispFontsize;
        dispIsText = b.dispIsText;
        dispActive = b.modelId == droid::chain::MDB8E
                  && (b.dispHeader[0] || b.dispText[0] || b.dispValue != 0.f || b.dispIsText);
    }

    void process(const ProcessArgs& args) override { relay(args.sampleTime); }
};

// 128x64 OLED. Pragmatic render (SPEC design §4): black background, a header
// line (top) and a body line (middle: text if dispIsText, else the plain value
// formatted %.4g — NO numbermode formatting, that is M7), scaled by a coarse
// fontsize mapping. When the display carries no content, draws a single idle
// status string. Font loaded lazily and null-checked so a missing font never
// crashes (bg still drawn, text skipped).
struct DB8EDisplay : Widget {
    DroidDB8E* module = nullptr;

    // DROID fontsizes run 0..18; map to three OLED pixel sizes. Tuned for the
    // 6 HP DB8E's small OLED box (~23 mm wide); bumped ~35% for readability
    // (UAT round 4: too small).
    static float bodyPx(uint8_t fontsize) {
        if (fontsize <= 6)  return 11.f;
        if (fontsize <= 12) return 15.f;
        return 18.f;   // capped so the biggest tier still clears the header
    }

    void draw(const DrawArgs& args) override {
        NVGcontext* vg = args.vg;
        // Black OLED background.
        nvgBeginPath(vg);
        nvgRect(vg, 0, 0, box.size.x, box.size.y);
        nvgFillColor(vg, nvgRGB(0, 0, 0));
        nvgFill(vg);

        // Font (ShareTechMono ships with the Rack app: res/fonts/). Null-safe.
        std::shared_ptr<Font> font =
            APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
        if (!font || font->handle < 0)
            return;   // no font: bg-only, never crash
        nvgFontFaceId(vg, font->handle);

        const NVGcolor fg = nvgRGB(180, 210, 255);
        nvgFillColor(vg, fg);

        if (!module || !module->dispActive) {
            // Idle/status. Full DB8E states (not connected / config error) are
            // best-effort; a single idle string is the pragmatic choice here.
            nvgFontSize(vg, 8.5f);
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgFillColor(vg, nvgRGB(90, 100, 130));
            nvgText(vg, box.size.x / 2.f, box.size.y / 2.f, "not used by patch", NULL);
            return;
        }

        // Header line (top).
        if (module->dispHeader[0]) {
            nvgFontSize(vg, 9.5f);
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
            nvgText(vg, box.size.x / 2.f, 2.f, module->dispHeader, NULL);
        }

        // Body line (middle): text, or the plain numeric value.
        char bodyBuf[32];
        const char* body;
        if (module->dispIsText) {
            body = module->dispText;
        } else {
            std::snprintf(bodyBuf, sizeof bodyBuf, "%.4g", module->dispValue);
            body = bodyBuf;
        }
        nvgFontSize(vg, bodyPx(module->dispFontsize));
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(vg, box.size.x / 2.f, box.size.y * 0.6f, body, NULL);
    }
};

// Value-ring display bound to the encoder's ring value (+ L-register overlay).
// The DB8E faceplate bakes the same SQUARE 32-LED ring as the E4 (9 cells per
// side, corners shared); `half`/`cell` are measured from res/faceplates/db8e.png
// and set by the widget below so the drawn LEDs land exactly on the art cells.
struct DB8ERingWidget : Widget {
    DroidDB8E* module = nullptr;
    float half = 0.f;   // centre -> side LED-centre row (px)
    float cell = 0.f;   // LED square side (px)
    void draw(const DrawArgs& args) override {
        float v  = module ? module->ring    : 0.f;
        float ov = module ? module->ringLed : 0.f;
        drawValueRingSquare(args.vg, box.size.div(2), half, cell, v, ov);
    }
};

struct DroidDB8EWidget : VcvoidModuleWidget {
    DroidDB8EWidget(DroidDB8E* module) {
        setModule(module);
        // Layout -> render px through the faceplate art (dw::ArtMap): the
        // PNG is 692x2918 px and stretched to the module box.
        dw::ArtMap A = dw::setupPanel(this, "db8e", "DB8E", 692.f, 2918.f);
        const auto* L = droid::layout::find("db8e");

        // OLED is not a Forge register, so it has no layout entry: the baked
        // screen GLASS measured off db8e.png is x 85..606, y 301..572 art px
        // (the gold bezel outline extends further down, to y 789 — only the
        // black glass is the display). The widget overlays the glass exactly
        // (UAT round 4: the old mm guess was much taller than the printed
        // screen).
        auto* oled = new DB8EDisplay;
        oled->module = module;
        oled->box.pos = Vec(A.x(85.f), A.y(301.f));
        oled->box.size = Vec(A.x(606.f - 85.f), A.y(572.f - 301.f));
        addChild(oled);

        // Encoder + value ring. Geometry measured from res/faceplates/db8e.png
        // (692x2918 px; art px -> Rack px is the box/image ratio): the panel
        // bakes the SAME square 32-LED ring as the E4 — cell columns at x
        // 95..596 (pitch 62.6, cell 44), ring centre (345.5, 2411) — around a
        // glossy knob centred (346.5, 2404) whose cap incl. rim reaches ~350 px.
        // The layout 'E' position is the same centre; sizes/offsets come from
        // the art so the overlay covers the baked print exactly (UAT round 2:
        // the old 4.1 HP encoder + circular dot ring sat on top of the baked
        // square cells).
        {
            const float sx = box.size.x / 692.f;
            const float sy = box.size.y / 2918.f;
            float half = 250.5f * sx, cell = 44.f * sx;
            float side = 2.f * half + cell;
            auto* ring = new DB8ERingWidget;
            ring->module = module;
            ring->half = half;
            ring->cell = cell;
            ring->box.size = Vec(side, side);
            ring->box.pos = Vec(345.5f * sx - side / 2.f, 2411.f * sy - side / 2.f);
            addChild(ring);
            float kd = 350.f * sx;
            auto* enc = new DroidEndlessEncoder;
            enc->box.size = Vec(kd, kd);
            enc->box.pos = Vec(346.5f * sx - kd / 2.f, 2404.f * sy - kd / 2.f);
            if (module) {
                enc->detentCount = &module->detent;
                enc->pushed = &module->push;
            }
            addChild(enc);
        }

        // 8 face buttons + LEDs. The baked caps are white discs with printed
        // legends — draw only translucent LED-glow/pressed overlays on top so
        // the labels stay readable (an opaque cap would erase them).
        for (int i = 0; i < 8; i++) {
            auto* b = createParamCentered<dw::DroidButton>(
                A.vec(L->pos('B', i + 1)), module, DroidDB8E::BUTTON_PARAMS + i);
            b->capDiaHP = 1.73f;   // glow radius: out to the bezel ring (200 art px)
            b->opaqueCap = false;  // keep the baked label visible
            b->lightId = DroidDB8E::BUTTON_LIGHTS + i;
            addParam(b);
        }
    }
};

Model* modelDroidDB8E = createModel<DroidDB8E, DroidDB8EWidget>("db8e");
