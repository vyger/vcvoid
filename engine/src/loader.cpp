#include "loader.hpp"
#include "controllers.hpp"
#include "ram.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>
#include <set>
#include <sstream>
#include <vector>

namespace droid {

namespace {
// Master-aware register validity. Master16 rules unchanged; Master18 rules
// mirror the Forge (droidforge/patch/patch.cpp updateProblems + registerAvailable):
// I1-I2 only (gate ins), no N registers, native gate outs G1-G4 (== G1.1-G1.4)
// plus G8s at G2.x-G5.x and X7 gates G9-G12, R5-R16 absent (R1-R4 = rear
// diagnostic LEDs), X1 absent.
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
            // The dotted ctrl counts GATE DEVICES; canonicalize() has already
            // rewritten bare G1-G8 to G1.x on both masters. Device 1 is the first
            // G8 on the MASTER (G1.x-G4.x = the four expanders), but the master's
            // own four gate outputs on the MASTER18, which pushes its expanders to
            // G2.x-G5.x (manual/basics.md register tables, hardware.md 7.3).
            // ctrl 0 survives canonicalize only for the X7's gates G9-G12.
            if (r.ctrl == 0 && r.num >= 9 && r.num <= 12) return true;
            if (m18) {
                if (r.ctrl == 1 && r.num >= 1 && r.num <= 4) return true;   // native gate outs
                if (r.ctrl == 1 && r.num >= 5 && r.num <= 8) {
                    err = "Invalid gate number " + std::to_string(r.num) +
                          " (the MASTER18 has only the gate outputs G1 ... G4)";
                    return false;
                }
                if (r.ctrl >= 2 && r.ctrl <= 5 && r.num >= 1 && r.num <= 8) return true;
            } else {
                if (r.ctrl >= 1 && r.ctrl <= 4 && r.num >= 1 && r.num <= 8) return true;
            }
            break;
        case 'X':
            if (!m18 && r.ctrl == 0 && r.num == 1) return true;
            break;
        case 'R':
            // R lives in TWO namespaces. Undotted (ctrl == 0) it is the master's
            // own LED-matrix colour bank R1-R56 (basics.md §5.5). DOTTED it is a
            // controller register: the M4's four touch-plate LED colours
            // (hardware.md §6.11 "In addition, there is a R register that
            // controls the color of the LED, similar to those on the master"),
            // and no other model has one. So a dotted R falls through to the
            // shared controller branch below, which range-checks it per model
            // and names the model in its error (issue #80).
            if (r.ctrl == 0) {
                if (r.num >= 1 && r.num <= 56 && !(m18 && r.num >= 5 && r.num <= 16))
                    return true;
                break;
            }
            [[fallthrough]];
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

// Short form of one parameter name inside one circuit, mirroring the Forge's
// DroidFirmware::jackShortname (droidforge/main/droidfirmware.cpp): the name is
// looked up among the circuit's inputs first, then its outputs; an array jack
// matches as prefix + element number and keeps that number after the shortened
// prefix. A name the firmware does not know, or a jack with no short form,
// stays exactly as written.
std::string jackShortname(const gen::CircuitDef& c, const std::string& param) {
    for (int pass = 0; pass < 2; pass++) {          // 0 = inputs, 1 = outputs
        for (unsigned i = 0; i < c.numJacks; i++) {
            const gen::JackDef& j = c.jacks[i];
            if (j.isInput != (pass == 0)) continue;
            size_t nl = std::strlen(j.name);
            if (param.size() < nl || param.compare(0, nl, j.name) != 0) continue;
            if (j.count == 1) {
                if (param.size() != nl) continue;
                return j.shortName[0] ? std::string(j.shortName) : param;
            }
            // Array element: only the numbers the firmware actually defines
            // (calibrator's tune0 … tune8 is the one zero-based array).
            for (int k = j.startAt; k < int(j.startAt) + int(j.count); k++) {
                if (param.compare(nl, std::string::npos, std::to_string(k)) != 0) continue;
                return j.shortName[0] ? j.shortName + std::to_string(k) : param;
            }
        }
    }
    return param;
}

// Rewrite every parameter name to its short form, leaving values, comments and
// layout byte-for-byte alone. Parameter lines are recognised the way
// tools/inicompress.py recognises them (indent, identifier, '='), and only
// inside a section the firmware knows as a circuit — a controller section
// ([p2b8], [x7]) has no jacks to abbreviate.
std::string abbreviatePatch(const std::string& text) {
    std::istringstream in(text);
    std::string raw, out;
    const gen::CircuitDef* cur = nullptr;
    auto lower = [](std::string s) {
        for (auto& ch : s) ch = char(std::tolower((unsigned char)ch));
        return s;
    };
    while (std::getline(in, raw)) {
        size_t b = raw.find_first_not_of(" \t");
        if (b != std::string::npos && raw[b] == '[') {
            size_t e = raw.find(']', b);
            if (e != std::string::npos)
                cur = gen::findCircuit(lower(raw.substr(b + 1, e - b - 1)));
        } else if (cur && b != std::string::npos && std::isalpha((unsigned char)raw[b])) {
            size_t e = b;
            while (e < raw.size() && std::isalnum((unsigned char)raw[e])) e++;
            size_t eq = raw.find_first_not_of(" \t", e);
            if (eq != std::string::npos && raw[eq] == '=') {
                std::string name = lower(raw.substr(b, e - b));
                std::string s = jackShortname(*cur, name);
                if (s.size() < name.size()) raw = raw.substr(0, b) + s + raw.substr(e);
            }
        }
        out += raw;
        out += '\n';
    }
    return out;
}
} // namespace

size_t deployedPatchSize(const std::string& text) {
    return stripPatch(abbreviatePatch(text)).size();
}

LoadResult compilePatch(const std::string& text, MasterType master, CompiledPatch& out,
                        const LoadOptions& opts) {
    LoadResult res;
    out = CompiledPatch{};

    // Forge parity (#41): the limit applies to the patch as the master receives
    // it — with abbreviated parameter names. Measuring the verbose text instead
    // refused generated patches (MFPS output) that fit on real hardware.
    size_t deployed = deployedPatchSize(text);
    if (deployed > kMaxPatchSize) {
        std::string msg = "patch exceeds the maximum size of " +
                          std::to_string(kMaxPatchSize) + " bytes (" +
                          std::to_string(deployed) +
                          " bytes with abbreviated parameter names, as the "
                          "master measures it)";
        // "Ignore memory limits" means exactly that: the patch loads as a
        // plain running patch, with no warning (an amber ring / tooltip on
        // every load of a large patch is noise once the user opted in).
        if (!opts.ignoreMemoryLimits) {
            res.errors.push_back({0, msg, ErrorCode::PatchTooBig});
            return res;
        }
    }
    ParseResult pr = parsePatch(text);
    res.errors = pr.errors;
    out.texts = pr.texts;   // interned text table threads through unchanged

    // Pass 1: controllers + circuit resolution + jack resolution.
    for (auto& sec : pr.sections) {
        if (gen::findController(sec.name)) {
            // x7 never counts for controller numbering (matches hardware) —
            // but the declaration is still recorded, so a caller can tell
            // "this patch wants an X7" from "this patch wants controller 1".
            if (sec.name == "x7") out.x7Declared = true;
            else out.controllers.push_back(sec.name);
            continue;
        }
        const gen::CircuitDef* cdef = gen::findCircuit(sec.name);
        if (!cdef) {
            res.errors.push_back({sec.line, "Unknown circuit '" + sec.name + "'",
                                  ErrorCode::UnknownCircuit});
            continue;
        }
        if (cdef->deprecated)
            res.warnings.push_back("circuit '" + sec.name + "' is deprecated");
        // Experimental circuits (#12) exist only in vcvoid — they are not DROID
        // firmware and the Forge does not know them. Refused unless the module
        // opted in, so a patch built here stays hardware-compatible by default.
        if (cdef->experimental && !opts.allowExperimental)
            res.errors.push_back({sec.line,
                "Circuit '" + sec.name + "' is experimental (vcvoid only, not "
                "available on DROID hardware). Enable \"Allow experimental "
                "circuits\" in the module's context menu to load this patch.",
                ErrorCode::UnknownCircuit});
        // vcotuner + sinfonionlink use MASTER18-only hardware (Forge parity:
        // droidfirmware.cpp circuitNeedsMaster18).
        if (master == MasterType::Master16 &&
            (sec.name == "vcotuner" || sec.name == "sinfonionlink"))
            res.errors.push_back({sec.line,
                "Circuit '" + sec.name + "' needs a MASTER18",
                ErrorCode::UnknownCircuit});

        CompiledCircuit cc;
        cc.def = cdef;
        cc.line = sec.line;
        // resolve params; later duplicate of the same (jack, arrayIndex) wins
        std::map<std::pair<const gen::JackDef*, int>, size_t> seen;
        for (auto& p : sec.params) {
            int idx = 1;
            const gen::JackDef* jd = gen::findJack(*cdef, p.name, idx);
            if (!jd) {
                res.errors.push_back({p.line, "Circuit '" + sec.name + "' has no parameter '" + p.name + "'",
                                      ErrorCode::UnknownParameter});
                continue;
            }
            CompiledParam cp{jd, idx, p.a, p.b, p.c, p.simple, p.line};
            // canonicalize register atoms
            for (Atom* a : {&cp.a, &cp.b, &cp.c})
                if (a->kind == Atom::Kind::Register) a->reg = canonicalize(a->reg, master);
            if (!jd->isInput) {
                if (!cp.simple || cp.a.kind == Atom::Kind::Number) {
                    res.errors.push_back({p.line, "output must be a single output register or internal cable",
                                          ErrorCode::InvalidSyntax});
                    continue;
                }
                // Input-only registers cannot be written. I/P/B/S are inputs; E
                // (encoders) is also input-only — the Forge rejects `output = E1.1`
                // with "You cannot use an encoder as output" (atomregister.cpp).
                if (cp.a.kind == Atom::Kind::Register &&
                    (cp.a.reg.type == 'I' || cp.a.reg.type == 'P' ||
                     cp.a.reg.type == 'B' || cp.a.reg.type == 'S' ||
                     cp.a.reg.type == 'E')) {
                    res.errors.push_back({p.line, "register " + toString(cp.a.reg) + " cannot be used as an output",
                                          ErrorCode::UnknownRegister});
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
                        res.errors.push_back({p.line, err, ErrorCode::UnknownRegister});
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
                        res.errors.push_back({p.line, "Duplicate usage of " + toString(p.a.reg) + " as output",
                                              ErrorCode::CableMisuse});
                    else outputUse.insert(k);
                }
            }
        }
    }
    for (uint32_t k : oUsedAsInput)
        if (!oUsedAsOutput.count(k)) {
            RegId r{'O', uint8_t((k >> 8) & 0xff), uint8_t(k & 0xff)};
            res.errors.push_back({0, "Output register " + toString(r) + " is just used as an input",
                                  ErrorCode::CableMisuse});
        }
    for (auto& [name, lines] : cableWrites) {
        if (lines.size() > 1)
            res.errors.push_back({lines[1], "Duplicate usage of patch cable " + name + " as output",
                                  ErrorCode::CableMisuse});
        if (!cableReads.count(name))
            res.errors.push_back({lines[0], "Patch cable " + name + " is never used as an input",
                                  ErrorCode::CableMisuse});
        out.cableNames.push_back(name);
    }
    for (auto& [name, lines] : cableReads)
        if (!cableWrites.count(name))
            res.errors.push_back({lines[0], "Patch cable " + name + " is never used as an output",
                                  ErrorCode::CableMisuse});
    std::sort(out.cableNames.begin(), out.cableNames.end());

    std::vector<LoadError> ramErrors;
    out.ramUsed = computeRam(out, master, ramErrors);
    res.ramUsed = out.ramUsed;
    // Same policy as the size cap: with the limits ignored, overflows are
    // silently accepted rather than downgraded to warnings.
    if (!opts.ignoreMemoryLimits) {
        res.errors.insert(res.errors.end(), ramErrors.begin(), ramErrors.end());
    }
    res.ok = res.errors.empty();
    return res;
}

} // namespace droid
