// queue — Clocked CV shift register with 64 cells. Spec: manual/circuits/queue.md.
// On each clock rising edge the cells shift forward (the last is dropped) and the
// current input is copied into cell 1. Eight outputs each read a placeable cell
// (outputpos1..8, 1..64, defaulting to 1..8). Standard A*B+C input math applies.
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include <cmath>

namespace droid {

class Queue : public Circuit {
public:
    void tick(EngineState& s) override {
        if (clockGate_.risingEdge(in("clock").value(s))) {
            for (int i = 63; i >= 1; i--) cells_[i] = cells_[i - 1];
            cells_[0] = in("input").value(s);
        }
        for (int k = 1; k <= 8; k++) {
            Input& op = in("outputpos", k);
            long pos = op.connected() ? std::lround(op.value(s)) : k;  // smart default k
            if (pos < 1) pos = 1;
            if (pos > 64) pos = 64;
            out("output", k).set(s, cells_[pos - 1]);
        }
    }

private:
    GateReader clockGate_;
    float cells_[64] = {0.0f};   // all cells start at 0
};

DROID_REGISTER_CIRCUIT(queue, Queue)

} // namespace droid
