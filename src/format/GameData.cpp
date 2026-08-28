#include "GameData.h"
#include "Chunk.h"
#include "ChunkNames.h"
#include "StringChunks.h"
#include "../core/Crypto.h"
#include "../core/Logger.h"
#include "../core/Settings.h"
#include "../core/Zlib.h"
#include <map>

namespace ctfak {

void GameData::read(BinaryReader& reader) {
    std::string magic = reader.readAscii(4);
    if (magic == "PAMU") {
        Settings::unicode = true;
    } else if (magic == "PAME") {
        Settings::unicode = false;
    } else if (magic == "CRUF") {
        Settings::setGameType(GameType::F3);
    } else {
        Logger::error("Couldn't find any known headers: " + magic);
    }
    if (Settings::hasParameter("f1.5")) Settings::setGameType(GameType::Mmf15);
    if (Settings::hasParameter("android")) Settings::setGameType(GameType::Android);
    if (Settings::hasParameter("f3")) Settings::setGameType(GameType::F3);
    Logger::log("Game Header: " + magic);

    runtimeVersion = reader.readI16();
    runtimeSubversion = reader.readI16();
    productVersion = reader.readI32();
    productBuild = reader.readI32();
    Settings::build = productBuild;
    Logger::log("Fusion Build: " + std::to_string(productBuild));

    int chunkIndex = 0;
    int frameIndex = 0;
    while (true) {
        if (reader.eof()) break;
        Chunk chunk;
        std::vector<uint8_t> data;
        try {
            data = chunk.read(reader);
        } catch (const ReadError& e) {
            Logger::error(std::string("Chunk read failed: ") + e.what());
            break;
        }
        if (chunk.id == 32494 && Settings::f3()) Settings::fusion3Seed = true;
        if (chunk.id == 8787 && !Settings::f3()) Settings::setGameType(GameType::TwoFivePlus);

        BinaryReader r(data.data(), data.size());
        ++chunkIndex;
        const char* cname = chunkName(chunk.id);
        if (Logger::verbose) {
            std::string line = std::string("Reading chunk ") + (cname ? cname : "UNKNOWN") + " (" +
                               std::to_string(chunk.id) + "), decompressed " +
                               std::to_string(data.size()) + " bytes";
            Logger::log(line);
        }

        try {
        switch (chunk.id) {
            case 8739:
                header = std::make_unique<AppHeader>();
                header->read(r);
                break;
            case 8740:
                name = readAppNameChunk(r, chunk.size);
                break;
            case 8741:
                author = readStringChunk(r);
                break;
            case 8743: // extension path (unused)
                break;
            case 8745:
            case 8767: {
                int32_t count = r.readI32();
                for (int32_t i = 0; i < count; ++i) {
                    auto info = std::make_shared<ObjectInfo>();
                    info->read(r, shaders);
                    frameItems[info->handle] = info;
                }
                break;
            }
            case 8747: {
                frameHandles.clear();
                while (r.hasMemory(2)) frameHandles.push_back(r.readI16());
                break;
            }
            case 8748: // extension data: filename + blob
                break;
            case 8750:
                editorFilename = readStringChunk(r);
                if (Settings::build > 284)
                    Crypto::makeKey(name, copyright, editorFilename);
                else
                    Crypto::makeKey(editorFilename, name, copyright);
                break;
            case 8751:
                targetFilename = readStringChunk(r);
                break;
            case 8754:
                globalValues.read(r);
                break;
            case 8755:
                globalStrings.read(r);
                break;
            case 8756:
                extensions.read(r);
                break;
            case 8760:
                binaryFiles.read(r);
                break;
            case 8763:
                copyright = readStringChunk(r);
                break;
            case 8768:
                exeOnly = r.readU8() != 0;
                break;
            case 8771:
                shaders.read(r);
                break;
            case 8773:
                extHeader = std::make_unique<ExtendedHeader>();
                extHeader->read(r);
                break;
            case 8787: { // 2.5+ object headers
                while (!r.eof()) {
                    auto info = std::make_shared<ObjectInfo>();
                    info->readHeader(r);
                    frameItems[info->handle] = info;
                }
                break;
            }
            case 8788: { // 2.5+ object names
                int index = 0;
                while (!r.eof()) {
                    if (frameItems.count(index))
                        frameItems[index]->name = r.readYuniversal();
                    else
                        r.readYuniversal();
                    ++index;
                }
                break;
            }
            case 8789: { // 2.5+ object shaders
                int64_t end = r.size();
                int current = 0;
                while (r.tell() < end) {
                    int64_t paramStart = r.tell() + 4;
                    int32_t size = r.readI32();
                    if (size == 0) {
                        ++current;
                        continue;
                    }
                    auto it = frameItems.find(current);
                    if (it != frameItems.end()) {
                        int32_t shaderHandle = r.readI32();
                        int32_t numberOfParams = r.readI32();
                        const Shader* shader = shaders.find(shaderHandle);
                        if (shader) {
                            it->second->shaderData.hasShader = true;
                            it->second->shaderData.name = shader->name;
                            it->second->shaderData.handle = shaderHandle;
                            for (int32_t k = 0; k < numberOfParams; ++k) {
                                if (static_cast<size_t>(k) >= shader->parameters.size()) break;
                                const auto& p = shader->parameters[k];
                                ShaderParamValue v;
                                v.name = p.name;
                                v.type = p.type;
                                switch (p.type) {
                                    case 1: v.value = r.readF32(); break;
                                    case 0:
                                    case 2:
                                    case 3: v.value = r.readI32(); break;
                                    default: break;
                                }
                                it->second->shaderData.parameters.push_back(std::move(v));
                            }
                        }
                    }
                    r.seek(paramStart + size);
                    ++current;
                }
                break;
            }
            case 8790: { // 2.5+ object properties
                int64_t end = r.size();
                r.readI32();
                int current = 0;
                while (r.tell() < end) {
                    int64_t entryPos = r.tell();
                    int32_t entrySize = r.readI32();
                    try {
                        auto decompressed = zlibInflate(r.readBytes(entrySize));
                        BinaryReader dr(decompressed.data(), decompressed.size());
                        auto it = frameItems.find(current);
                        if (it != frameItems.end()) {
                            auto info = it->second;
                            if (info->objectType == 0) {
                                info->quickBackdrop = std::make_unique<QuickBackdrop>();
                                info->quickBackdrop->read(dr);
                            } else if (info->objectType == 1) {
                                info->backdrop = std::make_unique<Backdrop>();
                                info->backdrop->read(dr);
                            } else {
                                info->common = std::make_shared<ObjectCommon>();
                                info->common->read(dr, info->objectType);
                            }
                        }
                    } catch (const std::runtime_error&) {
                        // corrupt entry: skip
                    }
                    r.seek(entryPos + entrySize + 8);
                    ++current;
                }
                break;
            }
            case 8793:
                trueTypeFonts.read(r);
                break;
            case 13107: {
                std::string excludeParam = "excludeframe(" + std::to_string(frameIndex) + ")";
                ++frameIndex;
                if (Settings::hasParameter(excludeParam)) break;
                auto frame = std::make_shared<Frame>();
                frame->read(r, shaders);
                frames.push_back(std::move(frame));
                break;
            }
            case 17664: // image shapes
                break;
            case 26213: // bank offsets
                break;
            case 26214:
                images.read(r);
                break;
            case 26215:
                fonts.read(r);
                break;
            case 26216:
                if (Settings::hasParameter("nosounds")) break;
                sounds.read(r);
                break;
            case 26217:
                music.read(r);
                break;
            case 32639:
                // terminator handled by outer loop condition on EOF
                break;
            default:
                Logger::debug(std::string("No reader for chunk ") + (cname ? cname : "?") + " (" +
                              std::to_string(chunk.id) + ")");
                break;
        }
        } catch (const ReadError& e) {
            Logger::error("Chunk " + std::to_string(chunk.id) + " read failed: " + e.what());
        } catch (const std::exception& e) {
            Logger::error(std::string("Chunk processing error: ") + e.what());
        }
        // The 0x7F7F terminator signals the logical end of the game data.
        if (chunk.id == 32639) break;
    }

    Logger::log("Reading finished. Frames: " + std::to_string(frames.size()) + ", images: " +
                std::to_string(images.items.size()) + ", sounds: " + std::to_string(sounds.items.size()));
}

} // namespace ctfak
