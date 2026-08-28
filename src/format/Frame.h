// Frame chunk (13107): nested chunk stream with header/name/palette/instances/
// layers/events/fades/effects.
#pragma once

#include "../core/BinaryReader.h"
#include "../core/BitDict.h"
#include "ObjectInfo.h"
#include <memory>
#include <vector>

namespace ctfak {

class Shaders;
class EventsChunk;

struct ObjectInstance {
    uint16_t handle = 0;
    uint16_t objectInfo = 0;
    int32_t x = 0, y = 0;
    int16_t parentType = 0;
    int16_t parentHandle = 0;
    int16_t layer = 0;
    int16_t instance = 0;
    void read(BinaryReader& reader);
};

struct Layer {
    BitDict flags = {"XCoefficient", "YCoefficient", "DoNotSaveBackground", "", "Visible",
                     "WrapHorizontally", "WrapVertically", "", "", "", "", "", "", "", "", "",
                     "Redraw", "ToHide", "ToShow"};
    float xCoeff = 0, yCoeff = 0;
    int32_t numberOfBackgrounds = 0;
    int32_t backgroundIndex = 0;
    std::string name;
    int16_t effect = 0, effectParam = 0;
    BinaryReader::Color rgbCoeff;
    int32_t inkEffect = -1;
    ShaderData shaderData;
    void read(BinaryReader& reader);
};

struct Transition {
    std::string module; // 4 ASCII chars
    std::string name;   // 4 ASCII chars
    int32_t duration = 0;
    int32_t flags = 0;
    BinaryReader::Color color;
    std::string moduleFile;
    std::vector<uint8_t> parameterData;
    void read(BinaryReader& reader);
};

struct Frame {
    std::string name;
    int32_t width = 0, height = 0;
    BinaryReader::Color background;
    BitDict flags = {"DisplayTitle", "GrabDesktop", "KeepDisplay", "Unk1", "Unk2",
                     "HandleCollision", "Unk3", "Unk4", "ResizeAtStart", "Unk5", "Unk6", "Unk7",
                     "Unk8", "Unk9", "Unk10", "TimeMovements", "Unk11", "Unk12", "DontInclude",
                     "DontEraseBG"};
    std::vector<ObjectInstance> objects;
    std::vector<Layer> layers;
    std::vector<BinaryReader::Color> palette;
    std::unique_ptr<Transition> fadeIn, fadeOut;
    int32_t virtualLeft = 0, virtualTop = 0, virtualRight = 0, virtualBottom = 0;
    bool hasVirtualRect = false;
    int32_t randomSeed = 0;
    int32_t movementTimer = 60;
    int16_t effect = 0, effectParam = 0;
    BinaryReader::Color rgbCoeff;
    int32_t inkEffect = -1;
    ShaderData shaderData;
    std::shared_ptr<EventsChunk> events;

    void read(BinaryReader& reader, const Shaders& shaders);
};

} // namespace ctfak
