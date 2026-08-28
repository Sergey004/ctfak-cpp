// Font bank (26215), TTF bank (8793) and music bank (26217).
#pragma once

#include "../core/BinaryReader.h"
#include <memory>
#include <string>
#include <vector>

namespace ctfak {

struct FontItem {
    uint32_t handle = 0;
    int32_t checksum = 0;
    int32_t references = 0;
    int32_t size = 0;
    // LOGFONTW fields
    int32_t height = 0, width = 0, escapement = 0, orientation = 0, weight = 0;
    uint8_t italic = 0, underline = 0, strikeOut = 0, charSet = 0, outPrecision = 0,
            clipPrecision = 0, quality = 0, pitchAndFamily = 0;
    std::string faceName;

    void read(BinaryReader& reader);
};

struct FontBank {
    int32_t count = 0;
    std::vector<std::shared_ptr<FontItem>> items;
    void read(BinaryReader& reader);
};

struct FontInfo {
    int32_t fontSize = 0;
    uint8_t fontStyle = 0;
    bool bold = false, italicFlag = false, underlineFlag = false, strikeoutFlag = false;
    int16_t scriptType = 0;
    std::string fontName;
};

struct TrueTypeMeta {
    std::vector<FontInfo> items;
    void read(BinaryReader& reader);
};

struct TrueTypeFonts {
    std::vector<std::vector<uint8_t>> fonts;
    void read(BinaryReader& reader);
};

struct MusicFile {
    int32_t handle = 0;
    int32_t checksum = 0;
    int32_t references = 0;
    uint32_t flags = 0;
    std::string name;
    std::vector<uint8_t> data;
    void read(BinaryReader& reader);
};

struct MusicBank {
    int32_t numOfItems = 0;
    std::vector<std::shared_ptr<MusicFile>> items;
    void read(BinaryReader& reader);
};

} // namespace ctfak
