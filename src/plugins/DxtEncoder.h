// Minimal DXT1/DXT5 (BC1/BC3) block compressor, used by RecompPack to build
// GPU-ready textures for the Xbox 360 recompilation runtime.
//
// The compressor takes 8-bit RGBA input (R,G,B,A byte order) and emits the
// standard 4x4 block layout. Multi-byte words inside each block are stored
// BIG-ENDIAN: the console runtime's PakLoader comment states the pak payload
// is expected "already tiled+BE swapped", and the Xbox 360 GPU reads texture
// words in its native big-endian byte order. Matching reference decoders are
// provided so tests can verify the round trip.
#pragma once

#include <cstdint>
#include <vector>

namespace ctfak {

// Compress an RGBA image into DXT1 (useDxt5 == false) or DXT5 blocks.
// w and h must be multiples of 4 (pad the image yourself, e.g. with
// edge replication). Output size is (w/4)*(h/4)*(useDxt5 ? 16 : 8) bytes.
// Returns false on invalid input; out is only appended on success.
bool dxtCompress(const uint8_t* rgba, int w, int h, bool useDxt5,
                 std::vector<uint8_t>& out);

// Reference block decoders (big-endian word order, matching dxtCompress).
// Both write 16 pixels (64 bytes, R,G,B,A per pixel) to outRGBA.
void dxt1DecodeBlock(const uint8_t* in, uint8_t* outRGBA);
void dxt5DecodeBlock(const uint8_t* in, uint8_t* outRGBA);

// Pad an RGBA image to newW x newH by replicating the right column and
// bottom row (avoids dark halos when GPU samplers bleed past the original
// rectangle). Fills the top-left newW0 x newH0 region unchanged when the
// source is smaller.
void padImageRGBA(const std::vector<uint8_t>& src, int w, int h,
                  std::vector<uint8_t>& dst, int newW, int newH);

} // namespace ctfak
