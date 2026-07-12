// midiout — CV to MIDI converter. Spec: manual/circuits/midiout.md.
//
// The only DROID circuit with no output jacks: everything leaves via MIDI
// (manual line 27). It plays up to 8 poly voices (pitch/gate/velocity), 16 fixed
// note gates, a raft of continuous controllers (CCs, modwheel, volume, pitchbend,
// channel/key pressure), pedals, program/bank, transport and a MIDI clock. Needs
// MIDI hardware — an X7 in the chain or a MASTER18 (native ports): with neither
// this is a runtime no-op — it emits NOTHING (the patch still loads), exactly
// like midiin/midithrough.
//
// Engine conventions used here (all traced to the manual):
//   * Pitch is 1V/oct with MIDI note 24 (C0) = 0 V (manual line 346): note 0 =
//     -2 V, note 127 = 8.583 V, so note = round(pitch*120)+24 in engine units
//     (1.0 == 10 V; 1 semitone == 0.1/12). 0.3 -> note 60.
//   * 0..1 CV -> 0..127 MIDI (round). velocity default 1.0 -> 127 (line 348).
//   * channel 1..16 -> low nibble 0..15 (line 343).
//   * trs/usb are master-aware port NUMBERS (lines 98-133): 1..n = specific
//     port, 10 = all ports of the kind, 0 = off; both set -> both kinds. The
//     Master16 gate table (lines 77-87) degenerates to numbers 0/1 naturally.
//   * MIDI clock 24 PPQN. `midiclock` passes 0xF8 1:1; `clock` takes a 16th clock
//     and multiplies by 6, interpolating five pulses between measured edges
//     (lines 223-227, 383). START/STOP are realtime 0xFA/0xFC (lines 372-374).
//   * Continuous controllers (cc1..8, modwheel CC#1, volume CC#7+#39, pitchbend,
//     channelpressure, key pressure) send once initially then on change,
//     rate-limited by updaterate (default 50/s, line 386) and held back at start
//     by delayinitialccs (default 1.0 s, line 368). cctrigger switches a CC to
//     trigger-only (line 366); updateccs forces a resend of all used CCs (367).
//
// CHOSEN READINGS where the manual is silent (also logged in circuits-status.yaml):
//   * Pitch stabilization settle window = 2 ms ("stable for at least some very
//     short time", lines 278-279 — no figure given). A stable pitch therefore
//     always incurs this minimal latency; a moving pitch keeps resetting it. No
//     maximum-wait cap is modelled (matches "if it never stabilizes, disable it",
//     lines 282-284) — a never-settling pitch simply never fires the note.
//   * Voice count = highest index with any of pitchN/gateN/velocityN/
//     noteoffvelocityN/pressureN patched.
//   * allnotesoff/allsoundoff (lines 376-377) emit only their CC (#123/#120,
//     value 0) — NOT per-note note-offs — but also clear the circuit's own note
//     bookkeeping, so a still-high gate does not later emit a redundant note-off
//     ("the same as releasing all currently held keys").
//   * program/bank: one initial Program Change is sent at start (program/bank
//     patched); afterwards a PC is sent on any program/bank change or a
//     programchange trigger, with Bank Select MSB/LSB (CC#0/#32) sent before the
//     PC whenever the bank changed. Not rate-limited.
//   * delayinitialccs gates the FIRST send of every continuous controller (not
//     just numbered CCs); key/channel pressure never send an initial value
//     (they start at 0 and only report changes while a note plays).
//
// HARNESS-UNTESTABLE (implemented, per manual, but not assertable by the golden
// harness which only expresses noteon/noteoff/cc/bend/program/aftertouch/
// polyaftertouch + clock/start/continue/stop): active sensing (0xFE every 300 ms,
// line 385) and systemreset (0xFF, line 375). Goldens set activesensing=0 to keep
// streams clean. See circuits-status.yaml.
//
// select/selectat (line 387) gate ONLY button/LED processing. midiout has no
// button/LED jacks, so select has NO effect on its MIDI output — the circuit
// keeps emitting when deselected (manual is explicit; this differs from the task
// brief's assumption, which the manual overrides). select is therefore read but
// intentionally does not gate anything here.
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include <cmath>
#include <cstdint>
#include <cstring>

namespace droid {

class MidiOut : public Circuit {
    static constexpr double kStabilizeMs = 2.0;      // pitch settle window (chosen)
    static constexpr double kActiveSensingMs = 300.0;
    static constexpr float  kPitchEps = 1e-4f;       // "pitch moved" threshold

    static bool gateHigh(float v) { return v >= kGateHighThreshold; }
    static int  clampi(long v, long lo, long hi) { return (int)(v < lo ? lo : v > hi ? hi : v); }
    // 0..1 CV -> 0..127.
    static int  frac127(float v) { long n = std::lround(v * 127.0f); return clampi(n, 0, 127); }
    // engine-unit pitch -> MIDI note (note 24 == 0 V; 1 semitone == 0.1/12).
    static int  pitchToNote(float p) { return clampi(std::lround(p * 120.0f) + 24, 0, 127); }
    static float noteToPitch(int note) { return (float(note) - 24.0f) * (0.1f / 12.0f); }
    // pitchbend CV (-1..1) -> 14-bit bend (center 8192).
    static int bend14(float p) {
        if (p > 1.0f) p = 1.0f; else if (p < -1.0f) p = -1.0f;
        long v = p >= 0.0f ? 8192 + std::lround(p * 8191.0f) : 8192 + std::lround(p * 8192.0f);
        return clampi(v, 0, 16383);
    }

    // --- config latched once -------------------------------------------------
    int  numVoices_ = -1;

    // --- port + emission -----------------------------------------------------
    uint8_t outMask_ = 0;                              // bit p = physical port p
    uint8_t ch_ = 0;                                   // low nibble
    void emit(EngineState& s, uint8_t status, uint8_t d1 = 0, uint8_t d2 = 0) {
        MidiEvent e{status, d1, d2};
        for (int p = 0; p < kNumMidiPorts; p++)
            if (outMask_ & (1u << p)) s.midi.out[p].push(e);
    }
    void emitCh(EngineState& s, uint8_t hi, uint8_t d1, uint8_t d2) { emit(s, hi | ch_, d1, d2); }

    // --- per-voice note state ------------------------------------------------
    GateReader gate_[8];
    bool     pending_[8] = {false};                    // gate high, note not yet sent
    uint64_t stableSince_[8] = {0};
    uint64_t stabStart_[8] = {0};                      // rise tick + triggerdelay
    float    lastPitch_[8] = {0};
    bool     active_[8] = {false};                     // note currently sounding
    int      note_[8] = {0};
    int      vel_[8] = {0};

    // --- note gates ----------------------------------------------------------
    GateReader ngate_[16];
    bool     ngActive_[16] = {false};
    int      ngNote_[16] = {0};

    // --- pedals --------------------------------------------------------------
    GateReader pedal_[5];                              // damper,sostenuto,soft,portamento,legato
    static constexpr uint8_t kPedalCC[5] = {64, 66, 67, 65, 68};
    static constexpr const char* kPedalJack[5] =
        {"damper", "sostenuto", "soft", "portamento", "legato"};

    // --- transport / triggers ------------------------------------------------
    GateReader start_, stop_, running_, sysreset_, allnotesoff_, allsoundoff_;
    GateReader updateccs_, programchange_, cctrig_[8];

    // --- clock ---------------------------------------------------------------
    GateReader clockIn_, midiclockIn_;
    int64_t  clockLastEdge_ = -1, clockEdgeTick_ = -1;
    double   clockPeriod_ = 0;
    int      clockInterp_ = 0;

    // --- program / bank ------------------------------------------------------
    int lastProgram_ = -1, lastBankMSB_ = -1, lastBankLSB_ = -1;

    // --- continuous controllers ----------------------------------------------
    struct Cont { bool inited = false; int lastVal = -1; uint64_t lastTick = 0; };
    Cont cc_[8], modwheel_, volume_, pitchbend_, chpressure_, pressureC_[8];

    // --- active sensing ------------------------------------------------------
    uint64_t asLast_ = 0; bool asStarted_ = false;

    // per-tick recomputed
    long stabTicks_ = 1, initDelayTicks_ = 0, ccPeriod_ = 1;
    bool ccUpdatesOn_ = true, continuousAllowed_ = false;

    int computeNumVoices() {
        static const char* jacks[] = {"pitch", "gate", "velocity", "noteoffvelocity", "pressure"};
        int n = 0;
        for (const char* j : jacks)
            for (int i = 1; i <= 8; i++)
                if (in(j, i).connected() && i > n) n = i;
        return n;
    }

    // Target-port mask, mirroring the manual's trs/usb tables (midiout.md lines
    // 68-133): values are master-aware port NUMBERS (1..n = specific port,
    // 10 = all ports of the kind, 0/out-of-range = none). Defaults: neither
    // specified -> first TRS port; one kind specified, other omitted -> other
    // off, unless the specified one is 0 -> first port of the other kind.
    // connected() distinguishes omitted from an explicit 0.
    void computePorts(EngineState& s) {
        bool m18 = s.midi.master18, x7 = s.midi.x7;
        auto maskFor = [&](PortKind k, int v) -> uint8_t {
            int ports[3];
            int n = portsOfKind(m18, x7, k, ports);
            uint8_t m = 0;
            if (v == 10) { for (int i = 0; i < n; i++) m |= uint8_t(1u << ports[i]); }
            else if (v >= 1 && v <= n) m = uint8_t(1u << ports[v - 1]);
            return m;
        };
        bool trsC = in("trs").connected(), usbC = in("usb").connected();
        int trsV = trsC ? (int)std::lround(in("trs").value(s)) : 0;
        int usbV = usbC ? (int)std::lround(in("usb").value(s)) : 0;
        if (!trsC && !usbC)      { trsV = 1; usbV = 0; }            // default: first TRS
        else if (!trsC)          { trsV = (usbV == 0) ? 1 : 0; }
        else if (!usbC)          { usbV = (trsV == 0) ? 1 : 0; }
        outMask_ = maskFor(PortKind::Trs, trsV) | maskFor(PortKind::Usb, usbV);
    }

    // A used continuous controller: initial send (once, after delay) then on
    // change, rate-limited. `emitFn(value)` pushes the MIDI for `value`.
    template <class F>
    void contUpdate(EngineState& s, Cont& c, int curVal, bool sendInitial, F emitFn) {
        if (!continuousAllowed_) return;
        if (!c.inited) {                               // first observation: baseline
            if (sendInitial) emitFn(curVal);
            c.inited = true; c.lastVal = curVal; c.lastTick = s.tick; return;
        }
        if (!ccUpdatesOn_) return;
        if (curVal != c.lastVal && (int64_t)(s.tick - c.lastTick) >= ccPeriod_) {
            emitFn(curVal); c.lastVal = curVal; c.lastTick = s.tick;
        }
    }

    void closeAllNotes() {
        for (int v = 0; v < numVoices_; v++) { active_[v] = false; pending_[v] = false; }
        for (int k = 0; k < 16; k++) ngActive_[k] = false;
    }

public:
    void tick(EngineState& s) override {
        if (!s.midi.available()) return;               // no MIDI hardware: emit nothing

        if (numVoices_ < 0) numVoices_ = computeNumVoices();
        float rate = s.tickRateHz;
        stabTicks_      = std::max(1L, std::lround(kStabilizeMs * rate / 1000.0));
        initDelayTicks_ = std::max(0L, std::lround((double)in("delayinitialccs").value(s) * rate));
        continuousAllowed_ = (int64_t)s.tick >= initDelayTicks_;
        float ur = in("updaterate").value(s);
        ccUpdatesOn_ = ur > 0.0f;
        ccPeriod_ = ccUpdatesOn_ ? std::max(1L, std::lround(rate / (double)ur)) : 1;

        computePorts(s);
        ch_ = (uint8_t)(clampi(std::lround(in("channel").value(s)), 1, 16) - 1);

        // --- system reset / all-notes-off / all-sound-off --------------------
        if (sysreset_.risingEdge(in("systemreset").value(s))) { emit(s, 0xFF); closeAllNotes(); }
        if (allnotesoff_.risingEdge(in("allnotesoff").value(s))) { emitCh(s, 0xB0, 123, 0); closeAllNotes(); }
        if (allsoundoff_.risingEdge(in("allsoundoff").value(s))) { emitCh(s, 0xB0, 120, 0); closeAllNotes(); }

        // --- clock -----------------------------------------------------------
        if (clockIn_.risingEdge(in("clock").value(s))) {
            emit(s, 0xF8);
            if (clockLastEdge_ >= 0) {
                clockPeriod_ = double(int64_t(s.tick) - clockLastEdge_);
                clockEdgeTick_ = int64_t(s.tick); clockInterp_ = 0;
            } else clockPeriod_ = 0;
            clockLastEdge_ = int64_t(s.tick);
        }
        if (clockPeriod_ > 0) {
            double spacing = clockPeriod_ / 6.0;
            int due = (int)std::floor((double(int64_t(s.tick) - clockEdgeTick_)) / spacing);
            if (due > 5) due = 5;
            while (clockInterp_ < due) { emit(s, 0xF8); clockInterp_++; }
        }
        if (midiclockIn_.risingEdge(in("midiclock").value(s))) emit(s, 0xF8);

        // --- transport -------------------------------------------------------
        if (start_.risingEdge(in("start").value(s))) emit(s, 0xFA);
        if (stop_.risingEdge(in("stop").value(s)))   emit(s, 0xFC);
        {
            float rv = in("running").value(s);
            bool nowHigh = gateHigh(rv), wasHigh = running_.high;
            if (nowHigh && !wasHigh) emit(s, 0xFA);
            else if (!nowHigh && wasHigh) emit(s, 0xFC);
            running_.high = nowHigh;
        }

        // --- voice note offs then note ons -----------------------------------
        int lo = clampi(std::lround(in("lowestnote").value(s)), 0, 127);
        int hi = clampi(std::lround(in("highestnote").value(s)), 0, 127);
        bool stabConn = in("pitchstabilization").connected();
        bool stabVal  = gateHigh(in("pitchstabilization").value(s));
        bool tdConn   = in("triggerdelay").connected();
        long tdTicks  = tdConn ? std::max(0L, std::lround((double)in("triggerdelay").value(s) * rate / 1000.0)) : 0;
        // effective stabilization: on by default; triggerdelay disables it unless
        // pitchstabilization is set explicitly (manual line 353).
        bool stabOn = stabConn ? stabVal : !tdConn;

        for (int v = 0; v < numVoices_; v++) {
            float g = in("gate", v + 1).value(s);
            bool rose = gate_[v].risingEdge(g);
            bool fell = false;
            // GateReader.high already updated by risingEdge; detect fall manually.
            if (rose) { pending_[v] = true; stabStart_[v] = s.tick + (uint64_t)tdTicks; }
            if (!gateHigh(g) && (active_[v] || pending_[v])) fell = true;

            if (fell) {
                if (active_[v]) {
                    int nvel = in("noteoffvelocity", v + 1).connected()
                                 ? frac127(in("noteoffvelocity", v + 1).value(s)) : vel_[v];
                    emitCh(s, 0x80, (uint8_t)note_[v], (uint8_t)nvel);
                    active_[v] = false;
                }
                pending_[v] = false;
                continue;
            }
            if (!pending_[v]) continue;
            if (s.tick < stabStart_[v]) continue;      // triggerdelay window
            float p = in("pitch", v + 1).value(s);
            if (s.tick == stabStart_[v]) { stableSince_[v] = s.tick; lastPitch_[v] = p; }
            else if (std::fabs(p - lastPitch_[v]) > kPitchEps) { stableSince_[v] = s.tick; lastPitch_[v] = p; }
            bool ready = !stabOn || (int64_t)(s.tick - stableSince_[v]) >= stabTicks_;
            if (!ready) continue;
            pending_[v] = false;
            int note = pitchToNote(p);
            if (note < lo || note > hi) continue;      // range filter (note gates exempt)
            int vel = frac127(in("velocity", v + 1).value(s));
            note_[v] = note; vel_[v] = vel; active_[v] = true;
            emitCh(s, 0x90, (uint8_t)note, (uint8_t)vel);
        }

        // --- note gates ------------------------------------------------------
        for (int k = 0; k < 16; k++) {
            float g = in("notegate", k + 1).value(s);
            bool rose = ngate_[k].risingEdge(g);
            bool fell = !gateHigh(g) && ngActive_[k];
            int nvel = frac127(in("notegatevelocity", k + 1).value(s));
            if (rose) {
                int note = in("note", k + 1).connected()
                             ? clampi(std::lround(in("note", k + 1).value(s)), 0, 127) : k;
                ngNote_[k] = note; ngActive_[k] = true;
                emitCh(s, 0x90, (uint8_t)note, (uint8_t)nvel);
            } else if (fell) {
                emitCh(s, 0x80, (uint8_t)ngNote_[k], (uint8_t)nvel);
                ngActive_[k] = false;
            }
        }

        // --- pedals ----------------------------------------------------------
        for (int i = 0; i < 5; i++) {
            if (!in(kPedalJack[i]).connected()) { pedal_[i].high = gateHigh(in(kPedalJack[i]).value(s)); continue; }
            float pv = in(kPedalJack[i]).value(s);
            bool wasHigh = pedal_[i].high;
            bool nowHigh = gateHigh(pv);
            if (nowHigh && !wasHigh) emitCh(s, 0xB0, kPedalCC[i], 127);
            else if (!nowHigh && wasHigh) emitCh(s, 0xB0, kPedalCC[i], 0);
            pedal_[i].high = nowHigh;
        }

        // --- program / bank --------------------------------------------------
        {
            bool progC = in("program").connected(), bankC = in("bank").connected();
            if (progC || bankC) {
                int prog = progC ? clampi(std::lround(in("program").value(s)), 1, 128) : 1;
                int bank = bankC ? clampi(std::lround(in("bank").value(s)), 1, 16384) : 1;
                int msb = (bank - 1) >> 7, lsb = (bank - 1) & 0x7F;
                bool first = (lastProgram_ < 0);
                bool bankChanged = bankC && (msb != lastBankMSB_ || lsb != lastBankLSB_);
                bool progChanged = progC && (prog != lastProgram_);
                bool trig = programchange_.risingEdge(in("programchange").value(s));
                if (bankChanged || (first && bankC)) { emitCh(s, 0xB0, 0, (uint8_t)msb); emitCh(s, 0xB0, 32, (uint8_t)lsb); }
                if (bankChanged || progChanged || trig || first) emitCh(s, 0xC0, (uint8_t)(prog - 1), 0);
                lastProgram_ = prog; lastBankMSB_ = msb; lastBankLSB_ = lsb;
            } else {
                programchange_.risingEdge(in("programchange").value(s));  // keep edge state fresh
            }
        }

        // --- numbered CCs ----------------------------------------------------
        bool forceAll = updateccs_.risingEdge(in("updateccs").value(s));
        for (int k = 0; k < 8; k++) {
            if (!in("cc", k + 1).connected()) continue;
            int num = clampi(std::lround(in("ccnumber", k + 1).value(s)), 0, 127);
            int val = frac127(in("cc", k + 1).value(s));
            if (forceAll) { emitCh(s, 0xB0, (uint8_t)num, (uint8_t)val); cc_[k].inited = true; cc_[k].lastVal = val; cc_[k].lastTick = s.tick; continue; }
            if (in("cctrigger", k + 1).connected()) {
                if (cctrig_[k].risingEdge(in("cctrigger", k + 1).value(s)))
                    emitCh(s, 0xB0, (uint8_t)num, (uint8_t)val);
            } else {
                contUpdate(s, cc_[k], val, true, [&](int x){ emitCh(s, 0xB0, (uint8_t)num, (uint8_t)x); });
            }
        }
        // updateccs also refreshes cctrigger-mode CCs' edge state when forced
        // above via the shared loop; nothing else needed here.

        // --- modwheel / volume ----------------------------------------------
        if (in("modwheel").connected())
            contUpdate(s, modwheel_, frac127(in("modwheel").value(s)), true,
                       [&](int x){ emitCh(s, 0xB0, 1, (uint8_t)x); });
        if (in("volume").connected()) {
            int v14 = clampi(std::lround(in("volume").value(s) * 16383.0f), 0, 16383);
            contUpdate(s, volume_, v14, true, [&](int x){
                emitCh(s, 0xB0, 7, (uint8_t)(x >> 7)); emitCh(s, 0xB0, 39, (uint8_t)(x & 0x7F)); });
        }

        // --- pitch bend (CV + pitch tracking) --------------------------------
        {
            int pt = clampi(std::lround(in("pitchtracking").value(s)), 0, 2);
            bool pbC = in("pitchbend").connected();
            if (pbC || pt != 0) {
                if (!pbC && !pitchbend_.inited) { pitchbend_.inited = true; pitchbend_.lastVal = 8192; }
                float pbCv = pbC ? in("pitchbend").value(s) : 0.0f;
                float track = 0.0f;
                if (pt != 0 && active_[0]) {
                    float pbr = in("pitchbendrange").value(s);
                    if (pbr <= 0.0f) pbr = 0.0166666f;
                    track = (in("pitch", 1).value(s) - noteToPitch(note_[0])) / pbr;
                    if (track > 1.0f) track = 1.0f; else if (track < -1.0f) track = -1.0f;  // level-1 clip
                }
                int v14 = bend14(pbCv + track);
                contUpdate(s, pitchbend_, v14, true, [&](int x){
                    emitCh(s, 0xE0, (uint8_t)(x & 0x7F), (uint8_t)(x >> 7)); });
            }
        }

        // --- key / channel pressure -----------------------------------------
        if (in("channelpressure").connected())         // sendInitial=false: report on change only
            contUpdate(s, chpressure_, frac127(in("channelpressure").value(s)), false,
                       [&](int x){ emitCh(s, 0xD0, (uint8_t)x, 0); });
        for (int v = 0; v < numVoices_; v++) {
            if (!in("pressure", v + 1).connected() || !active_[v]) continue;  // only while the note plays
            contUpdate(s, pressureC_[v], frac127(in("pressure", v + 1).value(s)), false,
                       [&](int x){ emitCh(s, 0xA0, (uint8_t)note_[v], (uint8_t)x); });
        }

        // --- active sensing (0xFE every 300 ms; harness-untestable) -----------
        if (gateHigh(in("activesensing").value(s))) {
            long asTicks = std::max(1L, std::lround(kActiveSensingMs * rate / 1000.0));
            if (!asStarted_) { asStarted_ = true; asLast_ = s.tick; }
            else if ((int64_t)(s.tick - asLast_) >= asTicks) { emit(s, 0xFE); asLast_ = s.tick; }
        } else asStarted_ = false;
    }
};

constexpr uint8_t MidiOut::kPedalCC[5];
constexpr const char* MidiOut::kPedalJack[5];

DROID_REGISTER_CIRCUIT(midiout, MidiOut)

} // namespace droid
