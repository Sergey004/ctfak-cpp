// Recomp Pack (tool #10): bridges extracted game assets to the C++
// recompilation project by producing a GPU-ready "fnaf1.pak" container plus
// regenerated asset mapping tables, all derived from the parsed game data.
//
// Outputs (under <dumps>/<Game>/RecompPack/):
//   fnaf1.pak          -- big-endian container consumed by PakLoader.cpp
//                         (magic 'FNAF', DXT1/DXT5 textures, PCM sounds)
//   pak_manifest.json  -- full layout listing of the generated pak
//   asset_mapping.hpp  -- ground-truth 605-entry {handle,name,w,h,alpha} table
#pragma once

#include "Plugin.h"
#include <cstdint>
#include <string>
#include <vector>

namespace ctfak {

// One texture blob as it will live inside the pak.
struct PakTextureSpec {
    std::string name;      // NUL-terminated lookup name, e.g. "img_39"
    uint32_t origW = 0, origH = 0;
    uint32_t alignW = 0, alignH = 0;
    uint32_t fmt = 3;      // 0 = DXT1, 2 = DXT5, 3 = A8R8G8B8
    uint32_t mip = 1;
    std::vector<uint8_t> data;
};

// One sound blob as it will live inside the pak.
struct PakSoundSpec {
    std::string name;      // e.g. "snd_Buzz_Fan"
    uint32_t fmt = 0;      // 0 = raw PCM, 2 = passthrough (not PCM)
    uint32_t sampleRate = 0;
    uint32_t channels = 0;
    uint32_t loopStart = 0, loopEnd = 0;
    std::vector<uint8_t> data;
};

// Assemble the big-endian pak container:
//   header: 'FNAF', version, numTex, numSnd, numMus, numFnt, stringsOff, dataOff
//   then texture entries (36 B each), sound entries (32 B each),
//   the name string pool, and 16-byte-aligned data blobs.
// Returns false and fills err on invalid input.
bool buildPak(const std::vector<PakTextureSpec>& textures,
              const std::vector<PakSoundSpec>& sounds,
              std::vector<uint8_t>& out, std::string& err);

// Parse a RIFF/WAVE payload. On success fills sampleRate/channels/bits and
// the absolute offset+size of the data chunk inside `data`.
bool parseWav(const std::vector<uint8_t>& data, uint32_t& sampleRate,
              uint32_t& channels, uint32_t& bits, size_t& pcmOffset,
              size_t& pcmSize, std::string& err);

class RecompPack : public FusionTool {
public:
    const char* name() const override { return "Recomp Pack"; }
    void execute(GameData& game) override;
};

} // namespace ctfak
