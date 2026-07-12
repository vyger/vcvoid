// switchedpot — Obsolete overlay pot: one physical pot drives up to eight
// virtual pots. Spec: manual/circuits/switchedpot.md. Superseded by `pot`; the
// DROID Forge flags it as deprecated (an accepted crosscheck divergence).
//
// switch1..switch8 gate which virtual pots move when the physical pot is turned
// (several may be active at once). Every virtual pot starts at center (0.5) and
// is nudged by the same proportional "pickup" algorithm as `pot`
// (potshape::pickup), so a selected virtual value catches up to the physical pot
// and both reach the same endpoint together. output1..output8 emit the eight
// virtual values; the `bipolar` gate maps each 0..1 value to -1..+1 (2v-1).
//
// Conventions / SPEC-GAPs:
//   * always virtual: this circuit is inherently a virtual-pot bank, so pickup
//     runs whenever a switch is high; a virtual pot with its switch low freezes
//     but keeps emitting. prevPhys is tracked every tick regardless, so toggling
//     a switch never causes a phantom jump.
//   * pickup granularity matches `pot`: a one-tick `set P..` jump is applied
//     from the pre-jump snapshot, reaching the shared endpoint in that tick.
//   * flash persistence of the virtual values (manual) is a headless no-op; all
//     eight seed to 0.5 at load.
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include "../src/potshape.hpp"

namespace droid {

class SwitchedPot : public Circuit {
    static constexpr int   N = 8;

public:
    void tick(EngineState& s) override {
        if (!inited_) init(s);

        float phys = in("pot").value(s);
        if (phys < 0.0f) phys = 0.0f; else if (phys > 1.0f) phys = 1.0f;
        bool bipolar = in("bipolar").value(s) >= kGateHighThreshold;

        for (int i = 1; i <= N; i++) {
            if (in("switch", i).value(s) >= kGateHighThreshold)
                virt_[i] = potshape::pickup(virt_[i], prevPhys_, phys);
            float v = virt_[i];
            out("output", i).set(s, bipolar ? (2.0f * v - 1.0f) : v);
        }
        prevPhys_ = phys;
    }

private:
    void init(EngineState& s) {
        for (int i = 1; i <= N; i++) virt_[i] = 0.5f;
        float phys = in("pot").value(s);
        if (phys < 0.0f) phys = 0.0f; else if (phys > 1.0f) phys = 1.0f;
        prevPhys_ = phys;
        inited_ = true;
    }

    bool  inited_ = false;
    float virt_[N + 1] = {};
    float prevPhys_ = 0.0f;
};

DROID_REGISTER_CIRCUIT(switchedpot, SwitchedPot)

} // namespace droid
