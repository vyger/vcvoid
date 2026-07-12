// recorder — Record and playback CVs, gates and a number. Spec:
// manual/circuits/recorder.md. A transport-driven virtual tape (shared concept
// with delay/cvlooper) recording one CV (-1..+1), eight gate tracks (0/1) and one
// integer (0..255) in parallel.
//
// Transport: STOP (input passthrough), PLAY (tape -> outputs), RECORD (passthrough
// AND write to tape). Driven either by the `mode` input (0/1/2 — buttons ignored)
// or, if mode is unpatched, by the recordbutton/playbutton/stopbutton triggers
// (record toggles record on/off; play (re)starts from the beginning; stop -> idle).
// Extras: loop, pause (a gate), playbackspeed (incl. negative = backwards),
// scrub (+scrubposition), trim (start/end), triggered mode (`sample`) with a
// `timewindow` settling period, and the recordled/playled/stopled LEDs.
//
// Key documented difference: with `mode`, playback does NOT auto-stop at the end
// (it holds the last sample); with the buttons it auto-stops back to idle.
//
// SPEC-GAPs / simplifications (like delay & cvlooper; need hardware to verify):
//   * The manual's 256-sample adaptive tape (avg 1 sample / 20 ms, 1 mV error,
//     min 5.12 s) is modelled as a dense per-engine-tick tape sized from the tick
//     rate (16 s, floor 65536 ticks), so recordings are lossless up to the tape
//     length; `overflow` fires when the recording exceeds the tape, not at the
//     true adaptive-budget exhaustion point (not derivable from the manual).
//   * Fractional play positions floor-index the dense tape (no interpolation
//     between samples); integer speeds are exact.
//   * save/load/filenumber are SD-card I/O -> no-ops headless. display/header
//     drive a DB8E display -> ignored.
//   * Convention corners (manual silent): pause holds the current play position;
//     bypass forces the signal outputs but leaves the LEDs reflecting the state;
//     when `select` deselects the circuit its buttons are not processed and its
//     LED outputs are not driven; same-tick button-edge precedence is
//     stop < record < play.
//   * Literal reading of the manual: when `mode` is patched the three buttons
//     "(and LED outputs) are ignored", so recordled/playled/stopled are not
//     driven at all. And while `scrub` is 1 "the current state (play, record,
//     stop) ... is ignored" -- so the transport is suspended: recording appends
//     nothing to the tape during scrub and resumes when scrub is released.
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include <cmath>
#include <vector>

namespace droid {

class Recorder : public Circuit {
    static constexpr double kTapeSeconds = 16.0;   // > manual's 5.12 s guarantee
    static constexpr long   kMinCapTicks = 65536;  // generous floor at low rates
    enum { STOP = 0, PLAY = 1, RECORD = 2 };

public:
    void tick(EngineState& s) override {
        if (cvTape_.empty()) {
            cap_ = (long)std::llround(kTapeSeconds * s.tickRateHz);
            if (cap_ < kMinCapTicks) cap_ = kMinCapTicks;
            cvTape_.assign(cap_, 0.0f);
            gateTape_.assign(cap_, 0);
            numTape_.assign(cap_, 0);
        }

        // --- read live inputs ----------------------------------------------
        float   cvin = clampCv(in("cvin").value(s));
        int     numin = clampNum(in("numberin").value(s));
        uint8_t gates = 0;
        for (int k = 0; k < 8; k++)
            if (in("gatein", k + 1).value(s) >= kGateHighThreshold)
                gates |= uint8_t(1u << k);

        bool bypass = in("bypass").value(s) >= kGateHighThreshold;
        bool pause  = in("pause").value(s) >= kGateHighThreshold;
        bool loop   = in("loop").value(s) >= kGateHighThreshold;
        bool scrub  = in("scrub").value(s) >= kGateHighThreshold;
        double speed = in("playbackspeed").value(s);

        // clock: makes time inputs (timewindow) count in clock ticks.
        bool clockConn = in("clock").connected();
        if (clockConn && clockGate_.risingEdge(in("clock").value(s))) {
            if (haveClock_) period_ = double(s.tick - lastClock_);
            lastClock_ = s.tick; haveClock_ = true;
        }

        // select/selectat overlay (buttons + LEDs only).
        bool selConn = in("select").connected();
        bool selected = true;
        if (selConn) {
            float sel = in("select").value(s);
            if (in("selectat").connected())
                selected = std::lround(sel) == std::lround(in("selectat").value(s));
            else
                selected = sel >= kGateHighThreshold;
        }

        // SD save/load are no-ops headless, but consume the edges.
        (void)saveGate_.risingEdge(in("save").value(s));
        (void)loadGate_.risingEdge(in("load").value(s));

        // --- transport state -----------------------------------------------
        bool modeConn = in("mode").connected();
        bool startRecord = false, startPlay = false;
        if (modeConn) {
            long m = std::lround(in("mode").value(s));
            if (m < STOP) m = STOP; if (m > RECORD) m = RECORD;
            int ns = (int)m;
            if (ns == RECORD && state_ != RECORD) startRecord = true;
            if (ns == PLAY   && state_ != PLAY)   startPlay = true;
            state_ = ns;
        } else if (selected) {
            bool recEdge  = recBtn_.risingEdge(in("recordbutton").value(s));
            bool playEdge = playBtn_.risingEdge(in("playbutton").value(s));
            bool stopEdge = stopBtn_.risingEdge(in("stopbutton").value(s));
            if (stopEdge) state_ = STOP;
            if (recEdge) {
                if (state_ == RECORD) state_ = STOP;
                else { state_ = RECORD; startRecord = true; }
            }
            if (playEdge) { state_ = PLAY; startPlay = true; }
        }

        int outState = state_;                     // state driving THIS tick's output

        if (startRecord) { recLen_ = 0; recStarted_ = false; recWindowUntil_ = 0; }

        // --- recording ------------------------------------------------------
        // Suspend the transport while scrubbing: the manual's Scrubbing section
        // says "While `scrub` is 1, the current state (play, record, stop) of the
        // recorder is ignored. It is in scrub mode." Taken literally, recording
        // does not append to the tape during scrub; the RECORD state resumes when
        // scrub is released.
        if (state_ == RECORD && !pause && !scrub) {
            bool triggered = in("sample").connected();
            if (triggered) {
                if (sampleGate_.risingEdge(in("sample").value(s))) {
                    appendSample(s, cvin, gates, numin);
                    recStarted_ = true;
                    double tw = in("timewindow").value(s);
                    double twTicks = (clockConn && period_ > 0.0)
                                     ? tw * period_ : tw * (double)s.tickRateHz;
                    recWindowUntil_ = s.tick + (uint64_t)std::llround(twTicks < 0 ? 0 : twTicks);
                } else if (recStarted_ && s.tick < recWindowUntil_ && recLen_ > 0) {
                    long i = recLen_ - 1;           // adapt the just-created sample
                    cvTape_[i] = cvin; gateTape_[i] = gates; numTape_[i] = (uint8_t)numin;
                }
            } else {
                appendSample(s, cvin, gates, numin);
            }
        }

        // --- trimmed region over the recording -----------------------------
        double ts = clamp01(in("trimstart").value(s));
        double te = clamp01(in("trimend").value(s));
        double rs = ts * (double)recLen_;
        double re = te * (double)recLen_;
        if (re < rs) re = rs;
        int lo = (int)std::floor(rs), hi = (int)std::ceil(re) - 1;
        if (lo < 0) lo = 0;
        if (recLen_ > 0) {
            if (lo > recLen_ - 1) lo = (int)recLen_ - 1;
            if (hi > recLen_ - 1) hi = (int)recLen_ - 1;
        }
        if (hi < lo) hi = lo;

        if (startPlay) playPos_ = (speed >= 0.0) ? rs : (re - 1e-6);
        playEndedThisStart(startPlay);

        // --- compute the play/scrub sample for THIS tick -------------------
        float   cvPlay = 0.0f; uint8_t gatePlay = 0; int numPlay = 0;
        if (recLen_ > 0) {
            int idx;
            if (scrub) {
                double p = clamp01(in("scrubposition").value(s));
                idx = clampIdx(std::floor(rs + p * (re - rs)), lo, hi);
            } else {
                idx = clampIdx(std::floor(playPos_), lo, hi);
            }
            cvPlay = cvTape_[idx]; gatePlay = gateTape_[idx]; numPlay = numTape_[idx];
        }

        // --- advance the play head (unless scrubbing/paused/ended) ----------
        if (outState == PLAY && !scrub && !pause && !playEnded_ && recLen_ > 0) {
            playPos_ += speed;
            bool past = (speed >= 0.0) ? (playPos_ >= re) : (playPos_ < rs);
            if (past) {
                double len = re - rs;
                if (loop && len > 0.0) {
                    while (playPos_ >= re) playPos_ -= len;
                    while (playPos_ < rs)  playPos_ += len;
                } else if (modeConn) {
                    playEnded_ = true;
                    playPos_ = (speed >= 0.0) ? (double)hi : (double)lo;
                } else {
                    state_ = STOP;                 // buttons: auto-stop at the end
                }
            }
        }

        // --- output stage ---------------------------------------------------
        float cvO; uint8_t gateO; int numO;
        if (bypass) {
            cvO = cvin; gateO = gates; numO = numin;
        } else if (scrub) {
            cvO = cvPlay; gateO = gatePlay; numO = numPlay;
        } else if (outState == PLAY) {
            cvO = cvPlay; gateO = gatePlay; numO = numPlay;
        } else {                                   // STOP or RECORD: passthrough
            cvO = cvin; gateO = gates; numO = numin;
        }

        out("cvout").set(s, cvO);
        out("numberout").set(s, (float)numO);
        for (int k = 0; k < 8; k++)
            out("gateout", k + 1).set(s, (gateO & (1u << k)) ? 1.0f : 0.0f);
        out("overflow").set(s, s.tick < overflowUntil_ ? 1.0f : 0.0f);

        // LEDs: scrub forces play-lit; otherwise reflect the tick's state. Not
        // driven when `mode` is patched (the manual: "If you patch `mode`, the
        // three buttons (and LED outputs) are ignored.") nor while the circuit is
        // deselected -- in both cases the LEDs are left for another circuit.
        if (!modeConn && (!selConn || selected)) {
            bool rl = !scrub && outState == RECORD;
            bool pl = scrub || outState == PLAY;
            bool sl = !scrub && outState == STOP;
            out("recordled").set(s, rl ? 1.0f : 0.0f);
            out("playled").set(s, pl ? 1.0f : 0.0f);
            out("stopled").set(s, sl ? 1.0f : 0.0f);
        }
    }

private:
    void appendSample(EngineState& s, float cv, uint8_t g, int num) {
        if (recLen_ < cap_) {
            cvTape_[recLen_] = cv; gateTape_[recLen_] = g; numTape_[recLen_] = (uint8_t)num;
            recLen_++;
        } else {
            overflowUntil_ = s.tick + (uint64_t)std::llround(0.5 * (double)s.tickRateHz);
        }
    }
    void playEndedThisStart(bool startPlay) { if (startPlay) playEnded_ = false; }

    static float clampCv(float v) { return v > 1.0f ? 1.0f : (v < -1.0f ? -1.0f : v); }
    static int clampNum(float v) {
        long n = std::lround(v);
        if (n < 0) n = 0; if (n > 255) n = 255;
        return (int)n;
    }
    static double clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }
    static int clampIdx(double pos, int lo, int hi) {
        long i = (long)pos;
        if (i < lo) i = lo; if (i > hi) i = hi;
        return (int)i;
    }

    std::vector<float>   cvTape_;
    std::vector<uint8_t> gateTape_;
    std::vector<uint8_t> numTape_;
    long cap_ = 0, recLen_ = 0;
    int  state_ = STOP;
    double playPos_ = 0.0;
    bool playEnded_ = false, recStarted_ = false, haveClock_ = false;
    uint64_t lastClock_ = 0, recWindowUntil_ = 0, overflowUntil_ = 0;
    double period_ = 0.0;
    GateReader recBtn_, playBtn_, stopBtn_, sampleGate_, clockGate_, saveGate_, loadGate_;
};

DROID_REGISTER_CIRCUIT(recorder, Recorder)

} // namespace droid
