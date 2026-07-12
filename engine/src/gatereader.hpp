#pragma once
// engine/src/gatereader.hpp — shared gate/trigger edge semantics.
//
// Threshold research (Task 3, Step 1): the manual documents a single,
// consistent gate/trigger-high threshold used across circuits: 0.1 in
// engine units, which is 1 V (engine units: 1.0 == 10 V, so 0.1 == 1 V).
// Quotes:
//   - manual/circuits/logic.md:149 (`threshold` input of the `logic`
//     circuit): "Input values at, or above this threshold value, are
//     considered high or on. The default is `0.1` which corresponds to
//     an input voltage of 1 V."
//   - manual/circuits/logic.md:26 (prose example): "`O1` will output 1
//     (on) if all of `I1`, `I2` and `I3` see on (voltage above 1 V)"
//   - manual/basics.md:903 (preset-load-by-CV trick): "sending a value
//     larger than `0.1` (which is the threshold for boolean 'true'
//     values)" — same 0.1/1V threshold, described generically for any
//     CV-as-boolean use, not just one circuit.
//   - manual/circuits/algoquencer.md:505 repeats the identical wording.
// No hysteresis is documented for this generic gate/trigger-high
// determination. (The `histeresis` parameters found on `quantizer` and
// `watch` are a different, unrelated concept — a minimum CV *change*
// required before re-quantizing/re-detecting a change — not a threshold
// band around the gate-high decision, and are configured per-circuit,
// not part of shared edge semantics.) So GateReader below uses a single
// fixed comparison for "is high": v >= kGateHighThreshold, with no
// separate low-threshold for exiting the high state.
//
// Note: manual/hardware.md's "2 gate inputs switching at 0.1 V" (MASTER18
// hardware spec, section on MASTER18 electricals) describes the physical
// electrical switching point of the I1/I2 gate input circuitry in real
// volts — the same numeric value (0.1 V) but a different, hardware-level
// fact; it corroborates rather than contradicts the 0.1 (engine units)
// software threshold used here for the emulated signal path.

#include <cstdint>

namespace droid {

constexpr float kGateHighThreshold = 0.1f; // engine units; == 1 V

// Caller-owned-state variant of GateReader::risingEdge: same threshold and
// edge semantics, but the previous-high flag lives in a bool the caller
// stores (used where a circuit tracks many independent edges in plain bools).
inline bool risingEdge(bool& prev, float v) {
    bool now = v >= kGateHighThreshold;
    bool rose = now && !prev;
    prev = now;
    return rose;
}

struct GateReader {
    bool high = false;

    // returns true exactly on a rising edge this tick (low -> high)
    bool risingEdge(float v) {
        bool newHigh = v >= kGateHighThreshold;
        bool rose = newHigh && !high;
        high = newHigh;
        return rose;
    }

    // returns true exactly on a falling edge this tick (high -> low)
    bool fallingEdge(float v) {
        bool newHigh = v >= kGateHighThreshold;
        bool fell = !newHigh && high;
        high = newHigh;
        return fell;
    }
};

} // namespace droid
