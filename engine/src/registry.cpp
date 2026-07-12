#include "registry.hpp"
#include <map>

namespace droid {

static std::map<std::string, CircuitFactory>& table() {
    static std::map<std::string, CircuitFactory> t;
    return t;
}

bool registerCircuit(const std::string& name, CircuitFactory f) {
    table()[name] = f;
    return true;
}

std::unique_ptr<Circuit> makeCircuit(const std::string& name) {
    auto it = table().find(name);
    return it == table().end() ? nullptr : it->second();
}

} // namespace droid
