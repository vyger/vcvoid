// polytool — Change number of voices in polyphonic setups.
// Spec: manual/circuits/polytool.md. A voice-allocation "transformer": maps up
// to 16 input voices (pitchinput/gateinput pairs) onto up to 16 output voices
// (pitchoutput/gateoutput pairs), like a MIDI-to-CV interface.
//
// Model: each patched input voice is monophonic (gate high = one active note).
//   - gateinput rising edge  -> a new note; allocate a free output for it.
//   - gateinput falling edge -> note off; its output's gate goes low.
//   - while held & assigned  -> the output tracks that voice's live pitch.
// Free-output search (per rising edge):
//   - roundrobin=0: scan active outputs from the first, take the lowest free.
//   - roundrobin=1: scan in a rotating fashion starting after the last assigned.
// When ALL outputs are busy, `voiceallocation` chooses the victim to steal
// (roundrobin has no influence here, per the manual):
//   0 oldest (default), 1 omit the new note, 2 lowest pitch, 3 highest pitch.
//
// SPEC-GAPs (manual specifies the allocation rules but is silent on these
// corners; literal readings taken):
//   - note-off holds the last pitch on the output (only the gate drops);
//   - an output tracks its held voice's live pitch (a glide passes through);
//   - a stolen/omitted note is dropped permanently — it is NOT restored when an
//     output later frees; only a fresh rising edge re-allocates;
//   - within one tick, note-offs are processed before note-ons, and simultaneous
//     rising edges are allocated in ascending input-voice index order;
//   - lowest/highest-pitch ties resolve to the lowest output index;
//   - the round-robin pointer starts at the first output (first note -> out1).
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include <cmath>
#include <vector>

namespace droid {

class Polytool : public Circuit {
public:
    void tick(EngineState& s) override {
        // Active output slots: any k with pitchoutput_k or gateoutput_k patched.
        std::vector<int> outs;
        for (int k = 1; k <= 16; k++)
            if (out("pitchoutput", k).connected() || out("gateoutput", k).connected())
                outs.push_back(k - 1);            // store 0-based slot index

        // Read gate levels for the 16 input voices (a voice needs a patched gate
        // to produce notes).
        bool gate[16];
        for (int v = 0; v < 16; v++) {
            const Input& g = in("gateinput", v + 1);
            gate[v] = g.connected() && g.value(s) >= kGateHighThreshold;
        }

        // 1. Note-offs first, freeing outputs for same-tick note-ons.
        for (int v = 0; v < 16; v++) {
            if (prevGate_[v] && !gate[v]) {
                int k = inAssigned_[v];
                if (k >= 0) { outOwner_[k] = -1; inAssigned_[v] = -1; }
            }
        }

        // 2. Note-ons, in ascending voice index.
        int va = (int)std::lround(in("voiceallocation").value(s));
        bool rr = in("roundrobin").value(s) >= kGateHighThreshold;
        for (int v = 0; v < 16; v++)
            if (!prevGate_[v] && gate[v])
                allocate(s, v, outs, va, rr);

        // 3. Drive outputs.
        for (int k : outs) {
            int v = outOwner_[k];
            if (v >= 0) heldPitch_[k] = in("pitchinput", v + 1).value(s);
            out("pitchoutput", k + 1).set(s, heldPitch_[k]);
            out("gateoutput", k + 1).set(s, v >= 0 ? 1.0f : 0.0f);
        }

        for (int v = 0; v < 16; v++) prevGate_[v] = gate[v];
    }

private:
    void allocate(EngineState& s, int v, const std::vector<int>& outs,
                  int va, bool rr) {
        int M = (int)outs.size();
        if (M == 0) return;

        // Find a free output.
        int pos = -1;                              // position within `outs`
        for (int i = 0; i < M; i++) {
            int idx = rr ? (rrNext_ + i) % M : i;
            if (outOwner_[outs[idx]] == -1) { pos = idx; break; }
        }

        if (pos < 0) {
            // All busy -> voiceallocation decides the victim (roundrobin ignored).
            if (va == 1) return;                   // omit the new note
            pos = 0;
            for (int i = 1; i < M; i++) {
                int a = outs[pos], b = outs[i];
                if (va == 2) {                     // steal lowest pitch
                    if (currentPitch(s, b) < currentPitch(s, a)) pos = i;
                } else if (va == 3) {              // steal highest pitch
                    if (currentPitch(s, b) > currentPitch(s, a)) pos = i;
                } else {                           // 0/default: steal oldest
                    if (outSeq_[b] < outSeq_[a]) pos = i;
                }
            }
            int old = outOwner_[outs[pos]];
            if (old >= 0) inAssigned_[old] = -1;   // drop the stolen note
        }

        int k = outs[pos];
        outOwner_[k] = v;
        inAssigned_[v] = k;
        outSeq_[k] = nextSeq_++;
        rrNext_ = (pos + 1) % M;
    }

    float currentPitch(EngineState& s, int k) {
        int v = outOwner_[k];
        return v >= 0 ? in("pitchinput", v + 1).value(s) : heldPitch_[k];
    }

    bool     prevGate_[16]   = {};
    int      outOwner_[16]   = { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 };
    int      inAssigned_[16] = { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 };
    uint64_t outSeq_[16]     = {};
    float    heldPitch_[16]  = {};
    uint64_t nextSeq_        = 0;
    int      rrNext_         = 0;
};

DROID_REGISTER_CIRCUIT(polytool, Polytool)

} // namespace droid
