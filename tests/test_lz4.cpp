#include "test_framework.h"
#include "core/LZ4.h"
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

TEST(lz4_literals_only) {
    // Token 0xF0 (15 -> extended) + ext byte 5 => 20 literals, no match.
    std::string text(20, 'x');
    for (int i = 0; i < 20; ++i) text[i] = static_cast<char>('A' + (i % 26));
    std::vector<uint8_t> block;
    block.push_back(0xF0);
    block.push_back(20 - 15);
    block.insert(block.end(), text.begin(), text.end());
    auto out = ctfak::lz4DecodeBlock(block, 20);
    EXPECT_EQ(out.size(), 20);
    EXPECT_BYTES(out.data(), out.size(), text.data(), text.size());
}

TEST(lz4_simple_match) {
    // "ABCABCABC": one token 0x32 = 3 literals + match len (2+4)=6 at offset 3.
    std::vector<uint8_t> block = {0x32, 'A', 'B', 'C', 0x03, 0x00};
    auto out = ctfak::lz4DecodeBlock(block, 9);
    std::string expected = "ABCABCABC";
    EXPECT_EQ(out.size(), 9);
    EXPECT_BYTES(out.data(), out.size(), expected.data(), expected.size());
}

TEST(lz4_extended_match_length) {
    // "WXYZ" + 25 match bytes at offset 4 (overlapping).
    // Token 0x4F: 4 literals, match nibble 0xF -> base 19, ext byte 6 (AFTER the
    // offset, per the LZ4 block format) -> 25.
    std::vector<uint8_t> block = {0x4F, 'W', 'X', 'Y', 'Z', 0x04, 0x00, 0x06};
    std::vector<uint8_t> expected(29, 0);
    memcpy(expected.data(), "WXYZ", 4);
    for (int i = 4; i < 29; ++i) expected[i] = expected[i - 4];
    auto out = ctfak::lz4DecodeBlock(block, 29);
    EXPECT_EQ(out.size(), 29);
    EXPECT_BYTES(out.data(), out.size(), expected.data(), expected.size());
}

TEST(lz4_offset_beyond_output_throws) {
    // 3 literals then a match pointing before the output start.
    std::vector<uint8_t> block = {0x30, 'A', 'B', 'C', 0x40, 0x00};
    bool threw = false;
    try {
        ctfak::lz4DecodeBlock(block, 7);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    EXPECT_TRUE(threw);
}
