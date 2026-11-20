// AudioDecoder.h — turns a game-bank audio blob into playable PCM16.
//
// XAudio2 consumes PCM16. The game decodes retained MP3 blobs with dr_mp3;
// this pack-side module handles Ogg/Vorbis and other sources that still
// need conversion before packing. It is also the content sniffer used to
// distinguish real MP3 data from headerless files misnamed ".wav".
//
// Two source families are handled here:
//   * RIFF/WAVE  — parsed in-process (PCM only; the game's banks carry
//     nothing else, and the ADPCM variant the console also supports is not
//     used by these games);
//   * MP3       — retained by Recomp Pack and decoded by the game runtime;
//   * Ogg/other — rejected here. No external tools, decoder processes,
//     temporary files, ffmpeg or sox are used.
//
// Nothing is resampled: the PCM keeps the source file's own sample rate and
// channel count, so the conversion adds no artefacts of its own.

#ifndef CTFAK_AUDIODECODER_H
#define CTFAK_AUDIODECODER_H

#include <cstdint>
#include <string>
#include <vector>

namespace ctfak {

enum class AudioKind {
    Unknown,
    WavPcm,     // RIFF/WAVE, WAVE_FORMAT_PCM
    WavAdpcm,   // RIFF/WAVE, WAVE_FORMAT_ADPCM (the console can play it)
    Mp3,        // MPEG-1/2/2.5 Layer III (ID3-prefixed or a bare frame stream)
    OggVorbis,  // Ogg container with a Vorbis stream
    Other       // anything else (module files, AIFF, ...)
};

// Recognises the container by its magic bytes. MP3 is detected by the ID3
// tag OR by a raw MPEG frame sync, so the headerless MP3s the FNAF banks
// ship under a ".wav" name are recognised too.
AudioKind sniffAudioKind(const std::vector<uint8_t>& data);

// Human-readable name for the manifest ("wav", "mp3", "ogg", ...).
const char* audioKindName(AudioKind kind);

// Decodes a blob to INTERLEAVED signed 16-bit little-endian PCM.
// Returns false when the format cannot be decoded (the caller then keeps
// the blob as-is and the sound stays silent instead of turning into noise).
// `tool` is empty for direct RIFF/WAVE handling and for rejected formats.
bool decodeToPcm16(const std::vector<uint8_t>& data,
                   AudioKind kind,
                   uint32_t& sampleRate,
                   uint32_t& channels,
                   std::vector<uint8_t>& pcm,
                   std::string& tool,
                   std::string& err);

} // namespace ctfak

#endif // CTFAK_AUDIODECODER_H
