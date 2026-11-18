#include "SoundExt.h"

namespace ctfak {

static bool hasMagic(const std::vector<uint8_t>& d, size_t offset, const char* magic, size_t len) {
    if (d.size() < offset + len) return false;
    for (size_t i = 0; i < len; ++i)
        if (d[offset + i] != static_cast<uint8_t>(magic[i])) return false;
    return true;
}

std::string sniffSoundExtension(const std::vector<uint8_t>& d) {
    if (d.size() < 4) return ".bin";
    if (hasMagic(d, 0, "RIFF", 4)) return ".wav";
    if (hasMagic(d, 0, "OggS", 4)) return ".ogg";
    if (hasMagic(d, 0, "FORM", 4)) return ".aiff";
    if (hasMagic(d, 0, "ID3", 3)) return ".mp3";
    // A headerless MPEG audio frame starts with 11 set bits (0xFF + a byte
    // whose top three bits are set). Several FNAF banks ship MP3s without
    // an ID3 tag, and without this check they used to fall through to the
    // ".wav" fallback below and be misnamed on disk.
    if (d[0] == 0xFF && (d[1] & 0xE0) == 0xE0) {
        const int version = (d[1] >> 3) & 3;
        const int layer = (d[1] >> 1) & 3;
        if (version != 1 && layer != 0) return ".mp3";
    }
    if (d.size() < 0x2C) return ".bin";
    if (hasMagic(d, 0, "IMPM", 4)) return ".it";
    if (hasMagic(d, 0, "Extended Module: ", 17)) return ".xm";

    // MOD signatures live at offset 0x438.
    static const char* modSigs[] = {
        "2CHN", "M.K.", "6CHN", "8CHN", "10CH", "12CH", "14CH", "16CH", "18CH", "20CH",
        "22CH", "24CH", "26CH", "28CH", "30CH", "32CH", "M!K!", "FLT4", "OCTA"
    };
    if (d.size() >= 0x43C) {
        for (const char* sig : modSigs) {
            bool match = true;
            for (int i = 0; i < 4; ++i)
                if (d[0x438 + i] != static_cast<uint8_t>(sig[i])) { match = false; break; }
            if (match) return ".mod";
        }
    }
    if (hasMagic(d, 0x2C, "SCRM", 4)) return ".s3m";
    return ".wav";
}

} // namespace ctfak
