// Chunk id -> human readable name table (format facts; see docs/FORMAT_NOTES.md).
#pragma once

#include <cstdint>
#include <string>

namespace ctfak {

const char* chunkName(int32_t id);

}
