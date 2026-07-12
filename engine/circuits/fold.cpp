// fold — CV folder: keep a CV within [minimum, maximum] by adding/subtracting
// `foldby` (rather than clamping). Spec: manual/circuits/fold.md. Below minimum,
// fold up until >= minimum; above maximum, fold down until <= maximum -- but
// folding down never crosses back below minimum, so folding UP takes precedence
// (the two documented anomalies: range < foldby, and minimum > maximum).
// Unpatched minimum/maximum apply no bound on that side. Standard A*B+C math.
#include "../src/registry.hpp"
#include <cmath>

namespace droid {

class Fold : public Circuit {
public:
    void tick(EngineState& s) override {
        float x = in("input").value(s);
        float foldby = in("foldby").value(s);
        Input& minI = in("minimum");
        Input& maxI = in("maximum");
        bool hasMin = minI.connected();
        bool hasMax = maxI.connected();

        if (foldby > 0.0f) {          // negative/zero foldby -> no folding
            float mn = minI.value(s);
            float mx = maxI.value(s);
            // fold up to the first value >= minimum
            if (hasMin && x < mn)
                x += std::ceil((mn - x) / foldby) * foldby;
            // fold down to the first value <= maximum, but never below minimum
            if (hasMax && x > mx) {
                float k = std::ceil((x - mx) / foldby);
                if (hasMin) {
                    float kMin = std::floor((x - mn) / foldby);  // keep >= minimum
                    if (k > kMin) k = kMin;
                }
                if (k > 0.0f) x -= k * foldby;
            }
        }
        out("output").set(s, x);
    }
};

DROID_REGISTER_CIRCUIT(fold, Fold)

} // namespace droid
