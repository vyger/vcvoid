#include "loader.hpp"
#include "controllers.hpp"
#include "ram.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <vector>

namespace droid {

namespace {
// Master-aware register validity. Master16 rules unchanged; Master18 rules
// mirror the Forge (droidforge/patch/patch.cpp updateProblems + registerAvailable):
// I1-I2 only (gate ins), no N registers, native G1-G4 plus X7 G9-G12,
// R5-R16 absent (R1-R4 = rear diagnostic LEDs), X1 absent.
bool validRegister(const RegId& r, MasterType master,
                   const std::vector<std::string>& controllers, std::string& err) {
    bool m18 = master == MasterType::Master18;
    switch (r.type) {
        case 'I':
            if (r.ctrl == 0 && r.num >= 1 && r.num <= (m18 ? 2 : 8)) return true;
            if (m18 && r.ctrl == 0) {
                err = "Invalid input number " + std::to_string(r.num) +
                      " (the MASTER18 has only the gate inputs I1 and I2)";
                return false;
            }
            break;
        case 'N':
            if (m18) { err = "The MASTER18 has no normalization registers"; return false; }
            if (r.ctrl == 0 && r.num >= 1 && r.num <= 8) return true;
            break;
        case 'O':
            if (r.ctrl == 0 && r.num >= 1 && r.num <= 8) return true;
            break;
        case 'G':
            // ctrl 1-4 = G8 expanders (both masters). ctrl 0: X7 gates G9-G12 on
            // both; native G1-G4 on Master18 only (on Master16 canonicalize()
            // rewrote bare G1-G8 to G1.x already).
            if (r.ctrl >= 1 && r.ctrl <= 4 && r.num >= 1 && r.num <= 8) return true;
            if (r.ctrl == 0 && r.num >= 9 && r.num <= 12) return true;
            if (m18 && r.ctrl == 0 && r.num >= 1 && r.num <= 4) return true;
            if (m18 && r.ctrl == 0 && r.num >= 5 && r.num <= 8) {
                err = "Invalid gate number " + std::to_string(r.num) +
                      " (the MASTER18 has only the gate outputs G1 ... G4)";
                return false;
            }
            break;
        case 'R':
            if (r.ctrl == 0 && r.num >= 1 && r.num <= 56 &&
                !(m18 && r.num >= 5 && r.num <= 16)) return true;
            break;
        case 'X':
            if (!m18 && r.ctrl == 0 && r.num == 1) return true;
            break;
        case 'P': case 'B': case 'L': case 'S': case 'E': {
            unsigned n = (unsigned)controllers.size();
            if (r.ctrl < 1 || r.ctrl > n) {
                err = "Register " + toString(r) + " refers to controller " +
                      std::to_string(r.ctrl) + ", but only " + std::to_string(n) +
                      " controllers are declared";
                return false;
            }
            // Per-model element range check (e.g. P on a b32, or B1.9 on a p2b8).
            const ControllerModel* model = findControllerModel(controllers[r.ctrl - 1]);
            if (!model) return true;   // unknown model: validate index only
            if (controllerHasElement(*model, r.type, r.num)) return true;
            err = "There is no register " + toString(r) + " on the " + model->name +
                  " at controller " + std::to_string(r.ctrl);
            return false;
        }
    }
    err = "There is no register " + toString(r) + " on this master";
    return false;
}
} // namespace

LoadResult compilePatch(const std::string& text, MasterType master, CompiledPatch& out) {
    LoadResult res;
    out = CompiledPatch{};

    if (stripPatch(text).size() > 64000) {
        res.errors.push_back({0, "patch exceeds the maximum size of 64000 bytes"});
        return res;
    }
    ParseResult pr = parsePatch(text);
    res.errors = pr.errors;
    out.texts = pr.texts;   // interned text table threads through unchanged

    // Pass 1: controllers + circuit resolution + jack resolution.
    for (auto& sec : pr.sections) {
        if (gen::findController(sec.name)) {
            // x7 never counts for controller numbering (matches hardware).
            if (sec.name != "x7") out.controllers.push_back(sec.name);
            continue;
        }
        const gen::CircuitDef* cdef = gen::findCircuit(sec.name);
        if (!cdef) {
            res.errors.push_back({sec.line, "Unknown circuit '" + sec.name + "'"});
            continue;
        }
        if (cdef->deprecated)
            res.warnings.push_back("circuit '" + sec.name + "' is deprecated");
        // vcotuner + sinfonionlink use MASTER18-only hardware (Forge parity:
        // droidfirmware.cpp circuitNeedsMaster18).
        if (master == MasterType::Master16 &&
            (sec.name == "vcotuner" || sec.name == "sinfonionlink"))
            res.errors.push_back({sec.line,
                "Circuit '" + sec.name + "' needs a MASTER18"});

        CompiledCircuit cc;
        cc.def = cdef;
        cc.line = sec.line;
        // resolve params; later duplicate of the same (jack, arrayIndex) wins
        std::map<std::pair<const gen::JackDef*, int>, size_t> seen;
        for (auto& p : sec.params) {
            int idx = 1;
            const gen::JackDef* jd = gen::findJack(*cdef, p.name, idx);
            if (!jd) {
                res.errors.push_back({p.line, "Circuit '" + sec.name + "' has no parameter '" + p.name + "'"});
                continue;
            }
            CompiledParam cp{jd, idx, p.a, p.b, p.c, p.simple, p.line};
            // canonicalize register atoms
            for (Atom* a : {&cp.a, &cp.b, &cp.c})
                if (a->kind == Atom::Kind::Register) a->reg = canonicalize(a->reg, master);
            if (!jd->isInput) {
                if (!cp.simple || cp.a.kind == Atom::Kind::Number) {
                    res.errors.push_back({p.line, "output must be a single output register or internal cable"});
                    continue;
                }
                // Input-only registers cannot be written. I/P/B/S are inputs; E
                // (encoders) is also input-only — the Forge rejects `output = E1.1`
                // with "You cannot use an encoder as output" (atomregister.cpp).
                if (cp.a.kind == Atom::Kind::Register &&
                    (cp.a.reg.type == 'I' || cp.a.reg.type == 'P' ||
                     cp.a.reg.type == 'B' || cp.a.reg.type == 'S' ||
                     cp.a.reg.type == 'E')) {
                    res.errors.push_back({p.line, "register " + toString(cp.a.reg) + " cannot be used as an output"});
                    continue;
                }
            }
            auto key = std::make_pair(jd, idx);
            auto it = seen.find(key);
            if (it != seen.end()) cc.params[it->second] = cp;   // last wins
            else { seen[key] = cc.params.size(); cc.params.push_back(cp); }
        }
        out.circuits.push_back(std::move(cc));
    }

    // Pass 2: cross-patch register/cable analysis.
    std::set<uint32_t> outputUse;               // packed O/N regs already written
    std::set<uint32_t> oUsedAsInput, oUsedAsOutput;
    std::map<std::string, std::vector<int>> cableWrites, cableReads;

    for (auto& cc : out.circuits) {
        for (auto& p : cc.params) {
            auto checkAtomRegs = [&](const Atom& a, bool isOutputPosition) {
                if (a.kind == Atom::Kind::Register) {
                    std::string err;
                    if (!validRegister(a.reg, master, out.controllers, err))
                        res.errors.push_back({p.line, err});
                    if (a.reg.type == 'O') {
                        if (isOutputPosition) oUsedAsOutput.insert(pack(a.reg));
                        else oUsedAsInput.insert(pack(a.reg));
                    }
                } else if (a.kind == Atom::Kind::Cable) {
                    (isOutputPosition ? cableWrites : cableReads)[a.cable].push_back(p.line);
                }
            };
            if (p.def->isInput) {
                checkAtomRegs(p.a, false); checkAtomRegs(p.b, false); checkAtomRegs(p.c, false);
            } else {
                checkAtomRegs(p.a, true);
                if (p.a.kind == Atom::Kind::Register &&
                    (p.a.reg.type == 'O' || p.a.reg.type == 'N')) {
                    uint32_t k = pack(p.a.reg);
                    if (outputUse.count(k))
                        res.errors.push_back({p.line, "Duplicate usage of " + toString(p.a.reg) + " as output"});
                    else outputUse.insert(k);
                }
            }
        }
    }
    for (uint32_t k : oUsedAsInput)
        if (!oUsedAsOutput.count(k)) {
            RegId r{'O', uint8_t((k >> 8) & 0xff), uint8_t(k & 0xff)};
            res.errors.push_back({0, "Output register " + toString(r) + " is just used as an input"});
        }
    for (auto& [name, lines] : cableWrites) {
        if (lines.size() > 1)
            res.errors.push_back({lines[1], "Duplicate usage of patch cable " + name + " as output"});
        if (!cableReads.count(name))
            res.errors.push_back({lines[0], "Patch cable " + name + " is never used as an input"});
        out.cableNames.push_back(name);
    }
    for (auto& [name, lines] : cableReads)
        if (!cableWrites.count(name))
            res.errors.push_back({lines[0], "Patch cable " + name + " is never used as an output"});
    std::sort(out.cableNames.begin(), out.cableNames.end());

    out.ramUsed = computeRam(out, master, res.errors);
    res.ramUsed = out.ramUsed;
    res.ok = res.errors.empty();
    return res;
}

} // namespace droid
