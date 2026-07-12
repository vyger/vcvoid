#include "numbers.hpp"

namespace droid {

// Locale-independent digit test. std::isdigit honors LC_CTYPE and is only
// defined for values representable as unsigned char / EOF, so an explicit range
// check is both safer and truly locale-proof (see the note below on strtof).
static inline bool isDigit(char c) { return c >= '0' && c <= '9'; }

bool parsePatchNumber(const std::string& token, float& out) {
    if (token == "on")  { out = 1.0f; return true; }
    if (token == "off") { out = 0.0f; return true; }
    if (token.empty()) return false;

    size_t i = 0;
    bool negative = false;
    if (token[i] == '-') { negative = true; i++; }

    // Locale-independent conversion: accumulate the digit runs ourselves.
    // (strtof honors LC_NUMERIC, so "0.45" could parse as 0.0 in a ','
    // decimal-separator locale — not acceptable inside a VCV Rack process
    // whose locale we don't control.)
    double value = 0.0;
    size_t digitsStart = i;
    while (i < token.size() && isDigit(token[i])) {
        value = value * 10.0 + (token[i] - '0');
        i++;
    }
    if (i == digitsStart) return false;              // need integer part
    if (i < token.size() && token[i] == '.') {
        i++;
        size_t fracStart = i;
        double place = 0.1;
        while (i < token.size() && isDigit(token[i])) {
            value += (token[i] - '0') * place;
            place *= 0.1;
            i++;
        }
        if (i == fracStart) return false;            // "1." not allowed
    }
    double divisor = 1.0;
    if (i < token.size()) {
        if (token[i] == 'V')      divisor = 10.0;
        else if (token[i] == '%') divisor = 100.0;
        else return false;
        i++;
    }
    if (i != token.size()) return false;
    if (negative) value = -value;
    out = (float)(value / divisor);
    return true;
}

} // namespace droid
