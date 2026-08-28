#include "BinaryReader.h"
#include "Settings.h"
#include <cstring>

namespace ctfak {

void BinaryReader::ensure(int64_t n) const {
    if (n < 0 || static_cast<int64_t>(_size - _pos) < n)
        throw ReadError("Read out of bounds at offset " + std::to_string(_pos) + ", need " + std::to_string(n));
}

void BinaryReader::seek(int64_t pos) {
    if (pos < 0) pos = 0;
    if (pos > static_cast<int64_t>(_size)) pos = static_cast<int64_t>(_size);
    _pos = static_cast<size_t>(pos);
}

uint8_t BinaryReader::readU8() { ensure(1); return _data[_pos++]; }
int8_t BinaryReader::readI8() { return static_cast<int8_t>(readU8()); }

uint16_t BinaryReader::readU16() { ensure(2); uint16_t v; memcpy(&v, _data + _pos, 2); _pos += 2; return v; }
int16_t BinaryReader::readI16() { return static_cast<int16_t>(readU16()); }
uint32_t BinaryReader::readU32() { ensure(4); uint32_t v; memcpy(&v, _data + _pos, 4); _pos += 4; return v; }
int32_t BinaryReader::readI32() { return static_cast<int32_t>(readU32()); }
uint64_t BinaryReader::readU64() { ensure(8); uint64_t v; memcpy(&v, _data + _pos, 8); _pos += 8; return v; }
int64_t BinaryReader::readI64() { return static_cast<int64_t>(readU64()); }

float BinaryReader::readF32() { ensure(4); float v; memcpy(&v, _data + _pos, 4); _pos += 4; return v; }
double BinaryReader::readF64() { ensure(8); double v; memcpy(&v, _data + _pos, 8); _pos += 8; return v; }

uint8_t BinaryReader::peekU8() { ensure(1); return _data[_pos]; }
uint16_t BinaryReader::peekU16() { ensure(2); uint16_t v; memcpy(&v, _data + _pos, 2); return v; }
int32_t BinaryReader::peekI32() { ensure(4); int32_t v; memcpy(&v, _data + _pos, 4); return v; }
int16_t BinaryReader::peekI16() { ensure(2); int16_t v; memcpy(&v, _data + _pos, 2); return v; }

std::vector<uint8_t> BinaryReader::readBytes(int64_t count) {
    if (count < 0) count = static_cast<int64_t>(_size) - static_cast<int64_t>(_pos);
    ensure(count);
    std::vector<uint8_t> out(_data + _pos, _data + _pos + count);
    _pos += static_cast<size_t>(count);
    return out;
}

std::string BinaryReader::readAscii(int len) {
    std::string out;
    if (eof()) return out;
    if (len >= 0) {
        for (int i = 0; i < len; ++i) {
            if (eof()) break;
            out.push_back(static_cast<char>(readU8()));
        }
    } else {
        uint8_t b = readU8();
        while (b != 0) {
            out.push_back(static_cast<char>(b));
            if (eof()) break;
            b = readU8();
        }
    }
    return out;
}

static void appendUtf16(std::string& out, uint16_t ch) {
    if (ch < 0x80) {
        out.push_back(static_cast<char>(ch));
    } else if (ch < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (ch >> 6)));
        out.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xE0 | (ch >> 12)));
        out.push_back(static_cast<char>(0x80 | ((ch >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
    }
}

std::string BinaryReader::readWideString(int len) {
    std::string out;
    if (eof()) return out;
    if (len >= 0) {
        for (int i = 0; i < len; ++i) {
            if (!hasMemory(2)) break;
            appendUtf16(out, readU16());
        }
    } else {
        uint16_t b = readU16();
        while (b != 0) {
            appendUtf16(out, b);
            if (!hasMemory(2)) break;
            b = readU16();
        }
    }
    return out;
}

std::string BinaryReader::readYuniversal(int len) {
    if (Settings::unicode) return readWideString(len);
    return readAscii(len);
}

BinaryReader::Color BinaryReader::readColor() {
    Color c;
    c.r = readU8();
    c.g = readU8();
    c.b = readU8();
    c.a = readU8();
    return c;
}

} // namespace ctfak
