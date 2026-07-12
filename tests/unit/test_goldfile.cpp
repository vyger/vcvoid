#include "harness.hpp"
// Pull the golden-file parser (header-free of main) straight into the unit
// binary so the turn/push grammar + validation is testable without the runner.
#include "runner/goldfile.cpp"
// midinames.cpp defines the symbolic MIDI event table (midiEventFromWords /
// midiEventToString) that goldfile.cpp calls; pull it in the same way so the
// unit binary links without adding runner sources to the Makefile's UNIT_SRC.
#include "runner/midinames.cpp"
#include <cstdio>
#include <fstream>
#include <string>

namespace gold = droid::gold;

static gold::GoldFile parseText(const std::string& body) {
    // Fixed scratch path (the unit binary is single-threaded); rewritten fresh
    // per call. build/ is created by the Makefile before the binary runs.
    const char* path = "build/.gold_unit_test.tmp";
    { std::ofstream f(path); f << body; }
    gold::GoldFile g = gold::parse(path);
    std::remove(path);
    return g;
}

static const char* kPatch =
    "patch <<<\n[e4]\n[encoder]\n  encoder = E1.1\n  output = O1\n>>>\n";

TEST(gold_turn_push_ok) {
    gold::GoldFile g = parseText(std::string(kPatch) +
        "@0 turn E1.1 -3\n@0 push E1.1 1\n@1 expect O1 0\n");
    CHECK(g.parseError.empty());
    // events sorted: turn, push (tick 0), expect (tick 1)
    CHECK(g.events.size() == 3);
    CHECK(g.events[0].kind == gold::Event::Kind::Turn);
    CHECK(g.events[0].value == -3.0f);
    CHECK(g.events[1].kind == gold::Event::Kind::Push);
    CHECK(g.events[1].value == 1.0f);
}

TEST(gold_turn_bare_global_ok) {
    gold::GoldFile g = parseText(std::string(kPatch) + "@0 turn E3 2\n");
    CHECK(g.parseError.empty());
    CHECK(g.events.size() == 1);
    CHECK(g.events[0].kind == gold::Event::Kind::Turn);
}

TEST(gold_turn_on_non_encoder_rejected) {
    gold::GoldFile g = parseText(std::string(kPatch) + "@0 turn P1.1 3\n");
    CHECK(!g.parseError.empty());
    CHECK(g.parseError.find("not an encoder register") != std::string::npos);
}

TEST(gold_push_on_cable_rejected) {
    gold::GoldFile g = parseText(std::string(kPatch) + "@0 push _FOO 1\n");
    CHECK(!g.parseError.empty());
}

TEST(gold_turn_after_expect_rejected) {
    // mutations (incl. turn) may not follow an expect within a tick
    gold::GoldFile g = parseText(std::string(kPatch) +
        "@0 expect O1 0\n@0 turn E1.1 1\n");
    CHECK(!g.parseError.empty());
    CHECK(g.parseError.find("after expect") != std::string::npos);
}

static const char* kFaderPatch =
    "patch <<<\n[m4]\n[motorfader]\n  fader = 1\n  output = O1\n>>>\n";

TEST(gold_move_touch_ok) {
    gold::GoldFile g = parseText(std::string(kFaderPatch) +
        "@0 move F2 0.35\n@0 touch F2 1\n@1 expect F2 0.35\n");
    CHECK(g.parseError.empty());
    CHECK(g.events.size() == 3);
    CHECK(g.events[0].kind == gold::Event::Kind::Move);
    CHECK(g.events[0].value == 0.35f);
    CHECK(g.events[1].kind == gold::Event::Kind::Touch);
    CHECK(g.events[1].value == 1.0f);
    CHECK(g.events[2].kind == gold::Event::Kind::Expect);
}

TEST(gold_move_on_non_fader_rejected) {
    gold::GoldFile g = parseText(std::string(kFaderPatch) + "@0 move O1 0.5\n");
    CHECK(!g.parseError.empty());
    CHECK(g.parseError.find("not a fader handle") != std::string::npos);
}

TEST(gold_move_zero_fader_rejected) {
    gold::GoldFile g = parseText(std::string(kFaderPatch) + "@0 move F0 0.5\n");
    CHECK(!g.parseError.empty());
}

TEST(gold_touch_after_expect_rejected) {
    gold::GoldFile g = parseText(std::string(kFaderPatch) +
        "@0 expect O1 0\n@0 touch F1 1\n");
    CHECK(!g.parseError.empty());
    CHECK(g.parseError.find("after expect") != std::string::npos);
}

static const char* kDisplayPatch =
    "patch <<<\n[copy]\n  input = I1\n  output = O1\n>>>\n";

TEST(gold_expectdisplay_parse) {
    // The three well-formed operand shapes: header (quoted string), value with
    // an explicit tolerance, and the bare `off` form.
    gold::GoldFile g = parseText(std::string(kDisplayPatch) +
        "@1 expectdisplay D1 header \"BPM\"\n"
        "@1 expectdisplay D1 value 95.03 tol 1e-2\n"
        "@2 expectdisplay D2 off\n");
    CHECK(g.parseError.empty());
    CHECK(g.events.size() == 3);
    CHECK(g.events[0].kind == gold::Event::Kind::ExpectDisplay);
    CHECK(g.events[0].target == "D1");
    CHECK(g.events[0].field == "header");
    CHECK(g.events[0].strValue == "BPM");
    CHECK(g.events[1].kind == gold::Event::Kind::ExpectDisplay);
    CHECK(g.events[1].field == "value");
    CHECK(g.events[1].value == 95.03f);
    CHECK(g.events[1].tol == 1e-2f);
    CHECK(g.events[2].field == "off");
}

TEST(gold_expectdisplay_text_mode_font) {
    gold::GoldFile g = parseText(std::string(kDisplayPatch) +
        "@1 expectdisplay D1 text \"hello\"\n"
        "@1 expectdisplay D1 mode 3\n"
        "@1 expectdisplay D1 font 2\n");
    CHECK(g.parseError.empty());
    CHECK(g.events.size() == 3);
    CHECK(g.events[0].field == "text");
    CHECK(g.events[0].strValue == "hello");
    CHECK(g.events[1].field == "mode");
    CHECK(g.events[1].value == 3.0f);
    CHECK(g.events[2].field == "font");
    CHECK(g.events[2].value == 2.0f);
}

TEST(gold_expectdisplay_bad_handle_rejected) {
    gold::GoldFile g = parseText(std::string(kDisplayPatch) +
        "@1 expectdisplay X1 header \"x\"\n");
    CHECK(!g.parseError.empty());
}

TEST(gold_expectdisplay_bad_field_rejected) {
    gold::GoldFile g = parseText(std::string(kDisplayPatch) +
        "@1 expectdisplay D1 pixels 3\n");
    CHECK(!g.parseError.empty());
}

TEST(gold_expectdisplay_unterminated_quote_rejected) {
    gold::GoldFile g = parseText(std::string(kDisplayPatch) +
        "@1 expectdisplay D1 text \"oops\n");
    CHECK(!g.parseError.empty());
}

// --- Harness hardening (M3) -------------------------------------------------

TEST(gold_expect_load_error_with_events_rejected) {
    // A gold that expects the patch to fail loading cannot also drive a timeline.
    gold::GoldFile g = parseText(
        "expect_load_error \"nope\"\n"
        "patch <<<\n[copy]\n  input = I1\n  output = O1\n>>>\n"
        "@0 expect O1 0\n");
    CHECK(!g.parseError.empty());
    CHECK(g.parseError.find("must not contain timeline events") != std::string::npos);
}

TEST(gold_expect_load_error_trailing_ws_unquoted) {
    // Trailing whitespace after the closing quote must not defeat unquote().
    gold::GoldFile g = parseText(
        "expect_load_error \"boom\"   \n"
        "patch <<<\n[copy]\n  input = I1\n  output = O1\n>>>\n");
    CHECK(g.parseError.empty());
    CHECK(g.expectLoadError == "boom");
}

TEST(gold_giant_tick_rejected) {
    // A 20+-digit tick token would overflow std::stol; it must become a clean
    // malformed-golden error, not an uncaught exception.
    gold::GoldFile g = parseText(std::string(kPatch) +
        "@999999999999999999999999 expect O1 0\n");
    CHECK(!g.parseError.empty());
    CHECK(g.parseError.find("out of range") != std::string::npos);
}

// --- MIDI directives: x7 header + midisend/expectmidi grammar ---------------

static const char* kMidiPatch =
    "patch <<<\n[copy]\n  input = I1\n  output = O1\n>>>\n";

TEST(gold_x7_header_default_true) {
    gold::GoldFile g = parseText(std::string(kMidiPatch) + "@0 expect O1 0\n");
    CHECK(g.parseError.empty());
    CHECK(g.x7 == true);
}

TEST(gold_x7_header_zero) {
    gold::GoldFile g = parseText(std::string("x7 0\n") + kMidiPatch + "@0 expect O1 0\n");
    CHECK(g.parseError.empty());
    CHECK(g.x7 == false);
}

TEST(gold_midisend_expectmidi_parse_ok) {
    gold::GoldFile g = parseText(std::string(kMidiPatch) +
        "@0 midisend trs noteon 1 60 100\n"
        "@0 expectmidi usb none\n");
    CHECK(g.parseError.empty());
    CHECK(g.events.size() == 2);
    CHECK(g.events[0].kind == gold::Event::Kind::MidiSend);
    CHECK(g.events[0].midiPort == (uint8_t)droid::MidiPort::X7Trs);
    CHECK(g.events[0].midi.status == 0x90);
    CHECK(g.events[0].midi.data1 == 60);
    CHECK(g.events[0].midi.data2 == 100);
    CHECK(g.events[1].kind == gold::Event::Kind::ExpectMidi);
    CHECK(g.events[1].midiPort == (uint8_t)droid::MidiPort::X7Usb);
    CHECK(g.events[1].midiNone == true);
}

TEST(gold_midisend_bad_channel_rejected) {
    gold::GoldFile g = parseText(std::string(kMidiPatch) +
        "@0 midisend trs noteon 0 60 100\n");
    CHECK(!g.parseError.empty());
}

TEST(gold_midisend_bad_port_rejected) {
    gold::GoldFile g = parseText(std::string(kMidiPatch) +
        "@0 midisend xyz clock\n");
    CHECK(!g.parseError.empty());
}

TEST(gold_expectmidi_unknown_event_rejected) {
    gold::GoldFile g = parseText(std::string(kMidiPatch) +
        "@0 expectmidi trs bogus\n");
    CHECK(!g.parseError.empty());
}

TEST(gold_expectmidi_after_expect_ok_sets_after_expectmidi_rejected) {
    // ExpectMidi is an assert kind: a set after it within the same tick must be
    // rejected by the sets-before-expects ordering rule.
    gold::GoldFile g = parseText(std::string(kMidiPatch) +
        "@0 expectmidi trs none\n@0 set I1 0.5\n");
    CHECK(!g.parseError.empty());
    CHECK(g.parseError.find("after expect") != std::string::npos);
}

// --- midiEventFromWords byte mapping ---------------------------------------

static droid::MidiEvent fromWords(std::initializer_list<std::string> w,
                                  bool& ok, std::string& err) {
    droid::MidiEvent ev{};
    ok = gold::midiEventFromWords(std::vector<std::string>(w), ev, err);
    return ev;
}

TEST(midi_noteon_channel_mapping) {
    bool ok; std::string err;
    droid::MidiEvent a = fromWords({"noteon", "1", "60", "100"}, ok, err);
    CHECK(ok);
    CHECK(a.status == 0x90);
    CHECK(a.data1 == 60);
    CHECK(a.data2 == 100);
    droid::MidiEvent b = fromWords({"noteon", "16", "60", "100"}, ok, err);
    CHECK(ok);
    CHECK(b.status == 0x9F);
}

TEST(midi_noteoff_polyat_cc_program_at) {
    bool ok; std::string err;
    CHECK(fromWords({"noteoff", "1", "60", "0"}, ok, err).status == 0x80);
    CHECK(fromWords({"polyaftertouch", "1", "60", "20"}, ok, err).status == 0xA0);
    CHECK(fromWords({"cc", "1", "7", "127"}, ok, err).status == 0xB0);
    droid::MidiEvent pr = fromWords({"program", "2", "5"}, ok, err);
    CHECK(pr.status == 0xC1);
    CHECK(pr.data1 == 5);
    CHECK(pr.data2 == 0);
    droid::MidiEvent at = fromWords({"aftertouch", "1", "64"}, ok, err);
    CHECK(at.status == 0xD0);
    CHECK(at.data1 == 64);
}

TEST(midi_bend_centre_mapping) {
    bool ok; std::string err;
    droid::MidiEvent b = fromWords({"bend", "1", "8192"}, ok, err);
    CHECK(ok);
    CHECK(b.status == 0xE0);
    CHECK(b.data1 == 0x00);
    CHECK(b.data2 == 0x40);
    droid::MidiEvent lo = fromWords({"bend", "1", "0"}, ok, err);
    CHECK(lo.data1 == 0x00);
    CHECK(lo.data2 == 0x00);
    droid::MidiEvent hi = fromWords({"bend", "1", "16383"}, ok, err);
    CHECK(hi.data1 == 0x7F);
    CHECK(hi.data2 == 0x7F);
    fromWords({"bend", "1", "16384"}, ok, err);
    CHECK(!ok);   // value14 out of range
}

TEST(midi_realtime_mapping) {
    bool ok; std::string err;
    droid::MidiEvent c = fromWords({"clock"}, ok, err);
    CHECK(ok);
    CHECK(c.status == 0xF8);
    CHECK(c.data1 == 0);
    CHECK(c.data2 == 0);
    CHECK(fromWords({"start"}, ok, err).status == 0xFA);
    CHECK(fromWords({"continue"}, ok, err).status == 0xFB);
    CHECK(fromWords({"stop"}, ok, err).status == 0xFC);
}

TEST(midi_roundtrip_tostring) {
    bool ok; std::string err;
    droid::MidiEvent a = fromWords({"noteon", "16", "60", "100"}, ok, err);
    CHECK(gold::midiEventToString(a) == "noteon 16 60 100");
    droid::MidiEvent b = fromWords({"bend", "1", "8192"}, ok, err);
    CHECK(gold::midiEventToString(b) == "bend 1 8192");
    droid::MidiEvent c = fromWords({"clock"}, ok, err);
    CHECK(gold::midiEventToString(c) == "clock");
}

TEST(midi_bad_event_name_rejected) {
    bool ok; std::string err;
    fromWords({"bogus", "1"}, ok, err);
    CHECK(!ok);
    CHECK(!err.empty());
}
