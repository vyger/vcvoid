#include "harness.hpp"
#include "EncoderGesture.hpp"
using vcvoid::EncoderGesture;

// Deferred click/turn classification for the E4/DB8E encoders: a drag
// necessarily begins with a mouse press, and the old
// "optimistic push" published that press upstream immediately — the engine's
// toggle circuits fired on the rising edge ~10 ms before the first drag event
// could reclassify the gesture as a turn, so every turn also clicked (e.g.
// muting a voice when dragging a transposition encoder). The classifier holds
// the press back until it knows what the gesture is:
//   - release before kHoldSeconds with < kTurnPx of travel = a click -> a
//     synthetic kPulseSeconds press pulse AFTER release;
//   - >= kTurnPx of travel = a turn -> never a press, and the pre-threshold
//     travel is replayed so no detents are lost;
//   - a still hold >= kHoldSeconds = a real push level (long-hold patch
//     semantics), which then survives turning (hardware push+turn).

// Advance time in engine-tick-sized steps, returning how long the level was
// high. dtPerStep mimics a ~5 kHz tick.
static float stepFor(EncoderGesture& g, float seconds, float dt = 0.0002f) {
    float high = 0.f;
    for (float t = 0.f; t < seconds; t += dt) {
        g.step(dt);
        if (g.level()) high += dt;
    }
    return high;
}

TEST(gesture_quick_click_pulses_after_release_only) {
    EncoderGesture g;
    g.press();
    CHECK(!g.level());                       // press alone publishes nothing
    stepFor(g, 0.1f);
    CHECK(!g.level());                       // still pending, still nothing
    g.release();
    CHECK(g.level());                        // click -> synthetic pulse begins
    float high = stepFor(g, 1.f);
    CHECK(!g.level());                       // pulse expired
    CHECK_NEAR(high, EncoderGesture::kPulseSeconds, 0.001f);
}

TEST(gesture_turn_never_presses_and_replays_pre_threshold_travel) {
    EncoderGesture g;
    g.press();
    // 1 px per UI frame: below threshold at first, classified once cumulative
    // travel crosses kTurnPx. The pre-threshold pixels must come back on the
    // classifying event so no drag distance is lost.
    float out = 0.f;
    for (int i = 0; i < 10; i++) {
        out += g.move(1.f);
        g.step(0.016f);
        CHECK(!g.level());                   // a turn never publishes a press
    }
    CHECK_NEAR(out, 10.f, 1e-6f);
    g.release();
    CHECK(!g.level());                       // and no click pulse either
    stepFor(g, 0.5f);
    CHECK(!g.level());
}

TEST(gesture_turn_replay_keeps_sign) {
    EncoderGesture g;
    g.press();
    float out = g.move(-2.f);                // below threshold: withheld
    CHECK_NEAR(out, 0.f, 1e-6f);
    out = g.move(-2.f);                      // crosses threshold: replay all -4
    CHECK_NEAR(out, -4.f, 1e-6f);
    g.release();
    CHECK(!g.level());
}

TEST(gesture_still_hold_becomes_real_push_level) {
    EncoderGesture g;
    g.press();
    stepFor(g, EncoderGesture::kHoldSeconds + 0.05f);
    CHECK(g.level());                        // committed push: level while held
    stepFor(g, 1.f);
    CHECK(g.level());                        // stays high as long as held
    g.release();
    CHECK(!g.level());                       // and drops on release, no pulse
    stepFor(g, 0.5f);
    CHECK(!g.level());
}

TEST(gesture_push_survives_turning) {
    EncoderGesture g;
    g.press();
    stepFor(g, EncoderGesture::kHoldSeconds + 0.05f);
    CHECK(g.level());
    float out = g.move(6.f);                 // hardware push+turn
    CHECK_NEAR(out, 6.f, 1e-6f);
    CHECK(g.level());                        // turning must NOT cancel the push
    g.release();
    CHECK(!g.level());
}

TEST(gesture_jitter_below_threshold_still_commits_push) {
    EncoderGesture g;
    g.press();
    g.move(1.f);                             // sub-threshold wobble while pushing
    g.move(-1.f);
    stepFor(g, EncoderGesture::kHoldSeconds + 0.05f);
    CHECK(g.level());
    g.release();
    CHECK(!g.level());
}

TEST(gesture_double_release_is_harmless) {
    EncoderGesture g;
    g.press();
    g.release();                             // onButton RELEASE ...
    float high = stepFor(g, 1.f);
    g.release();                             // ... then onDragEnd fires too
    CHECK(!g.level());                       // second release must not re-pulse
    CHECK_NEAR(high, EncoderGesture::kPulseSeconds, 0.001f);
}

TEST(gesture_click_pulse_outlives_an_engine_tick) {
    // The pulse must span comfortably many ticks at the slowest tick rate
    // (2 kHz -> 0.5 ms per tick) so the master's diff + the engine's edge
    // detection can't miss it across the 1-frame-per-hop chain latency.
    CHECK(EncoderGesture::kPulseSeconds >= 0.02f);
    // And the hold threshold must stay well under DROID's 1.5 s longpress so
    // held gestures still read as holds without absurd extra waiting.
    CHECK(EncoderGesture::kHoldSeconds <= 0.5f);
}
