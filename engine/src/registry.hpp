#pragma once
#include "circuit.hpp"
#include <memory>
#include <string>

namespace droid {

using CircuitFactory = std::unique_ptr<Circuit> (*)();
bool registerCircuit(const std::string& name, CircuitFactory f);
std::unique_ptr<Circuit> makeCircuit(const std::string& name);   // nullptr if not implemented

#define DROID_REGISTER_CIRCUIT(NAME, CLASS) \
    static bool reg_##NAME = droid::registerCircuit(#NAME, \
        []() -> std::unique_ptr<droid::Circuit> { return std::unique_ptr<droid::Circuit>(new CLASS()); });

} // namespace droid
