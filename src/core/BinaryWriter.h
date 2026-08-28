// BinaryWriter - little-endian binary stream writer. Clean-room implementation.
#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace ctfak {

class BinaryWriter {
public:
    std::vector<uint8_t> buffer;

    int64_t tell() const { return static_cast<int64_t>(buffer.size()); }
    void writeU8(uint8_t v) { buffer.push_back(v); }
    void writeI8(int8_t v) { writeU8(static_cast<uint8_t>(v)); }
    void writeU16(uint16_t v) { buffer.push_back(v & 0xFF); buffer.push_back((v >> 8) & 0xFF); }
    void writeI16(int16_t v) { writeU16(static_cast<uint16_t>(v)); }
    void writeU32(uint32_t v) {
        for (int i = 0; i < 4; ++i) buffer.push_back((v >> (8 * i)) & 0xFF);
    }
    void writeI32(int32_t v) { writeU32(static_cast<uint32_t>(v)); }
    void writeF32(float v) {
        uint32_t bits;
        static_assert(sizeof(float) == sizeof(bits), "float must be 32-bit");
        memcpy(&bits, &v, sizeof(bits));
        writeU32(bits);
    }
    void writeAscii(const std::string& s, bool zeroTerminate = false) {
        buffer.insert(buffer.end(), s.begin(), s.end());
        if (zeroTerminate) writeU8(0);
    }
    void writeUnicode(const std::string& s, bool zeroTerminate = false) {
        // UTF-8 -> UTF-16LE
        for (size_t i = 0; i < s.size();) {
            uint32_t cp = static_cast<uint8_t>(s[i]);
            size_t len = 1;
            if ((cp & 0x80) == 0) len = 1;
            else if ((cp & 0xE0) == 0xC0) { cp &= 0x1F; len = 2; }
            else if ((cp & 0xF0) == 0xE0) { cp &= 0x0F; len = 3; }
            else { cp &= 0x07; len = 4; }
            for (size_t k = 1; k < len && i + k < s.size(); ++k)
                cp = (cp << 6) | (static_cast<uint8_t>(s[i + k]) & 0x3F);
            i += len;
            if (cp < 0x10000) {
                writeU16(static_cast<uint16_t>(cp));
            } else {
                cp -= 0x10000;
                writeU16(static_cast<uint16_t>(0xD800 + (cp >> 10)));
                writeU16(static_cast<uint16_t>(0xDC00 + (cp & 0x3FF)));
            }
        }
        if (zeroTerminate) writeU16(0);
    }
    void writeBytes(const std::vector<uint8_t>& v) { buffer.insert(buffer.end(), v.begin(), v.end()); }
    void writeBytes(const uint8_t* p, size_t n) { buffer.insert(buffer.end(), p, p + n); }
    void writeWriter(const BinaryWriter& other) { writeBytes(other.buffer.data(), other.buffer.size()); }
};

} // namespace ctfak
