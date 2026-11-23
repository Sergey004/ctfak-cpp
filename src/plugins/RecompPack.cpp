// Recomp Pack implementation. See RecompPack.h.
//
// Texture pipeline: decoded RGBA -> alpha scan (any a != 255 => DXT5, else
// DXT1) -> edge-replicated padding to multiples of 32 (the alignment the
// console runtime expects for tiled textures) -> DXT compress -> pak blob.
// Sound pipeline: PCM WAV sources keep their complete RIFF/WAVE blob;
// MP3 sources pass through byte-exact with fmt=2 for the game's dr_mp3;
// Ogg/other supported inputs are converted to a canonical PCM16 WAV.

#include "RecompPack.h"
#include "DxtEncoder.h"
#include "../core/Logger.h"
#include "../format/Events.h"   // v2.81: the parsed event program (Kind::Sample)
#include "../io/WavWriter.h"    // v2.92: wraps decoded PCM back into a RIFF/WAVE
#include "../util/AudioDecoder.h"  // content sniffing + Ogg -> PCM16
#include "../util/Config.h"
#include "../util/Utils.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <map>
#include <set>

namespace ctfak {

namespace {

constexpr uint32_t kPakMagic = 0x464E4146u; // 'FNAF'
constexpr uint32_t kPakVersion = 1; // v1: 8 u32 per sound entry. The sound
                                    // parameters (rate/channels/bit depth)
                                    // live in the RIFF/WAVE header of the
                                    // blob itself — the same place the
                                    // original Clickteam app read them from.
                                    // (v2 briefly added a `bits` word to the
                                    // table; with the headers kept in the
                                    // pak it is redundant.)
constexpr int kAlignStep = 32;   // texture alignment the X360 tiling expects
constexpr uint32_t kFmtDxt1 = 0;
constexpr uint32_t kFmtDxt5 = 2;
constexpr uint32_t kFmtA8R8G8B8 = 3;

// Sound format constants
constexpr uint32_t kSndFmtPcm = 0;
constexpr uint32_t kSndFmtPassthrough = 2;

} // namespace

// Helper functions (in ctfak namespace for access by execute())
inline void putU32be(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back(static_cast<uint8_t>(x >> 24));
    v.push_back(static_cast<uint8_t>(x >> 16));
    v.push_back(static_cast<uint8_t>(x >> 8));
    v.push_back(static_cast<uint8_t>(x));
}

inline uint32_t roundUpTo(uint32_t v, uint32_t step) {
    return (v + step - 1) / step * step;
}

inline std::string baseNameNoExt(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    std::string base = (slash == std::string::npos) ? path : path.substr(slash + 1);
    size_t dot = base.find_last_of('.');
    if (dot != std::string::npos && dot > 0) base = base.substr(0, dot);
    // SoundBank wide-name conversion can leave trailing padding NULs inside
    // the string; drop them so pak/manifest names are clean C strings.
    while (!base.empty() && base.back() == '\0') base.pop_back();
    return base;
}

inline std::string jsonEscapeCopy(const std::string& s) {
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

inline std::string LowerTitle(const std::string& in) {
    std::string out = in;
    for (std::string::iterator c = out.begin(); c != out.end(); ++c) {
        if (*c >= 'A' && *c <= 'Z') *c = static_cast<char>(*c - 'A' + 'a');
    }
    return out;
}

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
    // v1 sound entry: eight u32 (nameOff, dataOff, dataSize, fmt, sr, ch,
    // loopStart, loopEnd) — the bit depth travels in the blob's RIFF header.
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
    // Sound entries (nameOff, dataOff, dataSize, fmt, sampleRate, channels, bits, loopStart, loopEnd).
    // v2: added bits field (bit depth, e.g., 8, 16) after channels.
    for (size_t i = 0; i < sounds.size(); ++i) {
        const auto& s = sounds[i];
        putU32be(out, nameOffsets[numTex + i]);
        putU32be(out, dataOffsets[numTex + i]);
        putU32be(out, static_cast<uint32_t>(s.data.size()));
        putU32be(out, s.fmt);
        putU32be(out, s.sampleRate);
        putU32be(out, s.channels);
        // v1: no bit-depth word — it is in the blob's RIFF header.
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
void ctfak::RecompPack::execute(GameData& game) {
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

    // ---------------- room-sorted blob order (streaming paks)
    // SISTER LOCATION ONLY (the user's call): the one game whose pak is far
    // too big for memory and streams room-to-room, so its blobs are
    // regrouped — COMMON images (used by >= 2 frames) first, then each
    // frame's exclusive images as one contiguous run in frame order, then
    // images referenced by no frame. A room preload then reads ~1 sequential
    // run instead of scattering across the whole multi-GB pak. Every other
    // game keeps the classic order (ascending image handle) byte-identical
    // to its previous pak (FNAF1's is verified against its reference).
    bool roomSort = false;
    {
        const std::string lowT = LowerTitle(game.name);
        if (lowT.find("sister location") != std::string::npos)
            roomSort = true;
    }
    std::vector<std::set<int>> frameImgs;
    std::set<int> commonImgs;
    // v2.81: the per-frame sound sets (ordinals into game.sounds.items order)
    std::vector<std::set<int>> frameSnds;
    std::set<int> commonSnds;
    if (roomSort) {
        // Per-frame image-handle sets from the parsed game data: every
        // instance's object -> animation cells / backdrop / counter frames.
        frameImgs.assign(game.frames.size(), std::set<int>());
        for (size_t fi = 0; fi < game.frames.size(); ++fi) {
            std::set<int> objs;
            for (const auto& inst : game.frames[fi]->objects)
                objs.insert((int)inst.objectInfo);
            std::set<int>& imgs = frameImgs[fi];
            for (int oi : objs) {
                auto it = game.frameItems.find(oi);
                if (it == game.frameItems.end() || !it->second) continue;
                const ObjectInfo& info = *it->second;
                if (info.common) {
                    for (const auto& anim : info.common->animations.items)
                        for (const auto& dir : anim.second)
                            for (uint16_t h : dir.second.frames)
                                imgs.insert((int)h);
                    if (info.common->counter)
                        for (uint16_t h : info.common->counter->frames)
                            imgs.insert((int)h);
                }
                if (info.backdrop) imgs.insert((int)info.backdrop->image);
                if (info.quickBackdrop && info.quickBackdrop->shape.image >= 0)
                    imgs.insert((int)info.quickBackdrop->shape.image);
            }
        }
        std::map<int, int> useCount;
        for (const auto& imgs : frameImgs)
            for (int h : imgs) ++useCount[h];
        for (const auto& u : useCount)
            if (u.second >= 2) commonImgs.insert(u.first);

        // The reorder itself. A texture missing from the pak (compression
        // failure) is skipped by the posOf lookup.
        std::map<std::string, size_t> posOf;   // pak name -> old position
        for (size_t i = 0; i < textures.size(); ++i) posOf[textures[i].name] = i;
        std::vector<bool> placed(textures.size(), false);
        auto placeHandle = [&](int h, std::vector<PakTextureSpec>& dst) {
            char nm[32];
            std::snprintf(nm, sizeof(nm), "img_%d", h);
            auto p = posOf.find(nm);
            if (p != posOf.end() && !placed[p->second]) {
                dst.push_back(textures[p->second]);
                placed[p->second] = true;
            }
        };
        std::vector<PakTextureSpec> ordered;
        ordered.reserve(textures.size());
        for (int h : commonImgs) placeHandle(h, ordered);
        for (const auto& imgs : frameImgs)
            for (int h : imgs)
                if (!commonImgs.count(h)) placeHandle(h, ordered);
        for (size_t i = 0; i < textures.size(); ++i)
            if (!placed[i]) ordered.push_back(textures[i]);
        textures.swap(ordered);
        Logger::log("Recomp Pack: room-sorted blob order: " +
                    std::to_string(commonImgs.size()) + " common images, " +
                    std::to_string(frameImgs.size()) + " frame runs");

        // ---- v2.81: per-frame SOUND sets from the parsed event program ----
        // Every Kind::Sample parameter (codes 6/7/35/36 — conditions and
        // actions alike) carries a sound bank handle; the per-frame sets feed
        // the port's async preload the same way frameImages does.
        frameSnds.assign(game.frames.size(), std::set<int>());
        std::map<int, int> sndHandleToOrdinal;
        {
            int ordinal = 0;
            for (const auto& item : game.sounds.items) {
                if (!item) continue;
                sndHandleToOrdinal[(int)item->handle] = ordinal++;
            }
        }
        for (size_t fi = 0; fi < game.frames.size(); ++fi) {
            if (!game.frames[fi]->events) continue;
            std::set<int>& snds = frameSnds[fi];
            for (const auto& g : game.frames[fi]->events->groups) {
                for (const auto& c : g.conditions)
                    for (const auto& p : c.parameters)
                        if (p.kind == Parameter::Kind::Sample) {
                            auto it = sndHandleToOrdinal.find((int)p.sampleHandle);
                            if (it != sndHandleToOrdinal.end()) snds.insert(it->second);
                        }
                for (const auto& a : g.actions)
                    for (const auto& p : a.parameters)
                        if (p.kind == Parameter::Kind::Sample) {
                            auto it = sndHandleToOrdinal.find((int)p.sampleHandle);
                            if (it != sndHandleToOrdinal.end()) snds.insert(it->second);
                        }
            }
        }
        std::map<int, int> sndUseCount;
        for (const auto& snds : frameSnds)
            for (int s : snds) ++sndUseCount[s];
        for (const auto& u : sndUseCount)
            if (u.second >= 2) commonSnds.insert(u.first);
        size_t sndFrames = 0;
        for (const auto& snds : frameSnds) if (!snds.empty()) ++sndFrames;
        Logger::log("Recomp Pack: per-frame sounds: " +
                    std::to_string(commonSnds.size()) + " common sounds, " +
                    std::to_string(sndFrames) + " frames with sounds");
    }

    // ---------------- sounds
    // Ordinary WAV sources keep their complete RIFF/WAVE blob. MP3 sources
    // stay compressed: the game's dr_mp3 adapter decodes them on playback.
    // Ogg still becomes a canonical PCM16 WAV here because the runtime has
    // no Vorbis decoder. Sample rates are never changed by the packer.
    std::vector<PakSoundSpec> sounds;
    sounds.reserve(game.sounds.items.size());
    uint64_t sndBytes = 0;
    size_t pcmCount = 0, passthroughCount = 0, convertedCount = 0;
    size_t runtimeMp3Count = 0;
    for (const auto& item : game.sounds.items) {
        if (!item) continue;
        PakSoundSpec s;
        s.name = "snd_" + baseNameNoExt(item->name);
        const AudioKind kind = sniffAudioKind(item->data);
        s.sourceFormat = audioKindName(kind);
        uint32_t rate = 0, channels = 0, bits = 16;
        size_t off = 0, size = 0;
        std::string werr;
        if (kind == AudioKind::WavPcm && parseWav(item->data, rate, channels, bits, off, size, werr)) {
            // v2.92: keep the WHOLE RIFF/WAVE file in the pak. The header
            // carries the ORIGINAL rate/channels/bit depth the Clickteam
            // app used, so the runtime reads them from there — the same
            // path FNAF1 always took (no extra field in the pak table,
            // no guessing). An 8-bit source stays 8-bit in the pak and is
            // expanded to 16-bit by the runtime at load time.
            s.fmt = 0;
            s.sampleRate = rate;
            s.channels = channels;
            s.bits = bits;
            s.data = item->data;
            ++pcmCount;
        } else if (kind == AudioKind::Mp3) {
            // Keep the exact compressed bytes. fmt=2 tells the game to use
            // dr_mp3; rate/channels are decoder output, not table metadata.
            s.fmt = 2;
            s.sampleRate = 0;
            s.channels = 0;
            s.bits = 0;
            s.data = item->data;
            ++runtimeMp3Count;
        } else {
            // Ogg and any other decodable source: make a canonical PCM16 WAV.
            std::vector<uint8_t> pcm;
            std::string tool, derr;
            if (decodeToPcm16(item->data, kind, rate, channels, pcm, tool, derr)) {
                s.fmt = 0;
                s.sampleRate = rate;
                s.channels = channels;
                s.bits = 16;
                s.data = WavWriter::buildWav(1, (uint16_t)channels, rate,
                                             rate * channels * 2,
                                             (uint16_t)(channels * 2), 16, pcm);
                ++pcmCount;
                if (!tool.empty()) ++convertedCount;
            } else {
                // Undecodable source: keep the bytes as they are. The runtime
                // marks such an entry unsupported and skips it in silence —
                // much better than handing compressed bytes to XAudio2 as PCM.
                s.fmt = 2;            // passthrough
                s.bits = 0;           // unknown bit depth
                s.sampleRate = 44100;  // metadata only; the sound stays silent
                s.channels = 2;
                s.data = item->data;
                ++passthroughCount;
                Logger::log("Recomp Pack: cannot decode '" + s.name + "' (" +
                            s.sourceFormat + "): " + derr + " — kept as passthrough");
            }
        }
        sndBytes += s.data.size();
        sounds.push_back(std::move(s));
    }

    // v2.81: room-sort the SOUND blobs with the same three-phase scheme
    // (COMMON used by >= 2 frames, then each frame's run in frame order).
    // The port reads sounds by name, so the reorder is invisible to it; a
    // room's sounds then read sequentially behind the same transitions.
    std::vector<int> sndOrdinalToNew;   // old ordinal -> new pak position
    if (roomSort && !frameSnds.empty() && !sounds.empty()) {
        std::vector<bool> placed(sounds.size(), false);
        std::vector<int> oldOrdinalOfNew;   // new position -> old ordinal
        oldOrdinalOfNew.reserve(sounds.size());
        auto placeSnd = [&](int ordinal) {
            if (ordinal < 0 || ordinal >= (int)sounds.size()) return;
            if (placed[ordinal]) return;
            placed[ordinal] = true;
            oldOrdinalOfNew.push_back(ordinal);
        };
        for (int s : commonSnds) placeSnd(s);
        for (const auto& snds : frameSnds)
            for (int s : snds) placeSnd(s);
        for (size_t i = 0; i < sounds.size(); ++i)
            if (!placed[i]) { placed[i] = true; oldOrdinalOfNew.push_back((int)i); }

        sndOrdinalToNew.assign(sounds.size(), -1);
        for (size_t n = 0; n < oldOrdinalOfNew.size(); ++n)
            sndOrdinalToNew[oldOrdinalOfNew[n]] = (int)n;

        std::vector<PakSoundSpec> ordered;
        ordered.reserve(sounds.size());
        for (int old : oldOrdinalOfNew) ordered.push_back(sounds[old]);
        sounds.swap(ordered);
        Logger::log("Recomp Pack: room-sorted sound order: " +
                    std::to_string(commonSnds.size()) + " common sounds, " +
                    std::to_string(sounds.size()) + " total");
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
    else if (low.find("freddy in space") != std::string::npos)
        pakName = "fis2.pak";                       // Freddy in Space 2
    else if (low.find("fury") != std::string::npos)
        pakName = "furysrage.pak";                  // Fury's Rage
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
    mapping += "};\n";

    // v2.29: streaming preload table — emitted for room-sorted paks only.
    // Per frame, the PAK INDICES of that frame's images, ascending by pak
    // position (each frame = one contiguous run). The game module feeds it
    // to PakLoader::PreloadIndices() while a dump-faithful transition
    // (the elevator ride, the load router) covers the read.
    if (roomSort) {
        std::map<int, int> pakPos;   // image handle -> pak position
        for (size_t i = 0; i < textures.size(); ++i)
            pakPos[std::atoi(textures[i].name.c_str() + 4)] = (int)i;
        mapping += "\n// Streaming preload table (room-sorted pak): per frame, the pak\n"
                   "// indices of the frame's images, ascending by pak position, so a\n"
                   "// room preload is ~1 sequential read. See PakLoader::PreloadIndices.\n"
                   "struct FrameImages { int frame; const char* name; int count; const int* images; };\n";
        std::string rows;
        for (size_t fi = 0; fi < frameImgs.size(); ++fi) {
            std::vector<std::pair<int, int>> idxs;   // (pak pos, handle)
            for (int h : frameImgs[fi]) {
                auto p = pakPos.find(h);
                if (p != pakPos.end()) idxs.push_back(std::make_pair(p->second, h));
            }
            if (idxs.empty()) continue;
            std::sort(idxs.begin(), idxs.end());
            std::string arrName = "f" + std::to_string(fi) + "_imgs";
            mapping += "static const int " + arrName + "[] = {";
            for (size_t k = 0; k < idxs.size(); ++k) {
                mapping += (k % 16 == 0 ? "\n    " : " ");
                mapping += std::to_string(idxs[k].first) + ",";
            }
            mapping += "\n};\n";
            rows += "    { " + std::to_string(fi) + ", \"" +
                    jsonEscapeCopy(game.frames[fi]->name) + "\", " +
                    std::to_string(idxs.size()) + ", " + arrName + " },\n";
        }
        mapping += "\nstatic const FrameImages frameImages[] = {\n" + rows + "};\n";
        mapping += "const int FRAME_COUNT = (int)(sizeof(frameImages) / sizeof(frameImages[0]));\n";

        // v2.81: the per-frame SOUND preload table (pak sound-table indices,
        // ascending by pak position). Consumed by PakLoader::PreloadSoundIndices.
        mapping += "\n// Streaming sound-preload table (room-sorted pak): per frame, the pak\n"
                   "// sound indices of the frame's event-referenced sounds. See\n"
                   "// PakLoader::PreloadSoundIndices.\n"
                   "struct FrameSounds { int frame; int count; const int* sounds; };\n";
        std::string sndRows;
        for (size_t fi = 0; fi < frameSnds.size(); ++fi) {
            std::vector<int> idxs;
            for (int s : frameSnds[fi])
                if (s >= 0 && s < (int)sndOrdinalToNew.size() && sndOrdinalToNew[s] >= 0)
                    idxs.push_back(sndOrdinalToNew[s]);
            if (idxs.empty()) continue;
            std::sort(idxs.begin(), idxs.end());
            std::string arrName = "f" + std::to_string(fi) + "_snds";
            mapping += "static const int " + arrName + "[] = {";
            for (size_t k = 0; k < idxs.size(); ++k) {
                mapping += (k % 16 == 0 ? "\n    " : " ");
                mapping += std::to_string(idxs[k]) + ",";
            }
            mapping += "\n};\n";
            sndRows += "    { " + std::to_string(fi) + ", " +
                       std::to_string(idxs.size()) + ", " + arrName + " },\n";
        }
        mapping += "\nstatic const FrameSounds frameSounds[] = {\n" + sndRows + "};\n";
        mapping += "const int FRAME_SOUND_COUNT = (int)(sizeof(frameSounds) / sizeof(frameSounds[0]));\n";
    }
    mapping += "} // namespace " + gameId + "\n";
    std::FILE* mf = std::fopen(pathJoin(dir, "asset_mapping_" + gameId + ".hpp").c_str(), "wb");
    if (mf) { std::fwrite(mapping.data(), 1, mapping.size(), mf); std::fclose(mf); }

    // ---------------- manifest
    std::string json;
    json += "{\n  \"magic\": \"FNAF\",\n  \"version\": " + std::to_string(kPakVersion) + ",\n";
    json += "  \"textureCount\": " + std::to_string(textures.size()) + ",\n";
    json += "  \"soundCount\": " + std::to_string(sounds.size()) + ",\n";
    json += "  \"roomSort\": " + std::string(roomSort ? "true" : "false") + ",\n";
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
                      "    {\"name\": \"%s\", \"format\": \"%s\", \"sourceFormat\": \"%s\", "
                      "\"sampleRate\": %u, \"channels\": %u, \"bits\": %u, \"bytes\": %u}%s\n",
                      jsonEscapeCopy(s.name).c_str(),
                      s.fmt == 0 ? "PCM" : (s.sourceFormat == "mp3" ? "MP3 (runtime)" : "passthrough"),
                      jsonEscapeCopy(s.sourceFormat).c_str(),
                      s.sampleRate, s.channels, s.bits,
                      static_cast<unsigned>(s.data.size()),
                      i + 1 < sounds.size() ? "," : "");
        json += entry;
    }
    json += "  ]";
    if (roomSort) {
        json += ",\n  \"frames\": [\n";
        bool firstFrame = true;
        for (size_t fi = 0; fi < frameImgs.size(); ++fi) {
            if (frameImgs[fi].empty()) continue;
            json += firstFrame ? "" : ",\n";
            firstFrame = false;
            json += "    {\"frame\": " + std::to_string(fi) + ", \"name\": \"" +
                    jsonEscapeCopy(game.frames[fi]->name) + "\", \"images\": [";
            bool firstImg = true;
            for (int h : frameImgs[fi]) {
                json += (firstImg ? "" : ", ") + std::to_string(h);
                firstImg = false;
            }
            json += "]}";
        }
        json += "\n  ]";
    }
    if (roomSort) {
        // v2.81: the per-frame sound lists (sound bank handles, for analysis
        // parity with the "frames" image lists; the header carries the pak
        // indices). frameSnds holds OLD ordinals — map them back to handles.
        json += ",\n  \"frameSounds\": [\n";
        std::vector<int> ordinalToHandle;
        ordinalToHandle.reserve(game.sounds.items.size());
        for (const auto& item : game.sounds.items) {
            if (!item) continue;
            ordinalToHandle.push_back((int)item->handle);
        }
        bool firstSndFrame = true;
        for (size_t fi = 0; fi < frameSnds.size(); ++fi) {
            if (frameSnds[fi].empty()) continue;
            json += firstSndFrame ? "" : ",\n";
            firstSndFrame = false;
            json += "    {\"frame\": " + std::to_string(fi) + ", \"sounds\": [";
            bool firstS = true;
            for (int s : frameSnds[fi]) {
                int h = (s >= 0 && s < (int)ordinalToHandle.size()) ? ordinalToHandle[s] : -1;
                json += (firstS ? "" : ", ") + std::to_string(h);
                firstS = false;
            }
            json += "]}";
        }
        json += "\n  ]";
    }
    json += "\n}\n";
    std::FILE* jf = std::fopen(pathJoin(dir, "pak_manifest.json").c_str(), "wb");
    if (jf) { std::fwrite(json.data(), 1, json.size(), jf); std::fclose(jf); }

    char summary[256];
    std::snprintf(summary, sizeof(summary),
                  "Recomp Pack done: %zu textures (%zu DXT1, %zu DXT5, %.1f MB), "
                  "%zu sounds (%zu PCM, %zu converted, %zu MP3-runtime, %zu passthrough, %.1f MB), "
                  "pak %.1f MB -> %s",
                  textures.size(), dxt1Count, dxt5Count, texBytes / 1048576.0,
                  sounds.size(), pcmCount, convertedCount, runtimeMp3Count, passthroughCount,
                  sndBytes / 1048576.0,
                  pak.size() / 1048576.0, pakPath.c_str());
    Logger::log(summary);
}

} // namespace ctfak
