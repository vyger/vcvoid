#pragma once
#include <string>
namespace droid {
// DROID patch number: [-]digits[.digits] with optional '%' (/100) or 'V' (/10)
// suffix, or "on"/"off". No leading '.', no scientific notation.
bool parsePatchNumber(const std::string& token, float& out);
}
