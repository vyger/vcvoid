#include "ChainModule.hpp"
#include "EncoderWidgets.hpp"
#include "DroidWidgets.hpp"
#include "src/controllerstate.hpp"   // droid::DisplayLayout (via -I../engine)
#include <algorithm>
#include <cmath>
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
// NUL-terminated ASCII via textForNumber; the payload named by the layout tag).
// This module publishes a monotonic detent counter + push level, and mirrors the
// downstream disp* content for the OLED widget's draw().
struct DroidDB8E : ChainModule {
    enum ParamId { ENUMS(BUTTON_PARAMS, 8), PARAMS_LEN };
    enum InputId { INPUTS_LEN };
    enum OutputId { OUTPUTS_LEN };
    enum LightId { ENUMS(BUTTON_LIGHTS, 8), LIGHTS_LEN };

    uint32_t detent = 0;   // monotonic encoder detent counter (widget ++/--)
    // Encoder click/turn/push classifier (EncoderGesture.hpp): the widget
    // feeds it press/move/release on the UI thread; process() steps its
    // timers and fillUpstream publishes its level() as the push bit.
    vcvoid::EncoderGesture gest;
    // Select-gated ring image (issue #15; see chain.hpp DownstreamBlock).
    uint8_t ringFlags = 0;
    float ringValue = 0.f;
    float ringColor = 0.f;
    float ringNegColor = 0.f;
    float ringOverlay = 0.f;
    float ringLed = 0.f;   // last downstream L-register white overlay 0..1

    // OLED content mirrored from the downstream disp* fields (fixed-size, no heap).
    char dispHeader[24] = {};
    char dispText[24] = {};
    float dispValue = 0.f;
    uint8_t dispNumbermode = 0, dispFontsize = 0;
    // Which layout the master is sending (droid::DisplayLayout as a byte) plus
    // each layout's payload — issue #22, Group C. An unknown tag draws the
    // hardware's "update firmware" screen rather than guessing.
    uint8_t dispLayout = 0;
    uint8_t dispBubbleCount = 0, dispBubbleIndex = 0;
    int16_t dispNoteSemitone = 0;
    bool dispNoteWithOctave = false;
    uint8_t dispGatePattern = 0, dispRangeFirst = 0, dispRangeLast = 0;
    bool dispActive = false;

    DroidDB8E() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        for (int i = 0; i < 8; i++)
            configButton(BUTTON_PARAMS + i, string::f("B%d", i + 1));
    }

    droid::chain::ModelId chainModel() const override { return droid::chain::MDB8E; }

    void fillUpstream(droid::chain::UpstreamBlock& b) override {
        b.detentCount[0] = detent;
        b.buttons = packButtonParams(BUTTON_PARAMS, 8);   // 8 face buttons -> bits 0-7
        if (gest.level()) b.buttons |= (1u << 8);         // encoder push -> bit 8 (Task 2 contract)
    }

    void applyDownstream(const droid::chain::DownstreamBlock& b, float sampleTime) override {
        for (int i = 0; i < 8; i++)                       // L1.1-L1.8 = the 8 button LEDs
            lights[BUTTON_LIGHTS + i].setBrightnessSmooth(b.leds[i], sampleTime);
        ringFlags    = b.ringFlags[0];
        ringValue    = b.ringValue[0];
        ringColor    = b.ringColor[0];
        ringNegColor = b.ringNegColor[0];
        ringOverlay  = b.ringOverlay[0];
        ringLed = b.leds[8];                              // L1.9 = the encoder ring overlay (un-shared from button 1)
        std::memcpy(dispHeader, b.dispHeader, sizeof dispHeader);
        std::memcpy(dispText, b.dispText, sizeof dispText);
        dispHeader[sizeof dispHeader - 1] = '\0';         // defensive NUL guards
        dispText[sizeof dispText - 1] = '\0';
        dispValue = b.dispValue;
        dispNumbermode = b.dispNumbermode;
        dispFontsize = b.dispFontsize;
        dispLayout = b.dispLayout;
        dispBubbleCount = b.dispBubbleCount;
        dispBubbleIndex = b.dispBubbleIndex;
        dispNoteSemitone = b.dispNoteSemitone;
        dispNoteWithOctave = b.dispNoteWithOctave != 0;
        dispGatePattern = b.dispGatePattern;
        dispRangeFirst = b.dispRangeFirst;
        dispRangeLast = b.dispRangeLast;
        // The engine's own DisplayState::active, not a guess from the content:
        // an `encoder` parked at output 0 with no header is real content that
        // the old "any field is non-empty" heuristic read as an idle screen.
        dispActive = b.modelId == droid::chain::MDB8E && b.dispActive;
    }

    void applyOwnLabels() override {
        // B9 (the encoder's push) and the encoder itself are custom widgets with
        // no ParamQuantity, so only the eight face buttons take tooltips; their
        // labels still draw as panel chips.
        vcvoid::labels::applyParamBank(this, BUTTON_PARAMS, 8, 'B', registerLabels,
                                       "B%d", true);
    }

    void process(const ProcessArgs& args) override {
        gest.step(args.sampleTime);
        relay(args.sampleTime);
    }
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

        // Header line (top), with the thin rule the hardware draws under it
        // (see the DB8E capture in issue #19: a small centred caption over a
        // full-width hairline, then the body).
        if (module->dispHeader[0]) {
            nvgFontSize(vg, 9.5f);
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
            nvgText(vg, box.size.x / 2.f, 2.f, module->dispHeader, NULL);
            float ruleY = std::round(2.f + 9.5f) + 0.5f;
            nvgBeginPath(vg);
            nvgMoveTo(vg, 2.f, ruleY);
            nvgLineTo(vg, box.size.x - 2.f, ruleY);
            nvgStrokeWidth(vg, 1.f);
            nvgStrokeColor(vg, fg);
            nvgStroke(vg);
        }

        // Body: the layout the master named (issue #22, Group C). Every
        // branch reads only its own payload, and an unrecognised tag draws the
        // DB8E's own "update firmware" screen (hardware.md §6.13) rather than
        // falling through to another layout — that is the hardware's behaviour
        // when a master sends a layout its display firmware predates, and it
        // keeps an engine/plugin version skew from drawing nonsense.
        const float bodyY = box.size.y * 0.6f;
        switch ((droid::DisplayLayout)module->dispLayout) {
            case droid::DisplayLayout::Bubbles:
                drawBubbleChain(vg, bodyY, module->dispBubbleCount,
                                module->dispBubbleIndex, fg);
                return;
            case droid::DisplayLayout::Text:
                drawBody(vg, bodyY, module->dispText);
                return;
            case droid::DisplayLayout::NoteName: {
                char noteBuf[16];
                formatNoteName(noteBuf, sizeof noteBuf, module->dispNoteSemitone,
                               module->dispNoteWithOctave);
                drawBody(vg, bodyY, noteBuf);
                return;
            }
            case droid::DisplayLayout::GatePattern:
                drawGatePattern(vg, bodyY, module->dispGatePattern, fg);
                return;
            case droid::DisplayLayout::Range: {
                char rangeBuf[16];
                std::snprintf(rangeBuf, sizeof rangeBuf, "%d-%d",
                              (int)module->dispRangeFirst, (int)module->dispRangeLast);
                drawBody(vg, bodyY, rangeBuf);
                return;
            }
            case droid::DisplayLayout::Value: {
                // SPEC-GAP: numbermode formatting beyond 1-3 (volts/percent/
                // note/gauge/sparkline) is not implemented; those show the
                // plain fraction.
                // %g's SIGNIFICANT-digit count is what the hardware appears to
                // use — every value read off the issue-#19 capture (652.74,
                // 1259.73, 2003.16, 514.97) carries six, as does the manual's
                // own `0.278` example. %.4g truncated all four of those to
                // 652.7 / 1260 / 2003 / 515.
                // A circuit that FIXES the format (display.md numbermode 1-3:
                // no / one / two decimal places) gets it; the encoquencer's
                // edit screens use 1 and 3, measured on hardware.
                char bodyBuf[32];
                switch (module->dispNumbermode) {
                    case 1:  std::snprintf(bodyBuf, sizeof bodyBuf, "%.0f", module->dispValue); break;
                    case 2:  std::snprintf(bodyBuf, sizeof bodyBuf, "%.1f", module->dispValue); break;
                    case 3:  std::snprintf(bodyBuf, sizeof bodyBuf, "%.2f", module->dispValue); break;
                    default: std::snprintf(bodyBuf, sizeof bodyBuf, "%.6g", module->dispValue); break;
                }
                drawBody(vg, bodyY, bodyBuf);
                return;
            }
        }
        // Unknown layout tag: the hardware's fallback screen.
        nvgFontSize(vg, 9.f);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(vg, box.size.x / 2.f, bodyY, "update firmware", NULL);
    }

private:
    // A note number as the DB8E spells it. Sharps, as hardware.md §6.12 writes
    // the DB8E's own pitch readout ("F♯1 +47"), with '#' standing in for '♯'
    // because ShareTechMono has no musical glyphs. With an octave the number is
    // a pitch counted from C0 (12 -> "C1"); without one it is a bare pitch
    // class 0..11 and no octave digit is drawn.
    static void formatNoteName(char* buf, size_t n, int semitone, bool withOctave) {
        static const char* const kNames[12] = {"C", "C#", "D", "D#", "E", "F",
                                               "F#", "G", "G#", "A", "A#", "B"};
        int pc = ((semitone % 12) + 12) % 12;
        if (withOctave) {
            int octave = (semitone - pc) / 12;
            std::snprintf(buf, n, "%s%d", kNames[pc], octave);
        } else {
            std::snprintf(buf, n, "%s", kNames[pc]);
        }
    }

    void drawBody(NVGcontext* vg, float y, const char* body) {
        nvgFontSize(vg, bodyPx(module->dispFontsize));
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(vg, box.size.x / 2.f, y, body, NULL);
    }

    // The sequencer's gate pattern for a step, as the DB8E draws it (measured
    // on hardware, encoquencer): 0 four squares in a row with the first filled
    // (only the first repetition plays), 1 all four filled (every repetition),
    // 2 one long bar (one gate held over the step), 3 the words "tie to next".
    void drawGatePattern(NVGcontext* vg, float y, int pattern, NVGcolor fg) {
        if (pattern == 3) { drawBody(vg, y, "tie to next"); return; }
        const float side = std::min(11.f, (box.size.x - 16.f) / 4.6f);
        const float gap = side * 0.2f;
        const float total = 4.f * side + 3.f * gap;
        const float x0 = box.size.x / 2.f - total / 2.f;
        const float top = y - side / 2.f;
        nvgStrokeColor(vg, fg);
        nvgFillColor(vg, fg);
        nvgStrokeWidth(vg, 1.f);
        if (pattern == 2) {
            nvgBeginPath(vg);
            nvgRect(vg, x0, top + side * 0.2f, total, side * 0.6f);
            nvgFill(vg);
            return;
        }
        for (int i = 0; i < 4; i++) {
            float x = x0 + (float)i * (side + gap);
            nvgBeginPath(vg);
            nvgRect(vg, x + 0.5f, top + 0.5f, side - 1.f, side - 1.f);
            nvgStroke(vg);
            if (pattern == 1 || i == 0) {
                nvgBeginPath(vg);
                nvgRect(vg, x + 2.f, top + 2.f, side - 4.f, side - 4.f);
                nvgFill(vg);
            }
        }
    }

    // The `button` circuit's state chain (button.md "Display"; measured on
    // hardware in issue #19): `count` bubbles in a row joined by short
    // horizontal segments, the one at `index` filled solid and the rest drawn
    // as outlines. The engine sends only (count, index) — radius, pitch and
    // stroke are chosen here, scaled so 2..4 bubbles sit comfortably in the
    // narrow 6 HP OLED box and a larger count still fits.
    void drawBubbleChain(NVGcontext* vg, float y, int count, int index,
                         NVGcolor fg) {
        if (count < 1) return;
        const float avail = box.size.x - 8.f;
        // pitch = 2r + gap, with the connecting segment spanning the gap.
        float r = std::min(5.5f, avail / (float)(3 * count + 1));
        if (r < 1.f) r = 1.f;
        const float gap = r * 2.f;
        const float pitch = 2.f * r + gap;
        const float total = pitch * (float)(count - 1);
        const float x0 = box.size.x / 2.f - total / 2.f;

        nvgStrokeColor(vg, fg);
        nvgStrokeWidth(vg, std::max(1.f, r * 0.28f));
        // Joining segments first, so the bubbles sit on top of them.
        for (int i = 0; i + 1 < count; i++) {
            nvgBeginPath(vg);
            nvgMoveTo(vg, x0 + pitch * (float)i + r, y);
            nvgLineTo(vg, x0 + pitch * (float)(i + 1) - r, y);
            nvgStroke(vg);
        }
        for (int i = 0; i < count; i++) {
            nvgBeginPath(vg);
            nvgCircle(vg, x0 + pitch * (float)i, y, r);
            if (i == index) { nvgFillColor(vg, fg); nvgFill(vg); }
            else            { nvgStroke(vg); }
        }
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
        RingDrawState rs;
        if (module) {
            uint8_t f = module->ringFlags;
            rs.active      = f & 1;
            rs.bipolar     = f & 2;
            rs.fill        = f & 4;
            rs.legacyGauge = f & 8;
            rs.value       = module->ringValue;
            rs.color       = module->ringColor;
            rs.negColor    = module->ringNegColor;
            rs.overlay     = module->ringOverlay;
            rs.lOverlay    = module->ringLed;
        }
        drawEncoderRingSquare(args.vg, box.size.div(2), half, cell, rs);
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
                enc->gesture = &module->gest;
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
        dw::addLabelOverlay(this, "db8e", A,
                            module ? &module->registerLabels : nullptr);
    }
};

Model* modelDroidDB8E = createModel<DroidDB8E, DroidDB8EWidget>("db8e");
