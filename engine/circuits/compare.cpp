// compare — Three-way comparator. Spec: manual/circuits/compare.md. Compares
// `input` against `compare` (with an optional `precision` equality band) and
// outputs ifless / ifequal / ifgreater. Standard A*B+C input math applies.
//
// Smart defaults: an omitted ifless/ifequal/ifgreater uses the `else` value
// (which itself defaults to 0). Exception (manual "Omitted inputs"): if ALL
// four of ifless/ifequal/ifgreater/else are omitted, ifequal defaults to 1 so a
// bare compare is a plain equality check.
#include "../src/registry.hpp"
#include <cmath>

namespace droid {

class Compare : public Circuit {
public:
    void tick(EngineState& s) override {
        float diff      = in("input").value(s) - in("compare").value(s);
        float precision = in("precision").value(s);

        Input& ifless    = in("ifless");
        Input& ifequal   = in("ifequal");
        Input& ifgreater = in("ifgreater");
        Input& elseIn    = in("else");
        float  elseVal   = elseIn.value(s);   // default 0 when unconnected

        float result;
        if (std::fabs(diff) <= precision) {                 // equal (within band)
            if (ifequal.connected())
                result = ifequal.value(s);
            else if (!ifless.connected() && !ifgreater.connected() &&
                     !elseIn.connected())
                result = 1.0f;                              // all four omitted
            else
                result = elseVal;
        } else if (diff < 0.0f) {                           // input < compare
            result = ifless.connected() ? ifless.value(s) : elseVal;
        } else {                                            // input > compare
            result = ifgreater.connected() ? ifgreater.value(s) : elseVal;
        }
        out("output").set(s, result);
    }
};

DROID_REGISTER_CIRCUIT(compare, Compare)

} // namespace droid
