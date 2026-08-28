#include "test_framework.h"
#include "io/PngWriter.h"
#include "core/Zlib.h"
#include <cstring>
#include <vector>

static uint32_t be32(const std::vector<uint8_t>& d, size_t off) {
    return (d[off] << 24) | (d[off + 1] << 16) | (d[off + 2] << 8) | d[off + 3];
}

TEST(png_encode_structure) {
    const int W = 4, H = 3;
    std::vector<uint8_t> rgba(static_cast<size_t>(W) * H * 4);
    for (size_t i = 0; i < rgba.size(); ++i) rgba[i] = static_cast<uint8_t>(i * 11);
    auto png = ctfak::PngWriter::encode(W, H, rgba.data(), rgba.size());

    // Signature
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    EXPECT_BYTES(png.data(), 8, sig, 8);

    // IHDR: length 13 at 8, type at 12-15, data at 16:
    // width 16-19, height 20-23, bit depth 24, color type 25.
    EXPECT_EQ(be32(png, 8), 13u);
    EXPECT_TRUE(png[12] == 'I' && png[13] == 'H' && png[14] == 'D' && png[15] == 'R');
    EXPECT_EQ(be32(png, 16), static_cast<uint32_t>(W));
    EXPECT_EQ(be32(png, 20), static_cast<uint32_t>(H));
    EXPECT_EQ(png[24], 8);
    EXPECT_EQ(png[25], 6);

    // IEND must be the last 12 bytes.
    const char* iend = "IEND";
    EXPECT_TRUE(memcmp(png.data() + png.size() - 8, iend, 4) == 0);
}

TEST(png_idat_roundtrip) {
    const int W = 5, H = 2;
    std::vector<uint8_t> rgba(static_cast<size_t>(W) * H * 4, 0);
    for (size_t i = 0; i < rgba.size(); ++i) rgba[i] = static_cast<uint8_t>((i * 31) & 0xFF);
    auto png = ctfak::PngWriter::encode(W, H, rgba.data(), rgba.size());

    // Locate IDAT payload.
    size_t pos = 8;
    std::vector<uint8_t> idat;
    while (pos + 8 <= png.size()) {
        uint32_t len = be32(png, pos);
        char type[5] = {0};
        memcpy(type, png.data() + pos + 4, 4);
        if (std::string(type) == "IDAT") {
            idat.assign(png.begin() + pos + 8, png.begin() + pos + 8 + len);
            break;
        }
        pos += 12 + len;
    }
    EXPECT_TRUE(!idat.empty());

    // Inflate; each row must start with filter byte 0 and match the source.
    auto raw = ctfak::zlibInflate(idat, static_cast<size_t>(H) * (W * 4 + 1));
    EXPECT_EQ(raw.size(), static_cast<size_t>(H) * (W * 4 + 1));
    for (int y = 0; y < H; ++y) {
        EXPECT_EQ(raw[static_cast<size_t>(y) * (W * 4 + 1)], 0);
        EXPECT_BYTES(raw.data() + static_cast<size_t>(y) * (W * 4 + 1) + 1, W * 4,
                     rgba.data() + static_cast<size_t>(y) * W * 4, W * 4);
    }
}
