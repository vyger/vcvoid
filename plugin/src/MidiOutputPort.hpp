#pragma once
#include <midi.hpp>
#include <app/MidiDisplay.hpp>
#include <helpers.hpp>

// A rack::midi::Output whose channel override is permanently OFF.
//
// Rack's Port::channel is "the channel to automatically set outbound messages"
// (midi.hpp): any value 0..15 makes Output::sendMessage rewrite the status
// nibble of every channel-voice message. That is right for a Rack module that
// generates one channel's worth of events, and wrong for these modules: a DROID
// master sets the channel PER CIRCUIT (each [midiout] has its own `channel`),
// and a patch routinely runs several channels out of one port. The override has
// no correct setting, so -1 (leave the generator's channel alone) is forced at
// construction and again after fromJson — a saved patch from before this fix
// may carry a latched 0..15, and it must be migrated on load, not kept
// (issue #35). The context menu for these ports omits the channel item
// (appendMidiOutputMenu below), so nothing can latch a value again.
namespace dmidi {

struct OutputPort : rack::midi::Output {
    OutputPort() { channel = -1; }
    void fromJson(json_t* rootJ) {
        rack::midi::Output::fromJson(rootJ);
        channel = -1;
    }
};

// rack::app::appendMidiMenu minus its "MIDI channel" section — driver and
// device only. Mirrors Rack's own item layout (flat check items under a label)
// so the output submenus look like the input ones, just shorter.
inline void appendMidiOutputMenu(rack::ui::Menu* menu, rack::midi::Port* port) {
    using namespace rack;
    menu->addChild(createMenuLabel("MIDI driver"));
    for (int driverId : midi::getDriverIds()) {
        menu->addChild(createCheckMenuItem(midi::getDriver(driverId)->getName(), "",
            [=]() { return port->getDriverId() == driverId; },
            [=]() { port->setDriverId(driverId); }));
    }
    menu->addChild(new ui::MenuSeparator);
    menu->addChild(createMenuLabel("MIDI device"));
    menu->addChild(createCheckMenuItem("(No device)", "",
        [=]() { return port->getDeviceId() == -1; },
        [=]() { port->setDeviceId(-1); }));
    for (int deviceId : port->getDeviceIds()) {
        menu->addChild(createCheckMenuItem(port->getDeviceName(deviceId), "",
            [=]() { return port->getDeviceId() == deviceId; },
            [=]() { port->setDeviceId(deviceId); }));
    }
}

} // namespace dmidi
