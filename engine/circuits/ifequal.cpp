// ifequal — Check if two values are equal. Spec: manual/circuits/ifequal.md.
// A lightweight `compare`: outputs `ifequal` when input1 is EXACTLY equal to
// input2, otherwise `else`. Standard A*B+C input math applies.
#include "../src/registry.hpp"

namespace droid {

class Ifequal : public Circuit {
public:
    void tick(EngineState& s) override {
        bool eq = in("input1").value(s) == in("input2").value(s);
        out("output").set(s, (eq ? in("ifequal") : in("else")).value(s));
    }
};

DROID_REGISTER_CIRCUIT(ifequal, Ifequal)

} // namespace droid
