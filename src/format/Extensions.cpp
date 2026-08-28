#include "Extensions.h"
#include "../core/Settings.h"
#include "../core/Logger.h"
#include <cstring>

namespace ctfak {

void Extensions::read(BinaryReader& reader) {
    uint16_t count = reader.readU16();
    preloadCount = reader.readU16();
    items.clear();
    for (uint16_t i = 0; i < count; ++i) {
        int64_t entryStart = reader.tell();
        int16_t sizeRaw = reader.readI16();
        size_t size = sizeRaw < 0 ? static_cast<size_t>(-sizeRaw) : static_cast<size_t>(sizeRaw);

        ExtensionItem item;
        item.handle = reader.readI16();
        item.magicNumber = reader.readI32();
        item.versionLs = reader.readI32();
        item.versionMs = reader.readI32();
        std::string extName = reader.readYuniversal();
        if (extName.empty()) {
            reader.seek(entryStart + size);
            continue;
        }
        size_t dot = extName.find('.');
        if (dot == std::string::npos) {
            // Reference quirk: returns without seeking; we log and abort this entry.
            Logger::warning("Extension name without dot: " + extName);
            item.name = extName;
            items.push_back(std::move(item));
            break;
        }
        item.name = extName.substr(0, dot);
        item.ext = extName.substr(dot + 1);
        item.subType = reader.readYuniversal();
        reader.seek(entryStart + size);
        items.push_back(std::move(item));
    }
}

void GlobalValues::read(BinaryReader& reader) {
    items.clear();
    int16_t count = reader.readI16();
    std::vector<int32_t> raw(static_cast<size_t>(count), 0);
    try {
        for (int16_t i = 0; i < count; ++i) raw[i] = reader.readI32();
        for (int16_t i = 0; i < count; ++i) {
            uint8_t type = reader.readU8();
            GlobalValue v;
            if (type == 2) {
                v.isFloat = true;
                memcpy(&v.floatValue, &raw[i], 4);
            } else if (type == 0) {
                v.intValue = raw[i];
            } else {
                continue; // type dropped but consumed
            }
            items.push_back(v);
        }
    } catch (const ReadError&) {
        // truncated block: keep what we have
    }
}

void GlobalStrings::read(BinaryReader& reader) {
    items.clear();
    try {
        int32_t count = reader.readI32();
        items.reserve(static_cast<size_t>(count));
        for (int32_t i = 0; i < count; ++i) items.push_back(reader.readWideString());
    } catch (const ReadError&) {
    }
}

void BinaryFiles::read(BinaryReader& reader) {
    files.clear();
    int32_t count = reader.readI32();
    files.reserve(static_cast<size_t>(count));
    for (int32_t i = 0; i < count; ++i) {
        BinaryFile file;
        int16_t nameLen = reader.readI16();
        file.name = Settings::unicode ? reader.readWideString(nameLen) : reader.readAscii(nameLen);
        int32_t dataLen = reader.readI32();
        file.data = reader.readBytes(dataLen);
        files.push_back(std::move(file));
    }
}

} // namespace ctfak
