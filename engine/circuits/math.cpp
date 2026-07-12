// math — Math utility circuit: 21 pure per-tick functions of input1/input2.
// Spec: manual/circuits/math.md. Standard A*B+C input math applies to every
// input before we read it; math's own operations are applied on top of that.
//
// "Unused inputs" (manual): the manual documents an explicit neutral-default
// override only for two ops: sum's missing operand reads as 0.0 (which is
// already the framework's default for a disconnected Input, so no override
// code is needed) and product's missing operand reads as 1.0 (which DOES
// need an explicit override below, since the framework default is 0.0).
// No other op gets a documented override; every other op here just reads
// in("input1"/"input2").value(s) directly, inheriting the framework's plain
// 0.0 default when disconnected. This is a deliberate SPEC-GAP for the
// multiplicative-family ops (quotient, power) whose omitted-input behavior
// the manual never states -- see the "conventions unverified vs hardware"
// ledger note and tests/golden/math/product-quotient.gold's commentary.
//
// "Very large number" outputs (quotient/reciprocal/power/logarithm/log2):
// the manual never pins an exact magnitude, only a sign/direction. This
// implementation uses a single sentinel kLarge = 1e9f throughout.
// SPEC-GAP: exact magnitude unverified vs hardware.
#include "../src/registry.hpp"
#include <cmath>

namespace droid {

namespace {

constexpr float kLarge = 1.0e9f;
constexpr float kTwoPi = 6.28318530717958647692f;

// input1^input2 when input1 < 0: the manual specifies input2 is rounded to
// the nearest integer first, then raised as an integer power (so the sign
// follows the exponent's parity). Implemented by hand (not std::pow, whose
// behavior for a negative base is only defined for integral exponents in
// the first place, and we want to control that path explicitly).
float ipow(float base, long n) {
    bool neg = n < 0;
    unsigned long un = neg ? (unsigned long)(-n) : (unsigned long)n;
    double result = 1.0;
    double b = base;
    while (un) {
        if (un & 1u) result *= b;
        b *= b;
        un >>= 1u;
    }
    return neg ? float(1.0 / result) : float(result);
}

} // namespace

class Math : public Circuit {
public:
    void tick(EngineState& s) override {
        Input& i1 = in("input1");
        Input& i2 = in("input2");
        float a = i1.value(s);
        float b = i2.value(s);
        // product's documented neutral default: an omitted operand reads as
        // 1.0, not the framework's plain 0.0.
        float aProd = i1.connected() ? a : 1.0f;
        float bProd = i2.connected() ? b : 1.0f;

        out("sum").set(s, a + b);
        out("difference").set(s, a - b);
        out("product").set(s, aProd * bProd);

        // quotient: natural IEEE division carries the correct sign through
        // zero (including the 0/0 -> NaN corner, handled below with a
        // documented SPEC-GAP convention of +kLarge).
        {
            float q = a / b;
            if (std::isnan(q)) q = kLarge;             // 0/0: convention, SPEC-GAP
            else if (std::isinf(q)) q = std::copysign(kLarge, q);
            out("quotient").set(s, q);
        }

        // modulo: fold input1 into [0, input2). Zero/negative divisor -> 0.
        {
            float m = (b > 0.0f) ? (a - b * std::floor(a / b)) : 0.0f;
            out("modulo").set(s, m);
        }

        // power: base<0 uses a rounded-integer exponent; base==0 has three
        // documented/convention sub-cases; base>0 is ordinary std::pow.
        {
            float p;
            if (a < 0.0f) {
                long n = (long)std::lround((double)b);
                p = ipow(a, n);
            } else if (a == 0.0f) {
                if (b > 0.0f) p = 0.0f;
                else if (b < 0.0f) p = kLarge;          // sign unspecified, SPEC-GAP
                else p = 1.0f;                           // 0^0, SPEC-GAP convention
            } else {
                p = std::pow(a, b);
            }
            out("power").set(s, p);
        }

        out("average").set(s, (a + b) * 0.5f);
        out("maximum").set(s, a > b ? a : b);
        out("minimum").set(s, a < b ? a : b);

        out("negation").set(s, -a);

        // reciprocal: same sign-preserving zero-division handling as quotient.
        {
            float r = 1.0f / a;
            if (std::isnan(r)) r = kLarge;              // unreachable (1/0 is never NaN)
            else if (std::isinf(r)) r = std::copysign(kLarge, r);
            out("reciprocal").set(s, r);
        }

        out("amount").set(s, std::fabs(a));
        out("sine").set(s, std::sin(kTwoPi * a));
        out("cosine").set(s, std::cos(kTwoPi * a));
        out("square").set(s, a * a);
        out("root").set(s, a < 0.0f ? -std::sqrt(-a) : std::sqrt(a));

        if (a > 0.0f) out("logarithm").set(s, std::log(a));
        else if (a < 0.0f) out("logarithm").set(s, -std::log(-a));
        else out("logarithm").set(s, -kLarge);

        if (a > 0.0f) out("log2").set(s, std::log2(a));
        else if (a < 0.0f) out("log2").set(s, -std::log2(-a));
        else out("log2").set(s, -kLarge);

        out("round").set(s, std::round(a));
        out("floor").set(s, std::floor(a));
        out("ceil").set(s, std::ceil(a));
    }
};

DROID_REGISTER_CIRCUIT(math, Math)

} // namespace droid
