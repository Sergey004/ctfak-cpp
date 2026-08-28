// Extensions list chunk (8744), global values/strings (8754/8755), binary files (8760).
#pragma once

#include "../core/BinaryReader.h"
#include <string>
#include <vector>
#include <memory>

namespace ctfak {

struct ExtensionItem {
    int16_t handle = 0;
    int32_t magicNumber = 0;
    int32_t versionLs = 0;
    int32_t versionMs = 0;
    std::string name;    // part before the dot
    std::string ext;     // part after the dot
    std::string subType; // subType string (may be empty)
};

struct Extensions {
    uint16_t preloadCount = 0;
    std::vector<ExtensionItem> items;
    void read(BinaryReader& reader);
};

struct GlobalValue {
    bool isFloat = false;
    int32_t intValue = 0;
    float floatValue = 0;
};

struct GlobalValues {
    std::vector<GlobalValue> items;
    void read(BinaryReader& reader);
};

struct GlobalStrings {
    std::vector<std::string> items;
    void read(BinaryReader& reader);
};

struct BinaryFile {
    std::string name;
    std::vector<uint8_t> data;
};

struct BinaryFiles {
    std::vector<BinaryFile> files;
    void read(BinaryReader& reader);
};

} // namespace ctfak
