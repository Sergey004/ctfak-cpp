#include "PngWriter.h"
#include "../core/Zlib.h"
#include "../core/ZlibLib.h"
#include <cstdio>
#include <cstring>

namespace ctfak {

static void be32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back((v >> 24) & 0xFF);
    out.push_back((v >> 16) & 0xFF);
    out.push_back((v >> 8) & 0xFF);
    out.push_back(v & 0xFF);
}

static void chunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& payload) {
    be32(out, static_cast<uint32_t>(payload.size()));
    size_t typePos = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), payload.begin(), payload.end());
    uint32_t crc = zlibCrc32(0, nullptr, 0);
    crc = zlibCrc32(crc, out.data() + typePos, 4 + payload.size());
    be32(out, crc);
}

std::vector<uint8_t> PngWriter::encode(int width, int height, const uint8_t* rgba, size_t size) {
    std::vector<uint8_t> png;
    static const uint8_t signature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    png.insert(png.end(), signature, signature + 8);

    // IHDR
    std::vector<uint8_t> ihdr;
    be32(ihdr, static_cast<uint32_t>(width));
    be32(ihdr, static_cast<uint32_t>(height));
    ihdr.push_back(8);  // bit depth
    ihdr.push_back(6);  // color type RGBA
    ihdr.push_back(0);  // compression
    ihdr.push_back(0);  // filter
    ihdr.push_back(0);  // interlace
    chunk(png, "IHDR", ihdr);

    // Raw scanlines with filter byte 0.
    size_t stride = static_cast<size_t>(width) * 4;
    std::vector<uint8_t> raw;
    raw.reserve((stride + 1) * static_cast<size_t>(height));
    for (int y = 0; y < height; ++y) {
        raw.push_back(0);
        size_t row = static_cast<size_t>(y) * stride;
        if (row + stride <= size)
            raw.insert(raw.end(), rgba + row, rgba + row + stride);
        else
            raw.insert(raw.end(), stride, 0);
    }
    auto compressed = zlibDeflate(raw, 6);
    chunk(png, "IDAT", compressed);
    chunk(png, "IEND", {});
    return png;
}

bool PngWriter::writeFile(const std::string& path, int width, int height, const std::vector<uint8_t>& rgba) {
    auto png = encode(width, height, rgba.data(), rgba.size());
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    bool ok = fwrite(png.data(), 1, png.size(), f) == png.size();
    fclose(f);
    return ok;
}

} // namespace ctfak
