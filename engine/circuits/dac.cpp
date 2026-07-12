// dac — DA converter: decode up to 12 gate bits (MSB first) into a value in
// [minimum, maximum]. Spec: manual/circuits/dac.md. The reverse of `adc`.
// N = highest patched bit index ("the LSB is the highest input you patch");
// value = sum(bit_k * 2^(N-k)); output = minimum + value/(2^N-1) * (max-min).
// Standard A*B+C input math applies to minimum/maximum (and to each bit before
// the gate-high test).
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"

namespace droid {

class Dac : public Circuit {
public:
    void tick(EngineState& s) override {
        int n = 0;
        for (int k = 1; k <= 12; k++)
            if (in("bit", k).connected()) n = k;

        float lo = in("minimum").value(s);
        float hi = in("maximum").value(s);

        if (n == 0) { out("output").set(s, lo); return; }   // no bits -> minimum

        long value = 0;
        for (int k = 1; k <= n; k++)
            if (in("bit", k).value(s) >= kGateHighThreshold)
                value |= (1L << (n - k));      // bit1 is the MSB (weight 2^(N-1))

        long maxVal = (1L << n) - 1;           // all-ones -> maximum
        float frac = (float)value / (float)maxVal;
        out("output").set(s, lo + frac * (hi - lo));
    }
};

DROID_REGISTER_CIRCUIT(dac, Dac)

} // namespace droid
