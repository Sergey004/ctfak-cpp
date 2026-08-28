// Helpers for writing PCM WAV files (used to wrap raw old-format sound data).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ctfak {

class WavWriter {
public:
    // Builds a canonical RIFF/WAVE file around PCM payload.
    static std::vector<uint8_t> buildWav(uint16_t format, uint16_t channels, uint32_t sampleRate,
                                         uint32_t byteRate, uint16_t blockAlign, uint16_t bitsPerSample,
                                         const std::vector<uint8_t>& data);
    static bool writeFile(const std::string& path, const std::vector<uint8_t>& wav);
};

} // namespace ctfak
