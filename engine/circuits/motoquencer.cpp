// motoquencer — Motor fader performance sequencer. The circuit itself (the M4
// editing surface on top of the shared sequencer core) is engine/src/motoquencer.hpp;
// it lives in a header so the experimental motoquencer2 can inherit it verbatim.
// This file is just the registration.
#include "../src/motoquencer.hpp"

namespace droid {

DROID_REGISTER_CIRCUIT(motoquencer, Motoquencer)

} // namespace droid
