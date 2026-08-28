#include "FontBank.h"
#include "../core/Settings.h"
#include "../core/Zlib.h"

namespace ctfak {

void FontItem::read(BinaryReader& reader) {
    handle = reader.readU32();
    // Compressed variant used when bank sets it; CCN bank items are zlib-wrapped
    // via the [decompSize][compSize][zlib] framing.
    int32_t decompSize = reader.readI32();
    int32_t compSize = reader.readI32();
    std::vector<uint8_t> body = zlibInflate(reader.readBytes(compSize), decompSize);
    BinaryReader r(body.data(), body.size());
    checksum = r.readI32();
    references = r.readI32();
    size = r.readI32();
    height = r.readI32();
    width = r.readI32();
    escapement = r.readI32();
    orientation = r.readI32();
    weight = r.readI32();
    italic = r.readU8();
    underline = r.readU8();
    strikeOut = r.readU8();
    charSet = r.readU8();
    outPrecision = r.readU8();
    clipPrecision = r.readU8();
    quality = r.readU8();
    pitchAndFamily = r.readU8();
    faceName = r.readYuniversal(32);
}

void FontBank::read(BinaryReader& reader) {
    if ((Settings::old() || Settings::f3() || Settings::fusion3Seed) && !Settings::isMFA) return;
    count = reader.readI32();
    int handleOffset = (Settings::build > 284) ? -1 : 0;
    items.clear();
    for (int32_t i = 0; i < count; ++i) {
        if (Settings::android()) continue; // skips without consuming (reference quirk)
        auto item = std::make_shared<FontItem>();
        try {
            item->read(reader);
        } catch (const ReadError&) {
            break;
        }
        item->handle = static_cast<uint32_t>(static_cast<int32_t>(item->handle) + handleOffset);
        items.push_back(std::move(item));
    }
}

void TrueTypeMeta::read(BinaryReader& reader) {
    items.clear();
    while (reader.hasMemory(92)) {
        FontInfo info;
        int32_t fontSize = reader.readI32();
        info.fontSize = -(fontSize + 6);
        reader.readI32(); // unk
        reader.readI32(); // unk
        reader.readI32(); // unk
        info.fontStyle = reader.readU8();
        uint8_t boldRaw = reader.readU8();
        info.bold = boldRaw > 1;
        reader.readI16(); // unk
        info.italicFlag = reader.readU8() != 0;
        info.underlineFlag = reader.readU8() != 0;
        info.strikeoutFlag = reader.readU8() != 0;
        int16_t script = reader.readI16();
        info.scriptType = script > 0 ? static_cast<int16_t>(179 - script) : script;
        info.fontName = reader.readWideString(32);
        reader.readI32(); // unk
        items.push_back(std::move(info));
    }
}

void TrueTypeFonts::read(BinaryReader& reader) {
    fonts.clear();
    while (!reader.eof()) {
        int32_t decompSize = reader.readI32();
        int32_t compSize = reader.readI32();
        fonts.push_back(zlibInflate(reader.readBytes(compSize), decompSize));
    }
}

void MusicFile::read(BinaryReader& reader) {
    handle = reader.readI32();
    int32_t decompSize = reader.readI32();
    int32_t compSize = reader.readI32();
    std::vector<uint8_t> body = zlibInflate(reader.readBytes(compSize), decompSize);
    BinaryReader r(body.data(), body.size());
    checksum = r.readI32();
    references = r.readI32();
    uint32_t size = r.readU32();
    flags = r.readU32();
    r.readI32(); // reserved
    int32_t nameLen = r.readI32();
    name = r.readWideString(nameLen);
    int64_t dataLen = static_cast<int64_t>(size) - 2 * static_cast<int64_t>(nameLen);
    if (dataLen < 0) dataLen = 0;
    data = r.readBytes(dataLen);
}

void MusicBank::read(BinaryReader& reader) {
    numOfItems = reader.readI32();
    items.clear();
    if (Settings::android()) return;
    for (int32_t i = 0; i < numOfItems; ++i) {
        auto file = std::make_shared<MusicFile>();
        try {
            file->read(reader);
        } catch (const ReadError&) {
            break;
        }
        items.push_back(std::move(file));
    }
}

} // namespace ctfak
