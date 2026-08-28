// Minimal ZIP archive reader (STORE + DEFLATE) used to unpack .apk containers.
// Reads the central directory; supports standard local file headers.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ctfak {

struct ZipEntry {
    std::string name;
    uint32_t compressedSize = 0;
    uint32_t uncompressedSize = 0;
    uint16_t method = 0;
    uint32_t localHeaderOffset = 0;
    uint32_t crc32 = 0;
};

class ZipReader {
public:
    bool open(const std::vector<uint8_t>& data);
    const std::vector<ZipEntry>& entries() const { return _entries; }
    // Decompresses the entry into `out`. Returns false on error/unknown method.
    bool extract(const ZipEntry& entry, std::vector<uint8_t>& out) const;

private:
    std::vector<ZipEntry> _entries;
    const uint8_t* _data = nullptr;
    size_t _size = 0;
};

} // namespace ctfak
