#include "WavWriter.h"
#include "../core/BinaryWriter.h"
#include <cstdio>

namespace ctfak {

std::vector<uint8_t> WavWriter::buildWav(uint16_t format, uint16_t channels, uint32_t sampleRate,
                                         uint32_t byteRate, uint16_t blockAlign, uint16_t bitsPerSample,
                                         const std::vector<uint8_t>& data) {
    BinaryWriter w;
    w.writeAscii("RIFF");
    w.writeI32(static_cast<int32_t>(data.size() + 36));
    w.writeAscii("WAVE");
    w.writeAscii("fmt ");
    w.writeI32(16);
    w.writeU16(format);
    w.writeU16(channels);
    w.writeU32(sampleRate);
    w.writeU32(byteRate);
    w.writeU16(blockAlign);
    w.writeU16(bitsPerSample);
    w.writeAscii("data");
    w.writeU32(static_cast<uint32_t>(data.size()));
    w.writeBytes(data);
    return std::move(w.buffer);
}

bool WavWriter::writeFile(const std::string& path, const std::vector<uint8_t>& wav) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    bool ok = fwrite(wav.data(), 1, wav.size(), f) == wav.size();
    fclose(f);
    return ok;
}

} // namespace ctfak
