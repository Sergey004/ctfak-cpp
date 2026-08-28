// Zlib wrappers over the system zlib library.
#pragma once

#include <cstdint>
#include <vector>

namespace ctfak {

// Raw zlib (RFC1950) inflate. Throws std::runtime_error on failure.
std::vector<uint8_t> zlibInflate(const uint8_t* data, size_t size, size_t hintSize = 0);
std::vector<uint8_t> zlibInflate(const std::vector<uint8_t>& data, size_t hintSize = 0);

// Raw zlib deflate at given level (0-9).
std::vector<uint8_t> zlibDeflate(const uint8_t* data, size_t size, int level = 6);
std::vector<uint8_t> zlibDeflate(const std::vector<uint8_t>& data, int level = 6);

} // namespace ctfak
