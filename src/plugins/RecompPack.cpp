// Recomp Pack implementation. See RecompPack.h.
//
// Texture pipeline: decoded RGBA -> alpha scan (any a != 255 => DXT5, else
// DXT1) -> edge-replicated padding to multiples of 32 (the alignment the
// console runtime expects for tiled textures) -> DXT compress -> pak blob.
// Sound pipeline: bank data -> RIFF/WAVE parse -> raw PCM payload with
// sample rate/channels recorded in the entry; non-PCM data passes through
// untouched with fmt = 2.

#include "RecompPack.h"
#include "DxtEncoder.h"
#include "../core/Logger.h"
#include "../util/Config.h"
#include "../util/Utils.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace ctfak {

namespace {

constexpr uint32_t kPakMagic = 0x464E4146u; // 'FNAF'
constexpr uint32_t kPakVersion = 1;
constexpr int kAlignStep = 32;   // texture alignment the X360 tiling expects
constexpr uint32_t kFmtDxt1 = 0;
constexpr uint32_t kFmtDxt5 = 2;
constexpr uint32_t kFmtA8R8G8B8 = 3;

void putU32be(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back(static_cast<uint8_t>(x >> 24));
    v.push_back(static_cast<uint8_t>(x >> 16));
    v.push_back(static_cast<uint8_t>(x >> 8));
    v.push_back(static_cast<uint8_t>(x));
}

uint32_t roundUpTo(uint32_t v, uint32_t step) {
    return (v + step - 1) / step * step;
}

std::string baseNameNoExt(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    std::string base = (slash == std::string::npos) ? path : path.substr(slash + 1);
    size_t dot = base.find_last_of('.');
    if (dot != std::string::npos && dot > 0) base = base.substr(0, dot);
    // SoundBank wide-name conversion can leave trailing padding NULs inside
    // the string; drop them so pak/manifest names are clean C strings.
    while (!base.empty() && base.back() == '\0') base.pop_back();
    return base;
}

std::string jsonEscapeCopy(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04X", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

} // namespace

bool parseWav(const std::vector<uint8_t>& data, uint32_t& sampleRate,
              uint32_t& channels, uint32_t& bits, size_t& pcmOffset,
              size_t& pcmSize, std::string& err) {
    auto tagAt = [&](size_t off) -> uint32_t {
        return data[off] | (data[off + 1] << 8) | (data[off + 2] << 16) |
               (static_cast<uint32_t>(data[off + 3]) << 24);
    };
    auto u16At = [&](size_t off) -> uint32_t {
        return data[off] | (data[off + 1] << 8);
    };
    auto u32At = [&](size_t off) -> uint32_t {
        return data[off] | (data[off + 1] << 8) | (data[off + 2] << 16) |
               (static_cast<uint32_t>(data[off + 3]) << 24);
    };

    if (data.size() < 12 || tagAt(0) != 0x46464952u /*'RIFF' little-endian*/ ||
        tagAt(8) != 0x45564157u /*'WAVE'*/) {
        err = "not a RIFF/WAVE payload";
        return false;
    }
    size_t pos = 12;
    bool haveFmt = false, haveData = false;
    while (pos + 8 <= data.size()) {
        uint32_t id = tagAt(pos);
        uint32_t size = u32At(pos + 4);
        size_t body = pos + 8;
        if (id == 0x20746D66u /*'fmt '*/ && size >= 16 && body + 16 <= data.size()) {
            uint32_t format = u16At(body);
            channels = u16At(body + 2);
            sampleRate = u32At(body + 4);
            bits = u16At(body + 14);
            if (format != 1) { err = "non-PCM wave (format != 1)"; return false; }
            haveFmt = true;
        } else if (id == 0x61746164u /*'data'*/) {
            size_t avail = data.size() - body;
            pcmOffset = body;
            pcmSize = std::min<uint64_t>(size, avail);
            haveData = true;
        }
        pos = body + size + (size & 1); // chunks are word-aligned
        if (size == 0 && id == 0) break; // corrupt tail guard
    }
    if (!haveFmt || !haveData) { err = "missing fmt or data chunk"; return false; }
    return true;
}

bool buildPak(const std::vector<PakTextureSpec>& textures,
              const std::vector<PakSoundSpec>& sounds,
              std::vector<uint8_t>& out, std::string& err) {
    out.clear();
    if (textures.empty() && sounds.empty()) {
        err = "no assets to pack";
        return false;
    }
    for (const auto& t : textures) {
        if (t.name.empty() || t.data.empty()) { err = "texture with empty name/data: " + t.name; return false; }
    }
    for (const auto& s : sounds) {
        if (s.name.empty() || s.data.empty()) { err = "sound with empty name/data: " + s.name; return false; }
    }

    const uint32_t numTex = static_cast<uint32_t>(textures.size());
    const uint32_t numSnd = static_cast<uint32_t>(sounds.size());
    const uint32_t headerSize = 8 * 4;
    const uint32_t texTable = numTex * 9 * 4;
    const uint32_t sndTable = numSnd * 8 * 4;
    const uint32_t stringsOffset = headerSize + texTable + sndTable;

    // String pool: names in stable order (textures first, then sounds).
    std::vector<uint8_t> strings;
    std::vector<uint32_t> nameOffsets;
    nameOffsets.reserve(numTex + numSnd);
    for (const auto& t : textures) {
        nameOffsets.push_back(stringsOffset + static_cast<uint32_t>(strings.size()));
        strings.insert(strings.end(), t.name.begin(), t.name.end());
        strings.push_back(0);
    }
    for (const auto& s : sounds) {
        nameOffsets.push_back(stringsOffset + static_cast<uint32_t>(strings.size()));
        strings.insert(strings.end(), s.name.begin(), s.name.end());
        strings.push_back(0);
    }
    const uint32_t dataOffset = stringsOffset + static_cast<uint32_t>(strings.size());

    // Pass 1: compute data blob offsets (16-byte aligned).
    std::vector<uint32_t> dataOffsets;
    dataOffsets.reserve(numTex + numSnd);
    uint32_t cursor = dataOffset;
    for (const auto& t : textures) {
        dataOffsets.push_back(cursor);
        cursor += static_cast<uint32_t>(t.data.size());
        cursor = roundUpTo(cursor, 16);
    }
    for (const auto& s : sounds) {
        dataOffsets.push_back(cursor);
        cursor += static_cast<uint32_t>(s.data.size());
        cursor = roundUpTo(cursor, 16);
    }

    // Header.
    putU32be(out, kPakMagic);
    putU32be(out, kPakVersion);
    putU32be(out, numTex);
    putU32be(out, numSnd);
    putU32be(out, 0); // numMus
    putU32be(out, 0); // numFnt
    putU32be(out, stringsOffset);
    putU32be(out, dataOffset);

    // Texture entries (nameOff, dataOff, dataSize, fmt, origW, origH, alignW, alignH, mip).
    for (size_t i = 0; i < textures.size(); ++i) {
        const auto& t = textures[i];
        putU32be(out, nameOffsets[i]);
        putU32be(out, dataOffsets[i]);
        putU32be(out, static_cast<uint32_t>(t.data.size()));
        putU32be(out, t.fmt);
        putU32be(out, t.origW);
        putU32be(out, t.origH);
        putU32be(out, t.alignW);
        putU32be(out, t.alignH);
        putU32be(out, t.mip);
    }
    // Sound entries (nameOff, dataOff, dataSize, fmt, sampleRate, channels, loopStart, loopEnd).
    for (size_t i = 0; i < sounds.size(); ++i) {
        const auto& s = sounds[i];
        putU32be(out, nameOffsets[numTex + i]);
        putU32be(out, dataOffsets[numTex + i]);
        putU32be(out, static_cast<uint32_t>(s.data.size()));
        putU32be(out, s.fmt);
        putU32be(out, s.sampleRate);
        putU32be(out, s.channels);
        putU32be(out, s.loopStart);
        putU32be(out, s.loopEnd);
    }
    out.insert(out.end(), strings.begin(), strings.end());

    // Data blobs with alignment padding.
    auto emitBlob = [&](const std::vector<uint8_t>& blob) {
        out.insert(out.end(), blob.begin(), blob.end());
        size_t pad = roundUpTo(static_cast<uint32_t>(out.size()), 16) - out.size();
        out.insert(out.end(), pad, 0);
    };
    for (const auto& t : textures) emitBlob(t.data);
    for (const auto& s : sounds) emitBlob(s.data);
    return true;
}

// ------------------------------------------------------------------ the tool
// Case-folded copy of the app title for canonical-pak-name matching.
static std::string LowerTitle(const std::string& in) {
    std::string out = in;
    for (std::string::iterator c = out.begin(); c != out.end(); ++c) {
        if (*c >= 'A' && *c <= 'Z') *c = static_cast<char>(*c - 'A' + 'a');
    }
    return out;
}

void RecompPack::execute(GameData& game) {
    std::string dir = pathJoin(pathJoin(g_dumpsDir,
        sanitizeGameName(game.name.empty() ? "Unknown Game" : game.name)), "RecompPack");
    createDirectories(dir);
    Logger::log("Recomp Pack: building pak + asset mapping from parsed game data...");

    // ---------------- textures
    std::vector<PakTextureSpec> textures;
    textures.reserve(game.images.items.size());
    uint64_t texBytes = 0;
    size_t dxt1Count = 0, dxt5Count = 0;
    for (auto& [handle, image] : game.images.items) {
        if (!image) continue;
        image->decode();
        PakTextureSpec t;
        t.name = "img_" + std::to_string(handle);
        t.origW = static_cast<uint32_t>(image->width);
        t.origH = static_cast<uint32_t>(image->height);
        t.alignW = roundUpTo(t.origW, kAlignStep);
        t.alignH = roundUpTo(t.origH, kAlignStep);

        bool hasAlpha = false;
        const std::vector<uint8_t>& rgba = image->rgba;
        const size_t pxCount = rgba.size() / 4;
        for (size_t p = 0; p < pxCount; ++p) {
            if (rgba[p * 4 + 3] != 255) { hasAlpha = true; break; }
        }

        std::vector<uint8_t> padded;
        if (t.alignW != t.origW || t.alignH != t.origH) {
            padImageRGBA(rgba, image->width, image->height, padded,
                         static_cast<int>(t.alignW), static_cast<int>(t.alignH));
        } else {
            padded = rgba;
        }
        t.fmt = hasAlpha ? kFmtDxt5 : kFmtDxt1;
        if (hasAlpha) ++dxt5Count; else ++dxt1Count;
        if (!dxtCompress(padded.data(), static_cast<int>(t.alignW),
                         static_cast<int>(t.alignH), hasAlpha, t.data)) {
            Logger::log("Recomp Pack: failed to compress img_" + std::to_string(handle) + ", skipped");
            continue;
        }
        texBytes += t.data.size();
        textures.push_back(std::move(t));
    }

    // ---------------- sounds
    std::vector<PakSoundSpec> sounds;
    sounds.reserve(game.sounds.items.size());
    uint64_t sndBytes = 0;
    size_t pcmCount = 0, passthroughCount = 0;
    for (const auto& item : game.sounds.items) {
        if (!item) continue;
        PakSoundSpec s;
        s.name = "snd_" + baseNameNoExt(item->name);
        uint32_t rate = 0, channels = 0, bits = 0;
        size_t off = 0, size = 0;
        std::string werr;
        if (parseWav(item->data, rate, channels, bits, off, size, werr)) {
            s.fmt = 0; // raw PCM
            s.sampleRate = rate;
            s.channels = channels;
            s.data.assign(item->data.begin() + off, item->data.begin() + off + size);
            ++pcmCount;
        } else {
            s.fmt = 2; // passthrough (format recorded for future players)
            s.data = item->data;
            ++passthroughCount;
        }
        sndBytes += s.data.size();
        sounds.push_back(std::move(s));
    }

    // ---------------- assemble + write pak
    std::vector<uint8_t> pak;
    std::string err;
    if (!buildPak(textures, sounds, pak, err)) {
        Logger::log(std::string("Recomp Pack: pak assembly failed: ") + err);
        return;
    }
    // Pak filename follows the recomp convention for the classic series.
    // The OLD heuristic (digit in the title -> fnafN.pak, else any
    // "Freddy" -> fnaf1.pak) MISIDENTIFIES "Freddy Fazbear's Pizzeria
    // Simulator" as fnaf1.pak, so titles are matched by a canonical table
    // first; the digit rule and the sanitized-name fallback remain for
    // unknown games (and future ones like UCN).
    std::string pakName;
    const std::string low = LowerTitle(game.name);   // case-folded for matching
    if (low.find("pizzeria") != std::string::npos || low.find("pizza simulator") != std::string::npos)
        pakName = "ffps.pak";                        // Freddy Fazbear's Pizzeria Simulator
    else if (low.find("sister location") != std::string::npos)
        pakName = "sisterlocation.pak";
    else if (low.find("ultimate custom night") != std::string::npos)
        pakName = "ucn.pak";
    if (pakName.empty()) {
        for (std::string::const_iterator c = game.name.begin(); c != game.name.end(); ++c) {
            if (*c >= '2' && *c <= '9') { pakName = std::string("fnaf") + *c + ".pak"; break; }
        }
    }
    if (pakName.empty()) {
        if (low.find("freddy") != std::string::npos) {
            pakName = "fnaf1.pak";                   // the original FNAF1
        } else {
            std::string clean;
            for (std::string::const_iterator c = game.name.begin(); c != game.name.end(); ++c) {
                const char L = (*c >= 'A' && *c <= 'Z') ? static_cast<char>(*c - 'A' + 'a') : *c;
                if ((L >= 'a' && L <= 'z') || (L >= '0' && L <= '9')) clean += L;
            }
            pakName = clean + ".pak";
        }
    }
    std::string pakPath = pathJoin(dir, pakName);
    FILE* f = std::fopen(pakPath.c_str(), "wb");
    if (!f || std::fwrite(pak.data(), 1, pak.size(), f) != pak.size()) {
        Logger::log("Recomp Pack: failed to write " + pakPath);
        if (f) std::fclose(f);
        return;
    }
    std::fclose(f);

    // ---------------- ground-truth asset mapping table
    // v2.29: per-game header — the game id (fnaf1/fnaf2/../sisterlocation)
    // names BOTH the pak and the emitted namespace/file, so any number of
    // tables can coexist in the port (each game module includes only its
    // own:  #include "assets/asset_mapping_fnaf2.hpp"  ->  fnaf2::assets).
    std::string gameId = pakName.substr(0, pakName.size() - 4);   // strip ".pak"
    std::string mapping = "#pragma once\n\n"
                          "// Generated by ctfak-cpp Recomp Pack from the parsed game data\n"
                          "// (" + game.name + "). `filename` values are the pak texture names;\n"
                          "// alpha reflects whether the source pixels contain any\n"
                          "// transparency (DXT5 vs DXT1).\n"
                          "namespace " + gameId + " {\n"
                          "struct AssetMapping { int index; const char* filename; int width; int height; bool alpha; };\n"
                          "const int ASSET_COUNT = " + std::to_string(textures.size()) + ";\n"
                          "const AssetMapping assets[" + std::to_string(textures.size()) + "] = {\n";
    for (const auto& t : textures) {
        char line[160];
        std::snprintf(line, sizeof(line), "    { %s, \"%s\", %u, %u, %s },\n",
                      t.name.c_str() + 4, t.name.c_str(), t.origW, t.origH,
                      t.fmt == kFmtDxt5 ? "true" : "false");
        mapping += line;
    }
    mapping += "};\n} // namespace " + gameId + "\n";
    std::FILE* mf = std::fopen(pathJoin(dir, "asset_mapping_" + gameId + ".hpp").c_str(), "wb");
    if (mf) { std::fwrite(mapping.data(), 1, mapping.size(), mf); std::fclose(mf); }

    // ---------------- manifest
    std::string json;
    json += "{\n  \"magic\": \"FNAF\",\n  \"version\": " + std::to_string(kPakVersion) + ",\n";
    json += "  \"textureCount\": " + std::to_string(textures.size()) + ",\n";
    json += "  \"soundCount\": " + std::to_string(sounds.size()) + ",\n";
    json += "  \"pakBytes\": " + std::to_string(pak.size()) + ",\n";
    json += "  \"textures\": [\n";
    for (size_t i = 0; i < textures.size(); ++i) {
        const auto& t = textures[i];
        char entry[256];
        std::snprintf(entry, sizeof(entry),
                      "    {\"name\": \"%s\", \"format\": \"%s\", \"width\": %u, \"height\": %u, "
                      "\"alignedWidth\": %u, \"alignedHeight\": %u, \"bytes\": %u}%s\n",
                      jsonEscapeCopy(t.name).c_str(),
                      t.fmt == kFmtDxt5 ? "DXT5" : "DXT1",
                      t.origW, t.origH, t.alignW, t.alignH,
                      static_cast<unsigned>(t.data.size()),
                      i + 1 < textures.size() ? "," : "");
        json += entry;
    }
    json += "  ],\n  \"sounds\": [\n";
    for (size_t i = 0; i < sounds.size(); ++i) {
        const auto& s = sounds[i];
        char entry[256];
        std::snprintf(entry, sizeof(entry),
                      "    {\"name\": \"%s\", \"format\": \"%s\", \"sampleRate\": %u, "
                      "\"channels\": %u, \"bytes\": %u}%s\n",
                      jsonEscapeCopy(s.name).c_str(),
                      s.fmt == 0 ? "PCM" : "passthrough",
                      s.sampleRate, s.channels,
                      static_cast<unsigned>(s.data.size()),
                      i + 1 < sounds.size() ? "," : "");
        json += entry;
    }
    json += "  ]\n}\n";
    std::FILE* jf = std::fopen(pathJoin(dir, "pak_manifest.json").c_str(), "wb");
    if (jf) { std::fwrite(json.data(), 1, json.size(), jf); std::fclose(jf); }

    char summary[256];
    std::snprintf(summary, sizeof(summary),
                  "Recomp Pack done: %zu textures (%zu DXT1, %zu DXT5, %.1f MB), "
                  "%zu sounds (%zu PCM, %zu passthrough, %.1f MB), pak %.1f MB -> %s",
                  textures.size(), dxt1Count, dxt5Count, texBytes / 1048576.0,
                  sounds.size(), pcmCount, passthroughCount, sndBytes / 1048576.0,
                  pak.size() / 1048576.0, pakPath.c_str());
    Logger::log(summary);
}

} // namespace ctfak
