#pragma once
#include "plugin.hpp"

// Build identity stamped in by plugin/Makefile at compile time: the short
// commit hash, plus the tag the build is exactly at or the branch it was
// built from (empty for a detached HEAD that isn't at a tag).
#ifndef VOIDBOT_GIT_HASH
#define VOIDBOT_GIT_HASH "unknown"
#endif
#ifndef VOIDBOT_GIT_REF
#define VOIDBOT_GIT_REF ""
#endif

// "build: a95b302 (main)" / "build: a95b302 (v2.0.0)" / "build: a95b302"
inline std::string voidbotBuildString() {
    std::string s = std::string("build: ") + VOIDBOT_GIT_HASH;
    if (VOIDBOT_GIT_REF[0])
        s += std::string(" (") + VOIDBOT_GIT_REF + ")";
    return s;
}

// Every voidbot module's context menu ends with this line.
inline void appendBuildInfoMenu(rack::ui::Menu* menu) {
    menu->addChild(new MenuSeparator);
    menu->addChild(createMenuLabel(voidbotBuildString()));
}
