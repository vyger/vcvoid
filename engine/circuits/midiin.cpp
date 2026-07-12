// midiin — MIDI to CV converter. Spec: manual/circuits/midiin.md.
//
// Converts an inbound MIDI stream into pitch/gate/velocity/pressure CV, per-note
// gates, clock/transport, CCs, pitch bend, program/bank and pedal outputs. Needs
// MIDI hardware — an X7 in the chain or a MASTER18 (native ports): with neither
// this is a runtime no-op — it writes NONE of its outputs, exactly like
// midithrough (the patch still loads). trs/usb select ports by master-aware
// user number (midiin.md "Port selection"): 0 = off, 1..n = specific, 10 = auto.
//
// Engine conventions used here (all traced to the manual):
//   * Pitch is 1V/oct with MIDI note 24 (C0) = 0 V. midiout.md:346 fixes the
//     reference (note 0 = -2 V, note 127 = 8.583 V), so pitch = (note-24)/120 in
//     engine units (1.0 == 10 V; 1 semitone == 0.1/12). Note 60 -> 0.3.
//   * Velocity / pressure / CC: 0..127 -> 0.0..1.0 (value/127).
//   * MIDI clock is 24 PPQ. The clock outputs divide the 0xF8 pulse stream:
//     midiclock = every pulse (1), clock/clock16 = 16th (6), clock8t = 8th
//     triplet (8), clock8 = 8th (12), clock4 = quarter (24). A START resets the
//     pulse phase; the downbeat pulse (0) fires every division.
//   * Trigger lengths: the per-note `trigger` outputs are 5 ms (manual line 366);
//     every other trigger (start/stop/continue, programchange, cctrigger, and
//     the clock pulses) uses the engine-standard 10 ms trigger.
//
// CHOSEN READINGS where the manual is silent (also logged in circuits-status.yaml):
//   * Voice count = the highest voice index for which ANY per-voice output
//     (pitchN/gateN/velocityN/pressureN/triggerN) is patched.
//   * Pitch bend is normalized asymmetrically so the MAXIMUM 14-bit value
//     (16383) outputs +pitchbendrange and the MINIMUM (0) outputs -pitchbendrange
//     (manual line 337 states exactly that): up (v-8192)/8191, down (v-8192)/8192.
//   * Port auto-detect locks PER KIND (TRS/USB) to the first valid port of that
//     kind that carries data and stays locked (the manual's "seconds" re-detect
//     timeout is NOT modelled). Active TRS + USB streams merge, TRS first.
//   * notegap (millisecond forced gate gap) and the legato pedal (CC 68) are NOT
//     modelled — the default legato behaviour (notegap 0) is what we emit.
//     tuningmode drives all pitch outputs to tuningpitch and gates a 120 BPM
//     square (a coarse rendering of "gates at 120 BPM").
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include "../src/midivoices.hpp"
#include <cmath>
#include <cstdint>
#include <cstring>

namespace droid {

class MidiIn : public Circuit {

    // MIDI note -> engine-unit pitch (note 24 == 0 V; 1 semitone == 0.1/12).
    static float notePitch(int note) { return (float(note) - 24.0f) * (0.1f / 12.0f); }
    static bool gateHigh(float v) { return v >= kGateHighThreshold; }

    MidiVoiceAllocator alloc_;
    int      numVoices_ = -1;
    int      lastRr_ = -1, lastVa_ = -1;
    int      initialRunning_ = -1;      // latched once at first tick

    // Per-voice extra state (allocator holds gate/note/velocity).
    bool     struck_[8] = {false};      // ever played -> pitch is meaningful
    float    pressure_[8] = {0};
    uint64_t voiceTrigOff_[8] = {0};    // 5 ms note trigger

    // Clock: pulse phase + per-division 10 ms trigger-off ticks.
    uint32_t clockPulse_ = 0;
    uint64_t divOff_[5] = {0};          // [midiclock, 16th, 8th-triplet, 8th, quarter]
    static constexpr int kDiv[5] = {1, 6, 8, 12, 24};

    // Transport / event triggers.
    uint64_t startOff_ = 0, stopOff_ = 0, contOff_ = 0, pcOff_ = 0;
    uint64_t ccTrigOff_[4] = {0};
    bool     running_ = false, active_ = false, started_ = false;

    // Continuous controllers.
    float    ccVal_[4] = {0};
    float    modwheel_ = 0, volume_ = 0, pitchbendNorm_ = 0;
    int      program_ = 0, bankMSB_ = 0, bankLSB_ = 0;

    // Physical note state for the note gates (NOT range-filtered).
    uint8_t  heldVel_[128] = {0};

    // Pedals.
    bool     damperDown_ = false, sostenutoDown_ = false;
    bool     portamentoDown_ = false, softDown_ = false;
    bool     sostenutoCaptured_[128] = {false};
    bool     deferred_[128] = {false};  // note-off seen but sustained by a pedal

    // Port auto-detect locks, one per kind (TRS / USB).
    int      lockedTrs_ = -1, lockedUsb_ = -1;

    GateReader resetGate_;
    long     trig5_ = 1, trig10_ = 1;   // recomputed each tick from tick rate

    // --- helpers ------------------------------------------------------------
    int computeNumVoices() {
        static const char* jacks[] = {"pitch", "gate", "velocity", "pressure", "trigger"};
        int n = 0;
        for (const char* j : jacks)
            for (int i = 1; i <= 8; i++)
                if (out(j, i).connected() && i > n) n = i;
        return n;
    }

    void resetAll() {
        alloc_.allNotesOff();
        std::memset(heldVel_, 0, sizeof heldVel_);
        std::memset(deferred_, 0, sizeof deferred_);
        std::memset(sostenutoCaptured_, 0, sizeof sostenutoCaptured_);
        for (int i = 0; i < 8; i++) pressure_[i] = 0;
        for (int i = 0; i < 4; i++) ccVal_[i] = 0;
        modwheel_ = volume_ = pitchbendNorm_ = 0;
        damperDown_ = sostenutoDown_ = portamentoDown_ = softDown_ = false;
        clockPulse_ = 0;
        running_ = (initialRunning_ == 1);
        started_ = (initialRunning_ == 1);
    }

    void handleNoteOn(EngineState& s, int note, int vel) {
        if (note < 0 || note > 127) return;   // note-state arrays are [128]; never index OOB
        heldVel_[note] = (uint8_t)vel;
        if (!started_ && initialRunning_ == 2) {   // auto: first note simulates START
            running_ = true; started_ = true; startOff_ = s.tick + trig10_;
        }
        int lo = (int)std::lround(in("lowestnote").value(s));
        int hi = (int)std::lround(in("highestnote").value(s));
        if (note < lo || note > hi) return;        // voice path only; note gates unaffected
        deferred_[note] = false;
        int v = alloc_.noteOn((uint8_t)note, (uint8_t)vel);
        if (v >= 0) { struck_[v] = true; voiceTrigOff_[v] = s.tick + trig5_; }
    }

    void handleNoteOff(int note) {
        if (note < 0 || note > 127) return;   // note-state arrays are [128]; never index OOB
        heldVel_[note] = 0;
        bool sustain = damperDown_ || (sostenutoDown_ && sostenutoCaptured_[note]);
        if (sustain) deferred_[note] = true;
        else alloc_.noteOff((uint8_t)note);
    }

    void releasePedal(bool sostenuto) {
        for (int n = 0; n < 128; n++) {
            if (!deferred_[n]) continue;
            bool otherHolds = sostenuto ? damperDown_
                                        : (sostenutoDown_ && sostenutoCaptured_[n]);
            if (!otherHolds && heldVel_[n] == 0) { alloc_.noteOff((uint8_t)n); deferred_[n] = false; }
        }
        if (sostenuto) std::memset(sostenutoCaptured_, 0, sizeof sostenutoCaptured_);
    }

    void handleCC(EngineState& s, int num, int val) {
        bool down = val >= 64;
        switch (num) {
            case 64: if (!down && damperDown_) { damperDown_ = false; releasePedal(false); }
                     else damperDown_ = down; break;
            case 65: portamentoDown_ = down; break;
            case 66: if (down && !sostenutoDown_) {          // capture currently-gated notes
                         for (int i = 0; i < numVoices_; i++) {
                             const auto& v = alloc_.voice(i);
                             if (v.gate) sostenutoCaptured_[v.note] = true;
                         }
                         sostenutoDown_ = true;
                     } else if (!down && sostenutoDown_) { sostenutoDown_ = false; releasePedal(true); }
                     break;
            case 67: softDown_ = down; break;
            default: break;
        }
        if (num == 0)  bankMSB_ = val;
        if (num == 32) bankLSB_ = val;
        if (num == 1)  modwheel_ = val / 127.0f;
        if (num == 7)  volume_ = val / 127.0f;
        for (int k = 0; k < 4; k++)
            if ((int)std::lround(in("ccnumber", k + 1).value(s)) == num) {
                ccVal_[k] = val / 127.0f; ccTrigOff_[k] = s.tick + trig10_;
            }
    }

    void handleRealtime(EngineState& s, uint8_t status) {
        switch (status) {
            case 0xF8: {                                // clock pulse
                for (int d = 0; d < 5; d++)
                    if (clockPulse_ % (uint32_t)kDiv[d] == 0) divOff_[d] = s.tick + trig10_;
                clockPulse_++;
                break;
            }
            case 0xFA: running_ = true; started_ = true; startOff_ = s.tick + trig10_;
                       clockPulse_ = 0; break;          // START resets clock phase
            case 0xFB: running_ = true; started_ = true; contOff_ = s.tick + trig10_; break;
            case 0xFC: running_ = false; stopOff_ = s.tick + trig10_; break;
            default: break;
        }
    }

    void processEvent(EngineState& s, const MidiEvent& e) {
        active_ = true;
        if (e.status >= 0xF8) { handleRealtime(s, e.status); return; }
        uint8_t hi = e.status & 0xF0;
        int ch = (e.status & 0x0F) + 1;
        if (in("channel").connected() &&
            (int)std::lround(in("channel").value(s)) != ch) return;   // channel filter
        switch (hi) {
            case 0x90: if (e.data2 == 0) handleNoteOff(e.data1);       // vel 0 == note off
                       else handleNoteOn(s, e.data1, e.data2); break;
            case 0x80: handleNoteOff(e.data1); break;
            case 0xA0: {                                                // poly key pressure
                for (int i = 0; i < numVoices_; i++) {
                    const auto& v = alloc_.voice(i);
                    if (v.gate && v.note == e.data1) pressure_[i] = e.data2 / 127.0f;
                }
                break;
            }
            case 0xD0: for (int i = 0; i < numVoices_; i++) pressure_[i] = e.data1 / 127.0f; break;
            case 0xB0: handleCC(s, e.data1, e.data2); break;
            case 0xC0: program_ = e.data1; pcOff_ = s.tick + trig10_; break;
            case 0xE0: {                                                // pitch bend
                int v14 = int(e.data1) | (int(e.data2) << 7);
                pitchbendNorm_ = v14 >= 8192 ? (v14 - 8192) / 8191.0f
                                             : (v14 - 8192) / 8192.0f;
                break;
            }
            default: break;
        }
    }

    // Per-kind active-port resolution (manual midiin.md "Port selection"):
    // omitted+omitted -> both kinds auto; one specified, other omitted -> other
    // off unless the specified one is 0; value 0 -> off; 1..n -> that port;
    // 10 -> auto. Auto locks to the first valid port of the kind that carries
    // data and stays locked (the manual's seconds-of-silence re-detect is NOT
    // modelled — chosen reading, logged in circuits-status.yaml). Both kinds can
    // be active at once; their event streams merge, TRS first (manual: "up to
    // two MIDI streams in parallel - one from a USB jack and one from a TRS jack").
    int activePort(EngineState& s, PortKind k, const char* jack, const char* other,
                   int& locked) {
        bool c = in(jack).connected(), oc = in(other).connected();
        int v;
        if (!c) {
            // omitted: auto iff the other is also omitted or explicitly 0
            bool autoEn = !oc || (int)std::lround(in(other).value(s)) == 0;
            if (!autoEn) return -1;
            v = 10;
        } else {
            v = (int)std::lround(in(jack).value(s));
        }
        if (v == 0) return -1;
        if (v != 10) return resolvePort(s.midi.master18, s.midi.x7, k, v);
        if (locked < 0) {
            int ports[3];
            int n = portsOfKind(s.midi.master18, s.midi.x7, k, ports);
            for (int i = 0; i < n; i++)
                if (s.midi.tickCount[ports[i]] > 0) { locked = ports[i]; break; }
        }
        return locked;
    }

public:
    void tick(EngineState& s) override {
        if (!s.midi.available()) return;               // no MIDI hardware: write no outputs

        if (initialRunning_ < 0) {
            initialRunning_ = (int)std::lround(in("initialrunning").value(s));
            running_ = (initialRunning_ == 1);
            started_ = (initialRunning_ == 1);
        }
        if (numVoices_ < 0) numVoices_ = computeNumVoices();

        trig5_  = std::max(1L, std::lround(0.005 * s.tickRateHz));
        trig10_ = std::max(1L, std::lround(0.01  * s.tickRateHz));

        int rr = gateHigh(in("roundrobin").value(s)) ? 1 : 0;
        int va = (int)std::lround(in("voiceallocation").value(s));
        if (rr != lastRr_ || va != lastVa_) {
            alloc_.configure(numVoices_, rr != 0, va);  // resets rr pointer only
            lastRr_ = rr; lastVa_ = va;
        }

        if (resetGate_.risingEdge(in("systemreset").value(s))) resetAll();

        int trsPort = activePort(s, PortKind::Trs, "trs", "usb", lockedTrs_);
        int usbPort = activePort(s, PortKind::Usb, "usb", "trs", lockedUsb_);
        for (int port : {trsPort, usbPort})
            if (port >= 0)
                for (uint16_t k = 0; k < s.midi.tickCount[port]; k++)
                    processEvent(s, s.midi.tickEv[port][k]);

        writeOutputs(s);
    }

private:
    void writeOutputs(EngineState& s) {
        float transpose = in("transpose").value(s);
        float pbr = in("pitchbendrange").value(s);
        float pbCv = pitchbendNorm_ * pbr;
        bool bendpitch = gateHigh(in("bendpitch").value(s));
        bool tuning = gateHigh(in("tuningmode").value(s));
        bool holdvel = gateHigh(in("holdvelocity").value(s));
        float tuningpitch = in("tuningpitch").value(s);

        out("pitchbend").set(s, pbCv);

        long tuneP = std::max(1L, std::lround(0.5 * s.tickRateHz));   // 120 BPM period
        for (int i = 0; i < numVoices_; i++) {
            const auto& v = alloc_.voice(i);
            if (tuning) {
                out("pitch", i + 1).set(s, tuningpitch);
                out("gate", i + 1).set(s, (long)(s.tick % (uint64_t)tuneP) < tuneP / 2 ? 1.0f : 0.0f);
                out("velocity", i + 1).set(s, 0.0f);
                out("pressure", i + 1).set(s, 0.0f);
                out("trigger", i + 1).set(s, 0.0f);
                continue;
            }
            if (struck_[i])
                out("pitch", i + 1).set(s, notePitch(v.note) + transpose + (bendpitch ? pbCv : 0.0f));
            out("gate", i + 1).set(s, v.gate ? 1.0f : 0.0f);
            float vel = (v.gate || holdvel) ? v.velocity / 127.0f : 0.0f;
            out("velocity", i + 1).set(s, vel);
            out("pressure", i + 1).set(s, pressure_[i]);
            out("trigger", i + 1).set(s, s.tick < voiceTrigOff_[i] ? 1.0f : 0.0f);
        }

        out("midiclock").set(s, s.tick < divOff_[0] ? 1.0f : 0.0f);
        out("clock").set(s,     s.tick < divOff_[1] ? 1.0f : 0.0f);   // == clock16
        out("clock16").set(s,   s.tick < divOff_[1] ? 1.0f : 0.0f);
        out("clock8t").set(s,   s.tick < divOff_[2] ? 1.0f : 0.0f);
        out("clock8").set(s,    s.tick < divOff_[3] ? 1.0f : 0.0f);
        out("clock4").set(s,    s.tick < divOff_[4] ? 1.0f : 0.0f);

        out("start").set(s,     s.tick < startOff_ ? 1.0f : 0.0f);
        out("stop").set(s,      s.tick < stopOff_  ? 1.0f : 0.0f);
        out("continue").set(s,  s.tick < contOff_  ? 1.0f : 0.0f);
        out("running").set(s, running_ ? 1.0f : 0.0f);
        out("active").set(s, active_ ? 1.0f : 0.0f);

        out("programchange").set(s, s.tick < pcOff_ ? 1.0f : 0.0f);
        out("program").set(s, (float)program_);
        out("bank").set(s, (float)(bankMSB_ * 128 + bankLSB_));
        out("modwheel").set(s, modwheel_);
        out("volume").set(s, volume_);
        out("portamento").set(s, portamentoDown_ ? 1.0f : 0.0f);
        out("soft").set(s, softDown_ ? 1.0f : 0.0f);

        for (int k = 0; k < 4; k++) {
            out("cc", k + 1).set(s, ccVal_[k]);
            out("cctrigger", k + 1).set(s, s.tick < ccTrigOff_[k] ? 1.0f : 0.0f);
        }

        for (int k = 0; k < 16; k++) {
            int note = in("note", k + 1).connected()
                         ? (int)std::lround(in("note", k + 1).value(s)) : k;  // smart default 0,1,2..
            bool held = note >= 0 && note < 128 && heldVel_[note] > 0;
            out("notegate", k + 1).set(s, held ? 1.0f : 0.0f);
            out("notegatevelocity", k + 1).set(s, held ? heldVel_[note] / 127.0f : 0.0f);
        }
    }
};

constexpr int MidiIn::kDiv[5];

DROID_REGISTER_CIRCUIT(midiin, MidiIn)

} // namespace droid
