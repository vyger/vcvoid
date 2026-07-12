#include "chain.hpp"

namespace droid { namespace chain {

const char* modelName(ModelId id) {
    switch (id) {
        case MP2B8: return "p2b8"; case MP4B2: return "p4b2"; case MP10: return "p10";
        case MS10: return "s10";   case MP8S8: return "p8s8"; case MB32: return "b32";
        case ME4: return "e4";     case MM4: return "m4";     case MDB8E: return "db8e";
        case MG8: return "g8";     case MX7: return "x7";
        default: return "";
    }
}

bool isControllerModel(ModelId id) { return id != None && id != MG8 && id != MX7; }

bool defaultLedFromButton(ModelId id) {
    return id == MP2B8 || id == MP4B2 || id == MB32 || id == ME4 || id == MM4 || id == MDB8E;
}

bool defaultLedFromPot(ModelId id) { return id == MP8S8; }

void prependUpstream(const UpstreamBlock& mine, const UpstreamMessage& fromRight,
                     UpstreamMessage& out) {
    out.block[0] = mine;
    int copy = fromRight.count;
    if (copy > kMaxChainModules - 1) copy = kMaxChainModules - 1;   // drop farthest
    for (int i = 0; i < copy; i++) out.block[i + 1] = fromRight.block[i];
    out.count = uint8_t(copy + 1);
}

void shiftDownstream(const DownstreamMessage& fromLeft, DownstreamBlock& mine,
                     DownstreamMessage& out) {
    if (fromLeft.count == 0) { mine = DownstreamBlock{}; out.count = 0; return; }
    mine = fromLeft.block[0];
    int rest = fromLeft.count - 1;
    if (rest > kMaxChainModules - 1) rest = kMaxChainModules - 1;   // untrusted wire count: never over-read/write the fixed block[]
    for (int i = 0; i < rest; i++) out.block[i] = fromLeft.block[i + 1];
    out.count = uint8_t(rest);
}

int32_t detentDelta(uint32_t now, uint32_t last) {
    return (int32_t)(now - last);
}

void MidiUpstreamWindow::append(int port, const MidiEvent& e) {
    if (count[port] == kMidiEventsPerFrame) {
        for (int k = 1; k < kMidiEventsPerFrame; k++) ev[port][k - 1] = ev[port][k];
        count[port]--;
    }
    ev[port][count[port]++] = e;
    total[port]++;
}

void MidiUpstreamWindow::publish(MidiFrame& f) const {
    for (int port = 0; port < kChainMidiPorts; port++) {
        f.count[port] = count[port];
        f.total[port] = total[port];
        for (int k = 0; k < count[port]; k++) f.ev[port][k] = ev[port][k];
    }
}

int32_t consumeUpstreamMidi(const MidiFrame& f, int port, uint32_t lastTotal,
                            int& first, int& n) {
    first = 0;
    n = 0;
    int32_t newN = detentDelta(f.total[port], lastTotal);
    if (newN <= 0) return 0;   // nothing new (or resync glitch: consume nothing)
    int have = f.count[port] <= kMidiEventsPerFrame ? f.count[port] : kMidiEventsPerFrame;
    n = newN < have ? newN : have;
    first = have - n;
    return newN - n;           // events displaced from the window between samples
}

std::vector<std::string> controllerModels(const UpstreamMessage& m) {
    std::vector<std::string> v;
    for (int i = 0; i < m.count && i < kMaxChainModules; i++) {
        ModelId id = ModelId(m.block[i].modelId);
        if (isControllerModel(id)) v.push_back(modelName(id));
    }
    return v;
}

std::string validateChain(const std::vector<std::string>& declared,
                          const std::vector<std::string>& physical) {
    for (size_t i = 0; i < declared.size(); i++) {
        const std::string& have = i < physical.size() ? physical[i] : std::string();
        if (have != declared[i])
            return "controller " + std::to_string(i + 1) + ": patch declares " +
                   declared[i] + ", chain has " + (have.empty() ? "nothing" : have);
    }
    return "";
}

std::string x7ChainError(const UpstreamMessage& m) {
    int count = 0, first = -1;
    for (int i = 0; i < m.count && i < kMaxChainModules; i++) {
        if (ModelId(m.block[i].modelId) == MX7) {
            if (first < 0) first = i;
            count++;
        }
    }
    if (count == 0) return "";
    if (first != 0) return "x7 must be first in the chain";
    if (count > 1) return "only one x7 can be attached";
    return "";
}

}} // namespace droid::chain
