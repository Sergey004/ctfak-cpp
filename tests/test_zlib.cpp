#include "test_framework.h"
#include "core/Zlib.h"
#include "core/ZlibLib.h"
#include <cstring>

TEST(zlib_runtime_api) {
    // The runtime loader must bind a full, working zlib: version string is
    // non-empty and the CRC-32 known answer matches the IEEE polynomial.
    const auto& api = ctfak::zlibApi();
    const char* ver = api.zlibVersion();
    EXPECT_TRUE(ver != nullptr);
    EXPECT_TRUE(*ver != '\0');

    const uint8_t kat[] = "123456789";
    EXPECT_EQ(ctfak::zlibCrc32(0, kat, 9), 0xCBF43926u);
}

TEST(zlib_roundtrip) {
    std::vector<uint8_t> original;
    for (int i = 0; i < 10000; ++i) original.push_back(static_cast<uint8_t>(i * 7 + (i % 13)));
    auto compressed = ctfak::zlibDeflate(original, 6);
    EXPECT_TRUE(compressed.size() > 2);
    // zlib header magic 78
    EXPECT_EQ(compressed[0], 0x78);
    auto decompressed = ctfak::zlibInflate(compressed, original.size());
    EXPECT_EQ(decompressed.size(), original.size());
    EXPECT_BYTES(decompressed.data(), decompressed.size(), original.data(), original.size());
}

TEST(zlib_empty) {
    std::vector<uint8_t> original;
    auto compressed = ctfak::zlibDeflate(original, 6);
    auto decompressed = ctfak::zlibInflate(compressed);
    EXPECT_EQ(decompressed.size(), 0);
}

TEST(zlib_repetitive) {
    std::vector<uint8_t> original(4096, 0xAB);
    original[100] = 0x00;
    original[4000] = 0x11;
    auto compressed = ctfak::zlibDeflate(original, 9);
    auto decompressed = ctfak::zlibInflate(compressed, original.size());
    EXPECT_EQ(decompressed.size(), original.size());
    EXPECT_BYTES(decompressed.data(), decompressed.size(), original.data(), original.size());
}
