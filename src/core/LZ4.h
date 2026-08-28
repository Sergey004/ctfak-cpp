// LZ4 block-format decoder (see the public LZ4 Block Format specification).
// Only decompression is needed by this project; implemented from the format document.
#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace ctfak {

// Decodes a raw LZ4 block into `output` (which must be sized to the decompressed size).
// Returns the number of bytes written. Throws std::runtime_error on malformed input.
size_t lz4DecodeBlock(const uint8_t* src, size_t srcSize, uint8_t* output, size_t outputSize);
std::vector<uint8_t> lz4DecodeBlock(const std::vector<uint8_t>& block, size_t decompressedSize);

} // namespace ctfak
