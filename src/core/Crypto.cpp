#include "Crypto.h"
#include "../core/BinaryReader.h"
#include <cstring>

namespace ctfak {

std::vector<uint8_t> Crypto::_table(256, 0);
bool Crypto::_valid = false;

std::vector<uint8_t> Crypto::keyString(const std::string& utf8) {
    // Decode UTF-8 to code units; every code unit (up to U+FFFF, mirroring UTF-16
    // handling of the reference) contributes its low and high bytes.
    std::vector<uint16_t> units;
    for (size_t i = 0; i < utf8.size();) {
        uint32_t cp = static_cast<uint8_t>(utf8[i]);
        size_t len = 1;
        if ((cp & 0x80) == 0) len = 1;
        else if ((cp & 0xE0) == 0xC0) { cp &= 0x1F; len = 2; }
        else if ((cp & 0xF0) == 0xE0) { cp &= 0x0F; len = 3; }
        else if ((cp & 0xF8) == 0xF0) { cp &= 0x07; len = 4; }
        for (size_t k = 1; k < len && i + k < utf8.size(); ++k)
            cp = (cp << 6) | (static_cast<uint8_t>(utf8[i + k]) & 0x3F);
        i += len;
        if (cp < 0x10000) {
            units.push_back(static_cast<uint16_t>(cp));
        } else {
            cp -= 0x10000;
            units.push_back(static_cast<uint16_t>(0xD800 + (cp >> 10)));
            units.push_back(static_cast<uint16_t>(0xDC00 + (cp & 0x3FF)));
        }
    }
    std::vector<uint8_t> out;
    out.reserve(units.size() * 2);
    for (uint16_t code : units) {
        if ((code & 0xFF) != 0) out.push_back(static_cast<uint8_t>(code & 0xFF));
        if (((code >> 8) & 0xFF) != 0) out.push_back(static_cast<uint8_t>((code >> 8) & 0xFF));
    }
    return out;
}

std::vector<uint8_t> Crypto::makeKeyCombined(std::vector<uint8_t> data) {
    size_t dataLen = data.size();
    data.resize(256, 0);
    uint8_t v34 = kMagicChar;
    uint8_t lastKeyByte = kMagicChar;
    for (size_t i = 0; i <= dataLen; ++i) {
        v34 = rotl8(v34);
        data[i] ^= v34;
        lastKeyByte += static_cast<uint8_t>(data[i] * ((v34 & 1) + 2));
    }
    data[dataLen + 1] = lastKeyByte;
    return data;
}

void Crypto::initDecryptionTable(const std::vector<uint8_t>& magicKey, uint8_t magicChar) {
    for (int i = 0; i < 256; ++i) _table[i] = static_cast<uint8_t>(i);

    uint8_t accum = magicChar;
    uint8_t hash = magicChar;
    bool neverResetKey = true;
    uint8_t i2 = 0;
    uint32_t key = 0;

    for (int i = 0; i < 256; ++i, ++key) {
        hash = rotl8(hash);
        if (neverResetKey) {
            accum += ((hash & 1) == 0) ? 2 : 3;
            accum *= magicKey[key & 0xFF];
        }
        if (hash == magicKey[key & 0xFF]) {
            hash = rotl8(magicChar);
            key = 0;
            neverResetKey = false;
        }
        i2 = static_cast<uint8_t>(i2 + (hash ^ magicKey[key & 0xFF]) + _table[i]);
        std::swap(_table[i2], _table[i]);
    }
    _valid = true;
}

void Crypto::makeKey(const std::string& s1, const std::string& s2, const std::string& s3) {
    std::vector<uint8_t> bytes = keyString(s1);
    std::vector<uint8_t> part2 = keyString(s2);
    std::vector<uint8_t> part3 = keyString(s3);
    bytes.insert(bytes.end(), part2.begin(), part2.end());
    bytes.insert(bytes.end(), part3.begin(), part3.end());
    auto combined = makeKeyCombined(std::move(bytes));
    initDecryptionTable(combined, kMagicChar);
}

void Crypto::transformChunk(uint8_t* chunk, size_t size) {
    if (!_valid || size == 0) return;
    uint8_t tempBuf[256];
    memcpy(tempBuf, _table.data(), 256);
    uint8_t i = 0;
    uint8_t i2 = 0;
    for (size_t j = 0; j < size; ++j) {
        ++i;
        i2 += tempBuf[i];
        std::swap(tempBuf[i2], tempBuf[i]);
        uint8_t xored = tempBuf[static_cast<uint8_t>(tempBuf[i] + tempBuf[i2])];
        chunk[j] ^= xored;
    }
}

void Crypto::transformChunk(std::vector<uint8_t>& chunk) {
    transformChunk(chunk.data(), chunk.size());
}

} // namespace ctfak
