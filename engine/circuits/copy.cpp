// copy — Copy a signal, while applying attenuation and offset.
// Spec: manual/circuits/copy.md. All scaling is the standard A*B+C input math.
#include "../src/registry.hpp"

namespace droid {

class Copy : public Circuit {
public:
    void tick(EngineState& s) override {
        out("output").set(s, in("input").value(s));
    }
};

DROID_REGISTER_CIRCUIT(copy, Copy)

} // namespace droid
