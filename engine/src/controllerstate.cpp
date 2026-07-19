#include "controllerstate.hpp"
#include "controllers.hpp"

namespace droid {

void ControllerState::configure(const std::vector<std::string>& controllerModels) {
    slots_.clear();
    int faderCount = 0;
    int displayCount = 0;
    for (size_t i = 0; i < controllerModels.size(); i++) {
        const ControllerModel* m = findControllerModel(controllerModels[i]);
        if (!m) continue;
        for (uint8_t n = 1; n <= m->encoders; n++)
            slots_.push_back(Slot{uint8_t(i + 1), n});
        faderCount += m->faders;   // 4 per M4, in chain order
        if (controllerModels[i] == "db8e") displayCount++;
    }
    encoders_.assign(slots_.size(), EncoderState{});
    faders_.assign(faderCount, FaderState{});
    displays_.assign(displayCount, DisplayState{});
}

int ControllerState::globalIndexForReg(const RegId& r) const {
    if (r.type != 'E' || r.ctrl == 0) return 0;
    for (size_t i = 0; i < slots_.size(); i++)
        if (slots_[i].ctrl == r.ctrl && slots_[i].num == r.num) return int(i + 1);
    return 0;
}

int ControllerState::validateGlobal(int global1) const {
    return (global1 >= 1 && global1 <= (int)slots_.size()) ? global1 : 0;
}

RegId ControllerState::regForGlobal(int global1) const {
    if (global1 < 1 || global1 > (int)slots_.size()) return RegId{};
    return RegId{'E', slots_[global1 - 1].ctrl, slots_[global1 - 1].num};
}

int ControllerState::resolve(const RegId& r) const {
    if (r.type != 'E') return 0;
    if (r.ctrl == 0) return validateGlobal(r.num);   // E<n>: direct global index
    return globalIndexForReg(r);                     // E<ctrl>.<num>: chain lookup
}

void ControllerState::turnEncoder(int global1, long detents) {
    if (EncoderState* e = encoder(global1)) e->pendingDetents += detents;
}

void ControllerState::pushEncoder(int global1, bool down) {
    if (EncoderState* e = encoder(global1)) e->pushed = down;
}

bool ControllerState::turnEncoder(const RegId& r, long detents) {
    int g = resolve(r);
    if (!g) return false;
    turnEncoder(g, detents);
    return true;
}

bool ControllerState::pushEncoder(const RegId& r, bool down) {
    int g = resolve(r);
    if (!g) return false;
    pushEncoder(g, down);
    return true;
}

EncoderState* ControllerState::encoder(int global1) {
    if (global1 < 1 || global1 > (int)encoders_.size()) return nullptr;
    return &encoders_[global1 - 1];
}

const EncoderState* ControllerState::encoder(int global1) const {
    if (global1 < 1 || global1 > (int)encoders_.size()) return nullptr;
    return &encoders_[global1 - 1];
}

bool ControllerState::validateFader(int fader1) const {
    return fader1 >= 1 && fader1 <= (int)faders_.size();
}

void ControllerState::moveFader(int fader1, float pos) {
    if (FaderState* f = fader(fader1)) {
        if (pos < 0.0f) pos = 0.0f; else if (pos > 1.0f) pos = 1.0f;
        f->position = pos;
    }
}

void ControllerState::touchFader(int fader1, bool down) {
    if (FaderState* f = fader(fader1)) f->touched = down;
}

void ControllerState::pressFaderPlate(int fader1, bool down) {
    if (FaderState* f = fader(fader1)) f->plate = down;
}

FaderState* ControllerState::fader(int fader1) {
    if (fader1 < 1 || fader1 > (int)faders_.size()) return nullptr;
    return &faders_[fader1 - 1];
}

const FaderState* ControllerState::fader(int fader1) const {
    if (fader1 < 1 || fader1 > (int)faders_.size()) return nullptr;
    return &faders_[fader1 - 1];
}

float ControllerState::faderPosition(int fader1) const {
    const FaderState* f = fader(fader1);
    return f ? f->position : 0.0f;
}

void ControllerState::commandFader(int fader1, float pos) {
    if (FaderState* f = fader(fader1)) {
        if (pos < 0.0f) pos = 0.0f; else if (pos > 1.0f) pos = 1.0f;
        f->motorTarget = pos;
        f->position = pos;   // instant motor (SPEC-GAP: no speed spec -> snap)
    }
}

DisplayState* ControllerState::display(int db8e1) {
    if (db8e1 < 1 || db8e1 > (int)displays_.size()) return nullptr;
    return &displays_[db8e1 - 1];
}

const DisplayState* ControllerState::display(int db8e1) const {
    if (db8e1 < 1 || db8e1 > (int)displays_.size()) return nullptr;
    return &displays_[db8e1 - 1];
}

void ControllerState::beginTick() {
    // Ring ownership is re-established every tick by whichever circuit is
    // selected; clearing here (not endTick) lets the master read the image
    // after tick() returns (issue #15).
    for (auto& e : encoders_) e.ring = RingDisplay{};
}

void ControllerState::endTick() {
    for (auto& e : encoders_) e.pendingDetents = 0;
}

} // namespace droid
