// Windows EXE game-data entry point calculation (PE section scan).
#pragma once

#include "../core/BinaryReader.h"
#include <cstdint>

namespace ctfak {

// Returns the offset of the CCN data inside the EXE file image.
int64_t calculateEntryPoint(BinaryReader& reader);

} // namespace ctfak
