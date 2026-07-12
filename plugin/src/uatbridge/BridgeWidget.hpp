#pragma once
// Invisible scene widget: drains the bridge UI queue once per frame.
#include <rack.hpp>
#include "Bridge.hpp"

using namespace rack;

namespace uat {

struct BridgeWidget : rack::widget::TransparentWidget {
    void step() override {
        if (Bridge* b = Bridge::instance()) {
            b->drainUi();
            b->expireHolds();
        }
        rack::widget::TransparentWidget::step();
    }
};

inline void installBridgeWidget() {   // UI thread only; idempotent
    Bridge* b = Bridge::instance();
    if (!b || b->uiAttached() || !APP || !APP->scene) return;
    b->noteContext(APP);   // before setUiAttached: the /params gate relies on
                           // uiAttached implying the HTTP thread has a context
    APP->scene->addChild(new BridgeWidget());
    b->setUiAttached();
}

} // namespace uat
