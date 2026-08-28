#include "StringChunks.h"
#include "../core/BinaryReader.h"
#include "../core/Settings.h"

namespace ctfak {

std::string readStringChunk(BinaryReader& reader) {
    return reader.readYuniversal();
}

std::string readAppNameChunk(BinaryReader& reader, int64_t chunkSize) {
    int64_t start = reader.tell();
    std::string str = reader.readAscii();
    if (static_cast<int64_t>(str.size()) + 1 == chunkSize) {
        Settings::unicode = false;
        return str;
    }
    reader.seek(start);
    Settings::unicode = true;
    return reader.readWideString();
}

} // namespace ctfak
