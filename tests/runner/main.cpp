#include "goldfile.hpp"
#include "midinames.hpp"
#include "src/engine.hpp"
#include <cmath>
#include <cstdio>

using droid::Engine;
using droid::MasterType;
namespace gold = droid::gold;

// Evaluate an ExpectDisplay assertion against Task 4's per-DB8E display seam.
// Failure messages mirror the value-expect format (file:line, @tick, got vs want).
static bool evalExpectDisplay(const Engine& e, const gold::Event& ev,
                              const std::string& path, long t) {
    // Shared "FAIL file:line: @tick target" prefix; each branch appends its
    // field-specific detail (starting with its own separator, so the output
    // is byte-identical to the previous per-branch printfs). Returns false
    // so branches can `return fail(...)`.
    auto fail = [&](const char* fmt, auto... args) {
        std::printf("FAIL %s:%d: @%ld %s", path.c_str(), ev.line, t, ev.target.c_str());
        std::printf(fmt, args...);
        std::printf("\n");
        return false;
    };
    int n = std::atoi(ev.target.c_str() + 1);
    const droid::DisplayState* d = e.displayState(n);
    if (!d)
        return fail(": no DB8E %d in this patch", n);
    if (ev.field == "off") {
        if (d->active) return fail(" off: display is active");
        return true;
    }
    if (ev.field == "header") {
        const std::string& got = e.textForNumber(float(d->headerText));
        if (got != ev.strValue)
            return fail(" header = \"%s\", expected \"%s\"", got.c_str(), ev.strValue.c_str());
        return true;
    }
    if (ev.field == "text") {
        if (!d->isText) return fail(" text: display body is a value, not text");
        const std::string& got = e.textForNumber(float(d->bodyText));
        if (got != ev.strValue)
            return fail(" text = \"%s\", expected \"%s\"", got.c_str(), ev.strValue.c_str());
        return true;
    }
    if (ev.field == "value") {
        if (d->isText) return fail(" value: display body is text, not a value");
        if (std::fabs(d->value - ev.value) > ev.tol)
            return fail(" value = %g, expected %g (tol %g)", d->value, ev.value, ev.tol);
        return true;
    }
    if (ev.field == "mode") {
        if (int(d->numbermode) != int(ev.value))
            return fail(" mode = %d, expected %d", int(d->numbermode), int(ev.value));
        return true;
    }
    if (ev.field == "font") {
        if (int(d->fontsize) != int(ev.value))
            return fail(" font = %d, expected %d", int(d->fontsize), int(ev.value));
        return true;
    }
    // Unreachable today (the goldfile parser whitelists expectdisplay fields), but a
    // fallthrough to `return true` would be a test that cannot fail. Fail loudly on an
    // unknown field so a future field added to the parser but not handled here is caught.
    return fail(": unknown expectdisplay field '%s'", ev.field.c_str());
}

static bool runFile(const std::string& path) {
    gold::GoldFile g = gold::parse(path);
    if (!g.parseError.empty()) {
        std::printf("FAIL %s: %s\n", path.c_str(), g.parseError.c_str());
        return false;
    }
    Engine e(g.master == 18 ? MasterType::Master18 : MasterType::Master16,
             g.tickrate, g.seed);
    if (!g.midiFiles.empty()) {
        auto files = g.midiFiles;   // by value: outlives this scope in the lambda
        e.setFileProvider([files](int num, std::vector<uint8_t>& out) {
            auto it = files.find(num);
            if (it == files.end()) return false;
            out = it->second;
            return true;
        });
    }
    droid::LoadResult r = e.load(g.patch);

    if (!g.expectLoadError.empty()) {
        if (r.ok) { std::printf("FAIL %s: expected load error, but load succeeded\n", path.c_str()); return false; }
        for (auto& err : r.errors)
            if (err.message.find(g.expectLoadError) != std::string::npos) return true;
        std::printf("FAIL %s: no load error containing \"%s\"; got:\n", path.c_str(), g.expectLoadError.c_str());
        for (auto& err : r.errors) std::printf("  line %d: %s\n", err.line, err.message.c_str());
        return false;
    }
    if (!r.ok) {
        std::printf("FAIL %s: patch failed to load:\n", path.c_str());
        for (auto& err : r.errors) std::printf("  line %d: %s\n", err.line, err.message.c_str());
        return false;
    }
    if (!g.expectWarning.empty()) {
        bool found = false;
        for (auto& w : r.warnings) if (w.find(g.expectWarning) != std::string::npos) found = true;
        if (!found) { std::printf("FAIL %s: missing warning \"%s\"\n", path.c_str(), g.expectWarning.c_str()); return false; }
    }
    if (g.hasExpectRam && e.ramUsed() != g.expectRam) {
        std::printf("FAIL %s: ramUsed %u != expected %u\n", path.c_str(), e.ramUsed(), g.expectRam);
        return false;
    }

    e.setX7Present(g.x7);

    long maxTick = -1;
    for (auto& ev : g.events) maxTick = std::max(maxTick, ev.tick);
    size_t i = 0;
    bool ok = true;
    for (long t = 0; t <= maxTick; t++) {
        while (i < g.events.size() && g.events[i].tick == t &&
               g.events[i].kind != gold::Event::Kind::Expect &&
               g.events[i].kind != gold::Event::Kind::ExpectDisplay &&
               g.events[i].kind != gold::Event::Kind::ExpectMidi) {
            auto& ev = g.events[i];
            switch (ev.kind) {
                case gold::Event::Kind::Set:  e.setValue(ev.target, ev.value); break;
                case gold::Event::Kind::Turn: e.turnEncoder(ev.target, std::lround(ev.value)); break;
                case gold::Event::Kind::Push: e.pushEncoder(ev.target, ev.value >= 0.5f); break;
                case gold::Event::Kind::Move: e.moveFader(ev.target, ev.value); break;
                // touch = a finger on the plate below the fader: the plate press
                // plus the hold. hold = grabbing the fader itself (a Rack drag):
                // hold only, never a plate press.
                case gold::Event::Kind::Touch:
                    e.touchFader(ev.target, ev.value >= 0.5f);
                    e.pressFaderPlate(ev.target, ev.value >= 0.5f);
                    break;
                case gold::Event::Kind::Hold: e.touchFader(ev.target, ev.value >= 0.5f); break;
                case gold::Event::Kind::Unpatch: e.setInputPatched(std::stoi(ev.target.substr(1)), false); break;
                case gold::Event::Kind::MidiSend:
                    e.sendMidiIn((droid::MidiPort)ev.midiPort, ev.midi); break;
                case gold::Event::Kind::Probe:
                    e.setProbe(ev.probeNone ? 0.0f : ev.value, !ev.probeNone); break;
                default: break;   // Expect kinds never reach here
            }
            i++;
        }
        e.tick();
        while (i < g.events.size() && g.events[i].tick == t) {
            auto& ev = g.events[i];   // only Expect/ExpectDisplay/ExpectMidi remain
            if (ev.kind == gold::Event::Kind::ExpectDisplay) {
                if (!evalExpectDisplay(e, ev, path, t)) ok = false;
                i++;
                continue;
            }
            if (ev.kind == gold::Event::Kind::ExpectMidi) {
                droid::MidiEvent got;
                bool drained = e.drainMidiOut((droid::MidiPort)ev.midiPort, got);
                if (ev.midiNone) {
                    if (drained) {
                        std::printf("FAIL %s:%d: @%ld expectmidi %s none: got %s\n",
                                    path.c_str(), ev.line, t,
                                    gold::midiPortName(ev.midiPort),
                                    gold::midiEventToString(got).c_str());
                        ok = false;
                    }
                } else if (!drained) {
                    std::printf("FAIL %s:%d: @%ld expectmidi %s: out-queue empty, expected %s\n",
                                path.c_str(), ev.line, t,
                                gold::midiPortName(ev.midiPort),
                                gold::midiEventToString(ev.midi).c_str());
                    ok = false;
                } else if (got.status != ev.midi.status || got.data1 != ev.midi.data1 ||
                           got.data2 != ev.midi.data2) {
                    std::printf("FAIL %s:%d: @%ld expectmidi %s: got %s, expected %s\n",
                                path.c_str(), ev.line, t,
                                gold::midiPortName(ev.midiPort),
                                gold::midiEventToString(got).c_str(),
                                gold::midiEventToString(ev.midi).c_str());
                    ok = false;
                }
                i++;
                continue;
            }
            float got = e.getValue(ev.target);
            if (std::fabs(got - ev.value) > ev.tol) {
                std::printf("FAIL %s:%d: @%ld %s = %g, expected %g (tol %g)\n",
                            path.c_str(), ev.line, t, ev.target.c_str(), got, ev.value, ev.tol);
                ok = false;
            }
            i++;
        }
    }
    return ok;
}

int main(int argc, char** argv) {
    int failed = 0, total = 0;
    for (int i = 1; i < argc; i++) {
        total++;
        if (runFile(argv[i])) std::printf("PASS %s\n", argv[i]);
        else failed++;
    }
    std::printf("%d/%d golden files passed\n", total - failed, total);
    return failed;
}
