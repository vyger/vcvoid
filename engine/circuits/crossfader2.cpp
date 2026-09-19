// crossfader2 — EXPERIMENTAL (vcvoid only, #78): crossfader with a `loop` gate
// and a `curve` selector. Spec: manual/circuits/experimental/crossfader2.md.
// N = the highest patched input index (unpatched inputs read their 0 V default).
// `fade` 0..1 maps to a position p = fade*S with S = N-1 segments, or S = N
// when `loop` is on (so fade 1.0 is back at input1). p wraps with period N
// exactly as crossfader. curve 0 = crossfader's linear lerp, bit-identical;
// 1 = monotone cubic Hermite (Fritsch–Carlson-style tangents from the two
// neighbouring secants, never overshoots); 2 = Fourier (band-limited
// trigonometric interpolation through all N inputs, may ring). Values outside
// 0..2 clamp. Every mode works on the RING of N inputs, regardless of `loop`.
// Standard A*B+C input math.
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include <cmath>

namespace droid {

class Crossfader2 : public Circuit {
public:
    void tick(EngineState& s) override {
        int n = 0;
        for (int i = 1; i <= 8; i++)
            if (in("input", i).connected()) n = i;

        if (n == 0) { out("output").set(s, 0.0f); return; }
        if (n == 1) { out("output").set(s, in("input", 1).value(s)); return; }

        bool loop  = in("loop").value(s) >= kGateHighThreshold;
        int  segs  = loop ? n : n - 1;
        long curve = std::lround(in("curve").value(s));
        if (curve < 0) curve = 0;
        if (curve > 2) curve = 2;

        double p = (double)in("fade").value(s) * segs;
        // p modulo n, in [0, n): wraps the last input back to the first.
        double pm = p - std::floor(p / n) * n;
        int seg = (int)std::floor(pm);
        double frac = pm - seg;
        if (seg >= n) { seg = 0; frac = 0.0; }   // guard the pm==n float edge

        float a = in("input", seg + 1).value(s);            // 1-based jacks
        float b = in("input", (seg + 1) % n + 1).value(s);

        if (curve == 0) {
            out("output").set(s, float(a * (1.0 - frac) + b * frac));
            return;
        }

        if (curve == 2) {
            // Trigonometric interpolant on the ring: sum_k P[k] * D(pm - k),
            // D the periodic sinc for N samples (real-valued for even N: the
            // Nyquist bin enters as a cosine, which is the cos(pi x/N) factor).
            double acc = 0.0;
            for (int k = 0; k < n; k++)
                acc += (double)in("input", k + 1).value(s) * dirichlet(pm - k, n);
            out("output").set(s, float(acc));
            return;
        }

        // Ring neighbours of the segment: P[seg-1] and P[seg+2] (mod n).
        float pre  = in("input", (seg + n - 1) % n + 1).value(s);
        float post = in("input", (seg + 2) % n + 1).value(s);
        double ma = tangent(pre, a, b);
        double mb = tangent(a, b, post);

        double f  = frac, f2 = f * f, f3 = f2 * f;
        double h00 = 2 * f3 - 3 * f2 + 1;
        double h10 = f3 - 2 * f2 + f;
        double h01 = -2 * f3 + 3 * f2;
        double h11 = f3 - f2;
        out("output").set(s, float(h00 * a + h10 * ma + h01 * b + h11 * mb));
    }

private:
    // Tangent at knot k from its two secants: 0 if they disagree in sign (or
    // one is flat), else their harmonic mean. Unit segment length.
    static double tangent(double prev, double k, double next) {
        double dPrev = k - prev, dNext = next - k;
        if (dPrev * dNext <= 0.0) return 0.0;
        return 2.0 / (1.0 / dPrev + 1.0 / dNext);
    }

    // Periodic sinc: 1 at x = 0 (mod n), 0 at every other integer, so the
    // curve passes through every input. sin(pi x) cos(pi x/n) / (n sin(pi x/n)).
    static double dirichlet(double x, int n) {
        const double pi = 3.14159265358979323846;
        double sn = std::sin(pi * x / n);
        if (std::fabs(sn) < 1e-9) return 1.0;
        return std::sin(pi * x) * std::cos(pi * x / n) / (n * sn);
    }
};

DROID_REGISTER_CIRCUIT(crossfader2, Crossfader2)

} // namespace droid
