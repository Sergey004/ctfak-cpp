// Chunk framing: [i16 id][i16 flag][i32 size][payload] with per-flag processing.
#pragma once

#include "../core/BinaryReader.h"
#include <vector>

namespace ctfak {

enum class ChunkFlag : int16_t {
    NotCompressed = 0,
    Compressed = 1,
    Encrypted = 2,
    CompressedAndEncrypted = 3
};

struct Chunk {
    int16_t id = 0;
    int16_t flag = 0;
    int32_t size = 0;

    // Reads the chunk header + payload and applies decompression/decryption.
    // Returns the processed payload buffer.
    std::vector<uint8_t> read(BinaryReader& reader);
};

} // namespace ctfak
