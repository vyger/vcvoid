#pragma once

// Deferred click/turn classification for the E4/DB8E endless encoders.
// Pure logic, no Rack dependencies — unit-tested headless
// (tests/unit/test_encodergesture.cpp), consumed by DroidEndlessEncoder
// (EncoderWidgets.hpp) on the UI thread and by the E4/DB8E modules on the
// audio thread.
//
// Why deferred: on hardware the push switch and
// the rotation are physically independent, but in Rack both start with the
// same mouse press. The previous "optimistic push" published the press
// upstream immediately and cleared it when the first drag event arrived —
// but that first drag event is a UI frame (~10 ms ≈ dozens of engine ticks)
// behind, and the engine's toggle circuits fire on the RISING edge, which
// cannot be retracted. Net effect: every encoder drag also clicked the
// encoder (e.g. toggling a voice mute while dragging a transposition).
//
// So the press is withheld until the gesture declares itself:
//   - travel >= kTurnPx            -> a TURN: never a press; the withheld
//     pre-threshold travel is replayed so no detents are lost;
//   - release before either commit -> a CLICK: a synthetic kPulseSeconds
//     press pulse starting at release;
//   - still hold >= kHoldSeconds   -> a real PUSH level (patches use the
//     button as a hold level, manual/circuits/encoder.md), which then
//     survives turning — hardware push+turn keeps working.
// Trade-offs, accepted: a click's engine-side press lands at mouse-up with a
// fixed synthetic duration, and a deliberate hold's level starts kHoldSeconds
// late (DROID's 1.5 s longpress gestures therefore need the extra
// kHoldSeconds of holding).
//
// Threading: press/move/release run on the UI thread; step/level on the
// audio thread. Same single-writer-ish plain-field tolerance as the detent
// counter this rides next to — a torn read costs one frame of push level at
// worst, never a spurious toggle (the level can only rise via step()'s
// Pending-hold commit or release()'s pulse, both single transitions).

namespace vcvoid {

struct EncoderGesture {
    static constexpr float kTurnPx = 3.f;        // travel that classifies a turn
    static constexpr float kHoldSeconds = 0.3f;  // still-hold that commits a push
    static constexpr float kPulseSeconds = 0.05f;// synthetic click press length

    enum class Phase { Idle, Pending, Turn, Push };
    Phase phase = Phase::Idle;

    float heldSeconds = 0.f;   // time in Pending (audio thread, step())
    float travelPx = 0.f;      // |dy| accumulated in Pending
    float pendingDy = 0.f;     // signed dy withheld in Pending, replayed on Turn
    bool  held = false;        // committed push level
    float pulseLeft = 0.f;     // remaining synthetic click pulse

    // UI thread: left mouse press on the encoder.
    void press() {
        phase = Phase::Pending;
        heldSeconds = 0.f;
        travelPx = 0.f;
        pendingDy = 0.f;
    }

    // UI thread: drag movement (dy in px, + = increase). Returns the dy the
    // caller may turn into detents this event — 0 while the gesture is still
    // undeclared, the full withheld travel on the event that classifies it.
    float move(float dy) {
        switch (phase) {
            case Phase::Pending:
                pendingDy += dy;
                travelPx += (dy < 0.f ? -dy : dy);
                if (travelPx >= kTurnPx) {
                    phase = Phase::Turn;
                    float replay = pendingDy;
                    pendingDy = 0.f;
                    return replay;
                }
                return 0.f;
            case Phase::Turn:
            case Phase::Push:   // push+turn: detents count while held
                return dy;
            case Phase::Idle:
            default:
                return 0.f;
        }
    }

    // UI thread: mouse release (or DragEnd — call both; the second is a no-op
    // since only a Pending gesture pulses and the first call leaves Idle).
    void release() {
        if (phase == Phase::Pending) pulseLeft = kPulseSeconds;   // a click
        held = false;
        phase = Phase::Idle;
    }

    // Audio thread: advance time (call every process frame).
    void step(float dt) {
        if (phase == Phase::Pending) {
            heldSeconds += dt;
            if (heldSeconds >= kHoldSeconds) {
                phase = Phase::Push;   // deliberate hold: publish the level
                held = true;
            }
        }
        if (pulseLeft > 0.f) pulseLeft -= dt;
    }

    // The push level to publish upstream this frame.
    bool level() const { return held || pulseLeft > 0.f; }
};

} // namespace vcvoid
