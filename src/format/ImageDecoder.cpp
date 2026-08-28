#include "ImageDecoder.h"
#include "../util/Utils.h"
#include <cstring>

namespace ctfak {
namespace ImageDecoder {

// The reference stores decoded pixels in B,G,R,A memory order (GDI+ Format32bppArgb).
// Our PNG pipeline expects R,G,B,A, so every translator below writes channels in
// RGBA order while replicating the reference's exact field extraction rules
// (including its colour-key comparison quirks, marked with comments).

void androidMode0ToRGBA(const std::vector<uint8_t>& data, int width, int height, std::vector<uint8_t>& out) {
    out.assign(static_cast<size_t>(width) * height * 4, 0);
    int pad = getPadding(width, 4); // equals 0 for every width (4-byte rows divide by 2)
    size_t position = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t o = (static_cast<size_t>(y) * width + x) * 4;
            out[o + 0] = data[position + 0]; // R (reference writes this to B slot +2)
            out[o + 1] = data[position + 1];
            out[o + 2] = data[position + 2];
            out[o + 3] = data[position + 3];
            position += 4;
        }
        position += static_cast<size_t>(pad) * 3;
    }
}

void androidMode1ToRGBA(const std::vector<uint8_t>& data, int width, int height, std::vector<uint8_t>& out) {
    out.assign(static_cast<size_t>(width) * height * 4, 0);
    int pad = getPadding(width, 3);
    size_t position = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint16_t v = static_cast<uint16_t>(data[position] | (data[position + 1] << 8));
            uint8_t a = static_cast<uint8_t>((v & 0xF) << 4);
            uint8_t r = static_cast<uint8_t>(((v & 0xF000) >> 12) << 4);
            uint8_t g = static_cast<uint8_t>(((v & 0xF00) >> 8) << 4);
            uint8_t b = static_cast<uint8_t>(((v & 0xF0) >> 4) << 4);
            size_t o = (static_cast<size_t>(y) * width + x) * 4;
            out[o + 0] = r;
            out[o + 1] = g;
            out[o + 2] = b;
            out[o + 3] = a;
            position += 2;
        }
        position += static_cast<size_t>(pad) * 2;
    }
}

void androidMode2ToRGBA(const std::vector<uint8_t>& data, int width, int height, std::vector<uint8_t>& out) {
    out.assign(static_cast<size_t>(width) * height * 4, 0);
    int pad = getPadding(width, 3);
    size_t position = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint16_t v = static_cast<uint16_t>(data[position] | (data[position + 1] << 8));
            uint8_t r = static_cast<uint8_t>(((v & 0xF800) >> 11) << 3);
            uint8_t g = static_cast<uint8_t>(((v & 0x7C0) >> 6) << 3);
            uint8_t b = static_cast<uint8_t>(((v & 0x3E) >> 1) << 3);
            size_t o = (static_cast<size_t>(y) * width + x) * 4;
            out[o + 0] = r;
            out[o + 1] = g;
            out[o + 2] = b;
            out[o + 3] = (v == 0) ? 0 : 255;
            position += 2;
        }
        position += static_cast<size_t>(pad) * 2;
    }
}

void androidMode3ToRGBA(const std::vector<uint8_t>& data, int width, int height, std::vector<uint8_t>& out) {
    out.assign(static_cast<size_t>(width) * height * 4, 0);
    int pad = getPadding(width, 3, 4, true);
    size_t position = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t o = (static_cast<size_t>(y) * width + x) * 4;
            out[o + 0] = data[position + 0];
            out[o + 1] = data[position + 1];
            out[o + 2] = data[position + 2];
            out[o + 3] = 255;
            position += 3;
        }
        position += static_cast<size_t>(pad);
    }
}

void androidMode4ToRGBA(const std::vector<uint8_t>& data, int width, int height, std::vector<uint8_t>& out) {
    out.assign(static_cast<size_t>(width) * height * 4, 0);
    int pad = getPadding(width, 3);
    size_t position = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint16_t v = static_cast<uint16_t>(data[position] | (data[position + 1] << 8));
            uint8_t r = static_cast<uint8_t>(((v & 0xF800) >> 11) << 3);
            uint8_t g = static_cast<uint8_t>(((v & 0x7E0) >> 5) << 2);
            uint8_t b = static_cast<uint8_t>((v & 0x1F) << 3);
            size_t o = (static_cast<size_t>(y) * width + x) * 4;
            out[o + 0] = r;
            out[o + 1] = g;
            out[o + 2] = b;
            out[o + 3] = 255;
            position += 2;
        }
        position += static_cast<size_t>(pad) * 2;
    }
}

// 24-bit colour plane with optional trailing alpha plane (reference mode 4).
// Colour-key quirk replicated: the reference compares the FIRST stored byte against
// transparent.R, second against .G, third against .B (its decoded buffer holds B
// first, so effectively B==transparent.R etc.).
std::vector<uint8_t> mode24MaskedToRGBA(const std::vector<uint8_t>& data, int width, int height,
                                        bool alpha, BinaryReader::Color transparent, bool flipRGB) {
    std::vector<uint8_t> out(static_cast<size_t>(width) * height * 4, 0);
    int pad = getPadding(width, 3);
    size_t position = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t o = (static_cast<size_t>(y) * width + x) * 4;
            uint8_t c0 = data[position + 0];
            uint8_t c1 = data[position + 1];
            uint8_t c2 = data[position + 2];
            if (flipRGB) {
                // F3 variant: source bytes are R,G,B for this path.
                out[o + 0] = c0;
                out[o + 1] = c1;
                out[o + 2] = c2;
            } else {
                // Source bytes are B,G,R (Windows DIB order).
                out[o + 0] = c2; // R
                out[o + 1] = c1; // G
                out[o + 2] = c0; // B
            }
            out[o + 3] = 255;
            if (!alpha) {
                // Reference comparison against its B-first buffer.
                if (c0 == transparent.r && c1 == transparent.g && c2 == transparent.b)
                    out[o + 3] = 0;
            }
            position += 3;
        }
        position += static_cast<size_t>(pad) * 3;
    }
    if (alpha) {
        int aPad = getPadding(width, 1, 4);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (position >= data.size()) break;
                out[(static_cast<size_t>(y) * width + x) * 4 + 3] = data[position];
                position += 1;
            }
            position += static_cast<size_t>(aPad);
        }
    }
    return out;
}

static void decode15or16(const std::vector<uint8_t>& data, int width, int height, bool alpha,
                         BinaryReader::Color transparent, bool is16, std::vector<uint8_t>& out) {
    out.assign(static_cast<size_t>(width) * height * 4, 0);
    int pad = getPadding(width, 2);
    size_t position = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint16_t v = static_cast<uint16_t>(data[position] | (data[position + 1] << 8));
            uint8_t r, g, b;
            if (is16) {
                r = static_cast<uint8_t>(((v & 63488) >> 11) << 3);
                g = static_cast<uint8_t>(((v & 2016) >> 5) << 2);
                b = static_cast<uint8_t>((v & 31) << 3);
            } else {
                r = static_cast<uint8_t>(((v & 31744) >> 10) << 3);
                g = static_cast<uint8_t>(((v & 992) >> 5) << 3);
                b = static_cast<uint8_t>((v & 31) << 3);
            }
            size_t o = (static_cast<size_t>(y) * width + x) * 4;
            out[o + 0] = r;
            out[o + 1] = g;
            out[o + 2] = b;
            out[o + 3] = 255;
            if (!alpha && r == transparent.r && g == transparent.g && b == transparent.b)
                out[o + 3] = 0;
            position += 2;
        }
        position += static_cast<size_t>(pad) * 2;
    }
    if (alpha) {
        int aPad = getPadding(width, 1, 4);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (position >= data.size()) break;
                out[(static_cast<size_t>(y) * width + x) * 4 + 3] = data[position];
                position += 1;
            }
            position += static_cast<size_t>(aPad);
        }
    }
}

std::vector<uint8_t> mode15ToRGBA(const std::vector<uint8_t>& data, int width, int height, bool alpha,
                                  BinaryReader::Color transparent, std::vector<uint8_t>& out) {
    decode15or16(data, width, height, alpha, transparent, false, out);
    return out;
}

std::vector<uint8_t> mode16ToRGBA(const std::vector<uint8_t>& data, int width, int height, bool alpha,
                                  BinaryReader::Color transparent, std::vector<uint8_t>& out) {
    decode15or16(data, width, height, alpha, transparent, true, out);
    return out;
}

// 32-bit BGRA plane + optional alpha plane / colour-key (reference mode 8).
std::vector<uint8_t> mode32ToRGBA(const std::vector<uint8_t>& data, int width, int height, bool alpha,
                                  BinaryReader::Color transparent, bool rgba, bool flipRGB) {
    std::vector<uint8_t> out(static_cast<size_t>(width) * height * 4, 0);
    int pad = getPadding(width, 4); // always 0: 4-byte rows divide evenly
    size_t position = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t o = (static_cast<size_t>(y) * width + x) * 4;
            uint8_t b = data[position + 0];
            uint8_t g = data[position + 1];
            uint8_t r = data[position + 2];
            uint8_t a = data[position + 3];
            if (flipRGB) {
                out[o + 0] = b; // F3: source bytes are R,G,B,A
                out[o + 1] = g;
                out[o + 2] = r;
            } else {
                out[o + 0] = r; // source bytes are B,G,R,A
                out[o + 1] = g;
                out[o + 2] = b;
            }
            out[o + 3] = 255;
            if (alpha || rgba) {
                out[o + 3] = a;
            } else if (r == transparent.r && g == transparent.g && b == transparent.b) {
                out[o + 3] = 0;
            }
            position += 4;
        }
        position += static_cast<size_t>(pad) * 4;
    }
    if (position == data.size()) return out;
    if (alpha && !rgba) {
        int aPad = getPadding(width, 1, 4);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (position >= data.size()) break;
                out[(static_cast<size_t>(y) * width + x) * 4 + 3] = data[position];
                position += 1;
            }
            position += static_cast<size_t>(aPad);
        }
    }
    return out;
}

} // namespace ImageDecoder
} // namespace ctfak
