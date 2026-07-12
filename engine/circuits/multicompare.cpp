// multicompare — Compare an input against up to 16 values. Spec:
// manual/circuits/multicompare.md. The first patched compare_i that exactly
// equals `input` selects its paired ifequal_i; if none match, `else` (default 0)
// is output. Standard A*B+C input math applies.
#include "../src/registry.hpp"

namespace droid {

class Multicompare : public Circuit {
public:
    void tick(EngineState& s) override {
        float x = in("input").value(s);
        for (int i = 1; i <= 16; i++) {
            Input& c = in("compare", i);
            if (c.connected() && x == c.value(s)) {
                out("output").set(s, in("ifequal", i).value(s));
                return;
            }
        }
        out("output").set(s, in("else").value(s));
    }
};

DROID_REGISTER_CIRCUIT(multicompare, Multicompare)

} // namespace droid
