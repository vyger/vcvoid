#include "engine.hpp"
#include <cmath>
#include <cstring>

namespace droid {

const std::string& Engine::textForNumber(float v) const {
    static const std::string kEmpty;
    // Range-check AS FLOAT before any cast: a finite-but-out-of-range float cast to
    // a narrow int is UB (only NaN/inf are caught by isfinite; 1e20 is not), and
    // input math (= I1 * 1e20) can reach here. Comparing as float, then casting only
    // an in-range value, keeps the floor semantics (1.7 -> 1) with no UB.
    if (!std::isfinite(v) || v < 1.0f || v >= float(texts_.size())) return kEmpty;
    long n = long(std::floor(v));
    return texts_[size_t(n)];
}

Engine::Engine(MasterType master, float tickRateHz, uint32_t seed)
    : master_(master), tickRateHz_(tickRateHz), seed_(seed ? seed : 1) {
    state_.rngState = seed_;
    state_.tickRateHz = tickRateHz_;
    state_.midi.master18 = (master_ == MasterType::Master18);
}

Operand Engine::makeOperand(const Atom& a) const {
    Operand o;
    switch (a.kind) {
        case Atom::Kind::Number:
            o.kind = Operand::Kind::Const; o.constant = a.number; break;
        case Atom::Kind::Register:
            o.kind = Operand::Kind::Register; o.reg = a.reg; break;
        case Atom::Kind::Cable: {
            o.kind = Operand::Kind::Cable;
            auto it = cableIndex_.find(a.cable);
            // Unreachable on any validated patch (cableNames is built from the
            // same cables the parser collects), but makeOperand is on the
            // public load path and must never throw on bad input.
            if (it == cableIndex_.end()) { o.kind = Operand::Kind::Const; o.constant = 0.0f; break; }
            o.cableIndex = it->second;
            break;
        }
        case Atom::Kind::None:
            o.kind = Operand::Kind::Const; o.constant = 0.0f; break;
    }
    return o;
}

LoadResult Engine::load(const std::string& patchText) {
    CompiledPatch cp;
    LoadResult res = compilePatch(patchText, master_, cp);

    // Implementation coverage check (kept out of compilePatch: Forge parity there).
    for (auto& cc : cp.circuits)
        if (!makeCircuit(cc.def->name)) {
            res.errors.push_back({cc.line, std::string("circuit '") + cc.def->name +
                                           "' is not yet implemented in vcvoid"});
            res.ok = false;
        }
    if (!res.ok) return res;

    circuits_.clear();
    circuitPtrs_.clear();
    cableIndex_.clear();
    bool keepX7 = state_.midi.x7;      // X7 presence comes from the chain, not the
                                       // patch — it must survive the state reset
    state_ = EngineState{};
    for (int k = 0; k < 8; k++) haveSmooth_[k] = false;   // fresh conditioning state
    pokedClears_.clear();                                 // (dangling after circuits_.clear)
    state_.rngState = seed_;   // load resets state; the constructor seed must survive
    state_.tickRateHz = tickRateHz_;  // ...as must the configured tick rate
    state_.midi.x7 = keepX7;
    state_.midi.master18 = (master_ == MasterType::Master18);
    state_.fileProvider = fileProvider_;   // the card, not the patch, provides files
    state_.cables.assign(cp.cableNames.size(), 0.0f);
    for (uint16_t i = 0; i < cp.cableNames.size(); i++)
        cableIndex_[cp.cableNames[i]] = i;
    state_.controllers.configure(cp.controllers);

    declaredControllers_ = cp.controllers;
    texts_ = cp.texts;
    drivenRegs_.clear();
    for (auto& cc : cp.circuits)
        for (auto& p : cc.params)
            if (!p.def->isInput && p.a.kind == Atom::Kind::Register)
                drivenRegs_.insert(pack(p.a.reg));

    usesMidi_ = false;
    for (auto& cc : cp.circuits) {
        const char* n = cc.def->name;
        if (!std::strcmp(n, "midiin") || !std::strcmp(n, "midiout") ||
            !std::strcmp(n, "midithrough")) usesMidi_ = true;
        auto c = makeCircuit(cc.def->name);
        c->allocateSlots(cc.def);
        for (auto& p : cc.params) {
            int slot = c->slotIndex(p.def, p.arrayIndex);
            if (p.def->isInput)
                c->inputs[slot].bind(makeOperand(p.a), makeOperand(p.b), makeOperand(p.c));
            else
                c->outputs[slot].bind(makeOperand(p.a));
        }
        circuits_.push_back(std::move(c));
    }
    // Wire peer access so chainable circuits (sequencer) can reach neighbours.
    circuitPtrs_.reserve(circuits_.size());
    for (auto& c : circuits_) circuitPtrs_.push_back(c.get());
    for (size_t i = 0; i < circuits_.size(); i++) {
        circuits_[i]->peers_ = &circuitPtrs_;
        circuits_[i]->peerIndex_ = (int)i;
    }
    ramUsed_ = cp.ramUsed;
    loaded_ = true;
    return res;
}

void Engine::tick() {
    if (!loaded_) return;
    // Snapshot inbound MIDI: drain each port's in-queue into the per-tick
    // buffer (previous snapshot discarded). Circuits read tickEv/tickCount and
    // push responses to state_.midi.out during their tick().
    for (int p = 0; p < kNumMidiPorts; p++) {
        state_.midi.tickCount[p] = 0;
        MidiEvent e;
        while (state_.midi.in[p].pop(e) && state_.midi.tickCount[p] < kMidiQueueCap)
            state_.midi.tickEv[p][state_.midi.tickCount[p]++] = e;
    }
    for (auto& c : circuits_)
        c->tick(state_);
    state_.controllers.endTick();   // drain per-tick encoder movement

    // --- [droid] output conditioning (engine/circuits/droid.cpp) -----------
    // One-pole smoothing per O output, bypassed for intentional jumps
    // (|change| >= maxslope) and off entirely at maxslope 0. SPEC-GAP: the
    // manual gives no coefficient mapping; alpha = max((1-lpf)^2, 0.01) per
    // engine tick (lpf 0 -> passthrough, 1 -> heavy smoothing that still
    // converges). Applied after all circuits, i.e. at the "DAC" stage;
    // same-tick readers of O registers see the raw value.
    for (int k = 0; k < 8; k++) {
        if (!state_.master.conditioned[k]) { haveSmooth_[k] = false; continue; }
        RegId o{'O', 0, uint8_t(k + 1)};
        float raw = state_.regs.get(o);
        if (!haveSmooth_[k]) { smooth_[k] = raw; haveSmooth_[k] = true; }
        float ms = state_.master.maxslope[k];
        if (ms <= 0.0f || std::fabs(raw - smooth_[k]) >= ms) {
            smooth_[k] = raw;
        } else {
            float lpf = std::min(std::max(state_.master.lpf[k], 0.0f), 1.0f);
            float alpha = std::max((1.0f - lpf) * (1.0f - lpf), 0.01f);
            smooth_[k] += alpha * (raw - smooth_[k]);
        }
        state_.regs.set(o, smooth_[k]);
    }

    // --- [droid] clear/clearall broadcast -----------------------------------
    // Emulates "sends a trigger to the clear input of all circuits ... not
    // sent to circuits whose clear input is patched": pulse the DEFAULT value
    // of every unpatched clear/clearall input for exactly one tick — each
    // circuit's own edge detector then fires naturally, no per-circuit code.
    for (Input* i : pokedClears_) i->setDefault(0.0f);   // end last tick's pulse
    pokedClears_.clear();
    auto poke = [&](const char* jack) {
        for (auto& c : circuits_) {
            if (std::strcmp(c->def->name, "droid") == 0) continue;   // a poked droid would rebroadcast forever
            int idx = 1;
            const gen::JackDef* j = gen::findJack(*c->def, jack, idx, /*wantInput=*/1);
            if (!j) continue;
            Input& in = c->inputs[c->slotIndex(j, 1)];
            if (in.connected()) continue;
            in.setDefault(1.0f);
            pokedClears_.push_back(&in);
        }
    };
    if (state_.master.requestClear) poke("clear");
    if (state_.master.requestClearAll) poke("clearall");
    state_.master.requestClear = state_.master.requestClearAll = false;

    state_.tick++;
}

// --- persistent state (DROIDSTA.BIN contract, hardware.md §11.1) -----------
StateSnapshot Engine::saveState() const {
    StateSnapshot snap;
    std::unordered_map<std::string, int> ordinal;   // per-type running count
    for (const auto& c : circuits_) {
        if (!c->isStateful()) continue;
        int ord = ++ordinal[c->def->name];           // number ALL of the type
        if (c->dontsaveActive(state_)) continue;      // dontsave: skip saving
        CircuitState cs;
        cs.type = c->def->name;
        cs.ordinal = ord;
        cs.version = c->stateVersion();
        StateWriter w{cs.values};
        c->saveState(w);
        snap.entries.push_back(std::move(cs));
    }
    return snap;
}

void Engine::restoreState(const StateSnapshot& snap) {
    if (!loaded_) return;
    // Index the snapshot by (type, ordinal) for the type+appearance-order match.
    std::unordered_map<std::string, const CircuitState*> byKey;
    for (const auto& e : snap.entries)
        byKey[e.type + "#" + std::to_string(e.ordinal)] = &e;
    std::unordered_map<std::string, int> ordinal;
    for (auto& c : circuits_) {
        if (!c->isStateful()) continue;
        int ord = ++ordinal[c->def->name];            // number ALL of the type
        if (c->dontsaveActive(state_)) continue;       // dontsave: skip loading
        auto it = byKey.find(std::string(c->def->name) + "#" + std::to_string(ord));
        if (it == byKey.end()) continue;               // extra circuit: defaults
        const CircuitState* e = it->second;
        c->loadState(state_, e->version, e->values);   // circuit validates/ignores
    }
}

bool Engine::turnEncoder(const std::string& name, long detents) {
    auto r = parseRegisterName(name);
    if (!r || r->type != 'E') return false;
    return state_.controllers.turnEncoder(*r, detents);
}

bool Engine::pushEncoder(const std::string& name, bool down) {
    auto r = parseRegisterName(name);
    if (!r || r->type != 'E') return false;
    return state_.controllers.pushEncoder(*r, down);
}

bool Engine::moveFader(const std::string& name, float pos) {
    auto n = parseFaderName(name);
    if (!n || !state_.controllers.validateFader(*n)) return false;
    state_.controllers.moveFader(*n, pos);
    return true;
}

bool Engine::touchFader(const std::string& name, bool down) {
    auto n = parseFaderName(name);
    if (!n || !state_.controllers.validateFader(*n)) return false;
    state_.controllers.touchFader(*n, down);
    return true;
}

bool Engine::pressFaderPlate(const std::string& name, bool down) {
    auto n = parseFaderName(name);
    if (!n || !state_.controllers.validateFader(*n)) return false;
    state_.controllers.pressFaderPlate(*n, down);
    return true;
}

bool Engine::setValue(const std::string& name, float v) {
    if (!name.empty() && name[0] == '_') {
        auto it = cableIndex_.find(name);
        if (it == cableIndex_.end()) return false;
        state_.cables[it->second] = v;
        return true;
    }
    auto r = parseRegisterName(name);
    if (!r) return false;
    RegId id = canonicalize(*r, master_);
    if (id.type == 'I' && id.ctrl == 0) state_.regs.setInputPatched(id.num, true);
    state_.regs.set(id, v);
    return true;
}

float Engine::getValue(const std::string& name) const {
    if (!name.empty() && name[0] == '_') {
        auto it = cableIndex_.find(name);
        return it == cableIndex_.end() ? 0.0f : state_.cables[it->second];
    }
    if (auto fn = parseFaderName(name))   // "F<n>" -> fader position readback
        return state_.controllers.faderPosition(*fn);
    auto r = parseRegisterName(name);
    if (!r) return 0.0f;
    return state_.regs.get(canonicalize(*r, master_));
}

} // namespace droid
