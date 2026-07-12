#include "ChainModule.hpp"
#include "Layout.hpp"
#include "DroidWidgets.hpp"
#include "droidcolor.hpp"
#include "MidiConvert.hpp"
#include <app/MidiDisplay.hpp>

// DROID X7 expander: USB + TRS MIDI plus 4 gate outputs (G9-G12). Always first
// in the controller chain (block[0], immediately right of the master). The X7
// is the ONLY block that carries MIDI: upstream it drains its two MIDI input
// ports (TRS, USB) into the chain frame for the engine; downstream it sends the
// engine's outbound events out its two MIDI output ports and drives G9-G12.
// manual/hardware.md §8.5. The X7's physical type-A/B switch is omitted here
// (recorded deviation): USB-MIDI is always active.
//
// Contract with the master (DroidMaster.cpp) — see chain.hpp MidiFrame:
//   * Upstream is a PERSISTENT sliding window of the most recent events per
//     port plus a monotonic total, republished every frame; the master diffs
//     the total on its tick frames and consumes the unseen window tail, so
//     events written between master samples are never lost to the
//     double-buffer flips (ISSUE-2).
//   * Downstream we dedupe by the master's monotonic seq: process a frame once
//     per new seq value.
//   * Gates use the G8 rule: b.gates[j] >= 0.1 -> 5 V out.
struct DroidX7 : ChainModule {
    enum ParamId { PARAMS_LEN };
    enum InputId { INPUTS_LEN };
    enum OutputId { ENUMS(GATE_OUTPUTS, 4), OUTPUTS_LEN };
    // Four RGB status LEDs (SD, USB, TRS-in, TRS-out) + four RGB gate LEDs.
    enum LightId {
        ENUMS(STATUS_LIGHTS, 4 * 3),   // 0=SD 1=USB 2=TRS-in 3=TRS-out
        ENUMS(GATE_LIGHTS, 4 * 3),     // G9..G12
        LIGHTS_LEN
    };

    // MIDI ports. InputQueue buffers inbound messages and releases them by frame
    // timestamp (midi.hpp: tryPop(maxFrame)); Output wraps a device sendMessage.
    rack::midi::InputQueue trsIn, usbIn;
    rack::midi::Output trsOut, usbOut;

    int64_t frame_ = 0;             // current engine frame, for InputQueue::tryPop

    // Upstream MIDI window (persists across frames — see contract above).
    droid::chain::MidiUpstreamWindow midiUp;

    // Downstream dedupe.
    uint32_t lastSeqDown = 0;

    // Activity-LED hold: per status LED (1=USB,2=TRS-in,3=TRS-out; 0=SD is a
    // constant dim white) a countdown (seconds) and a target RGB colour so a
    // single event is visible for ~120 ms. manual/hardware.md §8.5 colour code:
    // green=note on, red=note off, blue=other MIDI event.
    static constexpr float kHold = 0.12f;
    float ledHold[4] = {};
    float ledColor[4][3] = {};

    DroidX7() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        for (int j = 0; j < 4; j++)
            configOutput(GATE_OUTPUTS + j, rack::string::f("G%d gate out", 9 + j));
    }

    // Classify a MIDI status byte into an activity colour (RGB) per §8.5.
    static void statusColor(const droid::MidiEvent& e, float rgb[3]) {
        uint8_t hi = e.status >> 4;
        bool noteOn  = (hi == 0x9) && (e.data2 != 0);   // note-on, velocity > 0
        bool noteOff = (hi == 0x8) || ((hi == 0x9) && (e.data2 == 0));
        if (noteOn)  { rgb[0] = 0.f; rgb[1] = 1.f; rgb[2] = 0.f; }        // green
        else if (noteOff) { rgb[0] = 1.f; rgb[1] = 0.f; rgb[2] = 0.f; }   // red
        else { rgb[0] = 0.f; rgb[1] = 0.3f; rgb[2] = 1.f; }              // blue
    }
    void flag(int led, const droid::MidiEvent& e) {
        ledHold[led] = kHold;
        statusColor(e, ledColor[led]);
    }

    void fillUpstream(droid::chain::UpstreamBlock& b) override {
        using namespace droid::chain;
        b.modelId = MX7;
        rack::midi::InputQueue* q[2]   = { &trsIn, &usbIn };
        int ledFor[2] = { 2 /*TRS-in*/, 1 /*USB*/ };
        for (int port = 0; port < droid::chain::kChainMidiPorts; port++) {
            rack::midi::Message msg;
            while (q[port]->tryPop(&msg, frame_)) {
                droid::MidiEvent e;
                if (!dmidi::fromRack(msg, e)) continue;   // sysex/oversized: skip
                midiUp.append(port, e);
                flag(ledFor[port], e);
            }
        }
        midiUp.publish(b.midi);
    }

    void applyDownstream(const droid::chain::DownstreamBlock& b, float sampleTime) override {
        using namespace droid::chain;
        // ---- outbound MIDI: dedupe by the master's monotonic seq -----------
        if (b.midi.seq != lastSeqDown) {
            lastSeqDown = b.midi.seq;
            rack::midi::Output* out[2] = { &trsOut, &usbOut };
            int ledFor[2] = { 3 /*TRS-out*/, 1 /*USB*/ };
            for (int port = 0; port < droid::chain::kChainMidiPorts; port++) {
                int n = std::min<int>(b.midi.count[port], kMidiEventsPerFrame);
                for (int k = 0; k < n; k++) {
                    const droid::MidiEvent& e = b.midi.ev[port][k];
                    out[port]->sendMessage(dmidi::toRack(e));
                    flag(ledFor[port], e);
                }
            }
        }
        // ---- status LEDs ---------------------------------------------------
        // The X7's 2x4 LED matrix is R49..R56 (manual §5.5): a patch may override
        // any LED with a DROID colour value. Row-major, matching the master fill:
        // R49..R52 = the status row (SD/USB/TRS-in/TRS-out), R53..R56 = the gate
        // row (G9..G12). A driven R LED shows its colour * ledbrightness instead
        // of the indicator default.
        auto rOverride = [&](int matrixIdx, float& r, float& g, float& bl) -> bool {
            if (!(b.rLedDriven & (1u << matrixIdx))) return false;
            droid::color::RGB c = droid::color::fromValue(b.rLeds[matrixIdx]);
            r = c.r * b.ledBrightness; g = c.g * b.ledBrightness; bl = c.b * b.ledBrightness;
            return true;
        };
        for (int i = 0; i < 4; i++) {                // 0=SD 1=USB 2=TRS-in 3=TRS-out
            float b3[3] = {0.f, 0.f, 0.f};
            if (!rOverride(i, b3[0], b3[1], b3[2])) {
                if (i == 0) {                        // SD: constant dim white
                    b3[0] = b3[1] = b3[2] = 0.15f;
                } else if (ledHold[i] > 0.f) {       // USB / TRS activity flash
                    ledHold[i] -= sampleTime;
                    b3[0] = ledColor[i][0]; b3[1] = ledColor[i][1]; b3[2] = ledColor[i][2];
                }
            } else if (i != 0 && ledHold[i] > 0.f) {
                ledHold[i] -= sampleTime;            // keep the activity timer running under override
            }
            for (int c = 0; c < 3; c++)
                lights[STATUS_LIGHTS + i * 3 + c].setBrightness(b3[c]);
        }
        // ---- gate outs G9-G12 + green gate LEDs (G8 rule) ------------------
        for (int j = 0; j < 4; j++) {
            bool high = b.gates[j] >= 0.1f;
            outputs[GATE_OUTPUTS + j].setVoltage(high ? 5.f : 0.f);
            float r = 0.f, g = high ? 1.f : 0.f, bl = 0.f;   // default: green gate
            rOverride(4 + j, r, g, bl);
            lights[GATE_LIGHTS + j * 3 + 0].setBrightnessSmooth(r, sampleTime);
            lights[GATE_LIGHTS + j * 3 + 1].setBrightnessSmooth(g, sampleTime);
            lights[GATE_LIGHTS + j * 3 + 2].setBrightnessSmooth(bl, sampleTime);
        }
    }

    void process(const ProcessArgs& args) override {
        frame_ = args.frame;
        relay(args.sampleTime);
    }

    // UAT bridge (M10): expose the four X7 ports by handle. Reached by the
    // bridge through DroidMasterBase::rightExpander.module (the X7 is always
    // physically adjacent to the master when present) cast to ChainModule*, so
    // the bridge never needs DroidX7's definition (private to this file).
    std::vector<ChainModule::MidiPortRef> midiPorts() override {
        return {
            {"x7usb", "in",  &usbIn},  {"x7usb", "out", &usbOut},
            {"x7trs", "in",  &trsIn},  {"x7trs", "out", &trsOut},
        };
    }

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "trsIn",  trsIn.toJson());
        json_object_set_new(root, "usbIn",  usbIn.toJson());
        json_object_set_new(root, "trsOut", trsOut.toJson());
        json_object_set_new(root, "usbOut", usbOut.toJson());
        return root;
    }
    void dataFromJson(json_t* root) override {
        if (json_t* j = json_object_get(root, "trsIn"))  trsIn.fromJson(j);
        if (json_t* j = json_object_get(root, "usbIn"))  usbIn.fromJson(j);
        if (json_t* j = json_object_get(root, "trsOut")) trsOut.fromJson(j);
        if (json_t* j = json_object_get(root, "usbOut")) usbOut.fromJson(j);
    }
};

struct DroidX7Widget : VcvoidModuleWidget {
    DroidX7Widget(DroidX7* module) {
        setModule(module);
        const auto* L = droid::layout::find("x7");
        box.size = Vec(L->hp * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
        addChild(dw::ImagePanel::create("res/faceplates/x7.png", L->hp, "X7"));
        // Layout -> render px through the faceplate art (dw::ArtMap): the
        // PNG is 462x2915 px and stretched to the module box.
        dw::ArtMap A{462.f, 2915.f, box.size};
        // Status LEDs (SD, USB, TRS-in, TRS-out) on R1-4; gate LEDs (G9-G12) on
        // R5-8 — the layout's 8 'R' slots cover both rows of RGB indicators.
        auto addSquareLight = [&](int rNum, int lightId) {
            auto* l = createLightCentered<dw::SquareLight<RedGreenBlueLight>>(
                A.vec(L->pos('R', rNum)), module, lightId);
            l->box.size = Vec(dw::hpPx(2.1f) * 0.8f, dw::hpPx(2.1f) * 0.8f);
            l->box.pos = A.vec(L->pos('R', rNum)).minus(l->box.size.div(2));
            addChild(l);
        };
        for (int i = 0; i < 4; i++)
            addSquareLight(i + 1, DroidX7::STATUS_LIGHTS + i * 3);
        for (int j = 0; j < 4; j++)
            addSquareLight(j + 5, DroidX7::GATE_LIGHTS + j * 3);
        for (int j = 0; j < 4; j++)
            addOutput(createOutputCentered<dw::DroidPort>(
                A.vec(L->pos('G', j + 1)).plus(A.off(dw::kJackArtDx, dw::kJackArtDy)),
                module, DroidX7::GATE_OUTPUTS + j));
    }

    void appendContextMenu(Menu* menu) override {
        auto* m = dynamic_cast<DroidX7*>(module);
        if (!m) return;
        menu->addChild(new MenuSeparator);
        auto sub = [&](const char* label, rack::midi::Port* port) {
            menu->addChild(createSubmenuItem(label, "", [port](Menu* sm) {
                rack::app::appendMidiMenu(sm, port);
            }));
        };
        sub("TRS input device",  &m->trsIn);
        sub("TRS output device", &m->trsOut);
        sub("USB input device",  &m->usbIn);
        sub("USB output device", &m->usbOut);
    }
};

Model* modelDroidX7 = createModel<DroidX7, DroidX7Widget>("x7");
