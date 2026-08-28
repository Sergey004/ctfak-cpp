// BinaryReader - little-endian binary stream reader over a memory buffer.
// Clean-room implementation for ctfak-cpp.
#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <stdexcept>

namespace ctfak {

class ReadError : public std::runtime_error {
public:
    explicit ReadError(const std::string& msg) : std::runtime_error(msg) {}
};

class BinaryReader {
public:
    BinaryReader() = default;
    // NOTE: the reader borrows the buffer; the caller must keep it alive.
    BinaryReader(const uint8_t* data, size_t size) : _data(data), _size(size) {}

    // Navigation ------------------------------------------------------------
    int64_t tell() const { return static_cast<int64_t>(_pos); }
    int64_t size() const { return static_cast<int64_t>(_size); }
    void seek(int64_t pos);
    void skip(int64_t count) { seek(static_cast<int64_t>(_pos) + count); }
    bool hasMemory(int64_t n) const { return static_cast<int64_t>(_size - _pos) >= n; }
    bool eof() const { return _pos >= _size; }
    const uint8_t* data() const { return _data; }

    // Primitives ------------------------------------------------------------
    uint8_t  readU8();
    int8_t   readI8();
    uint16_t readU16();
    int16_t  readI16();
    uint32_t readU32();
    int32_t  readI32();
    uint64_t readU64();
    int64_t  readI64();
    float    readF32();
    double   readF64();

    uint8_t  peekU8();
    uint16_t peekU16();
    int32_t  peekI32();
    int16_t  peekI16();

    std::vector<uint8_t> readBytes(int64_t count = -1); // -1 = rest of buffer

    // Strings ----------------------------------------------------------------
    // Fixed length: exactly n units. Variable (-1): until NUL terminator (consumed).
    std::string readAscii(int len = -1);
    std::string readWideString(int len = -1);
    // Reads ASCII or UTF-16 depending on the global unicode flag (Settings).
    std::string readYuniversal(int len = -1);

    // Color stored as 4 bytes in file order r,g,b,a.
    struct Color {
        uint8_t r = 0, g = 0, b = 0, a = 255;
        bool operator==(const Color& o) const { return r == o.r && g == o.g && b == o.b && a == o.a; }
    };
    Color readColor();

private:
    void ensure(int64_t n) const;
    const uint8_t* _data = nullptr;
    size_t _size = 0;
    size_t _pos = 0;
};

} // namespace ctfak
