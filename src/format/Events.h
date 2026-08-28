// Event program (13117): sections, groups, conditions, actions, parameters,
// expressions. Layout follows docs/FORMAT_NOTES.md section 6.
#pragma once

#include "../core/BinaryReader.h"
#include <memory>
#include <string>
#include <vector>

namespace ctfak {

// ------------------------------------------------------------------ expressions
struct ExpressionItem {
    int16_t objectType = 0;
    int16_t num = 0;
    int16_t objectInfo = 0;
    int16_t objectInfoList = 0;

    enum class Kind { None, Long, String, Double, Global, Extension, Object };
    Kind kind = Kind::None;

    int32_t longValue = 0;
    std::string stringValue;
    double doubleValue = 0;
    float floatValue = 0;
    int32_t globalValue = 0;
    int16_t extensionValue = 0;

    void read(BinaryReader& reader);
    std::string describe() const; // human readable single-token description
};

struct ExpressionParameter {
    int16_t comparsion = 0;
    std::vector<ExpressionItem> items;

    void read(BinaryReader& reader);
    const char* operatorString() const;
    std::string describe() const; // joined token description
};

// ------------------------------------------------------------------ parameters
struct Multivar {
    int32_t index = 0;
    int32_t op = 0;
    bool isDouble = false;
    int32_t intValue = 0;
    double doubleValue = 0;
};

struct Parameter {
    int16_t code = 0;

    enum class Kind {
        None,
        Object,     // 1
        Time,       // 2/42
        Short,      // 3,4,10,11,12,17,26,31,43,57,58,60,61
        Int,        // 5,25,29,34,48,56,67,70
        Sample,     // 6,7,35,36
        Create,     // 9/21
        Every,      // 13
        Key,        // 14/44
        Expression, // 15,22,23,27,28,45,46,52,53,54,59,62
        Position,   // 16
        Shoot,      // 18
        Zone,       // 19
        Colour,     // 24
        Click,      // 32
        Program,    // 33
        Filename,   // 40
        String,     // 41/64
        GlobalValue,// 49
        AlterableValue, // 50
        TwoShorts,  // 47/51
        Extension,  // 55
        Group,      // 38
        GroupPointer, // 39
        MultipleVariables, // 68
        ChildEvent  // 69
    };
    Kind kind = Kind::None;

    // scalar payloads
    int64_t value = 0;    // Short / Int / Key / GlobalValue / AlterableValue / Click.button / Every.delay / Time.timer
    int64_t value2 = 0;   // TwoShorts.v2 / Every.counter / Time.loops / Click.isDouble
    std::string str;      // String / Filename / Sample.name / Program.filename

    // Object param (1)
    int16_t objectInfoList = 0;
    uint16_t objectInfo = 0;
    int16_t objectType = 0;

    // Position (16) payload, reused by Create/Shoot
    struct Position {
        uint16_t parentInfo = 0;
        int16_t flags = 0, x = 0, y = 0, slope = 0, angle = 0;
        int32_t direction = 0;
        int16_t typeParent = 0, objectInfoList = 0, layer = 0;
    } position;

    // Create (9/21)
    uint16_t createInstances = 0, createObjectInfo = 0;
    // Shoot (18)
    uint16_t shootInstance = 0, shootObjectInfo = 0;
    int16_t shootSpeed = 0;
    // Sample (6/7/35/36)
    int16_t sampleHandle = 0;
    uint16_t sampleFlags = 0;
    // Program (33)
    int16_t programFlags = 0;
    std::string programCommand;
    // Zone (19)
    int16_t zone[4] = {0, 0, 0, 0};
    // Colour (24)
    uint8_t colour[4] = {0, 0, 0, 0};
    // Group (38)
    uint16_t groupFlags = 0, groupId = 0;
    std::string groupName;
    // GroupPointer (39)
    int32_t groupPointer = 0;
    int16_t groupPointerId = 0;
    // Extension (55)
    int16_t extSize = 0, extType = 0, extCode = 0;
    std::vector<uint8_t> extData;
    // MultipleVariables (68)
    int32_t mvFlags = 0, mvFlagMasks = 0, mvFlagValues = 0;
    std::vector<Multivar> multivars;
    // ChildEvent (69)
    int32_t childCount = 0;
    std::vector<int16_t> childObjects;

    std::shared_ptr<ExpressionParameter> expression;

    void read(BinaryReader& reader);
    const char* kindName() const;
};

// ------------------------------------------------------------------ conditions / actions
struct Condition {
    uint16_t size = 0;
    int16_t objectType = 0;
    int16_t num = 0;
    uint16_t objectInfo = 0;
    int16_t objectInfoList = 0;
    int8_t flags = 0;
    int8_t otherFlags = 0;
    uint8_t numParameters = 0;
    uint8_t defType = 0;
    int16_t identifier = 0;
    std::vector<Parameter> parameters;
    void read(BinaryReader& reader);
};

struct Action {
    uint16_t size = 0;
    int16_t objectType = 0;
    int16_t num = 0;
    uint16_t objectInfo = 0;
    int16_t objectInfoList = 0;
    int8_t flags = 0;
    int8_t otherFlags = 0;
    uint8_t numParameters = 0;
    uint8_t defType = 0;
    std::vector<Parameter> parameters;
    void read(BinaryReader& reader);
};

struct EventGroup {
    uint16_t flags = 0;
    uint8_t numConditions = 0;
    uint8_t numActions = 0;
    int16_t line = 0;
    int32_t isRestricted = 0;
    int32_t restrictCpt = 0;
    int16_t identifier = 0;
    int16_t undo = 0;
    std::vector<Condition> conditions;
    std::vector<Action> actions;
    void read(BinaryReader& reader);
};

struct Qualifier {
    uint16_t objectInfo = 0;
    int16_t type = 0;
    int32_t qualifier = 0;
};

struct EventsChunk {
    int16_t maxObjects = 0;
    int16_t maxObjectInfo = 0;
    int16_t numberOfPlayers = 0;
    std::vector<int16_t> conditionsPerObject; // 17 entries
    std::vector<Qualifier> qualifiers;
    int32_t optionFlags = 0;
    std::vector<EventGroup> groups;

    void read(BinaryReader& reader);
};

// Convenience factory used by Frame.
inline std::shared_ptr<EventsChunk> readEventsChunk(BinaryReader& reader) {
    auto events = std::make_shared<EventsChunk>();
    events->read(reader);
    return events;
}

} // namespace ctfak
