// Image bank (26214) items and pixel decoding for all graphic modes.
#pragma once

#include "../core/BinaryReader.h"
#include "../core/BitDict.h"
#include <map>
#include <memory>
#include <vector>

namespace ctfak {

class ImageBank;

struct ImageFlags {
    BitDict flags = {"RLE", "RLEW", "RLET", "LZX", "Alpha", "ACE", "Mac", "RGBA"};
};

struct ImageItem {
    int32_t handle = 0;
    int32_t width = 0, height = 0;
    int16_t hotX = 0, hotY = 0, actionX = 0, actionY = 0;
    uint8_t graphicMode = 0;
    BitDict flags = {"RLE", "RLEW", "RLET", "LZX", "Alpha", "ACE", "Mac", "RGBA"};
    BinaryReader::Color transparent;
    int32_t checksum = 0;
    int32_t references = 0;
    std::vector<uint8_t> pixelData;   // decompressed pixel payload (as stored)
    std::vector<uint8_t> rgba;        // decoded R,G,B,A buffer (filled by decode())

    void decode();

private:
    void decodeMode4(std::vector<uint8_t>& out) const;
    void decodeMode8(std::vector<uint8_t>& out) const;
};

struct ImageBank {
    std::map<int32_t, std::shared_ptr<ImageItem>> items;

    void read(BinaryReader& reader);
    std::shared_ptr<ImageItem> find(int32_t handle) const;
};

} // namespace ctfak
