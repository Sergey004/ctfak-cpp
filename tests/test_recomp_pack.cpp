// Tests for the Recomp Pack bridge: DXT compression round trips, WAV
// parsing, and the big-endian pak container layout consumed by the
// recompilation runtime's PakLoader.
#include "test_framework.h"
#include "plugins/DxtEncoder.h"
#include "plugins/RecompPack.h"
#include <cstring>
#include <cstdlib>
#include <cmath>

using ctfak::PakSoundSpec;
using ctfak::PakTextureSpec;

namespace {

void fillRgba(std::vector<uint8_t>& img, int w, int h) {
    img.resize(static_cast<size_t>(w) * h * 4);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            uint8_t* p = &img[(static_cast<size_t>(y) * w + x) * 4];
            p[0] = static_cast<uint8_t>(x * 16);
            p[1] = static_cast<uint8_t>(y * 16);
            p[2] = static_cast<uint8_t>((x + y) * 8);
            p[3] = 255;
        }
    }
}

uint32_t readBe32(const std::vector<uint8_t>& v, size_t off) {
    return (static_cast<uint32_t>(v[off]) << 24) | (static_cast<uint32_t>(v[off + 1]) << 16) |
           (static_cast<uint32_t>(v[off + 2]) << 8) | static_cast<uint32_t>(v[off + 3]);
}

// Decode a full block grid back to RGBA with proper 2D block placement
// (block (bx,by) covers pixels x=bx*4.., y=by*4..).
void decodeGrid(const std::vector<uint8_t>& dxt, int w, int h, bool dxt5,
                std::vector<uint8_t>& out) {
    const int blockSize = dxt5 ? 16 : 8;
    out.assign(static_cast<size_t>(w) * h * 4, 0);
    uint8_t tmp[64];
    for (int by = 0; by < h / 4; ++by) {
        for (int bx = 0; bx < w / 4; ++bx) {
            const uint8_t* src = dxt.data() + (static_cast<size_t>(by) * (w / 4) + bx) * blockSize;
            if (dxt5) ctfak::dxt5DecodeBlock(src, tmp);
            else      ctfak::dxt1DecodeBlock(src, tmp);
            for (int y = 0; y < 4; ++y)
                for (int x = 0; x < 4; ++x)
                    std::memcpy(&out[(static_cast<size_t>((by * 4 + y) * w) + bx * 4 + x) * 4],
                                tmp + (y * 4 + x) * 4, 4);
        }
    }
}

std::string readCstr(const std::vector<uint8_t>& v, uint32_t off) {
    std::string s;
    while (off < v.size() && v[off] != 0) s.push_back(static_cast<char>(v[off++]));
    return s;
}

void appendWav(std::vector<uint8_t>& wav, uint32_t rate, uint16_t channels, uint16_t bits,
               const std::vector<uint8_t>& payload) {
    auto le16 = [&wav](uint16_t v) { wav.push_back(v & 0xFF); wav.push_back(v >> 8); };
    auto le32 = [&wav](uint32_t v) {
        wav.push_back(v & 0xFF); wav.push_back((v >> 8) & 0xFF);
        wav.push_back((v >> 16) & 0xFF); wav.push_back((v >> 24) & 0xFF);
    };
    auto tag = [&wav](const char* t) { wav.insert(wav.end(), t, t + 4); };
    uint32_t dataPad = payload.size() & 1;
    tag("RIFF"); le32(payload.size() + 36); tag("WAVE");
    tag("fmt "); le32(16); le16(1); le16(channels);
    le32(rate); le32(rate * channels * bits / 8); le16(channels * bits / 8); le16(bits);
    tag("data"); le32(payload.size());
    wav.insert(wav.end(), payload.begin(), payload.end());
    if (dataPad) wav.push_back(0);
}

} // namespace

TEST(dxt1_roundtrip_solid) {
    // A solid block must survive a DXT1 encode/decode cycle exactly in the
    // color channels and stay opaque.
    std::vector<uint8_t> img;
    fillRgba(img, 8, 8);
    for (size_t i = 0; i < img.size(); i += 4) { img[i] = 200; img[i + 1] = 40; img[i + 2] = 60; }
    std::vector<uint8_t> dxt;
    EXPECT_TRUE(ctfak::dxtCompress(img.data(), 8, 8, false, dxt));
    EXPECT_EQ(dxt.size(), 2 * 2 * 8u);
    std::vector<uint8_t> back;
    decodeGrid(dxt, 8, 8, false, back);
    for (size_t i = 0; i < back.size(); i += 4) {
        EXPECT_TRUE(std::abs(static_cast<int>(back[i]) - 200) <= 9);
        EXPECT_TRUE(std::abs(static_cast<int>(back[i + 1]) - 40) <= 9);
        EXPECT_TRUE(std::abs(static_cast<int>(back[i + 2]) - 60) <= 9);
        EXPECT_EQ(back[i + 3], 255);
    }
}

TEST(dxt1_gradient_error_bounded) {
    std::vector<uint8_t> img;
    fillRgba(img, 16, 16);
    std::vector<uint8_t> dxt;
    EXPECT_TRUE(ctfak::dxtCompress(img.data(), 16, 16, false, dxt));
    EXPECT_EQ(dxt.size(), 4 * 4 * 8u);
    std::vector<uint8_t> back;
    decodeGrid(dxt, 16, 16, false, back);
    double err = 0;
    for (size_t i = 0; i < back.size(); i += 4) {
        err += std::abs(static_cast<int>(back[i]) - static_cast<int>(img[i]));
        err += std::abs(static_cast<int>(back[i + 1]) - static_cast<int>(img[i + 1]));
        err += std::abs(static_cast<int>(back[i + 2]) - static_cast<int>(img[i + 2]));
    }
    EXPECT_TRUE(err / (16 * 16 * 3) < 12.0);
}

TEST(dxt1_transparent_index3) {
    // A block with cutout transparency must use 3-color+black mode
    // (color0 <= color1) and decode transparent pixels as alpha 0.
    std::vector<uint8_t> img(4 * 4 * 4, 0);
    for (int i = 0; i < 4 * 4; ++i) {
        img[i * 4 + 0] = 30; img[i * 4 + 1] = 200; img[i * 4 + 2] = 90;
        img[i * 4 + 3] = (i < 8) ? 0 : 255;
    }
    std::vector<uint8_t> dxt;
    EXPECT_TRUE(ctfak::dxtCompress(img.data(), 4, 4, false, dxt));
    uint16_t c0 = static_cast<uint16_t>((dxt[0] << 8) | dxt[1]);
    uint16_t c1 = static_cast<uint16_t>((dxt[2] << 8) | dxt[3]);
    EXPECT_TRUE(c0 <= c1);
    std::vector<uint8_t> back(16 * 4);
    ctfak::dxt1DecodeBlock(dxt.data(), back.data());
    EXPECT_EQ(back[3], 0);       // pixel 0 is transparent
    EXPECT_EQ(back[8 * 4 + 3], 255);
    EXPECT_TRUE(std::abs(static_cast<int>(back[8 * 4 + 1]) - 200) <= 12);
}

TEST(dxt5_alpha_roundtrip) {
    std::vector<uint8_t> img(8 * 8 * 4, 0);
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) {
            uint8_t* p = &img[(static_cast<size_t>(y) * 8 + x) * 4];
            p[0] = 255; p[1] = 128; p[2] = 0;
            p[3] = static_cast<uint8_t>(y * 32);
        }
    std::vector<uint8_t> dxt;
    EXPECT_TRUE(ctfak::dxtCompress(img.data(), 8, 8, true, dxt));
    EXPECT_EQ(dxt.size(), 2 * 2 * 16u);
    std::vector<uint8_t> back;
    decodeGrid(dxt, 8, 8, true, back);
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) {
            uint8_t a = back[(static_cast<size_t>(y) * 8 + x) * 4 + 3];
            EXPECT_TRUE(std::abs(static_cast<int>(a) - y * 32) <= 20);
        }
}

TEST(dxt_rejects_unaligned) {
    std::vector<uint8_t> img(6 * 6 * 4, 128);
    std::vector<uint8_t> dxt;
    EXPECT_TRUE(!ctfak::dxtCompress(img.data(), 6, 6, false, dxt));
    EXPECT_TRUE(dxt.empty());
}

TEST(pad_image_replicates_edges) {
    std::vector<uint8_t> img(2 * 2 * 4);
    // pixel (1,1) is the distinctive one
    for (int i = 0; i < 4; ++i) { img[(0 * 2 + 0) * 4 + i] = 10; img[(0 * 2 + 1) * 4 + i] = 20; }
    for (int i = 0; i < 4; ++i) { img[(1 * 2 + 0) * 4 + i] = 30; img[(1 * 2 + 1) * 4 + i] = 40; }
    std::vector<uint8_t> padded;
    ctfak::padImageRGBA(img, 2, 2, padded, 4, 4);
    EXPECT_EQ(padded.size(), 4 * 4 * 4u);
    // Bottom-right padding pixel must replicate source pixel (1,1).
    for (int i = 0; i < 4; ++i) EXPECT_EQ(padded[(3 * 4 + 3) * 4 + i], 40);
    for (int i = 0; i < 4; ++i) EXPECT_EQ(padded[(0 * 4 + 3) * 4 + i], 20);
    for (int i = 0; i < 4; ++i) EXPECT_EQ(padded[(3 * 4 + 0) * 4 + i], 30);
}

TEST(wav_parser_pcm) {
    std::vector<uint8_t> payload(400, 0x5A);
    std::vector<uint8_t> wav;
    appendWav(wav, 44100, 2, 16, payload);
    uint32_t rate = 0, channels = 0, bits = 0;
    size_t off = 0, size = 0;
    std::string err;
    EXPECT_TRUE(ctfak::parseWav(wav, rate, channels, bits, off, size, err));
    EXPECT_EQ(rate, 44100u);
    EXPECT_EQ(channels, 2u);
    EXPECT_EQ(bits, 16u);
    EXPECT_EQ(size, payload.size());
    EXPECT_EQ(wav[off], 0x5A);
}

TEST(wav_parser_rejects_non_pcm) {
    std::vector<uint8_t> wav;
    appendWav(wav, 22050, 1, 16, std::vector<uint8_t>(10, 1));
    wav[20 + 0] = 0x55; wav[20 + 1] = 0xFF; // audioFormat 0xFF55 (non-PCM)
    uint32_t rate = 0, channels = 0, bits = 0;
    size_t off = 0, size = 0;
    std::string err;
    EXPECT_TRUE(!ctfak::parseWav(wav, rate, channels, bits, off, size, err));
}

TEST(wav_parser_rejects_garbage) {
    uint32_t rate = 0, channels = 0, bits = 0;
    size_t off = 0, size = 0;
    std::string err;
    std::vector<uint8_t> junk(64, 0);
    EXPECT_TRUE(!ctfak::parseWav(junk, rate, channels, bits, off, size, err));
}

TEST(pak_layout_roundtrip) {
    // Two textures (DXT1 + DXT5) and one sound; parse the container back
    // exactly the way the console-side PakLoader does.
    std::vector<uint8_t> texA(4 * 4 * 4, 0);
    for (size_t i = 0; i < texA.size(); i += 4) { texA[i] = 10; texA[i + 3] = 255; }
    std::vector<uint8_t> texB(4 * 4 * 4, 0);
    for (size_t i = 0; i < texB.size(); i += 4) { texB[i] = 250; texB[i + 3] = 0; }
    std::vector<uint8_t> dxt1, dxt5;
    EXPECT_TRUE(ctfak::dxtCompress(texA.data(), 4, 4, false, dxt1));
    EXPECT_TRUE(ctfak::dxtCompress(texB.data(), 4, 4, true, dxt5));

    PakTextureSpec t1;
    t1.name = "img_39"; t1.origW = 1600; t1.origH = 720;
    t1.alignW = 1600; t1.alignH = 736; t1.fmt = 0; t1.data = dxt1;
    PakTextureSpec t2;
    t2.name = "img_129"; t2.origW = 62; t2.origH = 120;
    t2.alignW = 64; t2.alignH = 128; t2.fmt = 2; t2.data = dxt5;
    PakSoundSpec s1;
    s1.name = "snd_Buzz"; s1.fmt = 0; s1.sampleRate = 44100; s1.channels = 1;
    s1.data.assign(64, 0x7F);

    std::vector<uint8_t> pak;
    std::string err;
    EXPECT_TRUE(ctfak::buildPak({t1, t2}, {s1}, pak, err));

    EXPECT_EQ(readBe32(pak, 0), 0x464E4146u); // 'FNAF'
    EXPECT_EQ(readBe32(pak, 4), 1u);          // version
    EXPECT_EQ(readBe32(pak, 8), 2u);          // numTex
    EXPECT_EQ(readBe32(pak, 12), 1u);         // numSnd
    EXPECT_EQ(readBe32(pak, 16), 0u);         // numMus
    EXPECT_EQ(readBe32(pak, 20), 0u);         // numFnt
    const uint32_t stringsOff = readBe32(pak, 24);
    const uint32_t dataOff = readBe32(pak, 28);
    EXPECT_EQ(stringsOff, 32u + 2 * 36u + 1 * 32u);

    // Texture entry 0.
    EXPECT_EQ(readCstr(pak, readBe32(pak, 32)), "img_39");
    const uint32_t t1data = readBe32(pak, 36);
    EXPECT_EQ(readBe32(pak, 40), dxt1.size());
    EXPECT_EQ(readBe32(pak, 44), 0u);         // DXT1
    EXPECT_EQ(readBe32(pak, 48), 1600u);
    EXPECT_EQ(readBe32(pak, 52), 720u);
    EXPECT_EQ(readBe32(pak, 56), 1600u);
    EXPECT_EQ(readBe32(pak, 60), 736u);
    // Texture entry 1 starts at 32+36.
    EXPECT_EQ(readCstr(pak, readBe32(pak, 68)), "img_129");
    EXPECT_EQ(readBe32(pak, 80), 2u);         // DXT5
    // Sound entry starts at 32+72.
    EXPECT_EQ(readCstr(pak, readBe32(pak, 104)), "snd_Buzz");
    EXPECT_EQ(readBe32(pak, 116), 0u);        // PCM
    EXPECT_EQ(readBe32(pak, 120), 44100u);
    EXPECT_EQ(readBe32(pak, 124), 1u);

    // Data region: blobs live after the string pool, 16-byte aligned.
    EXPECT_TRUE(t1data >= dataOff);
    EXPECT_EQ(t1data, dataOff);               // first blob directly after strings
    EXPECT_TRUE(std::memcmp(pak.data() + t1data, dxt1.data(), dxt1.size()) == 0);
    const uint32_t t2data = readBe32(pak, 72);
    EXPECT_EQ(t2data % 16u, 0u);
    EXPECT_TRUE(std::memcmp(pak.data() + t2data, dxt5.data(), dxt5.size()) == 0);
    const uint32_t s1data = readBe32(pak, 108);
    EXPECT_EQ(s1data % 16u, 0u);
    EXPECT_TRUE(std::memcmp(pak.data() + s1data, s1.data.data(), s1.data.size()) == 0);
}

TEST(pak_rejects_empty) {
    std::vector<uint8_t> pak;
    std::string err;
    EXPECT_TRUE(!ctfak::buildPak({}, {}, pak, err));
}
