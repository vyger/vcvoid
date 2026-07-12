#include "harness.hpp"
#include "src/engine.hpp"
#include <cmath>
#include <limits>
using namespace droid;

TEST(engine_copy_basic) {
    Engine e;
    auto r = e.load("[copy]\n input = I1 * 0.5 + 0.1\n output = O1\n");
    CHECK(r.ok);
    e.setValue("I1", 0.4f);
    e.tick();
    CHECK_NEAR(e.getValue("O1"), 0.3, 1e-6);
    CHECK(e.tickCount() == 1);
}

TEST(engine_defaults_when_unconnected) {
    Engine e;
    CHECK(e.load("[copy]\n output = O1\n").ok);    // input left unpatched -> default 0
    e.tick();
    CHECK_NEAR(e.getValue("O1"), 0.0, 1e-9);
}

TEST(engine_ordering_same_tick) {
    Engine e;
    CHECK(e.load("[copy]\n input = I1\n output = _X\n"
                 "[copy]\n input = _X\n output = O1\n").ok);
    e.setValue("I1", 0.5f);
    e.tick();
    CHECK_NEAR(e.getValue("O1"), 0.5, 1e-7);       // writer earlier: same tick
}

TEST(engine_ordering_next_tick) {
    Engine e;
    CHECK(e.load("[copy]\n input = _X\n output = O1\n"
                 "[copy]\n input = I1\n output = _X\n").ok);
    e.setValue("I1", 0.5f);
    e.tick();
    CHECK_NEAR(e.getValue("O1"), 0.0, 1e-7);       // writer later: one-tick delay
    e.tick();
    CHECK_NEAR(e.getValue("O1"), 0.5, 1e-7);
}

TEST(engine_normalization) {
    Engine e;
    CHECK(e.load("[copy]\n input = 0.3\n output = N1\n"
                 "[copy]\n input = I1\n output = O1\n").ok);
    e.tick();
    CHECK_NEAR(e.getValue("O1"), 0.3, 1e-7);       // I1 unpatched -> reads N1
    e.setValue("I1", 0.9f);
    e.tick();
    CHECK_NEAR(e.getValue("O1"), 0.9, 1e-7);
    e.setInputPatched(1, false);
    e.tick();
    CHECK_NEAR(e.getValue("O1"), 0.3, 1e-7);
}

TEST(engine_output_clamp) {
    Engine e;
    CHECK(e.load("[copy]\n input = I1 * 3\n output = O1\n").ok);
    e.setValue("I1", 0.5f);
    e.tick();
    CHECK_NEAR(e.getValue("O1"), 1.0, 1e-7);       // 1.5 clamps to 1.0 (10 V)
}

TEST(engine_seed_survives_load) {
    Engine e(MasterType::Master16, 6000.0f, 42);
    CHECK(e.rngState() == 42);
    CHECK(e.load("[copy]\n input = I1\n output = O1\n").ok);
    CHECK(e.rngState() == 42);                     // load must not reset the seed
    Engine z(MasterType::Master16, 6000.0f, 0);    // seed 0 normalizes to 1 (xorshift32)
    CHECK(z.load("[copy]\n output = O1\n").ok);
    CHECK(z.rngState() == 1);
}

TEST(engine_tickrate_in_state) {
    // The configured tick rate must be visible to circuits via EngineState,
    // and must survive load() (which resets the rest of the state).
    Engine e(MasterType::Master16, 48000.0f, 1);
    CHECK_NEAR(e.tickRateHz(), 48000.0, 1e-3);
    CHECK(e.load("[copy]\n input = I1\n output = O1\n").ok);
    CHECK_NEAR(e.tickRateHz(), 48000.0, 1e-3);   // accessor unchanged by load
    Engine d;                                    // default 6000 Hz
    CHECK_NEAR(d.tickRateHz(), 6000.0, 1e-3);
}

TEST(engine_unimplemented_circuit) {
    // sinfonionlink is status: not-feasible in circuits-status.yaml (it
    // bridges to physical Sinfonion hardware over a dedicated cable -- nothing
    // to emulate), so it is the PERMANENT unimplemented placeholder: every
    // feasible circuit has landed. (flipflop, midiin, superjust, then
    // calibrator previously served this role.)
    Engine m18(MasterType::Master18);   // sinfonionlink is MASTER18-only
    auto r = m18.load("[sinfonionlink]\n root = O1\n");   // valid patch, no impl
    CHECK(!r.ok);
    bool found = false;
    for (auto& err : r.errors)
        if (err.message.find("not yet implemented") != std::string::npos) found = true;
    CHECK(found);
}

TEST(engine_peer_access_chain) {
    // Peer access (Circuit::peers_/nextPeer, wired in Engine::load) lets the
    // sequencer's chaintonext reach the following sequencer's step inputs. Two
    // chained instances form one 4-step sequence; steps 3 & 4 come from the
    // second instance, proving the peer wiring is live.
    Engine e;
    CHECK(e.load(
        "[sequencer]\n clock = I1\n pitchoutput = O1\n"
        " pitch1 = 0.10\n pitch2 = 0.20\n chaintonext = 1\n"
        "[sequencer]\n pitch1 = 0.30\n pitch2 = 0.40\n").ok);
    auto step = [&](float pitch) {
        e.setValue("I1", 1.0f); e.tick();
        CHECK_NEAR(e.getValue("O1"), pitch, 1e-5);
        e.setValue("I1", 0.0f); e.tick();
    };
    step(0.10f); step(0.20f); step(0.30f); step(0.40f); step(0.10f);
}

TEST(engine_controller_seams) {
    Engine e;
    auto r = e.load(
        "[p2b8]\n"
        "[copy]\n"
        "    input = P1.2\n"
        "    output = L1.1\n");
    CHECK(r.ok);
    CHECK(e.declaredControllers().size() == 1);
    CHECK(e.declaredControllers()[0] == "p2b8");

    RegId p12{'P', 1, 2}, l11{'L', 1, 1}, l12{'L', 1, 2}, b11{'B', 1, 1};
    CHECK(e.registerDriven(l11));        // copy output binds L1.1
    CHECK(!e.registerDriven(l12));
    CHECK(!e.registerDriven(b11));
    CHECK(!e.registerDriven(p12));       // P1.2 is read, not driven

    e.setRegister(p12, 0.42f);
    CHECK_NEAR(e.getRegister(p12), 0.42, 1e-6);
    e.tick();
    CHECK_NEAR(e.getRegister(l11), 0.42, 1e-6);   // copy ran off the fast-path write

    // load() resets the seams
    auto r2 = e.load("[lfo]\n    hz = 1\n    square = O1\n");
    CHECK(r2.ok);
    CHECK(e.declaredControllers().empty());
    CHECK(!e.registerDriven(l11));
}

TEST(engine_r_register_led_override) {
    // R registers R1..R56 drive the master/G8/X7 matrix LEDs (manual §5.5). A
    // patch that writes one marks it driven; the plugin reads getRegister and
    // shows the DROID colour value instead of the LED's default function. Cover
    // both a master LED (R1) and the first G8's first LED (R17 = 16 + 1).
    Engine e;
    CHECK(e.load("[copy]\n input = 0.4\n output = R1\n"
                 "[copy]\n input = 0.8\n output = R17\n").ok);
    RegId r1{'R', 0, 1}, r17{'R', 0, 17}, r2{'R', 0, 2};
    CHECK(e.registerDriven(r1));
    CHECK(e.registerDriven(r17));
    CHECK(!e.registerDriven(r2));       // an untouched LED keeps its jack mirror
    e.tick();
    CHECK_NEAR(e.getRegister(r1), 0.4, 1e-6);
    CHECK_NEAR(e.getRegister(r17), 0.8, 1e-6);
}

TEST(engine_wave3_readbacks) {
    Engine e;
    auto r = e.load(
        "[e4]\n"
        "[m4]\n"
        "[encoder]\n"
        "    encoder = E1.1\n"
        "    output = O1\n"
        "[motorfader]\n"
        "    fader = 1\n"
        "    notches = 8\n"
        "    output = O2\n");
    CHECK(r.ok);
    // out-of-range is 0, never UB (the load-bearing bounds-safety of the seam)
    CHECK(e.encoderRing(0) == 0.0f);
    CHECK(e.encoderRing(99) == 0.0f);
    CHECK(e.faderMotorTarget(99) == 0.0f);
    CHECK(e.faderNotches(99) == 0);
    // ring follows the encoder's virtual position. Default normal mode, sensivity
    // 1 -> 96 detents per full 0..1 sweep, so +48 detents -> pos 0.5 (pinned by
    // tests/golden/encoder/basic-normal.gold). ringDisplay = clampf(pos) = 0.5.
    e.turnEncoder("E1.1", 48);           // 48/96 detents = half turn
    e.tick();
    CHECK_NEAR(e.encoderRing(1), 0.5, 1e-3);
    // notches config surfaces after the owning circuit runs (set during tick above).
    CHECK(e.faderNotches(1) == 8);
    // motorTarget tracks position via the CIRCUIT command, not the raw user move:
    // moveFader (user) writes only FaderState.position; the selected motorfader
    // circuit then reads that position, snaps it, and commandFader() sets
    // position == motorTarget in one call. So after the tick they are equal.
    e.moveFader("F1", 0.5f);
    e.tick();
    CHECK_NEAR(e.faderMotorTarget(1), e.getValue("F1"), 1e-6);  // snap model
}

TEST(engine_fader_led_seams) {
    // Verify-first (motorfader.cpp): the LED inputs are named `ledvalue` /
    // `ledcolor` (NOT `led`), and they only write FaderState.led/ledColor while
    // the circuit is SELECTED — default selected=true here since neither
    // `select` nor `selectat` is patched. ledvalue >= 0 passes straight through
    // to FaderState.led (see Motorfader::ledBrightness); ledcolor always passes
    // straight through to FaderState.ledColor.
    Engine e;
    auto r = e.load(
        "[m4]\n"
        "[motorfader]\n"
        "    fader = 1\n"
        "    ledvalue = 1\n"
        "    ledcolor = 0.5\n");
    CHECK(r.ok);
    CHECK(e.faderLed(0) == 0.0f);      // out of range (1-based)
    CHECK(e.faderLed(99) == 0.0f);
    CHECK(e.faderLedColor(99) == 0.0f);
    e.tick();
    CHECK_NEAR(e.faderLed(1), 1.0f, 1e-3f);
    CHECK_NEAR(e.faderLedColor(1), 0.5f, 1e-3f);
}

// motoquencer step LEDs (motoquencer.md "LED colors"): blue = enabled gate
// (buttonmode 0), white (negative sentinel) = currently played step, always.
TEST(engine_motoquencer_step_leds) {
    Engine e;
    auto r = e.load(
        "[m4]\n"
        "[motoquencer]\n"
        "    clock = I1\n"
        "    numsteps = 4\n"
        "    defaultgate = 1\n"
        "    cv = O1\n"
        "    gate = O2\n");
    CHECK(r.ok);
    e.tick();
    // Not yet clocked: no played step; all four default gates steady blue (1.2).
    for (int f = 1; f <= 4; f++) {
        CHECK_NEAR(e.faderLed(f), 1.0f, 1e-6f);
        CHECK_NEAR(e.faderLedColor(f), 1.2f, 1e-6f);
    }
    e.setValue("I1", 1.0f); e.tick();          // first clock -> step 1 plays
    CHECK_NEAR(e.faderLed(1), 1.0f, 1e-6f);
    CHECK(e.faderLedColor(1) < 0.0f);          // white sentinel
    CHECK_NEAR(e.faderLedColor(2), 1.2f, 1e-6f);
    e.setValue("I1", 0.0f); e.tick();
    e.setValue("I1", 1.0f); e.tick();          // second clock -> step 2 plays
    CHECK_NEAR(e.faderLedColor(1), 1.2f, 1e-6f);   // back to gate blue
    CHECK(e.faderLedColor(2) < 0.0f);
}

// buttonmode 1: green = start step, red = end step; other lanes dark.
TEST(engine_motoquencer_leds_startend) {
    Engine e;
    auto r = e.load(
        "[m4]\n"
        "[motoquencer]\n"
        "    clock = I1\n"
        "    numsteps = 4\n"
        "    defaultgate = 1\n"
        "    buttonmode = 1\n"
        "    startstep = 2\n"
        "    endstep = 3\n"
        "    cv = O1\n");
    CHECK(r.ok);
    e.tick();
    CHECK_NEAR(e.faderLed(1), 0.0f, 1e-6f);
    CHECK_NEAR(e.faderLedColor(2), 0.4f, 1e-6f);   // green
    CHECK_NEAR(e.faderLedColor(3), 0.8f, 1e-6f);   // red
    CHECK_NEAR(e.faderLed(4), 0.0f, 1e-6f);
}

// A deselected circuit releases the LEDs: cleared once on the falling edge so
// another overlaid circuit can drive them (manual `select` semantics).
TEST(engine_motoquencer_leds_select_release) {
    Engine e;
    auto r = e.load(
        "[m4]\n"
        "[motoquencer]\n"
        "    clock = I1\n"
        "    select = I2\n"
        "    numsteps = 4\n"
        "    defaultgate = 1\n"
        "    cv = O1\n");
    CHECK(r.ok);
    e.setValue("I2", 1.0f); e.tick();
    CHECK_NEAR(e.faderLed(1), 1.0f, 1e-6f);        // selected: gate blue lit
    e.setValue("I2", 0.0f); e.tick();
    CHECK_NEAR(e.faderLed(1), 0.0f, 1e-6f);        // deselected: released dark
}

// encoquencer mirrors the same LED semantics onto the E4's step LED (the
// middle-three ring cells; encoquencer.md: "the same function as the touch
// button's LED in the M4").
TEST(engine_encoquencer_step_leds) {
    Engine e;
    auto r = e.load(
        "[e4]\n"
        "[encoquencer]\n"
        "    clock = I1\n"
        "    numsteps = 4\n"
        "    defaultgate = 1\n"
        "    cv = O1\n"
        "    gate = O2\n");
    CHECK(r.ok);
    CHECK(e.encoderStepLed(0) == 0.0f);            // bounds-safe seam
    CHECK(e.encoderStepLed(99) == 0.0f);
    CHECK(e.encoderStepLedColor(99) == 0.0f);
    e.setValue("I1", 1.0f); e.tick();              // first clock -> step 1 plays
    CHECK_NEAR(e.encoderStepLed(1), 1.0f, 1e-6f);
    CHECK(e.encoderStepLedColor(1) < 0.0f);        // white sentinel
    CHECK_NEAR(e.encoderStepLedColor(2), 1.2f, 1e-6f);   // gate blue
}


TEST(engine_text_table) {
    // text is an ordinary number on any jack (Forge parity: droidcheck accepts a
    // quoted string on a copy input), so exercise Engine::texts() live against the
    // copy circuit (the display circuit's text handling is covered by its goldens).
    Engine e;
    auto r = e.load(
        "[copy]\n"
        "    input = \"BPM\"\n"
        "    output = O1\n");
    CHECK(r.ok);
    CHECK(e.texts().size() == 2);            // slot 0 + one unique text
    CHECK(e.textForNumber(1.0f) == "BPM");
    CHECK(e.textForNumber(1.7f) == "BPM");   // floor
    CHECK(e.textForNumber(0.0f) == "");      // 0 = empty text
    CHECK(e.textForNumber(-3.0f) == "");
    CHECK(e.textForNumber(99.0f) == "");
    // Finite-but-out-of-range floats must be range-checked AS FLOAT before any
    // cast (casting 1e20 to a narrow int is UB). Reachable via input math (= I1 * 1e20).
    CHECK(e.textForNumber(1e20f) == "");     // huge positive: no such text, not UB
    CHECK(e.textForNumber(-1e20f) == "");    // huge negative
    CHECK(e.textForNumber(std::numeric_limits<float>::infinity()) == "");
    CHECK(e.textForNumber(-std::numeric_limits<float>::infinity()) == "");
    CHECK(e.textForNumber(std::nanf("")) == "");
}

TEST(engine_display_same_tick_tiebreak) {
    // Two [display] circuits change on the same tick. The DisplayState.lastWriteTick
    // seam (Task 6) lets the later circuit's accepted write land last even though
    // the earlier one just installed a linger window. Cross-tick, linger still
    // applies. Both target DB8E #1 (default display=1).
    Engine e(MasterType::Master16, 100.0f);
    auto r = e.load(
        "[db8e]\n"
        "[display]\n"
        "    value = I1\n"
        "[display]\n"
        "    value = I2\n");
    CHECK(r.ok);
    e.setValue("I1", 0.3f);
    e.setValue("I2", 0.6f);
    e.tick();
    const DisplayState* d = e.displayState(1);
    CHECK(d != nullptr);
    CHECK(d->active);
    CHECK_NEAR(d->value, 0.6, 1e-6);          // later circuit (I2) wins the tick
    e.tick();
    CHECK_NEAR(d->value, 0.6, 1e-6);          // held (no change)
}

// MASTER18 frequency-probe seam: the adapter publishes hz/present into
// EngineState; vcotuner reads s.probe. Defaults: no signal. Survives nothing —
// load() resets it, and the adapter republishes every tick frame.
TEST(engine_probe_seam) {
    Engine e(MasterType::Master18);
    e.load("[copy]\n input = I1\n output = O1\n");
    e.setProbe(440.0f, true);   // state-only seam; vcotuner goldens exercise the readout
}

// midifileplayer: DROID's 6000-byte track cap -> error 0.25 (cyan). Built
// programmatically (too big for a golden hex fixture): a valid SMF whose one
// track chunk exceeds 6000 bytes of note events.
TEST(engine_midifileplayer_track_too_long) {
    std::vector<uint8_t> trk;
    while (trk.size() < 6400) {                        // 800 note-on/off pairs
        uint8_t on[] = {0x00, 0x90, 0x3c, 0x64}, off[] = {0x01, 0x80, 0x3c, 0x00};
        trk.insert(trk.end(), on, on + 4);
        trk.insert(trk.end(), off, off + 4);
    }
    uint8_t eot[] = {0x00, 0xff, 0x2f, 0x00};
    trk.insert(trk.end(), eot, eot + 4);
    std::vector<uint8_t> file = {'M','T','h','d', 0,0,0,6, 0,0, 0,1, 0,96,
                                 'M','T','r','k'};
    uint32_t n = (uint32_t)trk.size();
    file.push_back(n >> 24); file.push_back(n >> 16); file.push_back(n >> 8); file.push_back(n);
    file.insert(file.end(), trk.begin(), trk.end());

    Engine e;
    e.setFileProvider([file](int num, std::vector<uint8_t>& out) {
        if (num != 1) return false;
        out = file;
        return true;
    });
    e.load("[midifileplayer]\n error = O1\n gate = O2\n");
    e.tick();
    CHECK(std::fabs(e.getValue("O1") - 0.25f) < 1e-6);
    CHECK(e.getValue("O2") == 0.0f);
}

TEST(engine_droid_ledbrightness) {
    // [droid] ledbrightness is a render hint for the Rack adapter; L registers
    // stay unscaled (patches read LEDs back as logic values).
    Engine e;
    CHECK(e.load("[droid]\n ledbrightness = 0.4\n").ok);
    e.tick();
    CHECK_NEAR(e.ledBrightness(), 0.4, 1e-6);
}
