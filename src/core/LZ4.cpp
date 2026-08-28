#include "LZ4.h"
#include <stdexcept>
#include <cstring>

namespace ctfak {

size_t lz4DecodeBlock(const uint8_t* src, size_t srcSize, uint8_t* output, size_t outputSize) {
    if (outputSize == 0) return 0;
    size_t srcIndex = 0;
    size_t dstIndex = 0;

    auto readVarLen = [&]() -> size_t {
        size_t value = 0;
        uint8_t b;
        do {
            if (srcIndex >= srcSize) throw std::runtime_error("LZ4: truncated length");
            b = src[srcIndex++];
            value += b;
        } while (b == 255);
        return value;
    };

    while (srcIndex < srcSize) {
        // Token: high nibble = literal length, low nibble = match length - 4.
        uint8_t token = src[srcIndex++];
        size_t literalLen = token >> 4;
        if (literalLen == 15) literalLen += readVarLen();

        if (literalLen > srcSize - srcIndex) throw std::runtime_error("LZ4: literal overrun");
        if (literalLen > outputSize - dstIndex) throw std::runtime_error("LZ4: output overrun (literals)");
        memcpy(output + dstIndex, src + srcIndex, literalLen);
        srcIndex += literalLen;
        dstIndex += literalLen;

        if (srcIndex >= srcSize) break; // last literals, no match

        if (srcIndex + 2 > srcSize) throw std::runtime_error("LZ4: truncated offset");
        size_t offset = src[srcIndex] | (static_cast<size_t>(src[srcIndex + 1]) << 8);
        srcIndex += 2;
        if (offset == 0) throw std::runtime_error("LZ4: zero offset");
        if (offset > dstIndex) throw std::runtime_error("LZ4: offset beyond output");

        // Match length: base from token nibble, extension bytes follow the offset
        // (per the LZ4 block format specification).
        size_t matchLen = (token & 0x0F) + 4;
        if ((token & 0x0F) == 15) matchLen += readVarLen();

        if (matchLen > outputSize - dstIndex) throw std::runtime_error("LZ4: output overrun (match)");
        // Overlapping copies must be byte-by-byte.
        const uint8_t* matchSrc = output + dstIndex - offset;
        for (size_t i = 0; i < matchLen; ++i)
            output[dstIndex + i] = matchSrc[i];
        dstIndex += matchLen;
    }
    return dstIndex;
}

std::vector<uint8_t> lz4DecodeBlock(const std::vector<uint8_t>& block, size_t decompressedSize) {
    std::vector<uint8_t> out(decompressedSize);
    size_t written = lz4DecodeBlock(block.data(), block.size(), out.data(), out.size());
    out.resize(written);
    return out;
}

} // namespace ctfak
