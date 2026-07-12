// adc — AD converter: encode a scaled input value as up to 12 gate bits,
// MSB first. Spec: manual/circuits/adc.md. The reverse circuit is `dac`.
// Standard A*B+C input math applies to input/minimum/maximum before we read
// them.
//
// Resolution N is the highest patched bit output ("the LSB is the highest
// output you actually patch"). The range [minimum, maximum) is divided into
// 2^N equal pieces: value = floor((input-min)/(max-min) * 2^N), clamped to
// [0, 2^N-1]. bit1 is the MSB, so bit k carries value bit (N-k). Because the
// MSB is simply "value >= 2^(N-1)" (i.e. input past the midpoint), adding
// more (lower) bits never changes bit1 — matching the manual.
#include "../src/registry.hpp"
#include <cmath>

namespace droid {

class Adc : public Circuit {
public:
    void tick(EngineState& s) override {
        // N = highest connected bit index. Non-contiguous patching (e.g. bit1
        // and bit3 patched, bit2 not) still yields N=3: we take the maximum
        // connected index. SPEC-GAP: the manual assumes contiguous bit1..bitN
        // patching; this is the literal reading of "highest output you patch".
        int n = 0;
        for (int k = 1; k <= 12; k++)
            if (out("bit", k).connected()) n = k;
        if (n == 0) return;   // nothing patched -> nothing to emit

        float input = in("input").value(s);
        float lo    = in("minimum").value(s);
        float hi    = in("maximum").value(s);

        long levels = 1L << n;          // 2^N equal pieces
        long value;
        if (input <= lo) {
            value = 0;                  // at/below minimum -> all zeros (the
                                        // manual's min rule is inclusive; in a
                                        // degenerate range min>=max it wins)
        } else if (input >= hi) {
            value = levels - 1;         // at/above maximum -> all ones
        } else {
            // hi > lo here (the two branches above absorb hi <= lo), so the
            // divisor is strictly positive and t lands in [0,1).
            float t = (input - lo) / (hi - lo);
            value = (long)std::floor(t * (float)levels);
            if (value < 0) value = 0;
            else if (value > levels - 1) value = levels - 1;
        }

        // Emit each connected bit; bit1 is the MSB.
        for (int k = 1; k <= n; k++) {
            Output& o = out("bit", k);
            if (!o.connected()) continue;
            long bit = (value >> (n - k)) & 1L;
            o.set(s, bit ? 1.0f : 0.0f);
        }
    }
};

DROID_REGISTER_CIRCUIT(adc, Adc)

} // namespace droid
