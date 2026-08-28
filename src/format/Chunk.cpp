#include "Chunk.h"
#include "../core/Crypto.h"
#include "../core/Zlib.h"
#include "../core/Logger.h"
#include "../core/Settings.h"

namespace ctfak {

std::vector<uint8_t> Chunk::read(BinaryReader& reader) {
    id = reader.readI16();
    flag = reader.readI16();
    size = reader.readI32();
    std::vector<uint8_t> raw = reader.readBytes(size);

    switch (static_cast<ChunkFlag>(flag)) {
        case ChunkFlag::NotCompressed:
            return raw;

        case ChunkFlag::Encrypted:
            Crypto::transformChunk(raw);
            return raw;

        case ChunkFlag::Compressed: {
            BinaryReader r(raw.data(), raw.size());
            if (Settings::old()) {
                // Old (1.5) compression: optional size prefix, then zlib stream.
                // Trailing chunk bytes (if any) are unused; chunks are processed atomically.
                int32_t decompSize = 0;
                if (r.peekI32() != -1) decompSize = r.readI32();
                std::vector<uint8_t> rest = r.readBytes(-1);
                return zlibInflate(rest, decompSize);
            }
            int32_t decompSize = r.readI32();
            int32_t compSize = r.readI32();
            return zlibInflate(r.readBytes(compSize), decompSize);
        }

        case ChunkFlag::CompressedAndEncrypted: {
            BinaryReader r(raw.data(), raw.size());
            uint32_t decompSize = r.readU32();
            std::vector<uint8_t> rest = r.readBytes(-1);
            if ((id & 1) == 1 && Settings::build > 284 && !rest.empty())
                rest[0] ^= static_cast<uint8_t>((id & 0xFF) ^ ((id >> 8) & 0xFF));
            Crypto::transformChunk(rest);
            BinaryReader d(rest.data(), rest.size());
            uint32_t compSize = d.readU32();
            return zlibInflate(d.readBytes(compSize), decompSize);
        }
    }
    return raw;
}

} // namespace ctfak
