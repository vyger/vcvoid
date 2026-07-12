#pragma once
// Polyphonic voice allocator for the midiin circuit. Pure, header-only, fixed
// storage (no EngineState/Rack/engine includes) — same shape as encodercore.hpp.
//
// Spec: manual/circuits/midiin.md — the two allocation parameters:
//
//   roundrobin (rr), line 339:
//     Default 0 — the search for a free output always starts at pitch1, so "the
//     notes played first will always be played at the lowest numbered outputs".
//     1 — outputs are scanned round-robin "like a rotating switch"; every output
//     gets an equal chance at the next note. "When all outputs are currently
//     used by a note, roundrobin has no influence. Here voiceallocation selects
//     which of the notes will be dropped."
//
//   voiceallocation (va), line 340 — when more notes are needed than voices:
//     0  the OLDEST currently-playing note is cancelled (default)
//     1  the new note is not played and simply omitted
//     2  the LOWEST note is cancelled
//     3  the HIGHEST note is cancelled
//
//   notegap (line 341) is a time-domain gate feature (milliseconds of forced
//   gate-low between retriggered notes), NOT an assignment rule — it lives in
//   the circuit's timing path, not here. Deliberately absent from this API.
//
// Chosen readings where the manual is silent (flagged in the task report):
//   * A noteOn for a note already held retriggers the SAME voice (updating
//     velocity); at most one active voice per note number. This keeps noteOff
//     unambiguous.
//   * noteOff releases only the voice whose held note matches.
//   * In va modes 2/3 the lowest/highest currently-playing note is cancelled
//     literally and the new note takes that voice.
//   * A voice is "free" for allocation when its gate is low. A note that ended
//     (gate low) but is still ringing out an external release is reusable — this
//     is what lets roundrobin "transform a melody into chords" (line 339).
#include <cstdint>

namespace droid {

struct MidiVoiceAllocator {
    static constexpr int kMaxVoices = 8;   // midiin exposes up to 8 pitch/gate outs

    struct Voice { bool gate = false; uint8_t note = 0; uint8_t velocity = 0; };

    // mode = voiceallocation value (0..3); roundRobin = roundrobin gate.
    void configure(int numVoices, bool roundRobin, int allocationMode) {
        n_ = numVoices < 0 ? 0 : (numVoices > kMaxVoices ? kMaxVoices : numVoices);
        rr_ = roundRobin;
        mode_ = allocationMode;
        next_ = 0;
    }

    // Returns the affected voice index (0-based), or -1 if the note is dropped
    // (va=1 omit, or no voices configured).
    int noteOn(uint8_t note, uint8_t velocity) {
        if (n_ == 0) return -1;

        // Same note already held -> retrigger the same voice.
        for (int i = 0; i < n_; i++)
            if (v_[i].gate && v_[i].note == note) { strike(i, note, velocity); return i; }

        // Look for a free (gate-low) voice.
        int idx = -1;
        if (rr_) {
            for (int k = 0; k < n_; k++) {
                int c = (next_ + k) % n_;
                if (!v_[c].gate) { idx = c; break; }
            }
        } else {
            for (int i = 0; i < n_; i++)
                if (!v_[i].gate) { idx = i; break; }
        }

        // All voices busy: apply voiceallocation to pick one to steal.
        if (idx < 0) {
            if (mode_ == 1) return -1;                 // omit the new note
            idx = pickSteal();
        }

        strike(idx, note, velocity);
        if (rr_) next_ = (idx + 1) % n_;
        return idx;
    }

    // Releases only the voice holding `note`; returns its index or -1.
    int noteOff(uint8_t note) {
        for (int i = 0; i < n_; i++)
            if (v_[i].gate && v_[i].note == note) { v_[i].gate = false; return i; }
        return -1;
    }

    void allNotesOff() {
        for (int i = 0; i < n_; i++) v_[i].gate = false;
    }

    const Voice& voice(int i) const { return v_[i]; }
    int numVoices() const { return n_; }

  private:
    Voice    v_[kMaxVoices];
    uint32_t age_[kMaxVoices] = {0};   // strike order stamp (for oldest-steal)
    int      n_ = 0;
    int      mode_ = 0;
    int      next_ = 0;                // roundrobin rotation pointer
    uint32_t clock_ = 0;
    bool     rr_ = false;

    void strike(int i, uint8_t note, uint8_t velocity) {
        v_[i].gate = true;
        v_[i].note = note;
        v_[i].velocity = velocity;
        age_[i] = ++clock_;
    }

    // Choose a busy voice to cancel (called only when all voices are busy and
    // mode != 1). va: 0 oldest, 2 lowest note, 3 highest note.
    int pickSteal() const {
        int best = 0;
        for (int i = 1; i < n_; i++) {
            if (mode_ == 2) {                       // lowest note
                if (v_[i].note < v_[best].note) best = i;
            } else if (mode_ == 3) {                // highest note
                if (v_[i].note > v_[best].note) best = i;
            } else {                                // 0 (default): oldest
                if (age_[i] < age_[best]) best = i;
            }
        }
        return best;
    }
};

} // namespace droid
