#include "test_framework.h"
#include "core/BinaryReader.h"
#include "core/BinaryWriter.h"

using ctfak::BinaryReader;

static std::vector<uint8_t> buildSample() {
    std::vector<uint8_t> data;
    auto push16 = [&data](uint16_t v) { data.push_back(v & 0xFF); data.push_back((v >> 8) & 0xFF); };
    auto push32 = [&data](uint32_t v) {
        for (int i = 0; i < 4; ++i) data.push_back((v >> (8 * i)) & 0xFF);
    };
    push32(0x11223344);
    push16(0xBEEF);
    data.push_back(0x41); // 'A'
    data.push_back(0x42); // 'B'
    data.push_back(0x00); // terminator
    // Wide string "hi" + terminator
    push16('h');
    push16('i');
    push16(0);
    // color r,g,b,a
    data.push_back(0x10);
    data.push_back(0x20);
    data.push_back(0x30);
    data.push_back(0x40);
    return data;
}

TEST(reader_primitives) {
    auto data = buildSample();
    BinaryReader r(data.data(), data.size());
    EXPECT_EQ(r.size(), static_cast<int64_t>(data.size()));
    EXPECT_EQ(r.readU32(), 0x11223344u);
    EXPECT_EQ(r.readU16(), 0xBEEFu);
    EXPECT_EQ(r.readAscii(), "AB");
    EXPECT_EQ(r.readWideString(), "hi");
    auto c = r.readColor();
    EXPECT_EQ(c.r, 0x10);
    EXPECT_EQ(c.g, 0x20);
    EXPECT_EQ(c.b, 0x30);
    EXPECT_EQ(c.a, 0x40);
    EXPECT_TRUE(r.eof());
}

TEST(reader_seek_peek) {
    auto data = buildSample();
    BinaryReader r(data.data(), data.size());
    r.skip(4);
    EXPECT_EQ(r.peekI16(), static_cast<int16_t>(0xBEEF));
    EXPECT_EQ(r.tell(), 4);
    r.seek(0);
    EXPECT_EQ(r.readI32(), 0x11223344);
}

TEST(reader_bounds) {
    std::vector<uint8_t> small = {1, 2, 3};
    BinaryReader r(small.data(), small.size());
    bool threw = false;
    try {
        r.readU32();
    } catch (const ctfak::ReadError&) {
        threw = true;
    }
    EXPECT_TRUE(threw);
    EXPECT_EQ(r.tell(), 0);
}

TEST(writer_unicode) {
    ctfak::BinaryWriter w;
    w.writeUnicode("AB", true);
    EXPECT_EQ(w.buffer.size(), 6);
    EXPECT_EQ(w.buffer[0], 'A');
    EXPECT_EQ(w.buffer[2], 'B');
    EXPECT_EQ(w.buffer[4], 0);
    EXPECT_EQ(w.buffer[5], 0);
}
