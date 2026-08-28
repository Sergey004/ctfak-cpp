// Sound bank (26216) items.
#pragma once

#include "../core/BinaryReader.h"
#include <memory>
#include <string>
#include <vector>

namespace ctfak {

struct SoundItem {
    uint32_t handle = 0;
    int32_t checksum = 0;
    uint32_t references = 0;
    uint32_t flags = 0;
    bool isCompressed = true;
    std::string name;
    std::vector<uint8_t> data;

    void read(BinaryReader& reader);
};

struct SoundBank {
    int32_t numOfItems = 0;
    std::vector<std::shared_ptr<SoundItem>> items;
    bool isCompressed = true;

    void read(BinaryReader& reader);
};

} // namespace ctfak
