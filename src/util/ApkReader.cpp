#include "ApkReader.h"
#include "../core/Zlib.h"
#include <cstring>

namespace ctfak {

static uint16_t rd16(const uint8_t* p) { uint16_t v; memcpy(&v, p, 2); return v; }
static uint32_t rd32(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }

bool ZipReader::open(const std::vector<uint8_t>& data) {
    _data = data.data();
    _size = data.size();
    _entries.clear();

    // Locate End Of Central Directory record (scan backwards for signature).
    const uint32_t EOCD_SIG = 0x06054b50;
    if (_size < 22) return false;
    size_t found = 0;
    bool ok = false;
    size_t maxScan = 66000; // comment can be up to 65535 bytes
    size_t start = _size > maxScan ? _size - maxScan : 0;
    for (size_t i = _size - 22 + 1; i-- > start;) {
        if (rd32(_data + i) == EOCD_SIG) {
            found = i;
            ok = true;
            break;
        }
    }
    if (!ok) return false;

    uint16_t entryCount = rd16(_data + found + 10);
    uint32_t cdOffset = rd32(_data + found + 16);

    const uint32_t CD_SIG = 0x02014b50;
    size_t pos = cdOffset;
    for (int n = 0; n < entryCount; ++n) {
        if (pos + 46 > _size || rd32(_data + pos) != CD_SIG) break;
        ZipEntry e;
        e.method = rd16(_data + pos + 10);
        e.crc32 = rd32(_data + pos + 16);
        e.compressedSize = rd32(_data + pos + 20);
        e.uncompressedSize = rd32(_data + pos + 24);
        uint16_t nameLen = rd16(_data + pos + 28);
        uint16_t extraLen = rd16(_data + pos + 30);
        uint16_t commentLen = rd16(_data + pos + 32);
        e.localHeaderOffset = rd32(_data + pos + 42);
        if (pos + 46 + nameLen > _size) break;
        e.name.assign(reinterpret_cast<const char*>(_data + pos + 46), nameLen);
        _entries.push_back(std::move(e));
        pos += 46 + nameLen + extraLen + commentLen;
    }
    return true;
}

bool ZipReader::extract(const ZipEntry& entry, std::vector<uint8_t>& out) const {
    const uint32_t LH_SIG = 0x04034b50;
    size_t pos = entry.localHeaderOffset;
    if (pos + 30 > _size || rd32(_data + pos) != LH_SIG) return false;
    uint16_t nameLen = rd16(_data + pos + 26);
    uint16_t extraLen = rd16(_data + pos + 28);
    size_t dataPos = pos + 30 + nameLen + extraLen;
    if (dataPos + entry.compressedSize > _size) return false;

    if (entry.method == 0) {
        out.assign(_data + dataPos, _data + dataPos + entry.compressedSize);
        return true;
    }
    if (entry.method == 8) {
        out = zlibInflate(_data + dataPos, entry.compressedSize, entry.uncompressedSize);
        return true;
    }
    return false;
}

} // namespace ctfak
