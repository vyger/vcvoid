#pragma once
#include "plugin.hpp"

// Build identity stamped in by plugin/Makefile at compile time: the short
// commit hash, plus the tag the build is exactly at or the branch it was
// built from (empty for a detached HEAD that isn't at a tag).
#ifndef VCVOID_GIT_HASH
#define VCVOID_GIT_HASH "unknown"
#endif
#ifndef VCVOID_GIT_REF
#define VCVOID_GIT_REF ""
#endif

// "build: a95b302 (main)" / "build: a95b302 (v2.0.0)" / "build: a95b302"
inline std::string vcvoidBuildString() {
    std::string s = std::string("build: ") + VCVOID_GIT_HASH;
    if (VCVOID_GIT_REF[0])
        s += std::string(" (") + VCVOID_GIT_REF + ")";
    return s;
}

// Every vcvoid module's context menu ends with this line.
inline void appendBuildInfoMenu(rack::ui::Menu* menu) {
    menu->addChild(new MenuSeparator);
    menu->addChild(createMenuLabel(vcvoidBuildString()));
}
