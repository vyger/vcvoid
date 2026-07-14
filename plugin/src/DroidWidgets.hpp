#pragma once
// Overlay widgets for the faithful DROID panels. The faceplate PNG bakes in
// photoreal controls; these widgets sit exactly on top (positions from
// Layout.hpp) and are opaque where they replace a control. All drawing is
// nanovg — no SVG assets.
#include "plugin.hpp"
#include "Layout.hpp"
#include "BuildInfo.hpp"
#include "uatbridge/Bridge.hpp"

namespace dw {

inline Vec hpVec(droid::layout::Pos p) {
    return mm2px(Vec(p.x * droid::layout::kHPmm, p.y * droid::layout::kHPmm));
}
inline float hpPx(float hp) { return mm2px(hp * droid::layout::kHPmm); }

// Maps a Layout position (HP units) to render px THROUGH the faceplate art:
// layout HP -> mm -> art px (22.75 px/mm, the calibrated render scale from
// tools/check_art_alignment.py) -> render px (the PNG is stretched to fill the
// module box). The PNGs are ~0.2-0.3% larger than 22.75 px/mm x the module's
// nominal size, so placing overlays with plain hpVec() leaves them a couple of
// px up-left of the baked controls — worst at the panel bottom (UAT round 3:
// buttons and jacks visibly off). Every layout-placed overlay must go through
// this; drawn SIZES may stay in plain hpPx (a 0.3% size error is invisible).
struct ArtMap {
    float imgW, imgH;   // faceplate PNG pixel dimensions
    Vec box;            // module widget box.size
    Vec vec(droid::layout::Pos p) const {
        constexpr float kPxPerMm = 22.75f;
        return Vec(p.x * droid::layout::kHPmm * kPxPerMm * box.x / imgW,
                   p.y * droid::layout::kHPmm * kPxPerMm * box.y / imgH);
    }
    // art px -> render px, for geometry measured directly off the PNG
    float x(float artPx) const { return artPx * box.x / imgW; }
    float y(float artPx) const { return artPx * box.y / imgH; }
    // small residual offset measured off the PNG (art px), e.g. the baked
    // jack holes sit (+0.9, +3.3) art px from their layout centres
    Vec off(float artDx, float artDy) const { return Vec(x(artDx), y(artDy)); }
};

// The baked jack holes sit this far (art px) from their Layout centres —
// measured on master.png (spread 0.1 px across all 16 jacks; same render
// pipeline for every faceplate). Add A.off(kJackArtDx, kJackArtDy) to each
// jack's A.vec position (UAT round 4: overlays a hair high).
constexpr float kJackArtDx = 0.9f, kJackArtDy = 3.3f;

// ---- panel -------------------------------------------------------------
struct ImagePanel : widget::Widget {
    std::string path;      // absolute path to the PNG
    std::string fallback;  // module name shown if the image is missing
    bool warnedMissing = false;  // draw() runs every frame — WARN only once

    static ImagePanel* create(const std::string& resPath, float hp,
                              const std::string& name) {
        auto* p = new ImagePanel;
        p->path = asset::plugin(pluginInstance, resPath);
        p->fallback = name;
        p->box.pos = Vec(0, 0);
        p->box.size = Vec(hp * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
        return p;
    }

    void draw(const DrawArgs& args) override {
        std::shared_ptr<window::Image> img = APP->window->loadImage(path);
        if (img && img->handle >= 0) {
            NVGpaint paint = nvgImagePattern(args.vg, 0.f, 0.f,
                box.size.x, box.size.y, 0.f, img->handle, 1.f);
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
            nvgFillPaint(args.vg, paint);
            nvgFill(args.vg);
        } else {
            if (!warnedMissing) {   // draw() fires every frame; warn just once
                WARN("vcvoid: missing faceplate %s", path.c_str());
                warnedMissing = true;
            }
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
            nvgFillColor(args.vg, nvgRGB(0x1a, 0x1a, 0x1a));
            nvgFill(args.vg);
            nvgFillColor(args.vg, nvgRGB(0xd0, 0xc0, 0x90));
            nvgFontSize(args.vg, 10);
            nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgText(args.vg, box.size.x / 2, box.size.y / 2, fallback.c_str(), nullptr);
        }
        Widget::draw(args);
    }
};

// ---- knob ---------------------------------------------------------------
// Dark cap with a white pointer, matching the faceplate render it covers.
// Rotation range mirrors the hardware pot (~300 degrees).
struct DroidKnob : app::Knob {
    float minAngle = -0.83f * M_PI;
    float maxAngle = 0.83f * M_PI;
    // Visual cap DIAMETER in HP, art-measured: the FULL dark knob cap out to
    // the rim highlight (radius ~175 art px on the 5 HP pots), NOT just the
    // white pointer disc at its centre. The faceplate's printed control
    // (Layout size, e.g. 4.1 HP) additionally includes the tick-mark ring; the
    // drawn cap must cover the whole baked knob — cap, rim AND its static
    // printed pointer — or the printed pointer stays visible and never moves
    // (UAT round 2). Sized independently of the (larger) hit box.
    float capDiaHP = 3.03f;
    // Pointer style. The big DROID pots are printed with a white CENTRE DISC
    // plus a wedge reaching the cap edge; the small P10 pots have a plain
    // white bar near the rim. Art-measured per module.
    bool discPointer = true;

    DroidKnob() {
        box.size = Vec(hpPx(4.1f), hpPx(4.1f));
        // Rack's default drag maps the full range onto ~667 px of mouse travel
        // — far slower than turning the E4 encoder (4 px/detent, 96 px/rev).
        // 6x puts a full pot sweep at ~110 px, matching the E4's feel (UAT).
        speed = 6.f;
    }

    void draw(const DrawArgs& args) override {
        float v = 0.f;
        if (engine::ParamQuantity* pq = getParamQuantity())
            v = pq->getScaledValue();
        float angle = math::rescale(v, 0.f, 1.f, minAngle, maxAngle);
        Vec c = box.size.div(2);
        float r = hpPx(capDiaHP) * 0.5f;   // drawn cap radius (matches art)
        // cap
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, c.x, c.y, r);
        nvgFillColor(args.vg, nvgRGB(0x14, 0x14, 0x14));
        nvgFill(args.vg);
        nvgStrokeWidth(args.vg, 1.0f);
        nvgStrokeColor(args.vg, nvgRGB(0x30, 0x30, 0x30));
        nvgStroke(args.vg);
        // pointer
        nvgSave(args.vg);
        nvgTranslate(args.vg, c.x, c.y);
        nvgRotate(args.vg, angle);
        nvgFillColor(args.vg, nvgRGB(0xf0, 0xf0, 0xf0));
        if (discPointer) {
            // white centre disc (art: r ~ 0.51 cap radius) ...
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, 0.f, 0.f, r * 0.51f);
            nvgFill(args.vg);
            // ... with a tapered wedge to the cap edge; starts inside the disc
            // so the joint is seamless at any angle
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, -r * 0.20f, -r * 0.30f);
            nvgLineTo(args.vg,  r * 0.20f, -r * 0.30f);
            nvgLineTo(args.vg,  r * 0.09f, -r * 0.93f);
            nvgLineTo(args.vg, -r * 0.09f, -r * 0.93f);
            nvgClosePath(args.vg);
            nvgFill(args.vg);
        } else {
            // plain bar at the rim (small-pot style)
            nvgBeginPath(args.vg);
            nvgRoundedRect(args.vg, -r * 0.09f, -r * 0.95f, r * 0.18f, r * 0.45f, r * 0.09f);
            nvgFill(args.vg);
        }
        nvgRestore(args.vg);
    }
};
struct DroidKnobSmall : DroidKnob {
    DroidKnobSmall() {
        box.size = Vec(hpPx(1.5f), hpPx(1.5f));
        capDiaHP = 1.38f;      // full dark cap inside the printed gold ring
        discPointer = false;   // art: bar pointer near the rim, no centre disc
    }
};

// ---- button (LED in the cap, like the hardware) ---------------------------
struct DroidButton : app::Switch {
    int lightId = -1;   // module light index whose brightness tints the cap
    // Visual cap DIAMETER in HP, art-measured: the FULL printed button — outer
    // ring/bezel included — is 214 art px = 9.4 mm = 1.85 HP on every button
    // module (p2b8/p4b2/b32/db8e). Covering only the inner disc leaves the
    // baked bezel poking out around the drawn cap (UAT round 4). Sized
    // independently of the (larger, 2.7 HP) hit box.
    float capDiaHP = 1.85f;
    // When the faceplate bakes a LABELLED cap (DB8E's white buttons carry
    // printed legends), an opaque drawn cap would erase the label — set false
    // to draw only the LED glow / pressed shade over the baked art instead.
    bool opaqueCap = true;

    DroidButton() {
        momentary = true;
        box.size = Vec(hpPx(2.7f), hpPx(2.7f));
    }

    void draw(const DrawArgs& args) override {
        Vec c = box.size.div(2);
        float r = hpPx(capDiaHP) * 0.5f;   // drawn cap radius (matches art)
        float pressed = 0.f;
        if (engine::ParamQuantity* pq = getParamQuantity())
            pressed = pq->getValue();
        float glow = 0.f;
        if (module && lightId >= 0)
            glow = module->lights[lightId].getBrightness();
        glow = math::clamp(glow, 0.f, 1.f);
        if (opaqueCap) {
            float rr = r * (pressed > 0.5f ? 0.96f : 1.f);
            NVGcolor lit = nvgRGB(0xff, 0xf4, 0xd0);
            // bezel ring (art: darker grey annulus, inner disc at ~0.78 r)
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, c.x, c.y, rr);
            nvgFillColor(args.vg, nvgLerpRGBA(nvgRGB(0xa0, 0xa0, 0xa0), lit, glow));
            nvgFill(args.vg);
            nvgStrokeWidth(args.vg, 1.5f);
            nvgStrokeColor(args.vg, nvgRGB(0x10, 0x10, 0x10));
            nvgStroke(args.vg);
            // lighter cap disc, warm-white illumination from the LED behind it
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, c.x, c.y, rr * 0.78f);
            nvgFillColor(args.vg, nvgLerpRGBA(nvgRGB(0xc4, 0xc4, 0xc4), lit, glow));
            nvgFill(args.vg);
            nvgStrokeWidth(args.vg, 1.0f);
            nvgStrokeColor(args.vg, nvgRGB(0x30, 0x30, 0x30));
            nvgStroke(args.vg);
        } else {
            // translucent overlays only; the baked cap (and its label) shows
            if (glow > 0.f) {
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, c.x, c.y, r);
                nvgFillColor(args.vg, nvgRGBAf(1.f, 0.92f, 0.55f, 0.55f * glow));
                nvgFill(args.vg);
            }
            if (pressed > 0.5f) {
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, c.x, c.y, r);
                nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 0x30));
                nvgFill(args.vg);
            }
        }
    }
};

// ---- port (the baked jack render is the visual) ---------------------------
struct DroidPort : app::PortWidget {
    bool hover = false;
    DroidPort() { box.size = Vec(hpPx(2.0f), hpPx(2.0f)); }  // hit-box ~10 mm
    void onEnter(const EnterEvent& e) override { hover = true; PortWidget::onEnter(e); }
    void onLeave(const LeaveEvent& e) override { hover = false; PortWidget::onLeave(e); }
    void draw(const DrawArgs& args) override {
        if (hover) {
            Vec c = box.size.div(2);
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, c.x, c.y, c.x);
            nvgStrokeWidth(args.vg, 1.5f);
            nvgStrokeColor(args.vg, nvgRGBA(0xff, 0xf4, 0xd0, 0xa0));
            nvgStroke(args.vg);
        }
        // no cap drawing: the faceplate's jack render shows through
    }
};

// Variant of DroidPort for co-located bidirectional jacks (G8's gate in+out
// share one faceplate jack). Two PortWidgets stacked at the same center each
// draw as an OpaqueWidget and Rack hit-tests topmost-first, so a full-size
// port added last would consume every click and shadow the one beneath it
// (review finding). To keep both reachable, the pair is given split hit-boxes
// — each covers only half the jack (see G8.cpp) — while the hover ring is
// still drawn around the TRUE jack center at a fixed 1 HP radius (not the
// half-box center, which would sit off the jack). `jackCenterY` is the jack
// center's y in this widget's local coords, set by the owner after resizing.
struct DroidPortHalf : DroidPort {
    float jackCenterY = 0.f;
    void draw(const DrawArgs& args) override {
        if (hover) {
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, box.size.x / 2, jackCenterY, hpPx(1.0f));
            nvgStrokeWidth(args.vg, 1.5f);
            nvgStrokeColor(args.vg, nvgRGBA(0xff, 0xf4, 0xd0, 0xa0));
            nvgStroke(args.vg);
        }
        // no cap drawing: the faceplate's jack render shows through
    }
};

// ---- N-position toggle (S10 lower 8 / P8S8; 2- or 3-position configSwitch) -
struct DroidToggle : app::Switch {
    DroidToggle() {
        momentary = false;
        box.size = Vec(hpPx(2.0f), hpPx(2.0f));
    }
    void draw(const DrawArgs& args) override {
        float v = 0.f;
        if (engine::ParamQuantity* pq = getParamQuantity())
            v = pq->getScaledValue();
        Vec c = box.size.div(2);
        float r = c.x;
        // bushing
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, c.x, c.y, r * 0.55f);
        nvgFillColor(args.vg, nvgRGB(0x50, 0x50, 0x50));
        nvgFill(args.vg);
        // lever: vertical offset follows the scaled param value proportionally
        // (0 = down, 0.5 = centered, 1 = up), so 2-position and 3-position
        // (or any N-position) configSwitch params all render distinctly.
        // maxOff/leverH are chosen so the endpoints (v=0, v=1) exactly
        // reproduce the old fixed up/down rects (pivot-to-tip, length 0.95r);
        // at v=0.5 the lever is centered on the bushing as a short nub.
        float leverH = r * 0.95f;
        float maxOff = r * 0.475f;
        float offset = math::rescale(v, 0.f, 1.f, maxOff, -maxOff);
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, c.x - r * 0.16f,
                       c.y + offset - leverH * 0.5f,
                       r * 0.32f, leverH, r * 0.16f);
        nvgFillColor(args.vg, nvgRGB(0xc0, 0xc0, 0xc0));
        nvgFill(args.vg);
    }
};

// ---- vertical slider (P8S8 faders) ----------------------------------------
// The faceplate bakes a black slot AND an orange cap at mid-travel. This widget
// repaints the slot black (masking the baked cap) then draws the moving orange
// cap with the L-LED inside it, so the cap tracks the param instead of the
// double-image you'd get from drawing over the baked cap. All geometry is
// art-measured (art px / 22.75 = mm, /5.08 = HP): slot 0.545 HP wide x 2.423 HP
// tall, orange cap 0.389 x 0.917 HP, cap colour rgb(247,148,29). box height =
// slot travel and box is centred on the layout P position (= slot centre = the
// baked cap's mid-travel), so param 0.5 lands exactly on the baked cap.
struct DroidSlider : app::SliderKnob {
    int lightId = -1;          // module light whose brightness drives the cap LED
    float slotWHP = 0.545f;    // black slot width (HP)
    float capWHP  = 0.389f;    // orange cap width (HP)
    float capHHP  = 0.917f;    // orange cap height (HP)

    DroidSlider() {
        forceLinear = true;    // linear (fader) drag, not the radial knob mapping
        horizontal = false;    // vertical travel
        box.size = Vec(hpPx(0.90f), hpPx(2.423f));   // grab width x slot travel
    }

    void draw(const DrawArgs& args) override {
        float v = 0.f;
        if (engine::ParamQuantity* pq = getParamQuantity())
            v = pq->getScaledValue();
        float W = box.size.x, H = box.size.y;
        float slotW = hpPx(slotWHP);
        float capW = hpPx(capWHP), capH = hpPx(capHHP);
        // 1) black slot — reproduces the baked slot and masks the baked cap.
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, (W - slotW) * 0.5f, 0.f, slotW, H, slotW * 0.5f);
        nvgFillColor(args.vg, nvgRGB(0x00, 0x00, 0x00));
        nvgFill(args.vg);
        // 2) orange cap: v=0 -> bottom of travel, v=1 -> top.
        float capCY = math::rescale(v, 0.f, 1.f, H - capH * 0.5f, capH * 0.5f);
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, (W - capW) * 0.5f, capCY - capH * 0.5f,
                       capW, capH, capW * 0.18f);
        nvgFillColor(args.vg, nvgRGB(0xf7, 0x94, 0x1d));
        nvgFill(args.vg);
        // 3) white value LED inside the cap (brightness from the light).
        float glow = 0.f;
        if (module && lightId >= 0)
            glow = module->lights[lightId].getBrightness();
        float ledW = capW * 0.24f, ledH = capH * 0.42f;
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, W * 0.5f - ledW * 0.5f, capCY - ledH * 0.5f,
                       ledW, ledH, ledW * 0.4f);
        int a = (int)(math::clamp(0.20f + 0.80f * glow, 0.f, 1.f) * 255.f);
        nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0xff, a));
        nvgFill(args.vg);
    }
};

// ---- square LED (master matrix, G8/X7 status) -----------------------------
template <typename TBase = GrayModuleLightWidget>
struct SquareLight : TBase {
    void drawBackground(const widget::Widget::DrawArgs& args) override {
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0, 0, this->box.size.x, this->box.size.y);
        nvgFillColor(args.vg, this->bgColor);
        nvgStrokeWidth(args.vg, 0.5f);
        nvgStrokeColor(args.vg, this->borderColor);
        nvgFill(args.vg);
        nvgStroke(args.vg);
    }
    void drawLight(const widget::Widget::DrawArgs& args) override {
        if (this->color.a > 0.0) {
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0, 0, this->box.size.x, this->box.size.y);
            nvgFillColor(args.vg, this->color);
            nvgFill(args.vg);
        }
    }
};

// Common panel prologue shared by every controller/expander ModuleWidget
// ctor: look up the slug's Layout entry, size the widget box to it, add the
// faceplate PNG as the base layer, and return the ArtMap for placing
// overlays on top. Caller still calls setModule(module) itself first (that
// call, and what if anything happens after this returns, varies per widget).
inline ArtMap setupPanel(rack::app::ModuleWidget* w, const char* slug, const char* label,
                          float artW, float artH) {
    const auto* L = droid::layout::find(slug);
    w->box.size = Vec(L->hp * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
    w->addChild(ImagePanel::create(std::string("res/faceplates/") + slug + ".png", L->hp, label));
    return ArtMap{artW, artH, w->box.size};
}

} // namespace dw

// Shared base for every vcvoid controller/expander ModuleWidget. The UAT bridge
// drains its UI queue through a single invisible BridgeWidget attached to the
// scene; that attach can only happen on the UI thread once APP->scene exists,
// so it is driven from a module widget's step() (the first point guaranteed to
// run with the scene attached — widget ctors run before scene attach, where
// ensureWidget() would no-op). Masters already do this from
// DroidMasterBaseWidget::step(); every non-master vcvoid widget derives from
// this so ANY vcvoid module present in the rack — not just a master — keeps the
// bridge's queue serviced. ensureWidget() is idempotent and no-ops when the
// bridge is disabled (VCVOID_UAT_BRIDGE unset). A truly-empty rack (zero vcvoid
// modules) has no widget to run this, so at least one vcvoid module must exist
// before the generic rack ops (POST /modules etc.) will marshal onto the UI
// thread — documented as the bridge's precondition.
struct VcvoidModuleWidget : rack::app::ModuleWidget {
    void step() override {
        rack::app::ModuleWidget::step();
        if (auto* b = uat::Bridge::instance()) b->ensureWidget();
    }

    // Subclasses that override this (X7) must keep the build-info line last.
    void appendContextMenu(Menu* menu) override {
        appendBuildInfoMenu(menu);
    }
};
