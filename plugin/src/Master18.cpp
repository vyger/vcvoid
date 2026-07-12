#include "MasterBase.hpp"
#include "DroidWidgets.hpp"
#include "MidiConvert.hpp"
#include <app/MidiDisplay.hpp>

// DROID MASTER18: 8 CV outs, 2 gate/trigger ins (I1/I2), 4 gate/trigger outs
// (G1-G4). Runs the same engine as MASTER with MasterType::Master18 register
// validation. Native MIDI (manual hardware.md §9.5): 6 streams — USB in/out,
// MIDI1 (TRS) in/out, MIDI2 (TRS) in/out — feeding engine physical ports 0-2
// directly (no chain frame; the X7's ports are 3-4). The physical TRS type-B
// detail and the 3-position USB switch are omitted (recorded deviation, like
// the X7's A/B switch): USB MIDI is always active. The Sinfonion link is out
// of scope; the vcotuner frequency probe on I1 rides process() below.
struct DroidMaster18 : DroidMasterBase {
    enum InputId { ENUMS(GATE_INPUTS, 2), INPUTS_LEN };
    enum OutputId { ENUMS(CV_OUTPUTS, 8), ENUMS(GATE_OUTPUTS, 4), OUTPUTS_LEN };

    // MIDI ports (X7.cpp patterns: InputQueue releases by frame timestamp,
    // 1..3-byte non-sysex only; Output wraps a device sendMessage).
    rack::midi::InputQueue usbIn, trs1In, trs2In;
    rack::midi::Output usbOut, trs1Out, trs2Out;
    int64_t frame_ = 0;

    // Frequency probe on I1 (manual hardware.md §9.7): positive zero-crossing
    // period measurement at FULL Rack sample rate (the engine ticks at ~6 kHz,
    // far too slow for audio). The interpolated sign change (prev < 0 -> v >= 0)
    // gives the sub-frame crossing time — frame-quantized periods alone are up
    // to ~17 cents off at 880 Hz, useless for a 3-cent tuner — and a +-0.1 V
    // hysteresis pair confirms it as a real crossing (the signal must dip below
    // -0.1 V to arm and rise above +0.1 V to fire), matching the hardware's
    // "goes from below zero volts to above zero volts" for bipolar waveforms.
    // Range 1 Hz..20 kHz; "present" decays when no crossing arrives for 1 s.
    static constexpr float kProbeHyst = 0.1f;
    bool   probeArmed_ = false;         // saw the signal below -hyst since last fire
    float  probePrevV_ = 0.f;
    double probePendingCross_ = -1.0;   // interpolated sign-change frame, unconfirmed
    double probeLastCross_ = -1.0;      // last confirmed crossing (fractional frame)
    int64_t probeLastCrossFrame_ = -1;  // integer frame of last confirmation
    float  probeHz_ = 0.0f;
    bool   probePresent_ = false;

    DroidMaster18() : DroidMasterBase(droid::MasterType::Master18, 2, 8, 4, 0) {}

    void process(const ProcessArgs& args) override {
        frame_ = args.frame;
        float v = inputs[GATE_INPUTS + 0].getVoltage();
        if (probePrevV_ < 0.f && v >= 0.f)
            probePendingCross_ = double(args.frame - 1) +
                                 double(probePrevV_ / (probePrevV_ - v));
        if (probeArmed_ && v > kProbeHyst && probePendingCross_ >= 0.0) {
            if (probeLastCross_ >= 0.0) {
                double period = probePendingCross_ - probeLastCross_;
                if (period > 0.0) {
                    float hz = float(args.sampleRate / period);
                    if (hz >= 1.f && hz <= 20000.f) { probeHz_ = hz; probePresent_ = true; }
                }
            }
            probeLastCross_ = probePendingCross_;
            probeLastCrossFrame_ = args.frame;
            probeArmed_ = false;
        } else if (v < -kProbeHyst) {
            probeArmed_ = true;
        }
        if (probeLastCrossFrame_ >= 0 &&
            float(args.frame - probeLastCrossFrame_) > args.sampleRate)
            probePresent_ = false;      // silence > 1 s: signal lost
        probePrevV_ = v;
        DroidMasterBase::process(args);
    }

    // MASTER18 I1/I2 are gate inputs, not full CV inputs: they binarize at the
    // hardware comparator threshold. The patch must see exactly 1.0 at/above
    // threshold and 0.0 below (no in-between analog level). Threshold is 0.1 V
    // per manual hardware.md §"Specifications → MASTER18" ("2 gate inputs
    // switching at 0.1 V"); §9.8 documents I1/I2 as gate-only. No hysteresis is
    // specified. (This differs from the G8 gate inputs, which switch at 0.75 V
    // per manual §7.3 — see G8.cpp.) The base register loop calls this hook
    // just before engine->tick(); the vcotuner probe in process() reads the
    // raw analog I1 jack separately by design.
    float inputRegisterValue(int i) override {
        return inputs[GATE_INPUTS + i].getVoltage() >= 0.1f ? 1.f : 0.f;
    }

    void processExtraIO() override {
        // ---- frequency probe -> engine (vcotuner reads s.probe) ------------
        engine->setProbe(probeHz_, probePresent_);
        // ---- native MIDI: module ports <-> engine physical ports 0-2 -------
        // Runs on tick frames under the engine lock, after the engine tick;
        // the InputQueue buffers messages arriving between tick frames.
        static constexpr droid::MidiPort kPort[3] = {
            droid::MidiPort::M18Usb, droid::MidiPort::M18Trs1, droid::MidiPort::M18Trs2};
        rack::midi::InputQueue* ins[3] = { &usbIn, &trs1In, &trs2In };
        rack::midi::Output* outs[3] = { &usbOut, &trs1Out, &trs2Out };
        for (int i = 0; i < 3; i++) {
            rack::midi::Message msg;
            while (ins[i]->tryPop(&msg, frame_)) {
                droid::MidiEvent e;
                if (!dmidi::fromRack(msg, e)) continue;   // sysex/oversized: skip
                engine->sendMidiIn(kPort[i], e);
            }
            droid::MidiEvent e;
            while (engine->drainMidiOut(kPort[i], e))
                outs[i]->sendMessage(dmidi::toRack(e));
        }
        for (int j = 1; j <= 4; j++) {
            droid::RegId gr = droid::canonicalize({'G', 0, (uint8_t)j}, masterType_);
            outputs[GATE_OUTPUTS + j - 1].setVoltage(
                engine->registerDriven(gr) ? engine->getRegister(gr) * 5.f : 0.f);
        }
    }

    // UAT bridge (M10): expose the six native ports by handle for GET/POST
    // /master/{id}/midi-ports (Bridge.cpp).
    std::vector<ChainModule::MidiPortRef> midiPorts() override {
        return {
            {"usb",  "in",  &usbIn},  {"usb",  "out", &usbOut},
            {"trs1", "in",  &trs1In}, {"trs1", "out", &trs1Out},
            {"trs2", "in",  &trs2In}, {"trs2", "out", &trs2Out},
        };
    }

    json_t* dataToJson() override {
        json_t* root = DroidMasterBase::dataToJson();
        json_object_set_new(root, "usbIn",   usbIn.toJson());
        json_object_set_new(root, "usbOut",  usbOut.toJson());
        json_object_set_new(root, "trs1In",  trs1In.toJson());
        json_object_set_new(root, "trs1Out", trs1Out.toJson());
        json_object_set_new(root, "trs2In",  trs2In.toJson());
        json_object_set_new(root, "trs2Out", trs2Out.toJson());
        return root;
    }
    void dataFromJson(json_t* root) override {
        DroidMasterBase::dataFromJson(root);
        if (json_t* j = json_object_get(root, "usbIn"))   usbIn.fromJson(j);
        if (json_t* j = json_object_get(root, "usbOut"))  usbOut.fromJson(j);
        if (json_t* j = json_object_get(root, "trs1In"))  trs1In.fromJson(j);
        if (json_t* j = json_object_get(root, "trs1Out")) trs1Out.fromJson(j);
        if (json_t* j = json_object_get(root, "trs2In"))  trs2In.fromJson(j);
        if (json_t* j = json_object_get(root, "trs2Out")) trs2Out.fromJson(j);
    }
};

struct DroidMaster18Widget : DroidMasterBaseWidget {
    DroidMaster18Widget(DroidMaster18* module) {
        setModule(module);
        const auto* L = droid::layout::find("master18");
        box.size = Vec(L->hp * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
        addChild(dw::ImagePanel::create("res/faceplates/master18.png", L->hp, "MASTER18"));
        // Layout -> render px through the faceplate art (dw::ArtMap): the
        // PNG is 692x2917 px and stretched to the module box.
        dw::ArtMap A{692.f, 2917.f, box.size};
        for (int i = 0; i < 2; i++)
            addInput(createInputCentered<dw::DroidPort>(
                A.vec(L->pos('I', i + 1)).plus(A.off(dw::kJackArtDx, dw::kJackArtDy)),
                module, DroidMaster18::GATE_INPUTS + i));
        for (int i = 0; i < 8; i++)
            addOutput(createOutputCentered<dw::DroidPort>(
                A.vec(L->pos('O', i + 1)).plus(A.off(dw::kJackArtDx, dw::kJackArtDy)),
                module, DroidMaster18::CV_OUTPUTS + i));
        for (int i = 0; i < 4; i++)
            addOutput(createOutputCentered<dw::DroidPort>(
                A.vec(L->pos('G', i + 1)).plus(A.off(dw::kJackArtDx, dw::kJackArtDy)),
                module, DroidMaster18::GATE_OUTPUTS + i));
    }

    void appendContextMenu(Menu* menu) override {
        DroidMasterBaseWidget::appendContextMenu(menu);
        auto* m = dynamic_cast<DroidMaster18*>(module);
        if (!m) return;
        menu->addChild(new MenuSeparator);
        auto sub = [&](const char* label, rack::midi::Port* port) {
            menu->addChild(createSubmenuItem(label, "", [port](Menu* sm) {
                rack::app::appendMidiMenu(sm, port);
            }));
        };
        sub("USB MIDI input",     &m->usbIn);
        sub("USB MIDI output",    &m->usbOut);
        sub("MIDI1 input (TRS)",  &m->trs1In);
        sub("MIDI1 output (TRS)", &m->trs1Out);
        sub("MIDI2 input (TRS)",  &m->trs2In);
        sub("MIDI2 output (TRS)", &m->trs2Out);
    }
};

Model* modelDroidMaster18 = createModel<DroidMaster18, DroidMaster18Widget>("master18");
