// Object bank items: ObjectInfo (17476-17480), ObjectCommon and its sub-loaders:
// movements, animations, alterable values/strings, system objects (text/counters),
// quick backdrop / backdrop, shape, sub-application.
#pragma once

#include "../core/BinaryReader.h"
#include "../core/BitDict.h"
#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ctfak {

class Shaders;

constexpr int16_t kObjQuickBackdrop = 0;
constexpr int16_t kObjBackdrop = 1;
constexpr int16_t kObjActive = 2;
constexpr int16_t kObjText = 3;
constexpr int16_t kObjQuestion = 4;
constexpr int16_t kObjScore = 5;
constexpr int16_t kObjLives = 6;
constexpr int16_t kObjCounter = 7;
constexpr int16_t kObjRtf = 8;
constexpr int16_t kObjSubApplication = 9;
constexpr int16_t kObjExtension = 32;

const char* objectTypeName(int16_t type);

// --------------------------------------------------------------------------- shape
struct Shape {
    int16_t borderSize = 0;
    BinaryReader::Color borderColor;
    int16_t shapeType = 0;
    int16_t fillType = 0;
    int16_t lineFlags = 0;
    BinaryReader::Color color1, color2;
    int16_t gradFlags = 0;
    int16_t image = 0;
    void read(BinaryReader& reader);
};

struct QuickBackdrop {
    int32_t size = 0;
    int16_t obstacleType = 0;
    int16_t collisionType = 0;
    int32_t width = 0, height = 0;
    Shape shape;
    void read(BinaryReader& reader);
};

struct Backdrop {
    int32_t size = 0;
    int16_t obstacleType = 0;
    int16_t collisionType = 0;
    int32_t width = 0, height = 0;
    int16_t image = 0;
    void read(BinaryReader& reader);
};

// --------------------------------------------------------------------------- movements
struct MovementStep {
    uint8_t flags = 0, size = 0, speed = 0, direction = 0;
    int16_t destX = 0, destY = 0, cosinus = 0, sinus = 0, length = 0, pause = 0;
    std::vector<uint8_t> data;
    void read(BinaryReader& reader);
};

struct Movement {
    uint16_t player = 0;
    uint16_t type = 0;
    uint8_t movingAtStart = 0;
    int32_t directionAtStart = 0;
    std::string typeName;
    std::vector<std::pair<std::string, double>> fields;
    // path movement payload
    int16_t pathCount = 0, pathMinSpeed = 0, pathMaxSpeed = 0;
    uint8_t pathLoop = 0, pathRepositionAtEnd = 0, pathReverseAtEnd = 0;
    std::vector<MovementStep> pathSteps;
    std::vector<uint8_t> extensionData;

    void read(BinaryReader& reader, int64_t rootPos, bool old);
};

struct Animations {
    // key: animation index, value: per-direction data (key: direction 0..31)
    std::map<int, std::map<int, struct AnimationDirection>> items;
    void read(BinaryReader& reader);
};

struct AnimationDirection {
    int8_t minSpeed = 0, maxSpeed = 0;
    int16_t repeat = 0, backTo = 0;
    std::vector<uint16_t> frames;
    void read(BinaryReader& reader);
};

// --------------------------------------------------------------------------- system objects
struct Paragraph {
    uint16_t font = 0;
    BitDict flags = {"HorizontalCenter", "RightAligned", "VerticalCenter", "BottomAligned",
                     "None", "None", "None", "None", "Correct", "Relief"};
    BinaryReader::Color color;
    std::string value;
    void read(BinaryReader& reader, bool old);
};

struct Text {
    int32_t width = 0, height = 0;
    std::vector<Paragraph> paragraphs;
    void read(BinaryReader& reader, bool old);
};

struct SystemCounter {
    uint32_t width = 0, height = 0;
    uint16_t player = 0, displayType = 0;
    BitDict flags = {};
    int integerDigits = 0, floatDigits = 0, decimals = 0;
    bool formatFloat = false, useDecimals = false, addNulls = false, inverse = false;
    uint16_t font = 0;
    std::vector<uint16_t> frames;
    Shape shape;
    std::vector<uint8_t> rawHead;   // v2.66 diagnostic: the CNTR block head
    void read(BinaryReader& reader);
};

struct SubApplication {
    int32_t cx = 0, cy = 0;
    int16_t version = 0, startFrame = 0;
    int32_t options = 0;
    void read(BinaryReader& reader);
};

// --------------------------------------------------------------------------- object common
struct ObjectCommon {
    int16_t animationsOffset = 0, movementsOffset = 0, extensionOffset = 0, counterOffset = 0;
    int16_t systemObjectOffset = 0, valuesOffset = 0, stringsOffset = 0;
    int32_t version = 0;
    BitDict flags = {"DisplayInFront", "Background", "Backsave", "RunBeforeFadeIn", "Movements",
                     "Animations", "TabStop", "WindowProc", "Values", "Sprites", "InternalBacksave",
                     "ScrollingIndependant", "QuickDisplay", "NeverKill", "NeverSleep",
                     "ManualSleep", "Text", "DoNotCreateAtStart", "FakeSprite", "FakeCollisions"};
    BitDict newFlags = {"DoNotSaveBackground", "SolidBackground", "CollisionBox", "VisibleAtStart",
                        "ObstacleSolid", "ObstaclePlatform", "AutomaticRotation"};
    BitDict preferences = {"Backsave", "ScrollingIndependant", "QuickDisplay", "Sleep", "LoadOnCall",
                           "Global", "BackEffects", "Kill", "InkEffects", "Transitions",
                           "FineCollisions", "AppletProblems"};
    std::array<int16_t, 8> qualifiers{};
    std::string identifier;
    BinaryReader::Color backColor;
    uint32_t fadeIn = 0, fadeOut = 0;

    // sub-loader payloads
    Animations animations;
    std::vector<int32_t> alterableValues;
    std::vector<std::string> alterableStrings;
    std::vector<Movement> movements;
    std::unique_ptr<Text> text;
    std::unique_ptr<SystemCounter> counter;
    std::unique_ptr<SubApplication> subApplication;
    struct {
        int32_t totalSize = 0, version = 0, id = 0, priv = 0;
        std::vector<uint8_t> data;
    } extension;
    std::unique_ptr<Shape> counterShapeless; // reserved

    // parent object type dispatches system object loaders
    void read(BinaryReader& reader, int16_t objectType);

private:
    void readSubLoaders(BinaryReader& reader, int64_t base);
};

// --------------------------------------------------------------------------- object info
struct ShaderParamValue {
    std::string name;
    int type = 0;
    double value = 0;
};

struct ShaderData {
    bool hasShader = false;
    std::string name;
    int32_t handle = -1;
    std::vector<ShaderParamValue> parameters;
};

struct ObjectInfo {
    int16_t handle = 0;
    int16_t objectType = 0;
    int16_t flags = 0;
    uint8_t inkEffect = 0;
    uint8_t inkEffectValue = 0;
    uint8_t blend = 255;
    BinaryReader::Color rgbCoeff{255, 255, 255, 0};
    std::string name;
    std::shared_ptr<ObjectCommon> common;
    std::unique_ptr<Backdrop> backdrop;
    std::unique_ptr<QuickBackdrop> quickBackdrop;
    ShaderData shaderData;

    // Reads the nested chunk stream of one object entry (bank chunks 8745/8767).
    void read(BinaryReader& reader, const Shaders& shaders);

    // Reads the plain header layout used by chunk 8787 (2.5+ object headers).
    void readHeader(BinaryReader& reader);
};

} // namespace ctfak
