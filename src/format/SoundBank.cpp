#include "SoundBank.h"
#include "../core/Settings.h"
#include "../core/Zlib.h"

namespace ctfak {

void SoundItem::read(BinaryReader& reader) {
    handle = reader.readU32();
    if (handle > 0) handle -= 1;
    checksum = reader.readI32();
    references = reader.readU32();
    int32_t decompressedSize = reader.readI32();
    flags = reader.readU8();
    reader.skip(3);
    reader.readI32(); // reserved
    int32_t nameLen = reader.readI32();

    std::vector<uint8_t> body;
    if (isCompressed && flags != 33) {
        int32_t size = reader.readI32();
        body = zlibInflate(reader.readBytes(size));
    } else {
        body = reader.readBytes(decompressedSize);
    }

    BinaryReader r(body.data(), body.size());
    if (nameLen > 0) {
        // Name is stored as wide chars even in ANSI files.
        name = r.readWideString(nameLen);
    }
    if (flags == 33) r.seek(0);
    data = r.readBytes(-1);
}

void SoundBank::read(BinaryReader& reader) {
    numOfItems = reader.readI32();
    items.clear();
    if ((Settings::android() || Settings::old() || Settings::f3() || Settings::fusion3Seed) &&
        !Settings::isMFA)
        return; // reference skips all items entirely for these game types

    for (int32_t i = 0; i < numOfItems; ++i) {
        auto item = std::make_shared<SoundItem>();
        item->isCompressed = isCompressed;
        item->read(reader);
        items.push_back(std::move(item));
    }
}

} // namespace ctfak
