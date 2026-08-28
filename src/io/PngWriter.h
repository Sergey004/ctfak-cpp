// Minimal PNG writer (8-bit RGBA, non-interlaced) built on zlib deflate.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ctfak {

class PngWriter {
public:
    // rgba: width*height*4 bytes in R,G,B,A byte order.
    static bool writeFile(const std::string& path, int width, int height, const std::vector<uint8_t>& rgba);
    static std::vector<uint8_t> encode(int width, int height, const uint8_t* rgba, size_t size);
};

} // namespace ctfak
