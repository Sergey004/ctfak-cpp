#include "Frame.h"
#include "Chunk.h"
#include "ChunkNames.h"
#include "Events.h"
#include "Shaders.h"
#include "../core/Logger.h"
#include "../core/Settings.h"

namespace ctfak {

void ObjectInstance::read(BinaryReader& reader) {
    handle = reader.readU16();
    objectInfo = reader.readU16();
    if (Settings::old()) {
        y = reader.readI16();
        x = reader.readI16();
    } else {
        x = reader.readI32();
        y = reader.readI32();
    }
    parentType = reader.readI16();
    parentHandle = reader.readI16();
    if (Settings::old() || Settings::f3()) return;
    layer = reader.readI16();
    instance = reader.readI16();
}

void Layer::read(BinaryReader& reader) {
    flags.set(reader.readU32());
    xCoeff = reader.readF32();
    yCoeff = reader.readF32();
    numberOfBackgrounds = reader.readI32();
    backgroundIndex = reader.readI32();
    if (Settings::fusion3Seed) reader.skip(6);
    name = reader.readYuniversal();
    if (Settings::android()) {
        xCoeff = 1;
        yCoeff = 1;
    }
}

void Transition::read(BinaryReader& reader) {
    int64_t base = reader.tell();
    module = reader.readAscii(4);
    name = reader.readAscii(4);
    duration = reader.readI32();
    flags = reader.readI32();
    color = reader.readColor();
    int32_t nameOffset = reader.readI32();
    int32_t parameterOffset = reader.readI32();
    int32_t parameterSize = reader.readI32();
    if (nameOffset != 0) {
        reader.seek(base + nameOffset);
        moduleFile = reader.readYuniversal();
    }
    if (parameterOffset != 0 && parameterSize > 0) {
        reader.seek(base + parameterOffset);
        parameterData = reader.readBytes(parameterSize);
    }
}

void Frame::read(BinaryReader& reader, const Shaders& shaders) {
    while (true) {
        if (reader.eof()) break;
        Chunk nested;
        auto data = nested.read(reader);
        if (nested.id == 32639) break;

        if (Settings::hasParameter("onlyimages") && nested.id != 13109 && nested.id != 13112)
            continue;

        BinaryReader r(data.data(), data.size());
        switch (nested.id) {
            case 13108: // header
                if (Settings::old()) {
                    width = r.readI16();
                    height = r.readI16();
                    background = r.readColor();
                    flags.set(r.readU16());
                } else {
                    width = r.readI32();
                    height = r.readI32();
                    background = r.readColor();
                    flags.set(r.readU32());
                }
                break;
            case 13109: // name
                name = r.readYuniversal();
                break;
            case 13111: // palette
                palette.resize(257);
                for (auto& c : palette) c = r.readColor();
                break;
            case 13112: { // object instances
                int32_t count = r.readI32();
                objects.clear();
                for (int32_t i = 0; i < count; ++i) {
                    ObjectInstance inst;
                    inst.read(r);
                    objects.push_back(inst);
                }
                break;
            }
            case 13113:
            case 13115:
                fadeIn = std::make_unique<Transition>();
                fadeIn->read(r);
                break;
            case 13114:
            case 13116:
                fadeOut = std::make_unique<Transition>();
                fadeOut->read(r);
                break;
            case 13117: // events
                if (!Settings::hasParameter("noevnt")) events = readEventsChunk(r);
                break;
            case 13121: { // layers
                uint32_t count = r.readU32();
                layers.clear();
                for (uint32_t i = 0; i < count; ++i) {
                    Layer layer;
                    layer.read(r);
                    layers.push_back(std::move(layer));
                }
                break;
            }
            case 13122: // virtual rect
                virtualLeft = r.readI32();
                virtualTop = r.readI32();
                virtualRight = r.readI32();
                virtualBottom = r.readI32();
                hasVirtualRect = true;
                break;
            case 13124:
                randomSeed = r.readI16();
                break;
            case 13125: { // layer effects
                for (auto& layer : layers) {
                    if (!r.hasMemory(24)) break;
                    layer.effect = r.readI16();
                    layer.effectParam = r.readI16();
                    layer.rgbCoeff = r.readColor();
                    layer.inkEffect = r.readI32();
                    int32_t numberOfParams = r.readI32();
                    int32_t offset = r.readI32();
                    if (layer.inkEffect == -1 || layer.effect <= 0) continue;
                    const Shader* shader = shaders.find(layer.inkEffect);
                    if (!shader) continue;
                    layer.shaderData.hasShader = true;
                    layer.shaderData.name = shader->name;
                    layer.shaderData.handle = layer.inkEffect;
                    int64_t returnOffset = r.tell();
                    r.seek(offset);
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
                        layer.shaderData.parameters.push_back(std::move(v));
                    }
                    r.seek(returnOffset);
                }
                break;
            }
            case 13126:
                r.readI32();
                r.readI32();
                break;
            case 13127:
                movementTimer = r.readI32();
                break;
            case 13129: { // frame effects
                effect = r.readI16();
                effectParam = r.readI16();
                rgbCoeff = r.readColor();
                inkEffect = r.readI32();
                int32_t numberOfParams = r.readI32();
                if (inkEffect == -1 || effect <= 0) break;
                const Shader* shader = shaders.find(inkEffect);
                if (!shader) break;
                shaderData.hasShader = true;
                shaderData.name = shader->name;
                shaderData.handle = inkEffect;
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
                    shaderData.parameters.push_back(std::move(v));
                }
                break;
            }
            case 13130:
                r.readI16();
                r.readI16();
                break;
            default:
                Logger::debug(std::string("No reader for frame chunk ") + chunkName(nested.id));
                break;
        }
    }
    Logger::log("Frame Found: " + name + ", " + std::to_string(width) + "x" + std::to_string(height) +
                ", " + std::to_string(objects.size()) + " objects.");
}

} // namespace ctfak
