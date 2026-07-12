// logic — Logic operations utility. Spec: manual/circuits/logic.md. Each patched
// input1..input8 is "high" when its value >= threshold. Emits AND/OR/XOR/NAND/
// NOR over the patched inputs (as highvalue/lowvalue), the negate of input1, and
// counts of high/low inputs scaled by countvalue. Standard A*B+C input math.
#include "../src/registry.hpp"

namespace droid {

class Logic : public Circuit {
public:
    void tick(EngineState& s) override {
        float threshold = in("threshold").value(s);
        float lo = in("lowvalue").value(s);
        float hi = in("highvalue").value(s);
        float cval = in("countvalue").value(s);

        int nPatched = 0, nHigh = 0;
        for (int i = 1; i <= 8; i++) {
            Input& jack = in("input", i);
            if (!jack.connected()) continue;    // only patched inputs count
            nPatched++;
            if (jack.value(s) >= threshold) nHigh++;
        }
        int nLow = nPatched - nHigh;
        bool allHigh = (nHigh == nPatched);      // vacuously true if none patched
        bool anyHigh = (nHigh > 0);

        auto pick = [&](bool b) { return b ? hi : lo; };
        out("and").set(s, pick(allHigh));
        out("or").set(s, pick(anyHigh));
        out("xor").set(s, pick((nHigh & 1) != 0));   // high iff odd count
        out("nand").set(s, pick(!allHigh));
        out("nor").set(s, pick(!anyHigh));

        bool in1High = in("input", 1).value(s) >= threshold;
        out("negated").set(s, pick(!in1High));       // negate of input1 only

        out("count").set(s, (float)nHigh * cval);
        out("countlow").set(s, (float)nLow * cval);
    }
};

DROID_REGISTER_CIRCUIT(logic, Logic)

} // namespace droid
