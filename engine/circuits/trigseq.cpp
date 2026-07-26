// trigseq — declarative trigger sequencer. EXPERIMENTAL: vcvoid only, not a
// DROID firmware circuit (see docs/adr/0001-experimental-circuits.md). Spec:
// manual/circuits/experimental/trigseq.md.
//
// The rhythm is written as text — `pattern = "x...x.x."` — and each clock
// advances one character. `.`, space, `-` and `_` are rests; EVERY other
// character emits, so `0` and `|` emit too (no validation, no error path: the
// firmware's house style is to clamp or accept, never to reject at runtime).
//
// Behaviour is euklid's wherever the two overlap, deliberately, so that knowing
// one teaches the other (euklid.cpp is the reference):
//   * on an emitting step the clock is passed through to `output` with its
//     exact voltage and gate length; on a rest step to `offbeats`;
//   * `outputsignal`, when patched, is held for the whole active step instead
//     of the gated clock, and is read live rather than sampled;
//   * `reset` arms a restart consumed by the next clock, applied before the
//     clock within a tick, so a simultaneous reset+clock lands on step 1;
//   * nothing is emitted before the first clock.
//
// Chaining follows `sequencer`'s chaintonext (CONCATENATION into a longer
// sequence), not motoquencer's linktonext (parallel lanes). The head owns the
// clock, reset, outputsignal and both outputs; followers contribute only their
// pattern. chaintonext is dynamic, and the step counter is monotone, so a chain
// broken mid-run folds the position modulo the new total length.
//
// The pattern is NOT compiled or copied. A text costs the master 6 bytes as a
// pointer + length whether or not we duplicate it, so packing the steps into a
// bitfield would add storage rather than save it — and would impose a step
// ceiling that no hardware constraint asks for. trigseq keeps a resolved text
// number and a step counter, and indexes the string. Text is a compile-time
// constant on hardware, so the pattern is resolved once from the input's
// literal binding (the `encoder`/`encoderbank` primary() idiom) and cannot
// change while the patch runs.
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include <string>

namespace droid {

namespace {

const std::string kNoText;

// Rest characters: everything else emits. `0` and `|` are NOT rests — the
// manual page says so explicitly, because both are plausible things to type.
inline bool isRest(char c) {
    return c == '.' || c == ' ' || c == '-' || c == '_';
}

} // namespace

class TrigSeq : public Circuit {
public:
    void tick(EngineState& s) override {
        // --- the (possibly chained) pattern list ---------------------------
        // Rebuilt per tick because chaintonext is dynamic; the walk is short
        // (one link per chained instance) and touches no strings.
        chain_.clear();
        Circuit* c = this;
        long total = 0;
        for (;;) {
            const std::string& p = static_cast<TrigSeq*>(c)->patternOf(s);
            if (!p.empty()) {                      // empty patterns are transparent
                chain_.push_back(&p);
                total += (long)p.size();
            }
            if (c->in("chaintonext").value(s) < kGateHighThreshold) break;
            Circuit* nxt = c->nextPeer();
            if (!nxt || !nxt->def || nxt->def != def) break;   // same circuit type only
            c = nxt;
        }

        // --- transport (euklid: reset arms, clock consumes) -----------------
        if (resetGate_.risingEdge(in("reset").value(s))) step_ = -1;
        if (clockGate_.risingEdge(in("clock").value(s))) step_++;

        bool active = false, haveStep = false;
        if (step_ >= 0 && total > 0) {
            haveStep = true;
            long pos = step_ % total;               // monotone counter folds to
            for (const std::string* p : chain_) {   // the CURRENT total length
                if (pos < (long)p->size()) { active = !isRest((*p)[(size_t)pos]); break; }
                pos -= (long)p->size();
            }
        }

        // --- outputs (euklid's exact contract) ------------------------------
        if (in("outputsignal").connected()) {
            // Held for the whole step (not gated by the clock), read live.
            float sig = in("outputsignal").value(s);
            out("output").set(s,   (haveStep && active)  ? sig : 0.0f);
            out("offbeats").set(s, (haveStep && !active) ? sig : 0.0f);
        } else {
            // Pass the clock through (voltage + gate length).
            float clock = in("clock").value(s);
            out("output").set(s,   (haveStep && active)  ? clock : 0.0f);
            out("offbeats").set(s, (haveStep && !active) ? clock : 0.0f);
        }
    }

private:
    // Resolve `pattern` exactly once, from the input's LITERAL binding rather
    // than its runtime value: text is a compile-time constant on hardware, so
    // the pattern cannot change while the patch runs. Same idiom as
    // encoder.cpp's resolveEncoder. Unpatched, or a text number outside the
    // table, yields the empty pattern (an inert circuit).
    const std::string& patternOf(EngineState& s) {
        if (!resolved_) {
            resolved_ = true;
            const Input& in_ = in("pattern");
            const Operand& op = in_.primary();
            if (in_.connected() && op.kind == Operand::Kind::Const && s.texts) {
                long n = (long)op.constant;
                if (n >= 1 && n < (long)s.texts->size()) pattern_ = &(*s.texts)[(size_t)n];
            }
        }
        return pattern_ ? *pattern_ : kNoText;
    }

    const std::string* pattern_ = nullptr;
    bool resolved_ = false;
    long step_ = -1;                       // -1 = no clock yet / restart armed
    GateReader clockGate_, resetGate_;
    std::vector<const std::string*> chain_;   // scratch, reused every tick
};

DROID_REGISTER_CIRCUIT(trigseq, TrigSeq)

} // namespace droid
