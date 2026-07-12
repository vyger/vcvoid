// bernoulli — Random gate distributor ("bernoulli gate").
// Spec: manual/circuits/bernoulli.md. On each positive trigger edge at `input`
// a random decision routes the signal to output1 (probability = distribution)
// or output2. The chosen output then emits an exact copy of `input` (even a
// non-trigger value such as an envelope); the other emits 0 V. The choice
// latches until the next rising edge. Standard A*B+C input math applies.
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include "../src/rng.hpp"

namespace droid {

class Bernoulli : public Circuit {
public:
    void tick(EngineState& s) override {
        float input = in("input").value(s);

        // Decide only on a rising edge; a draw is taken only then, so idle
        // ticks don't perturb the shared RNG for other circuits.
        if (inputGate_.risingEdge(input)) {
            float u = randUniform(s.rngState);
            // distribution is the probability of choosing output1; u in [0,1).
            // distribution >= 1 -> always output1; <= 0 -> always output2.
            toOutput1_ = u < in("distribution").value(s);
            decided_ = true;
        }

        // Before the first decision nothing is routed ("from now on" in the
        // manual): both outputs sit at 0 V. SPEC-GAP: the pre-first-edge state
        // is not documented; we emit 0/0 rather than leak input to a
        // preselected output. See tests/golden/bernoulli/no-routing-before-edge.
        if (!decided_) {
            out("output1").set(s, 0.0f);
            out("output2").set(s, 0.0f);
            return;
        }

        out("output1").set(s, toOutput1_ ? input : 0.0f);
        out("output2").set(s, toOutput1_ ? 0.0f : input);
    }

private:
    GateReader inputGate_;
    bool toOutput1_ = true;   // unobservable until decided_
    bool decided_ = false;
};

DROID_REGISTER_CIRCUIT(bernoulli, Bernoulli)

} // namespace droid
