#include "ObjectInfo.h"
#include "Chunk.h"
#include "ChunkNames.h"
#include "Shaders.h"
#include "../core/Logger.h"
#include "../core/Settings.h"
#include <cstring>

namespace ctfak {

const char* objectTypeName(int16_t type) {
    switch (type) {
        case -7: return "Player";
        case -6: return "Keyboard";
        case -5: return "Create";
        case -4: return "Timer";
        case -3: return "Game";
        case -2: return "Speaker";
        case -1: return "System";
        case 0: return "QuickBackdrop";
        case 1: return "Backdrop";
        case 2: return "Active";
        case 3: return "Text";
        case 4: return "Question";
        case 5: return "Score";
        case 6: return "Lives";
        case 7: return "Counter";
        case 8: return "Rtf";
        case 9: return "SubApplication";
        default: return "Extension";
    }
}

// --------------------------------------------------------------------------- Shape
void Shape::read(BinaryReader& reader) {
    borderSize = reader.readI16();
    borderColor = reader.readColor();
    shapeType = reader.readI16();
    fillType = reader.readI16();
    if (shapeType == 1) {
        lineFlags = reader.readI16();
    } else if (fillType == 1) {
        color1 = reader.readColor();
    } else if (fillType == 2) {
        color1 = reader.readColor();
        color2 = reader.readColor();
        gradFlags = reader.readI16();
    }
    image = reader.readI16();
}

void QuickBackdrop::read(BinaryReader& reader) {
    size = reader.readI32();
    obstacleType = reader.readI16();
    collisionType = reader.readI16();
    width = reader.readI32();
    height = reader.readI32();
    shape.read(reader);
}

void Backdrop::read(BinaryReader& reader) {
    size = reader.readI32();
    obstacleType = reader.readI16();
    collisionType = reader.readI16();
    if (!Settings::old()) {
        width = reader.readI32();
        height = reader.readI32();
    }
    image = reader.readI16();
}

// --------------------------------------------------------------------------- Movement
void MovementStep::read(BinaryReader& reader) {
    flags = reader.readU8();
    size = reader.readU8();
    speed = reader.readU8();
    direction = reader.readU8();
    destX = reader.readI16();
    destY = reader.readI16();
    cosinus = reader.readI16();
    sinus = reader.readI16();
    length = reader.readI16();
    pause = reader.readI16();
    if (size >= 16) data = reader.readBytes(size - 16);
    else data.clear();
}

void Movement::read(BinaryReader& reader, int64_t rootPos, bool old) {
    if (old) {
        player = reader.readU16();
        type = reader.readU16();
        movingAtStart = reader.readU8();
        reader.skip(3);
        directionAtStart = reader.readI32();
        if (type == 5) return; // path movement unsupported in old builds
    } else {
        int32_t nameOffset = reader.readI32();
        (void)nameOffset;
        int32_t movementId = reader.readI32();
        (void)movementId;
        int32_t newOffset = reader.readI32();
        int32_t dataSize = reader.readI32();
        reader.seek(rootPos + newOffset);
        player = reader.readU16();
        type = reader.readU16();
        movingAtStart = reader.readU8();
        reader.skip(3);
        directionAtStart = reader.readI32();
        if (type == 14) extensionData = reader.readBytes(dataSize);
    }

    switch (type) {
        case 1:
            typeName = "Mouse";
            fields = {{"x1", reader.readI16()}, {"x2", reader.readI16()},
                      {"y1", reader.readI16()}, {"y2", reader.readI16()},
                      {"flags", reader.readI16()}};
            break;
        case 2:
            typeName = "Race";
            fields = {{"speed", reader.readI16()}, {"acceleration", reader.readI16()},
                      {"deceleration", reader.readI16()}, {"rotationSpeed", reader.readI16()},
                      {"bounceFactor", reader.readI16()}, {"angles", reader.readI16()},
                      {"reverseEnabled", reader.readI16()}};
            break;
        case 3:
            typeName = "EightDirections";
            fields = {{"speed", reader.readI16()}, {"acceleration", reader.readI16()},
                      {"deceleration", reader.readI16()}, {"bounceFactor", reader.readI16()},
                      {"directions", reader.readI32()}};
            break;
        case 4:
            typeName = "Ball";
            fields = {{"speed", reader.readI16()}, {"randomizer", reader.readI16()},
                      {"angles", reader.readI16()}, {"security", reader.readI16()},
                      {"deceleration", reader.readI16()}};
            break;
        case 5: {
            typeName = "Path";
            pathCount = reader.readI16();
            pathMinSpeed = reader.readI16();
            pathMaxSpeed = reader.readI16();
            pathLoop = reader.readU8();
            pathRepositionAtEnd = reader.readU8();
            pathReverseAtEnd = reader.readU8();
            reader.skip(1);
            for (int16_t i = 0; i < pathCount; ++i) {
                MovementStep step;
                step.read(reader);
                pathSteps.push_back(std::move(step));
            }
            break;
        }
        case 9:
            typeName = "Platform";
            fields = {{"speed", reader.readI16()}, {"acceleration", reader.readI16()},
                      {"deceleration", reader.readI16()}, {"control", reader.readI16()},
                      {"gravity", reader.readI16()}, {"jumpStrength", reader.readI16()}};
            break;
        case 14:
            typeName = "Extension";
            // dataSize from the movement table header is tracked by the caller
            // through the last table entry; we read what the loader would: it is
            // stored inline after the header. Reference reads Movement.DataSize bytes.
            // Here we conservatively read nothing; the enclosing table walk consumes
            // exactly 16-byte strides, so extension payloads live at newOffset too.
            break;
        default:
            if (type != 0) {
                typeName = "Unknown";
                Logger::debug("Unsupported movement type: " + std::to_string(type));
                return;
            }
            typeName = "None";
            break;
    }
}

// --------------------------------------------------------------------------- Animations
void AnimationDirection::read(BinaryReader& reader) {
    minSpeed = reader.readI8();
    maxSpeed = reader.readI8();
    repeat = reader.readI16();
    backTo = reader.readI16();
    uint16_t frameCount = reader.readU16();
    frames.resize(frameCount);
    for (uint16_t i = 0; i < frameCount; ++i) frames[i] = reader.readU16();
}

void Animations::read(BinaryReader& reader) {
    int64_t base = reader.tell();
    reader.readI16(); // size (unused)
    int16_t count = reader.readI16();
    std::vector<int16_t> offsets(static_cast<size_t>(count), 0);
    for (int16_t i = 0; i < count; ++i) offsets[i] = reader.readI16();

    items.clear();
    for (int16_t i = 0; i < count; ++i) {
        std::map<int, AnimationDirection> dirs;
        if (offsets[i] != 0) {
            reader.seek(base + offsets[i]);
            std::vector<int16_t> dirOffsets(32, 0);
            for (int d = 0; d < 32; ++d) dirOffsets[d] = reader.readI16();
            int64_t animBase = base + offsets[i];
            for (int d = 0; d < 32; ++d) {
                if (dirOffsets[d] != 0) {
                    reader.seek(animBase + dirOffsets[d]);
                    AnimationDirection dir;
                    dir.read(reader);
                    dirs[d] = std::move(dir);
                }
            }
        }
        items[i] = std::move(dirs);
    }
}

// --------------------------------------------------------------------------- Text / Paragraph
void Paragraph::read(BinaryReader& reader, bool old) {
    if (old) {
        reader.readU16(); // size prefix
        font = reader.readU16();
        color = reader.readColor();
        flags.set(reader.readU16());
    } else {
        font = reader.readU16();
        flags.set(reader.readU16());
        color = reader.readColor();
    }
    value = reader.readYuniversal();
}

void Text::read(BinaryReader& reader, bool old) {
    int64_t base = reader.tell();
    if (old) {
        reader.readI32(); // size
        width = reader.readI16();
        height = reader.readI16();
        int16_t count = reader.readI16();
        std::vector<int16_t> offsets(count);
        for (int16_t i = 0; i < count; ++i) offsets[i] = reader.readI16();
        for (int16_t i = 0; i < count; ++i) {
            reader.seek(base + offsets[i]);
            Paragraph p;
            p.read(reader, true);
            paragraphs.push_back(std::move(p));
        }
    } else {
        reader.readI32(); // size
        width = reader.readI32();
        height = reader.readI32();
        int32_t count = reader.readI32();
        std::vector<int32_t> offsets(count);
        for (int32_t i = 0; i < count; ++i) offsets[i] = reader.readI32();
        for (int32_t i = 0; i < count; ++i) {
            reader.seek(base + offsets[i]);
            Paragraph p;
            p.read(reader, false);
            paragraphs.push_back(std::move(p));
        }
    }
}

// --------------------------------------------------------------------------- SystemCounter
void SystemCounter::read(BinaryReader& reader) {
    width = reader.readU32();
    height = reader.readU32();
    player = reader.readU16();
    displayType = reader.readU16();
    uint16_t flagValue = reader.readU16();
    flags = BitDict({}, flagValue);
    integerDigits = flagValue & 0xF;
    formatFloat = (flagValue & 0x0200) != 0;
    floatDigits = (flagValue & 0xF0) >> 5; // replicates the reference precedence quirk
    useDecimals = (flagValue & 0x0400) != 0;
    decimals = (flagValue & 0xF000) >> 12;
    addNulls = (flagValue & 0x0800) != 0;
    inverse = (flagValue & 0x0100) != 0;
    font = reader.readU16();

    if (displayType == 0) return;
    if (displayType == 1 || displayType == 4 || displayType == 50) {
        int16_t count = reader.readI16();
        frames.resize(count);
        for (int16_t i = 0; i < count; ++i) frames[i] = reader.readU16();
    } else if (displayType == 2 || displayType == 3 || displayType == 5) {
        shape.read(reader);
    }
}

void SubApplication::read(BinaryReader& reader) {
    cx = reader.readI32();
    cy = reader.readI32();
    version = reader.readI16();
    startFrame = reader.readI16();
    options = reader.readI32();
}

// --------------------------------------------------------------------------- ObjectCommon
void ObjectCommon::read(BinaryReader& reader, int16_t objectType) {
    int64_t s = reader.tell();

    if (Settings::build >= 284 && Settings::gameType == static_cast<uint32_t>(GameType::Normal)) {
        // Branch A with build-284 probe.
        int32_t sizeField = reader.readI32();
        (void)sizeField;
        reader.skip(2);
        int32_t check = reader.readI32();
        reader.seek(s + 4);
        if (Settings::build == 284 && check == 0) {
            counterOffset = reader.readI16();     // +4
            version = reader.readI32();           // +6 (same bytes as check)
            movementsOffset = reader.readI16();   // +10
            extensionOffset = reader.readI16();   // +12
            animationsOffset = reader.readI16();  // +14
        } else {
            animationsOffset = reader.readI16();  // +4
            movementsOffset = reader.readI16();   // +6
            version = reader.readI32();           // +8
            extensionOffset = reader.readI16();   // +12
            counterOffset = reader.readI16();     // +14
        }
    } else if (Settings::gameType == static_cast<uint32_t>(GameType::Normal) ||
               Settings::gameType == static_cast<uint32_t>(GameType::Mmf2)) {
        // Branch B: build < 284 normal or MMF2 (own tail layout).
        reader.readI32(); // size
        movementsOffset = reader.readI16();
        animationsOffset = reader.readI16();
        version = reader.readI16();
        counterOffset = reader.readI16();
        systemObjectOffset = reader.readI16();
        reader.skip(2);
        flags.set(reader.readU16());
        int16_t extraFlagsB = reader.readI16();
        if (extraFlagsB == 6) flags.setBit("DoNotCreateAtStart", true);
        for (auto& q : qualifiers) q = reader.readI16();
        extensionOffset = reader.readI16();
        valuesOffset = reader.readI16();
        stringsOffset = reader.readI16();
        newFlags.set(reader.readU16());
        preferences.set(reader.readU16());
        identifier = reader.readAscii(2);
        backColor = reader.readColor();
        fadeIn = reader.readU32();
        fadeOut = reader.readU32();
        readSubLoaders(reader, s);
        return;
    } else if (Settings::twoFivePlus() || Settings::f3() || Settings::fusion3Seed) {
        // Branch C: 2.5+ / Fusion 3.
        reader.readI32(); // size
        animationsOffset = reader.readI16();
        movementsOffset = reader.readI16();
        version = reader.readU16();
        reader.skip(2);
        extensionOffset = reader.readI16();
        counterOffset = reader.readI16();
    } else if (Settings::android()) {
        if (Settings::build >= 290) {
            // Branch D1: absolute offsets quirk.
            int32_t size = reader.readI32();
            reader.skip(-4);
            reader.skip(size + 4);
            reader.skip(-size + 4);
            movementsOffset = reader.readI16(); // +8
            version = reader.readU16();         // +10
            extensionOffset = reader.readI16(); // +12
            counterOffset = reader.readI16();   // +14
            valuesOffset = reader.readI16();    // +16
            flags.set(reader.readU16());        // +18
            int16_t extraFlags = reader.readI16();
            if (extraFlags == 6) flags.setBit("DoNotCreateAtStart", true);
            for (auto& q : qualifiers) q = reader.readI16();
            systemObjectOffset = reader.readI16(); // +38
            animationsOffset = reader.readI16();   // +40
            reader.skip(2);                        // +42
            stringsOffset = reader.readI16();      // +44
            newFlags.set(reader.readU32());        // +46
            preferences.set(reader.readU16());     // +50
            identifier = reader.readAscii(4);      // +52
            backColor = reader.readColor();        // +56
            fadeIn = reader.readU32();             // +60
            fadeOut = reader.readU32();            // +64
            readSubLoaders(reader, 0);
            return;
        }
        if (Settings::old()) {
            // Branch D2.
            reader.readU16(); // size
            reader.readU16(); // checksum
            movementsOffset = reader.readI16();
            animationsOffset = reader.readI16();
            version = reader.readU16();
            counterOffset = reader.readI16();
            systemObjectOffset = reader.readI16();
            reader.readU32(); // ocVariable
        } else if (Settings::build == 288 || Settings::build == 284) {
            // Branch D3.
            reader.readI32(); // size
            movementsOffset = reader.readI16();
            version = reader.readI16();
            reader.readI16();
            counterOffset = reader.readI16();
            systemObjectOffset = reader.readI16();
            extensionOffset = reader.readI16();
        } else {
            // Branch D4.
            reader.readI32(); // size
            movementsOffset = reader.readI16();
            animationsOffset = reader.readI16();
            version = reader.readI16();
            counterOffset = reader.readI16();
            systemObjectOffset = reader.readI16();
            valuesOffset = reader.readI16();
        }
    } else {
        // No branch matched (e.g. iOS): reference reads nothing further.
        return;
    }

    // Common tail (not reached by android D1 which returned above).
    flags.set(reader.readU16());
    int16_t extraFlags = reader.readI16();
    if (extraFlags == 6) flags.setBit("DoNotCreateAtStart", true);
    for (auto& q : qualifiers) q = reader.readI16();

    if (Settings::android() && Settings::old()) {
        // D2 has no stringsOffset and reads extension/values/newFlags/preferences here.
        extensionOffset = reader.readI16();
        valuesOffset = reader.readI16();
        newFlags.set(reader.readU16());
        preferences.set(reader.readU16());
        identifier = reader.readAscii(4);
        backColor = reader.readColor();
        fadeIn = reader.readU32();
        fadeOut = reader.readU32();
        readSubLoaders(reader, s);
        return;
    }
    if (Settings::android()) {
        // D3 / D4 tails.
        if (Settings::build == 288 || Settings::build == 284) {
            animationsOffset = reader.readI16(); // D3 extra
            valuesOffset = reader.readI16();
            stringsOffset = reader.readI16();
            newFlags.set(reader.readU32());
            preferences.set(reader.readU16());
            identifier = reader.readAscii(4);
            backColor = reader.readColor();
            fadeIn = reader.readU32();
            fadeOut = reader.readU32();
        } else {
            extensionOffset = reader.readI16();
            reader.skip(2);
            stringsOffset = reader.readI16();
            newFlags.set(reader.readU32());
            preferences.set(reader.readU16());
            identifier = reader.readAscii(4);
            backColor = reader.readColor();
            fadeIn = reader.readU32();
            fadeOut = reader.readU32();
        }
        readSubLoaders(reader, s);
        return;
    }

    if (Settings::twoFivePlus() || Settings::f3() || Settings::fusion3Seed) {
        if (reader.tell() > reader.size() - 20) {
            Logger::error("E216: Ran out of bytes reading ObjectCommon (" +
                          std::to_string(reader.tell()) + "/" + std::to_string(reader.size()) + ")");
            return;
        }
    }

    systemObjectOffset = reader.readI16();
    valuesOffset = reader.readI16();
    stringsOffset = reader.readI16();
    newFlags.set(reader.readU16());
    preferences.set(reader.readU16());
    if (Settings::gameType == static_cast<uint32_t>(GameType::Normal) && Settings::build < 284) {
        identifier = reader.readAscii(2); // branch B reads 2 chars
    } else {
        identifier = reader.readAscii(4);
    }
    backColor = reader.readColor();
    fadeIn = reader.readU32();
    fadeOut = reader.readU32();

    // ObjectCommon for backdrop-ish types has no sub-loaders in the reference
    // (dispatch happens for every type; loaders are selected by offsets).
    readSubLoaders(reader, s);
}

void ObjectCommon::readSubLoaders(BinaryReader& reader, int64_t base) {
    try {
        if (animationsOffset > 0) {
            reader.seek(base + animationsOffset);
            animations.read(reader);
        }
        if (valuesOffset > 0) {
            reader.seek(base + valuesOffset);
            int16_t count = reader.readI16();
            alterableValues.clear();
            for (int16_t i = 0; i < count; ++i) alterableValues.push_back(reader.readI32());
            try {
                reader.readI32(); // value flags (best effort)
            } catch (const ReadError&) {
            }
        }
        if (stringsOffset > 0) {
            reader.seek(base + stringsOffset);
            int16_t count = reader.readI16();
            alterableStrings.clear();
            for (int16_t i = 0; i < count; ++i) alterableStrings.push_back(reader.readWideString());
        }
        if (movementsOffset > 0) {
            reader.seek(base + movementsOffset);
            if (Settings::old()) {
                Movement m;
                m.read(reader, 0, true);
                movements.push_back(std::move(m));
            } else {
                int64_t rootPos = reader.tell();
                uint32_t count = reader.readU32();
                int64_t tableStart = reader.tell();
                for (uint32_t i = 0; i < count; ++i) {
                    reader.seek(tableStart + 16 * i);
                    Movement m;
                    m.read(reader, rootPos, false);
                    movements.push_back(std::move(m));
                }
            }
        }
        if (systemObjectOffset > 0) {
            reader.seek(base + systemObjectOffset);
            if (identifier == "XT\xFF\xFF" || identifier == "TE" || identifier == "TEXT") {
                text = std::make_unique<Text>();
                text->read(reader, Settings::old());
            } else if (identifier == "TR\xFF\xFF" || identifier == "CNTR" || identifier == "CN" ||
                       identifier == "LIVE") {
                counter = std::make_unique<SystemCounter>();
                counter->read(reader);
            } else if (identifier == "CCA ") {
                subApplication = std::make_unique<SubApplication>();
                subApplication->read(reader);
            }
        }
        if (extensionOffset > 0) {
            reader.seek(base + extensionOffset);
            extension.totalSize = reader.readI32();
            reader.skip(4); // maxSize
            extension.version = reader.readI32();
            extension.id = reader.readI32();
            extension.priv = reader.readI32();
            int32_t dataSize = extension.totalSize - 20;
            if (dataSize != 0) extension.data = reader.readBytes(dataSize);
        }
        if (counterOffset > 0) {
            reader.seek(base + counterOffset);
            reader.readI16(); // size
            reader.readI32(); // initial
            reader.readI32(); // minimum
            reader.readI32(); // maximum
        }
    } catch (const ReadError& e) {
        Logger::debug(std::string("ObjectCommon sub-loader stopped: ") + e.what());
    }
}

// --------------------------------------------------------------------------- ObjectInfo
void ObjectInfo::readHeader(BinaryReader& reader) {
    handle = reader.readI16();
    objectType = reader.readI16();
    flags = reader.readI16();
    reader.skip(2);
    inkEffect = reader.readU8();
    if (inkEffect != 1) {
        reader.skip(3);
        uint8_t r = reader.readU8();
        uint8_t g = reader.readU8();
        uint8_t b = reader.readU8();
        rgbCoeff = {b, g, r, 0}; // reference stores R/B swapped
        blend = reader.readU8();
    } else {
        reader.skip(1);
        reader.skip(2);
        inkEffectValue = reader.readU8();
        reader.skip(3);
    }
    if (Settings::old() || Settings::gameType == static_cast<uint32_t>(GameType::Mmf2)) {
        rgbCoeff = {255, 255, 255, 0};
        blend = 255;
    }
}

void ObjectInfo::read(BinaryReader& reader, const Shaders& shaders) {
    while (true) {
        if (reader.eof()) break;
        Chunk nested;
        auto data = nested.read(reader);
        if (nested.id == 32639) break;

        if (Settings::hasParameter("onlyimages") && nested.id != 17476 && nested.id != 17477 &&
            nested.id != 17478)
            continue;

        BinaryReader r(data.data(), data.size());
        switch (nested.id) {
            case 17476:
                readHeader(r);
                break;
            case 17477:
                name = r.readYuniversal();
                break;
            case 17478:
                if (objectType == kObjQuickBackdrop) {
                    quickBackdrop = std::make_unique<QuickBackdrop>();
                    quickBackdrop->read(r);
                } else if (objectType == kObjBackdrop) {
                    backdrop = std::make_unique<Backdrop>();
                    backdrop->read(r);
                } else {
                    common = std::make_shared<ObjectCommon>();
                    common->read(r, objectType);
                }
                break;
            case 17480: {
                try {
                    int32_t shaderHandle = r.readI32();
                    int32_t numberOfParams = r.readI32();
                    const Shader* shader = shaders.find(shaderHandle);
                    if (!shader) break;
                    shaderData.hasShader = true;
                    shaderData.name = shader->name;
                    shaderData.handle = shaderHandle;
                    for (int32_t i = 0; i < numberOfParams; ++i) {
                        if (static_cast<size_t>(i) >= shader->parameters.size()) break;
                        const auto& p = shader->parameters[i];
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
                } catch (const ReadError&) {
                    shaderData.hasShader = false;
                }
                break;
            }
            default:
                Logger::debug(std::string("No ObjectInfo reader for chunk ") + chunkName(nested.id));
                break;
        }
    }
}

} // namespace ctfak
