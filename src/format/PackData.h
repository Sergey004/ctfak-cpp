// EXE pack data: 32-byte pack header + per-file records (see FORMAT_NOTES 1.1).
#pragma once

#include "../core/BinaryReader.h"
#include <string>
#include <vector>

namespace ctfak {

struct PackFile {
    std::string packFilename;
    bool compressed = false;
    std::vector<uint8_t> data;
};

struct PackData {
    uint32_t headerSize = 0;
    uint32_t dataSize = 0;
    uint32_t formatVersion = 0;
    int32_t check = 0;
    int32_t check2 = 0;
    uint32_t count = 0;
    std::vector<PackFile> items;

    // reader positioned at the pack header start
    void read(BinaryReader& reader);
};

} // namespace ctfak
