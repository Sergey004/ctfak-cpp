// AudioDecoder.cpp — in-process audio decoding only.

#include "AudioDecoder.h"

#include <cstdlib>
#include <cstring>

namespace ctfak {

namespace {

bool hasMagic(const std::vector<uint8_t>& d, size_t off, const char* magic, size_t len) {
    if (d.size() < off + len) return false;
    return std::memcmp(d.data() + off, magic, len) == 0;
}

uint32_t rdLE32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

uint16_t rdLE16(const uint8_t* p) {
    return (uint16_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8));
}

// A raw MPEG audio frame starts with 11 set bits: 0xFF followed by a byte
// whose top three bits are set. The FNAF banks ship plenty of headerless
// MP3s (no ID3 tag), so this is the only reliable way to spot them.
bool looksLikeMpegFrame(const std::vector<uint8_t>& d) {
    if (d.size() < 4) return false;
    if (d[0] != 0xFF) return false;
    if ((d[1] & 0xE0) != 0xE0) return false;
    const int version = (d[1] >> 3) & 3;
    const int layer   = (d[1] >> 1) & 3;
    if (version == 1) return false;
    if (layer == 0) return false;
    return true;
}

struct WavInfo {
    bool        ok = false;
    uint16_t    format = 0;
    uint16_t    channels = 0;
    uint32_t    sampleRate = 0;
    uint16_t    bits = 0;
    size_t      dataOff = 0;
    size_t      dataSize = 0;
};

// Walks the chunk list of a RIFF/WAVE file. Tolerates a leading JUNK chunk
// (some of the game's files carry one) and unknown chunks in general.
WavInfo parseRiffWav(const std::vector<uint8_t>& d) {
    WavInfo w;
    if (d.size() < 12) return w;
    if (!hasMagic(d, 0, "RIFF", 4) || !hasMagic(d, 8, "WAVE", 4)) return w;

    size_t pos = 12;
    while (pos + 8 <= d.size()) {
        const char* id = reinterpret_cast<const char*>(d.data() + pos);
        uint32_t size = rdLE32(d.data() + pos + 4);
        size_t body = pos + 8;
        if (size > d.size() - body) size = (uint32_t)(d.size() - body);
        if (std::memcmp(id, "fmt ", 4) == 0 && size >= 16) {
            w.format     = rdLE16(d.data() + body + 0);
            w.channels   = rdLE16(d.data() + body + 2);
            w.sampleRate = rdLE32(d.data() + body + 4);
            w.bits       = rdLE16(d.data() + body + 14);
        } else if (std::memcmp(id, "data", 4) == 0) {
            w.dataOff = body;
            w.dataSize = size;
        }
        if (w.format && w.dataSize) { w.ok = true; break; }
        pos = body + size + (size & 1);
    }
    return w;
}

// Expands unsigned 8-bit PCM to signed 16-bit (the Xbox 360's XAudio2 2.x
// has no 8-bit sample format).
void expandU8ToS16(const uint8_t* src, size_t n, std::vector<uint8_t>& out) {
    out.resize(n * 2);
    for (size_t i = 0; i < n; ++i) {
        const int v = ((int)src[i] - 128) << 8;
        out[i * 2 + 0] = (uint8_t)(v & 0xFF);
        out[i * 2 + 1] = (uint8_t)((v >> 8) & 0xFF);
    }
}

} // namespace

AudioKind sniffAudioKind(const std::vector<uint8_t>& data) {
    if (hasMagic(data, 0, "RIFF", 4) && hasMagic(data, 8, "WAVE", 4)) {
        const WavInfo w = parseRiffWav(data);
        if (w.format == 1) return AudioKind::WavPcm;
        if (w.format == 2) return AudioKind::WavAdpcm;
        if (w.format == 0xFFFE) {
            // WAVE_FORMAT_EXTENSIBLE: the real tag hides in the SubFormat GUID.
            if (data.size() >= 12 + 8 + 16 + 2) {
                const size_t fmtBody = 12 + 8;
                if (rdLE16(data.data() + fmtBody) == 0xFFFE && data.size() >= fmtBody + 26) {
                    const uint16_t sub = rdLE16(data.data() + fmtBody + 24);
                    if (sub == 1) return AudioKind::WavPcm;
                    if (sub == 2) return AudioKind::WavAdpcm;
                }
            }
        }
        return AudioKind::Other;
    }
    if (hasMagic(data, 0, "OggS", 4)) return AudioKind::OggVorbis;
    if (hasMagic(data, 0, "ID3", 3)) return AudioKind::Mp3;
    if (looksLikeMpegFrame(data)) return AudioKind::Mp3;
    return AudioKind::Other;
}

const char* audioKindName(AudioKind kind) {
    switch (kind) {
        case AudioKind::WavPcm:    return "wav";
        case AudioKind::WavAdpcm:  return "wav-adpcm";
        case AudioKind::Mp3:       return "mp3";
        case AudioKind::OggVorbis: return "ogg";
        default:                   return "other";
    }
}

bool decodeToPcm16(const std::vector<uint8_t>& data,
                   AudioKind kind,
                   uint32_t& sampleRate,
                   uint32_t& channels,
                   std::vector<uint8_t>& pcm,
                   std::string& tool,
                   std::string& err) {
    tool.clear();
    err.clear();
    pcm.clear();
    if (data.empty()) { err = "empty blob"; return false; }

    if (kind == AudioKind::WavPcm) {
        const WavInfo w = parseRiffWav(data);
        if (!w.ok || !w.dataSize) { err = "malformed RIFF/WAVE"; return false; }
        if (w.channels < 1 || w.channels > 2) { err = "unsupported channel count"; return false; }
        sampleRate = w.sampleRate;
        channels   = w.channels;
        if (w.bits == 8) {
            expandU8ToS16(data.data() + w.dataOff, w.dataSize, pcm);
        } else if (w.bits == 16) {
            pcm.assign(data.begin() + (long)w.dataOff,
                       data.begin() + (long)(w.dataOff + w.dataSize));
        } else {
            err = "unsupported bit depth";
            return false;
        }
        return true;
    }

    // MP3 is intentionally retained by RecompPack and decoded by the game.
    // No current game bank contains Ogg; adding another decoder here would be
    // dead code, so unsupported formats fail closed instead of invoking tools.
    err = "unsupported audio format (MP3 is handled by the game runtime)";
    return false;
}

} // namespace ctfak
