// midifileplayer — MIDI file player. Spec: manual/circuits/midifileplayer.md.
//
// Plays one track of a Standard MIDI File from the SD card (s.fileProvider —
// Rack: midi<N>.mid next to the loaded droid.ini; goldens: the `midifile`
// directive). Voice outputs, note gates, CCs, pitchbend, program/bank,
// modwheel/volume and the portamento/soft pedal states mirror midiin; the
// event SOURCE is the parsed file played back on a tick-accurate position
// instead of a live stream. No X7 needed (master-only binding).
//
// Engine conventions (same as midiin): pitch = (note-24)/120 engine units
// (note 60 -> 0.3); velocity/CC = value/127; per-note `trigger` = 5 ms; all
// other triggers (clockout, midiclock, endoftrack, cctrigger, programchange)
// = 10 ms; pitch bend normalized asymmetrically (16383 -> +range, 0 -> -range).
//
// Playback model: position advances in file ticks; rate = division *
// (1e6/usPerQuarter) / tickRateHz * speed. Set Tempo metas from ALL tracks
// (merged tempo map — format-1 files keep tempo in a meta-only track) apply
// as the position passes their tick. External clock (input `clock` patched):
// speed and file tempo are ignored; edge n snaps the position to (n-1)*q16
// (q16 = division/4) and then advances smoothly toward n*q16 at the measured
// edge-period rate, never crossing the boundary early (CHOSEN READING — the
// manual pins one 16th per clock with full resolution but no algorithm).
//
// Errors (output `error`): -1 file/card missing, 1 corrupt, 0.75 no non-empty
// track, 0.25 track chunk > 6000 bytes.
//
// CHOSEN READINGS where the manual is silent (also in circuits-status.yaml):
//   * `reset` also releases all held notes before restarting from 0.
//   * On loop wrap the noteoff-at-end and noteon-at-0 land on the same engine
//     tick (no gap) — legato, consistent with notegap's default-0 semantics.
//   * notegap is NOT modelled (default-0 legato is what we emit), matching
//     the midiin implementation.
//   * Track selection counts tracks with at least one CHANNEL event as
//     "non-empty" ("tracks that just contain meta information" don't count).
//   * The file's channels are ignored (manual) unless `channel` is patched,
//     which filters events to that channel (manual `channel` input).
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include "../src/midivoices.hpp"
#include "../src/smf.hpp"
#include <cmath>
#include <cstdint>
#include <cstring>

namespace droid {

class MidiFilePlayer : public Circuit {
    static constexpr uint32_t kMaxTrackBytes = 6000;

    static float notePitch(int note) { return (float(note) - 24.0f) * (0.1f / 12.0f); }
    static bool gateHigh(float v) { return v >= kGateHighThreshold; }

    // --- loaded file/track ---------------------------------------------------
    SmfFile smf_;
    int loadedFile_ = -1, loadedTrack_ = -1;   // -1 = nothing loaded yet
    int trackIdx_ = -1;                         // index into smf_.tracks; -1 = none
    float error_ = 0.0f;
    bool playable_ = false;

    // --- playback position ---------------------------------------------------
    double pos_ = 0.0;                          // file ticks
    size_t evIdx_ = 0;                          // next event in the track
    size_t tempoIdx_ = 0;                       // next tempo-map entry
    uint32_t usPerQuarter_ = 500000;
    bool stopped_ = false;                      // reached end with loop off

    // External clock.
    GateReader clockIn_;
    uint32_t clockEdges_ = 0;
    int64_t lastEdgeTick_ = -1;
    double clockRate_ = 0.0;                    // file ticks per engine tick

    GateReader resetIn_;

    // --- steady clocks / transport triggers ----------------------------------
    double nextClockOut_ = 0.0, nextMidiClock_ = 0.0;
    uint64_t clockOutOff_ = 0, midiClockOff_ = 0, eotOff_ = 0;

    // --- voices (mirrors midiin) ----------------------------------------------
    MidiVoiceAllocator alloc_;
    int numVoices_ = -1;
    int lastRr_ = -1, lastVa_ = -1;
    bool struck_[8] = {false};
    float pressure_[8] = {0};
    uint64_t voiceTrigOff_[8] = {0};

    // --- controllers / note gates ---------------------------------------------
    uint8_t heldVel_[128] = {0};
    float ccVal_[4] = {0};
    uint64_t ccTrigOff_[4] = {0}, pcOff_ = 0;
    float modwheel_ = 0, volume_ = 0, pitchbendNorm_ = 0;
    int program_ = 0, bankMSB_ = 0, bankLSB_ = 0;
    bool portamentoDown_ = false, softDown_ = false;

    long trig5_ = 1, trig10_ = 1;

    // --- helpers ---------------------------------------------------------------
    int computeNumVoices() {
        static const char* jacks[] = {"pitch", "gate", "velocity", "pressure", "trigger"};
        int n = 0;
        for (const char* j : jacks)
            for (int i = 1; i <= 8; i++)
                if (out(j, i).connected() && i > n) n = i;
        return n;
    }

    // Pick the played track: `track` counts non-empty tracks only; < 1 -> 1;
    // beyond the end -> the last non-empty track.
    int pickTrack(int wanted) const {
        if (wanted < 1) wanted = 1;
        int count = 0, last = -1;
        for (size_t i = 0; i < smf_.tracks.size(); i++) {
            if (!smf_.tracks[i].hasChannelEvents) continue;
            last = (int)i;
            if (++count == wanted) return (int)i;
        }
        return last;   // -1 when no non-empty track exists
    }

    void restart(EngineState& s) {
        pos_ = 0.0;
        evIdx_ = 0;
        tempoIdx_ = 0;
        usPerQuarter_ = 500000;
        stopped_ = false;
        nextClockOut_ = 0.0;
        nextMidiClock_ = 0.0;
        clockEdges_ = 0;
        lastEdgeTick_ = -1;
        clockRate_ = 0.0;
        alloc_.allNotesOff();
        std::memset(heldVel_, 0, sizeof heldVel_);
        (void)s;
    }

    void loadTrack(EngineState& s, int fileNum, int trackNum) {
        loadedFile_ = fileNum;
        loadedTrack_ = trackNum;
        playable_ = false;
        trackIdx_ = -1;
        error_ = 0.0f;
        smf_ = SmfFile{};
        std::vector<uint8_t> bytes;
        if (!s.fileProvider || !s.fileProvider(fileNum, bytes)) {
            error_ = -1.0f;                      // white: SD card / file missing
        } else if (!parseSmf(bytes.data(), bytes.size(), smf_)) {
            error_ = 1.0f;                       // magenta: corrupted
        } else if ((trackIdx_ = pickTrack(trackNum)) < 0) {
            error_ = 0.75f;                      // orange: no non-empty track
        } else if (smf_.tracks[trackIdx_].byteLength > kMaxTrackBytes) {
            error_ = 0.25f;                      // cyan: track too long
            trackIdx_ = -1;
        } else {
            playable_ = true;
        }
        restart(s);
    }

    // Apply tempo-map entries the position has passed.
    void applyTempo() {
        while (tempoIdx_ < smf_.tempoMap.size() &&
               smf_.tempoMap[tempoIdx_].tick <= pos_)
            usPerQuarter_ = smf_.tempoMap[tempoIdx_++].usPerQuarter;
    }

    void processEvent(EngineState& s, const SmfEvent& e) {
        if (in("channel").connected()) {
            int ch = (e.status & 0x0f) + 1;
            if ((int)std::lround(in("channel").value(s)) != ch) return;
        }
        uint8_t hi = e.status & 0xf0;
        switch (hi) {
            case 0x90:
                if (e.data2 == 0) { noteOff(e.data1); break; }
                noteOn(s, e.data1, e.data2);
                break;
            case 0x80: noteOff(e.data1); break;
            case 0xa0:                                   // poly key pressure
                for (int i = 0; i < numVoices_; i++) {
                    const auto& v = alloc_.voice(i);
                    if (v.gate && v.note == e.data1) pressure_[i] = e.data2 / 127.0f;
                }
                break;
            case 0xd0:                                   // channel pressure
                for (int i = 0; i < numVoices_; i++) pressure_[i] = e.data1 / 127.0f;
                break;
            case 0xb0: handleCC(s, e.data1, e.data2); break;
            case 0xc0: program_ = e.data1; pcOff_ = s.tick + trig10_; break;
            case 0xe0: {
                int v14 = int(e.data1) | (int(e.data2) << 7);
                pitchbendNorm_ = v14 >= 8192 ? (v14 - 8192) / 8191.0f
                                             : (v14 - 8192) / 8192.0f;
                break;
            }
            default: break;
        }
    }

    void noteOn(EngineState& s, int note, int vel) {
        if (note < 0 || note > 127) return;   // heldVel_ is [128]; never index OOB
        heldVel_[note] = (uint8_t)vel;
        int lo = (int)std::lround(in("lowestnote").value(s));
        int hi = (int)std::lround(in("highestnote").value(s));
        if (note < lo || note > hi) return;              // note gates unaffected
        int v = alloc_.noteOn((uint8_t)note, (uint8_t)vel);
        if (v >= 0) { struck_[v] = true; voiceTrigOff_[v] = s.tick + trig5_; }
    }

    void noteOff(int note) {
        if (note < 0 || note > 127) return;   // heldVel_ is [128]; never index OOB
        heldVel_[note] = 0;
        alloc_.noteOff((uint8_t)note);
    }

    void handleCC(EngineState& s, int num, int val) {
        if (num == 0)  bankMSB_ = val;
        if (num == 32) bankLSB_ = val;
        if (num == 1)  modwheel_ = val / 127.0f;
        if (num == 7)  volume_ = val / 127.0f;
        if (num == 65) portamentoDown_ = val >= 64;
        if (num == 67) softDown_ = val >= 64;
        for (int k = 0; k < 4; k++)
            if ((int)std::lround(in("ccnumber", k + 1).value(s)) == num) {
                ccVal_[k] = val / 127.0f;
                ccTrigOff_[k] = s.tick + trig10_;
            }
    }

    // Fire every event with tick <= pos (track order).
    void fireDue(EngineState& s) {
        if (trackIdx_ < 0) return;
        const auto& evs = smf_.tracks[trackIdx_].events;
        while (evIdx_ < evs.size() && (double)evs[evIdx_].tick <= pos_)
            processEvent(s, evs[evIdx_++]);
    }

public:
    void tick(EngineState& s) override {
        trig5_  = std::max(1L, std::lround(0.005 * s.tickRateHz));
        trig10_ = std::max(1L, std::lround(0.01  * s.tickRateHz));
        if (numVoices_ < 0) numVoices_ = computeNumVoices();

        int rr = gateHigh(in("roundrobin").value(s)) ? 1 : 0;
        int va = (int)std::lround(in("voiceallocation").value(s));
        if (rr != lastRr_ || va != lastVa_) {
            alloc_.configure(numVoices_, rr != 0, va);
            lastRr_ = rr; lastVa_ = va;
        }

        // (Re)load on file/track change; hardware also reloads on start.
        int fileNum = (int)std::lround(in("file").value(s));
        int trackNum = (int)std::lround(in("track").value(s));
        if (fileNum != loadedFile_ || trackNum != loadedTrack_)
            loadTrack(s, fileNum, trackNum);

        out("error").set(s, error_);

        if (playable_) {
            if (resetIn_.risingEdge(in("reset").value(s))) restart(s);

            const SmfTrack& trk = smf_.tracks[trackIdx_];
            double q16 = smf_.division / 4.0;
            // Playing end: `end` in quarters when patched, else the natural EOT.
            double endTicks = in("end").connected()
                ? std::lround(in("end").value(s)) * (double)smf_.division
                : (double)trk.lengthTicks;

            if (!stopped_) {
                // ---- advance the position --------------------------------
                if (in("clock").connected()) {
                    if (clockIn_.risingEdge(in("clock").value(s))) {
                        clockEdges_++;
                        if (lastEdgeTick_ >= 0) {
                            double period = double((int64_t)s.tick - lastEdgeTick_);
                            if (period > 0) clockRate_ = q16 / period;
                        }
                        lastEdgeTick_ = (int64_t)s.tick;
                        double snap = double(clockEdges_ - 1) * q16;
                        if (pos_ < snap) pos_ = snap;
                        fireDue(s);
                    }
                    if (clockEdges_ > 0) {
                        double cap = double(clockEdges_) * q16 - 1e-6;
                        pos_ = std::fmin(pos_ + clockRate_, cap);
                    }
                } else {
                    applyTempo();
                    float speed = in("speed").value(s);
                    if (speed < 0.0f) speed = 0.0f;
                    double rate = (double)smf_.division * (1e6 / (double)usPerQuarter_)
                                  / (double)s.tickRateHz * (double)speed;
                    pos_ += rate;
                }
                fireDue(s);

                // ---- steady clock outputs --------------------------------
                while (pos_ >= nextClockOut_)
                    { clockOutOff_ = s.tick + trig10_; nextClockOut_ += q16; }
                double qMidi = smf_.division / 24.0;
                while (pos_ >= nextMidiClock_)
                    { midiClockOff_ = s.tick + trig10_; nextMidiClock_ += qMidi; }

                // ---- end of track / loop ----------------------------------
                if (endTicks > 0 && pos_ >= endTicks) {
                    eotOff_ = s.tick + trig10_;
                    if (gateHigh(in("loop").value(s))) {
                        pos_ -= endTicks;
                        evIdx_ = 0;
                        tempoIdx_ = 0;
                        nextClockOut_ = 0.0;
                        nextMidiClock_ = 0.0;
                        fireDue(s);
                        nextClockOut_ = q16;      // position-0 clocks just fired above
                        nextMidiClock_ = qMidi;
                        clockOutOff_ = s.tick + trig10_;
                        midiClockOff_ = s.tick + trig10_;
                    } else {
                        stopped_ = true;
                    }
                }
            } else if (resetIn_.risingEdge(in("reset").value(s))) {
                restart(s);
                fireDue(s);
            }
        }

        // ---- outputs (mirrors midiin) -----------------------------------------
        float transpose = in("transpose").value(s);
        float pbr = in("pitchbendrange").value(s);
        float pbCv = pitchbendNorm_ * pbr;
        bool bendpitch = gateHigh(in("bendpitch").value(s));
        bool tuning = gateHigh(in("tuningmode").value(s));
        bool holdvel = gateHigh(in("holdvelocity").value(s));
        float tuningpitch = in("tuningpitch").value(s);

        out("pitchbend").set(s, pbCv);

        long tuneP = std::max(1L, std::lround(0.5 * s.tickRateHz));   // 120 BPM
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

        out("clockout").set(s, s.tick < clockOutOff_ ? 1.0f : 0.0f);
        out("midiclock").set(s, s.tick < midiClockOff_ ? 1.0f : 0.0f);
        out("endoftrack").set(s, s.tick < eotOff_ ? 1.0f : 0.0f);

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
                         ? (int)std::lround(in("note", k + 1).value(s)) : k;
            bool held = note >= 0 && note < 128 && heldVel_[note] > 0;
            out("notegate", k + 1).set(s, held ? 1.0f : 0.0f);
            out("notegatevelocity", k + 1).set(s, held ? heldVel_[note] / 127.0f : 0.0f);
        }
    }
};

DROID_REGISTER_CIRCUIT(midifileplayer, MidiFilePlayer)

} // namespace droid
