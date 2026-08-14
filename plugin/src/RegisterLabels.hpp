#pragma once
// Register labels in Rack: the names a patch gives its jacks and controls
// (engine/src/labels.hpp), surfaced as tooltips and as the Forge's little white
// label chips on the panel. Issue #26.
//
// THREADING: everything here is UI-THREAD ONLY. Rack's tooltips read
// PortInfo::name / ParamQuantity::name as std::string, and rebuild their text
// every frame while visible, so labels update live — but a patch can be loaded
// from the UAT bridge's HTTP thread, so the master publishes a generation
// counter and the widgets copy the labels across on their own step().
// Data and tooltip plumbing only — the panel chips are drawn by dw::LabelOverlay
// in DroidWidgets.hpp, which includes this. Keeping the widget out avoids a
// cycle: DroidWidgets -> ChainModule -> RegisterLabels.
#include "plugin.hpp"
#include "Layout.hpp"
#include "src/labels.hpp"
#include <string>

namespace vcvoid {
namespace labels {

// One module's view of the patch's labels: the label set plus the register
// numbering that identifies THIS module's controls within it.
struct ModuleLabels {
    droid::PatchLabels patch;
    unsigned controller = 0;   // controller number for P/B/L/S/E (0 = a master)
    unsigned gateG8 = 0;       // G-register expander number (MASTER18 uses 1)
    unsigned gateOffset = 0;   // added to G numbers (the X7's gates are G9..G12)
    unsigned rOffset = 0;      // added to R numbers (G8 banks start at R17)
    bool show = true;          // "Show register labels" context-menu toggle
    uint32_t gen = 0;          // master's labelGen this view was copied from
    bool active = false;       // a patch's labels are live on this module

    // The label for one of this module's registers, or null.
    const droid::RegisterLabel* find(char type, unsigned number) const {
        if (!active) return nullptr;
        if (type == 'G') return patch.find('G', 0, gateG8, number + gateOffset);
        if (type == 'R') return patch.find('R', 0, 0, number + rOffset);
        return patch.find(type, controller, 0, number);
    }

    // The label a DRAWN control shows. Some registers share one physical
    // control — an input jack is also its normalization N, a button contains
    // its LED L — and the Forge paints both labels at identical coordinates,
    // one on top of the other. We resolve to a single label per control
    // instead, and fold the loser into the tooltip (see compose()).
    const droid::RegisterLabel* primary(char type, unsigned number) const {
        if (const droid::RegisterLabel* l = find(type, number)) return l;
        if (type == 'I') return find('N', number);
        return nullptr;
    }
};

// The Rack name/description pair for one control. Shorthand-first: a
// "[CLK] master clock" label shows as "CLK" with "master clock" underneath,
// which is both the Forge's own emphasis and Rack's "names are short"
// convention. Without a shorthand the whole comment is the name.
//
struct Text {
    std::string name, description;
    bool empty() const { return name.empty() && description.empty(); }
};

inline Text compose(const droid::RegisterLabel* l) {
    Text t;
    if (!l) return t;
    if (!l->shorthand.empty()) {
        t.name = l->shorthand;
        t.description = l->text;
    } else {
        t.name = l->text;
    }
    return t;
}

// Tooltip for a control that hosts TWO registers: an input jack is also its
// normalization N, and a controller's button contains its LED L. The secondary
// register's label becomes an extra description line — a light widget over a
// button cap would swallow the button's clicks (ModuleLightWidget::onHover
// consumes the event whenever LightInfo is set), so the button's own tooltip is
// where its LED label has to live. With no primary label the secondary is
// promoted, so labelling only N1 still names input 1.
inline Text composePair(const droid::RegisterLabel* main,
                        const droid::RegisterLabel* extra,
                        const char* extraPrefix) {
    if (!main) return compose(extra);
    Text t = compose(main);
    if (extra) {
        std::string line = extraPrefix;
        line += extra->shorthand.empty()
              ? extra->text
              : (extra->text.empty() ? extra->shorthand
                                     : extra->shorthand + " — " + extra->text);
        if (!t.description.empty()) t.description += "\n";
        t.description += line;
    }
    return t;
}

// Write a composed label onto a port / param / light, falling back to the
// module's own default name when the patch does not label it. Rewriting the
// FULL set on every load is what stops a previous patch's labels from
// surviving into one that does not mention the register.
inline void applyPort(rack::engine::PortInfo* info, const Text& t,
                      const std::string& fallback) {
    if (!info) return;
    info->name = t.name.empty() ? fallback : t.name;
    info->description = t.description;
}

inline void applyParam(rack::engine::ParamQuantity* pq, const Text& t,
                       const std::string& fallback) {
    if (!pq) return;
    pq->name = t.name.empty() ? fallback : t.name;
    pq->description = t.description;
}

inline void applyLight(rack::engine::LightInfo* info, const Text& t,
                       const std::string& fallback) {
    if (!info) return;
    info->name = t.name.empty() ? fallback : t.name;
    info->description = t.description;
}

// ---- banks --------------------------------------------------------------
// Every module labels its controls in same-type runs mapped to consecutive
// Rack IDs, so these three cover the whole plugin. `fallbackFmt` is a printf
// format taking the 1-based register number ("P%d"), used when the patch does
// not name that register.

// `withLed` folds the matching L-register label into each control's tooltip —
// on for buttons and for the P8S8's sliders, whose LEDs are drawn inside them.
// `firstNumber` is the register number the first control carries — the S10's
// eight toggles are S3..S10, so its second bank starts at 3.
inline void applyParamBank(rack::engine::Module* m, int firstParamId, int count,
                           char type, const ModuleLabels& labels,
                           const char* fallbackFmt, bool withLed = false,
                           unsigned firstNumber = 1) {
    for (int i = 0; i < count; i++) {
        unsigned n = firstNumber + unsigned(i);
        Text t = withLed ? composePair(labels.find(type, n), labels.find('L', n),
                                       "LED: ")
                         : compose(labels.find(type, n));
        applyParam(m->paramQuantities[firstParamId + i], t,
                   rack::string::f(fallbackFmt, n));
    }
}

inline void applyPortBank(rack::engine::Module* m, rack::engine::Port::Type portType,
                          int firstPortId, int count, char type,
                          const ModuleLabels& labels, const char* fallbackFmt,
                          unsigned firstNumber = 1) {
    for (int i = 0; i < count; i++) {
        unsigned n = firstNumber + unsigned(i);
        // An input jack carries its normalization's label too (N feeds exactly
        // this jack's default).
        Text t = (type == 'I')
            ? composePair(labels.find('I', n), labels.find('N', n), "Normalization: ")
            : compose(labels.find(type, n));
        std::string fallback = rack::string::f(fallbackFmt, n);
        if (portType == rack::engine::Port::INPUT)
            applyPort(m->inputInfos[firstPortId + i], t, fallback);
        else
            applyPort(m->outputInfos[firstPortId + i], t, fallback);
    }
}

// `stride` is the number of Rack lights per physical LED (3 for RGB).
inline void applyLightBank(rack::engine::Module* m, int firstLightId, int count,
                           int stride, char type, const ModuleLabels& labels,
                           const char* fallbackFmt, unsigned firstNumber = 1) {
    for (int i = 0; i < count; i++) {
        unsigned n = firstNumber + unsigned(i);
        applyLight(m->lightInfos[firstLightId + i * stride],
                   compose(labels.find(type, n)), rack::string::f(fallbackFmt, n));
    }
}

} // namespace labels
} // namespace vcvoid
