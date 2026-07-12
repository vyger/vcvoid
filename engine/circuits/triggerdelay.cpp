// triggerdelay — Trigger/gate delay with multi-tap and optional clocking.
// Spec: manual/circuits/triggerdelay.md.
//
// Listens for rising edges (gates) at `input` and re-emits each one `delay`
// later at `output`, always as a full 10 V (engine 1.0) gate regardless of the
// input voltage level. Up to 16 input triggers can be "in the delay" at once;
// a 17th while all 16 are pending is dropped and raises a 0.5 s `overflow` gate.
//
//   * gate length: preserved from the input gate, UNLESS `gatelength` is patched,
//     which overrides it with a fixed length.
//   * `repeats`: each input trigger is emitted `repeats` times, spaced by `delay`
//     (repeat i at input_rise + i*delay, i = 1..repeats).
//   * `mute`: while high, an output gate that WOULD START is suppressed; a gate
//     already in progress finishes normally.
//   * clocked mode (`clock` patched): `delay` and (override) `gatelength` are in
//     clock cycles instead of seconds; the clock period is learned from
//     consecutive rising edges (like `delay`/`clocktool`). A non-override gate
//     keeps the input's own real-time length even in clocked mode.
//
// Conventions / SPEC-GAPs (manual silent, literal readings):
//   * `delay`, `repeats`, and (override) `gatelength` are LATCHED at the input
//     rising edge — live modulation of an already-scheduled trigger is ignored
//     (matches burst latching count/skip at its trigger).
//   * In clocked mode, a trigger arriving before the clock period is known (fewer
//     than two clock edges seen) cannot be scheduled and is dropped (does not
//     count against memory, does not overflow).
//   * repeats <= 0 produces no output gates.
//   * A fresh overflow re-arms the 0.5 s window.
#include "../src/registry.hpp"
#include "../src/gatereader.hpp"
#include <cmath>
#include <cstdint>
#include <vector>

namespace droid {

class TriggerDelay : public Circuit {
    static constexpr int kMaxMemory = 16;   // manual: up to 16 triggers in flight

    struct Gate { long start; long end; };  // absolute ticks; end == -1 == open

    // One in-flight input trigger. Generates `repeats` output gates spaced by
    // `delayTicks`; started gates live in `gates` until they finish.
    struct Slot {
        bool   active = false;
        uint64_t rise = 0;      // input rising-edge tick (absolute)
        long   delayTicks = 0;  // spacing / delay, latched at rise
        long   repeats = 0;     // number of output gates, latched at rise
        long   nextRepeat = 1;  // next repeat index (1-based) still to start
        bool   overrideLen = false;
        long   lenTicks = -1;   // gate length; override -> known at rise, else
                                // -1 until the input gate falls
        bool   open = false;    // non-override: input gate still high (len unknown)
        std::vector<Gate> gates;
    };

public:
    void tick(EngineState& s) override {
        long t = (long)s.tick;

        // --- clock period learning (consecutive rising edges) --------------
        bool clockMode = in("clock").connected();
        if (clockMode && clockGate_.risingEdge(in("clock").value(s))) {
            if (haveClock_) period_ = double(s.tick - lastClock_);
            lastClock_ = s.tick;
            haveClock_ = true;
        }

        // --- reap finished gates / slots (frees memory before this tick's
        //     allocation decision) -------------------------------------------
        for (auto& sl : slots_) {
            if (!sl.active) continue;
            for (size_t k = 0; k < sl.gates.size();) {
                if (sl.gates[k].end >= 0 && t >= sl.gates[k].end)
                    sl.gates.erase(sl.gates.begin() + k);
                else
                    ++k;
            }
            if (sl.nextRepeat > sl.repeats && sl.gates.empty() && !sl.open)
                sl.active = false;
        }

        // --- input edges ---------------------------------------------------
        bool rose = inputGate_.risingEdge(in("input").value(s));
        bool fell = inputHigh_ && !inputGate_.high;   // risingEdge advanced .high
        inputHigh_ = inputGate_.high;

        if (fell && openSlot_ >= 0) {
            Slot& sl = slots_[openSlot_];
            long g = t - (long)sl.rise;               // measured input gate length
            if (g < 0) g = 0;
            sl.lenTicks = g;
            sl.open = false;
            for (Gate& gate : sl.gates)               // finalise started gates
                if (gate.end < 0) gate.end = gate.start + g;
            openSlot_ = -1;
        }

        if (rose)
            handleRise(s, t, clockMode);

        // --- start due repeats & drive output ------------------------------
        bool muteHigh = in("mute").value(s) >= kGateHighThreshold;
        bool outHigh = false;
        for (auto& sl : slots_) {
            if (!sl.active) continue;
            while (sl.nextRepeat <= sl.repeats) {
                long start = (long)sl.rise + sl.delayTicks * sl.nextRepeat;
                if (start > t) break;
                if (!muteHigh) {
                    // end known now iff the length is known (override, or the
                    // input gate has already closed); otherwise open (-1).
                    long end = (sl.overrideLen || !sl.open) ? start + sl.lenTicks : -1;
                    sl.gates.push_back({start, end});
                }
                sl.nextRepeat++;
            }
            for (const Gate& gate : sl.gates)
                if (gate.end < 0 || t < gate.end) outHigh = true;
        }

        out("output").set(s, outHigh ? 1.0f : 0.0f);
        out("overflow").set(s, t < (long)overflowUntil_ ? 1.0f : 0.0f);
    }

private:
    void handleRise(EngineState& s, long t, bool clockMode) {
        long delayTicks;
        if (clockMode) {
            if (!haveClock_ || period_ <= 0.0) return;   // period unknown -> drop
            delayTicks = (long)std::lround(in("delay").value(s) * period_);
        } else {
            delayTicks = (long)std::lround((double)in("delay").value(s) * (double)s.tickRateHz);
        }
        if (delayTicks < 0) delayTicks = 0;

        // find a free memory slot; none -> overflow (0.5 s) and drop
        Slot* slot = nullptr;
        int slotIdx = -1;
        for (size_t k = 0; k < slots_.size(); k++)
            if (!slots_[k].active) { slot = &slots_[k]; slotIdx = (int)k; break; }
        if (!slot && slots_.size() < (size_t)kMaxMemory) {
            slotIdx = (int)slots_.size();
            slots_.emplace_back();
            slot = &slots_.back();
        }
        if (!slot) {
            overflowUntil_ = (uint64_t)t + (uint64_t)std::llround(0.5 * (double)s.tickRateHz);
            return;
        }

        slot->active = true;
        slot->rise = (uint64_t)t;
        slot->delayTicks = delayTicks;
        slot->repeats = (long)std::lround(in("repeats").value(s));
        if (slot->repeats < 0) slot->repeats = 0;
        slot->nextRepeat = 1;
        slot->gates.clear();

        slot->overrideLen = in("gatelength").connected();
        if (slot->overrideLen) {
            double gl = (double)in("gatelength").value(s);
            double lenTicks = clockMode ? gl * period_ : gl * (double)s.tickRateHz;
            slot->lenTicks = (long)std::lround(lenTicks);
            if (slot->lenTicks < 1) slot->lenTicks = 1;  // at least one tick high
            slot->open = false;
        } else {
            slot->lenTicks = -1;      // learned when the input gate falls
            slot->open = true;
            openSlot_ = slotIdx;
        }
    }

    GateReader inputGate_, clockGate_;
    bool inputHigh_ = false;
    int  openSlot_ = -1;                 // index of the currently-open slot, or -1
    std::vector<Slot> slots_;            // grows up to kMaxMemory
    uint64_t lastClock_ = 0, overflowUntil_ = 0;
    double period_ = 0.0;
    bool haveClock_ = false;
};

DROID_REGISTER_CIRCUIT(triggerdelay, TriggerDelay)

} // namespace droid
