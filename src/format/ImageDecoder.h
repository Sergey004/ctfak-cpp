// Pixel-format translators: convert stored pixel payloads into RGBA buffers.
// Output byte order: R,G,B,A per pixel (matches our PNG encoder).
// The reference implementations store B,G,R,A in memory and hand that to GDI+
// (Format32bppArgb); our PNG encoder maps R at +0, so translators emit true RGBA.
#pragma once

#include "../core/BinaryReader.h"
#include <vector>

namespace ctfak {

namespace ImageDecoder {

// mode 0: android, 4 bytes per pixel RGBA
void androidMode0ToRGBA(const std::vector<uint8_t>& data, int width, int height, std::vector<uint8_t>& out);
// mode 1: android, 2 bytes per pixel RGBA4444
void androidMode1ToRGBA(const std::vector<uint8_t>& data, int width, int height, std::vector<uint8_t>& out);
// mode 2: android, 2 bytes per pixel, 5/5/5 + 1-bit transparency
void androidMode2ToRGBA(const std::vector<uint8_t>& data, int width, int height, std::vector<uint8_t>& out);
// mode 3: android, 3 bytes per pixel RGB (modular padding)
void androidMode3ToRGBA(const std::vector<uint8_t>& data, int width, int height, std::vector<uint8_t>& out);
// mode 4 android: 2 bytes per pixel RGB565
void androidMode4ToRGBA(const std::vector<uint8_t>& data, int width, int height, std::vector<uint8_t>& out);

// mode 4: 24-bit colour plane + optional 1-byte alpha plane (or colour-key transparency)
std::vector<uint8_t> mode24MaskedToRGBA(const std::vector<uint8_t>& data, int width, int height,
                                        bool alpha, BinaryReader::Color transparent, bool flipRGB);
// mode 6: 15 bpp
std::vector<uint8_t> mode15ToRGBA(const std::vector<uint8_t>& data, int width, int height, bool alpha,
                                  BinaryReader::Color transparent, std::vector<uint8_t>& out);
// mode 7: 16 bpp (565)
std::vector<uint8_t> mode16ToRGBA(const std::vector<uint8_t>& data, int width, int height, bool alpha,
                                  BinaryReader::Color transparent, std::vector<uint8_t>& out);
// mode 8: 32-bit BGRA + optional alpha plane / colour-key
std::vector<uint8_t> mode32ToRGBA(const std::vector<uint8_t>& data, int width, int height, bool alpha,
                                  BinaryReader::Color transparent, bool rgba, bool flipRGB);

} // namespace ImageDecoder

} // namespace ctfak
