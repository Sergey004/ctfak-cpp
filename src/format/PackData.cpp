#include "PackData.h"
#include "../core/Zlib.h"
#include "../core/Logger.h"
#include "../core/Settings.h"
#include <string>     // std::to_string (MSVC does not leak it transitively)

namespace ctfak {

void PackData::read(BinaryReader& reader) {
    int64_t start = reader.tell();
    reader.skip(8); // first 8 bytes (0x7777 marker etc.), not validated
    headerSize = reader.readU32();
    dataSize = reader.readU32();
    formatVersion = reader.readU32();
    check = reader.readI32();
    check2 = reader.readI32();
    count = reader.readU32();

    if (headerSize != 32)
        Logger::warning("Unexpected pack header size: " + std::to_string(headerSize));

    // Magic lives at start + dataSize - 32.
    int64_t magicPos = start + static_cast<int64_t>(dataSize) - 32;
    if (magicPos > 0 && magicPos + 4 <= reader.size()) {
        reader.seek(magicPos);
        std::string magic = reader.readAscii(4);
        if (magic == "PAMU") {
            Settings::setGameType(GameType::Normal);
            Settings::unicode = true;
        } else if (magic == "PAME") {
            if (Settings::gameType != static_cast<uint32_t>(GameType::Mmf15))
                Settings::setGameType(GameType::Mmf2);
            Settings::unicode = false;
        } else {
            Logger::warning("Found \"" + magic + "\" pack header");
        }
    }

    // File records.
    reader.seek(start + 32);
    items.clear();
    for (uint32_t i = 0; i < count; ++i) {
        PackFile file;
        uint16_t nameLen = reader.readU16();
        file.packFilename = Settings::unicode ? reader.readWideString(nameLen) : reader.readAscii(nameLen);
        reader.skip(4); // unused field
        int32_t size = reader.readI32();
        // zlib "best compression" streams start with 78 DA.
        bool isZlib = size >= 2 && reader.peekU8() == 0x78 && reader.data()[reader.tell() + 1] == 0xDA;
        if (isZlib) {
            file.compressed = true;
            file.data = zlibInflate(reader.readBytes(size));
        } else {
            file.data = reader.readBytes(size);
        }
        items.push_back(std::move(file));
    }
}

} // namespace ctfak
