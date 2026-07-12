// case — Priority selector: copy the value whose case condition is first
// non-zero. Spec: manual/circuits/case.md. case1..case16 are checked in order;
// the first non-zero one selects its paired value1..value16. If none is
// non-zero, `else` (default 0) is copied. Standard A*B+C input math applies.
//
// SPEC-GAP: the manual says "non-zero"; we take that literally (x != 0, so
// negatives and sub-0.1 fractions also select) rather than applying the 0.1
// gate-high threshold used elsewhere. See tests/golden/case/nonzero-literal.
#include "../src/registry.hpp"

namespace droid {

class Case : public Circuit {
public:
    void tick(EngineState& s) override {
        for (int i = 1; i <= 16; i++) {
            if (in("case", i).value(s) != 0.0f) {
                out("output").set(s, in("value", i).value(s));
                return;
            }
        }
        out("output").set(s, in("else").value(s));
    }
};

DROID_REGISTER_CIRCUIT(case, Case)

} // namespace droid
