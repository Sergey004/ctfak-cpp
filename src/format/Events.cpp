#include "Events.h"
#include "../core/Logger.h"
#include "../core/Settings.h"
#include <cstring>
#include <string>     // std::to_string (MSVC does not leak it transitively)

namespace ctfak {

// ------------------------------------------------------------------ ExpressionItem
void ExpressionItem::read(BinaryReader& reader) {
    int64_t start = reader.tell();
    objectType = reader.readI16();
    num = reader.readI16();
    if (objectType == 0 && num == 0) return; // terminator (4 bytes)

    int16_t size = reader.readI16();

    if (objectType == -1) { // system expressions
        switch (num) {
            case 0:
                kind = Kind::Long;
                longValue = reader.readI32();
                break;
            case 3:
                kind = Kind::String;
                stringValue = reader.readYuniversal();
                break;
            case 23: {
                kind = Kind::Double;
                doubleValue = reader.readF64();
                floatValue = reader.readF32();
                break;
            }
            case 24:
            case 50:
                kind = Kind::Global;
                reader.readI32(); // discarded
                globalValue = reader.readI32();
                break;
            default:
                break; // payload skipped via size
        }
    } else if (objectType >= 2 || objectType == -7) {
        kind = Kind::Object;
        objectInfo = reader.readU16();
        objectInfoList = reader.readI16();
        if (num == 16 || num == 19) {
            kind = Kind::Extension;
            extensionValue = reader.readI16();
        }
    }

    reader.seek(start + size);
}

std::string ExpressionItem::describe() const {
    switch (kind) {
        case Kind::Long: return std::to_string(longValue);
        case Kind::String: return "\"" + stringValue + "\"";
        case Kind::Double: {
            char buf[64];
            snprintf(buf, sizeof(buf), "%g", doubleValue);
            return buf;
        }
        case Kind::Global: return "GlobalValue(" + std::to_string(globalValue) + ")";
        case Kind::Extension: return "Ext(objectInfo=" + std::to_string(objectInfo) + ", value=" +
                                     std::to_string(extensionValue) + ")";
        case Kind::Object:
            return "Object(info=" + std::to_string(objectInfo) + ", list=" +
                   std::to_string(objectInfoList) + ", num=" + std::to_string(num) + ")";
        default:
            return "System(type=" + std::to_string(objectType) + ", num=" + std::to_string(num) + ")";
    }
}

// ------------------------------------------------------------------ ExpressionParameter
void ExpressionParameter::read(BinaryReader& reader) {
    comparsion = reader.readI16();
    items.clear();
    while (true) {
        ExpressionItem item;
        item.read(reader);
        if (item.objectType == 0 && item.num == 0) break;
        items.push_back(std::move(item));
    }
}

const char* ExpressionParameter::operatorString() const {
    switch (comparsion) {
        case 0: return "==";
        case 1: return "!=";
        case 2: return "<=";
        case 3: return "<";
        case 4: return ">=";
        case 5: return ">";
        default: return "err";
    }
}

std::string ExpressionParameter::describe() const {
    std::string out;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i) out += ", ";
        out += items[i].describe();
    }
    return out;
}

// ------------------------------------------------------------------ Parameter
const char* Parameter::kindName() const {
    switch (kind) {
        case Kind::Object: return "Object";
        case Kind::Time: return "Time";
        case Kind::Short: return "Short";
        case Kind::Int: return "Int";
        case Kind::Sample: return "Sample";
        case Kind::Create: return "Create";
        case Kind::Every: return "Every";
        case Kind::Key: return "Key";
        case Kind::Expression: return "Expression";
        case Kind::Position: return "Position";
        case Kind::Shoot: return "Shoot";
        case Kind::Zone: return "Zone";
        case Kind::Colour: return "Colour";
        case Kind::Click: return "Click";
        case Kind::Program: return "Program";
        case Kind::Filename: return "Filename";
        case Kind::String: return "String";
        case Kind::GlobalValue: return "GlobalValue";
        case Kind::AlterableValue: return "AlterableValue";
        case Kind::TwoShorts: return "TwoShorts";
        case Kind::Extension: return "Extension";
        case Kind::Group: return "Group";
        case Kind::GroupPointer: return "GroupPointer";
        case Kind::MultipleVariables: return "MultipleVariables";
        case Kind::ChildEvent: return "ChildEvent";
        default: return "None";
    }
}

void Parameter::read(BinaryReader& reader) {
    int64_t start = reader.tell();
    int16_t size = reader.readI16();
    code = reader.readI16();

    switch (code) {
        case 1: {
            kind = Kind::Object;
            objectInfoList = reader.readI16();
            objectInfo = reader.readU16();
            objectType = reader.readI16();
            break;
        }
        case 2:
        case 42: {
            kind = Kind::Time;
            value = reader.readI32();  // timer
            value2 = reader.readI32(); // loops
            break;
        }
        case 3: case 4: case 10: case 11: case 12: case 17: case 26: case 31: case 43:
        case 57: case 58: case 60: case 61: {
            kind = Kind::Short;
            value = reader.readI16();
            break;
        }
        case 5: case 25: case 29: case 34: case 48: case 56: case 67: case 70: {
            kind = Kind::Int;
            value = reader.readI32();
            break;
        }
        case 6: case 7: case 35: case 36: {
            kind = Kind::Sample;
            sampleHandle = reader.readI16();
            sampleFlags = reader.readU16();
            str = reader.readYuniversal();
            break;
        }
        case 9:
        case 21: {
            kind = Kind::Create;
            position.parentInfo = reader.readU16();
            position.flags = reader.readI16();
            position.x = reader.readI16();
            position.y = reader.readI16();
            position.slope = reader.readI16();
            position.angle = reader.readI16();
            position.direction = reader.readI32();
            position.typeParent = reader.readI16();
            position.objectInfoList = reader.readI16();
            position.layer = reader.readI16();
            createInstances = reader.readU16();
            createObjectInfo = reader.readU16();
            break;
        }
        case 13: {
            kind = Kind::Every;
            value = reader.readI32();  // delay
            value2 = reader.readI32(); // counter
            break;
        }
        case 14:
        case 44: {
            kind = Kind::Key;
            value = reader.readU16();
            break;
        }
        case 15: case 22: case 23: case 27: case 28: case 45: case 46: case 52: case 53:
        case 54: case 59: case 62: {
            kind = Kind::Expression;
            expression = std::make_shared<ExpressionParameter>();
            expression->read(reader);
            break;
        }
        case 16: {
            kind = Kind::Position;
            position.parentInfo = reader.readU16();
            position.flags = reader.readI16();
            position.x = reader.readI16();
            position.y = reader.readI16();
            position.slope = reader.readI16();
            position.angle = reader.readI16();
            position.direction = reader.readI32();
            position.typeParent = reader.readI16();
            position.objectInfoList = reader.readI16();
            position.layer = reader.readI16();
            break;
        }
        case 18: {
            kind = Kind::Shoot;
            position.parentInfo = reader.readU16();
            position.flags = reader.readI16();
            position.x = reader.readI16();
            position.y = reader.readI16();
            position.slope = reader.readI16();
            position.angle = reader.readI16();
            position.direction = reader.readI32();
            position.typeParent = reader.readI16();
            position.objectInfoList = reader.readI16();
            position.layer = reader.readI16();
            shootInstance = reader.readU16();
            shootObjectInfo = reader.readU16();
            reader.skip(4);
            shootSpeed = reader.readI16();
            break;
        }
        case 19: {
            kind = Kind::Zone;
            zone[0] = reader.readI16();
            zone[1] = reader.readI16();
            zone[2] = reader.readI16();
            zone[3] = reader.readI16();
            break;
        }
        case 24: {
            kind = Kind::Colour;
            colour[0] = reader.readU8();
            colour[1] = reader.readU8();
            colour[2] = reader.readU8();
            colour[3] = reader.readU8(); // ignored by reference
            break;
        }
        case 32: {
            kind = Kind::Click;
            value = reader.readU8();  // button
            value2 = reader.readU8(); // isDouble
            break;
        }
        case 33: {
            kind = Kind::Program;
            programFlags = reader.readI16();
            str = reader.readAscii(260);
            programCommand = reader.readAscii();
            break;
        }
        case 38: {
            kind = Kind::Group;
            groupFlags = reader.readU16();
            groupId = reader.readU16();
            groupName = reader.readWideString();
            if (Settings::build >= 293) groupName = "Group " + std::to_string(groupId);
            int64_t used = reader.tell() - start;
            int64_t target = start + 2 /*size*/ + 2 /*code*/ + 4 + 190;
            // Padding accounts for the name length as stored (before renaming).
            (void)used;
            (void)target;
            break;
        }
        case 39: {
            kind = Kind::GroupPointer;
            groupPointer = reader.readI32();
            groupPointerId = reader.readI16();
            break;
        }
        case 40: {
            kind = Kind::Filename;
            str = reader.readAscii();
            break;
        }
        case 41:
        case 64: {
            kind = Kind::String;
            str = reader.readAscii();
            break;
        }
        case 49: {
            kind = Kind::GlobalValue;
            value = reader.readI16();
            break;
        }
        case 50: {
            kind = Kind::AlterableValue;
            value = reader.readI16();
            break;
        }
        case 47:
        case 51: {
            kind = Kind::TwoShorts;
            value = reader.readI16();
            value2 = reader.readI16();
            break;
        }
        case 55: {
            kind = Kind::Extension;
            extSize = reader.readI16();
            extType = reader.readI16();
            extCode = reader.readI16();
            if (extSize > 0) extData = reader.readBytes(extSize);
            break;
        }
        case 68: {
            kind = Kind::MultipleVariables;
            mvFlags = reader.readI32();
            mvFlagMasks = reader.readI32();
            mvFlagValues = reader.readI32();
            int nValues = 0;
            uint32_t mask = 1;
            while (nValues < 4) {
                if ((mvFlags & mask) == 0) break;
                mask <<= 4;
                ++nValues;
            }
            uint32_t maskDouble = 4;
            for (int i = 0; i < nValues; ++i) {
                Multivar mv;
                mv.index = reader.readI32();
                mv.op = reader.readI32();
                mv.isDouble = (mvFlags & maskDouble) != 0;
                if (mv.isDouble) {
                    mv.doubleValue = reader.readF64();
                } else {
                    mv.intValue = reader.readI32();
                    reader.skip(4);
                }
                maskDouble <<= 4;
                multivars.push_back(std::move(mv));
            }
            break;
        }
        case 69: {
            kind = Kind::ChildEvent;
            childCount = reader.readI32();
            for (int32_t i = 0; i < childCount * 2; ++i)
                childObjects.push_back(reader.readI16());
            reader.skip(4);
            break;
        }
        default: {
            kind = Kind::None;
            Logger::warning("Loader is null: " + std::to_string(code));
            return; // reference leaves the stream position untouched
        }
    }

    reader.seek(start + size);
}

// ------------------------------------------------------------------ Condition / Action
void Condition::read(BinaryReader& reader) {
    size = reader.readU16();
    objectType = reader.readI16();
    num = reader.readI16();
    objectInfo = reader.readU16();
    objectInfoList = reader.readI16();
    flags = reader.readI8();
    otherFlags = reader.readI8();
    numParameters = reader.readU8();
    defType = reader.readU8();
    identifier = reader.readI16();
    for (uint8_t i = 0; i < numParameters; ++i) {
        Parameter p;
        p.read(reader);
        parameters.push_back(std::move(p));
    }
}

void Action::read(BinaryReader& reader) {
    size = reader.readU16();
    objectType = reader.readI16();
    num = reader.readI16();
    objectInfo = reader.readU16();
    objectInfoList = reader.readI16();
    flags = reader.readI8();
    otherFlags = reader.readI8();
    numParameters = reader.readU8();
    defType = reader.readU8();
    for (uint8_t i = 0; i < numParameters; ++i) {
        Parameter p;
        p.read(reader);
        parameters.push_back(std::move(p));
    }
}

// ------------------------------------------------------------------ EventGroup
static void fixCondition(Condition& cond) {
    int num = cond.num;
    if ((num == -42 || num == -43) && cond.objectType != -1) num = -27;
    if (cond.objectType == -1) {
        if (num >= -33 && num <= -28) num = -8;
    }
    cond.num = num;
}

static void fixAction(Action& act) {
    int num = act.num;
    if (act.objectType == -1) {
        if (num == 27 || num == 28 || num == 29 || num == 30) num = 3;
        if (num == 35 || num == 32 || num == 33 || num == 34) num = 4;
        if (num == 31 || num == 36 || num == 37 || num == 38) num = 5;
    }
    act.num = num;
}

static int g_identifierCounter = 0;

void EventGroup::read(BinaryReader& reader) {
    int64_t currentPosition = reader.tell();
    int16_t size = reader.readI16(); // stored negative
    numConditions = reader.readU8();
    numActions = reader.readU8();
    flags = reader.readU16();

    if (Settings::old()) {
        isRestricted = reader.readI16();
        restrictCpt = reader.readI16();
        identifier = reader.readI16();
        undo = reader.readI16();
    } else if (Settings::build >= 284) {
        line = reader.readI16();
        isRestricted = reader.readI32();
        restrictCpt = reader.readI32();
    } else {
        isRestricted = reader.readI16();
        restrictCpt = reader.readI16();
        identifier = reader.readI16();
        undo = reader.readI16();
    }

    conditions.clear();
    actions.clear();

    for (uint8_t i = 0; i < numConditions; ++i) {
        Condition item;
        item.read(reader);
        item.identifier += g_identifierCounter;
        fixCondition(item);

        bool expanded = false;
        if ((item.num == -25 || item.num == -41) && !item.parameters.empty()) {
            for (auto& param : item.parameters) {
                if (param.kind == Parameter::Kind::MultipleVariables) {
                    // Expand obfuscated alterable-value conditions.
                    int cnt = 0;
                    uint32_t mask = 1;
                    while (mask != 0) {
                        if ((mask & param.mvFlagMasks) == 0) {
                            mask <<= 1;
                            ++cnt;
                            continue;
                        }
                        Condition flagCond;
                        flagCond.objectInfo = item.objectInfo;
                        flagCond.objectType = item.objectType;
                        flagCond.flags = item.flags;
                        flagCond.otherFlags = item.otherFlags;
                        flagCond.defType = item.defType;
                        flagCond.identifier = item.identifier + cnt;
                        flagCond.num = (mask & param.mvFlagValues) == 0 ? -24 : -25;
                        Parameter flagParam;
                        flagParam.kind = Parameter::Kind::Expression;
                        flagParam.code = 22;
                        flagParam.expression = std::make_shared<ExpressionParameter>();
                        flagParam.expression->comparsion = 0;
                        ExpressionItem longItem;
                        longItem.kind = ExpressionItem::Kind::Long;
                        longItem.objectType = -1;
                        longItem.num = 0;
                        longItem.longValue = cnt;
                        flagParam.expression->items.push_back(std::move(longItem));
                        flagCond.parameters.push_back(std::move(flagParam));
                        conditions.push_back(std::move(flagCond));
                        mask <<= 1;
                        ++cnt;
                        ++g_identifierCounter;
                    }
                    for (const auto& mv : param.multivars) {
                        Condition valueCond;
                        valueCond.objectInfo = item.objectInfo;
                        valueCond.objectType = item.objectType;
                        valueCond.flags = item.flags;
                        valueCond.otherFlags = item.otherFlags;
                        valueCond.defType = item.defType;
                        valueCond.identifier = item.identifier;
                        valueCond.num = -27;
                        Parameter alterableParam;
                        alterableParam.kind = Parameter::Kind::AlterableValue;
                        alterableParam.code = 50;
                        alterableParam.value = mv.index;
                        valueCond.parameters.push_back(std::move(alterableParam));
                        Parameter exprParam;
                        exprParam.kind = Parameter::Kind::Expression;
                        exprParam.code = 23;
                        exprParam.expression = std::make_shared<ExpressionParameter>();
                        exprParam.expression->comparsion = static_cast<int16_t>(mv.op);
                        ExpressionItem valueItem;
                        if (mv.isDouble) {
                            valueItem.kind = ExpressionItem::Kind::Double;
                            valueItem.objectType = -1;
                            valueItem.num = 23;
                            valueItem.doubleValue = mv.doubleValue;
                        } else {
                            valueItem.kind = ExpressionItem::Kind::Long;
                            valueItem.objectType = -1;
                            valueItem.num = 0;
                            valueItem.longValue = mv.intValue;
                        }
                        exprParam.expression->items.push_back(std::move(valueItem));
                        valueCond.parameters.push_back(std::move(exprParam));
                        conditions.push_back(std::move(valueCond));
                        ++g_identifierCounter;
                    }
                    expanded = true;
                    break; // reference expands once per condition item
                }
            }
        }
        if (!expanded) conditions.push_back(std::move(item));
    }

    for (uint8_t i = 0; i < numActions; ++i) {
        Action item;
        item.read(reader);
        fixAction(item);

        if (item.num == 2 && item.parameters.size() == 2 && item.objectType == -1) {
            // Split combined position action into X / Y actions.
            Action xAct;
            xAct.objectInfo = item.objectInfo;
            xAct.objectType = item.objectType;
            xAct.flags = item.flags;
            xAct.otherFlags = item.otherFlags;
            xAct.defType = item.defType;
            xAct.num = 2;
            xAct.parameters.push_back(item.parameters[0]);
            Action yAct = xAct;
            yAct.num = 3;
            yAct.parameters[0] = item.parameters[1];
            actions.push_back(std::move(xAct));
            actions.push_back(std::move(yAct));
        } else {
            actions.push_back(std::move(item));
        }
    }

    reader.seek(currentPosition + (size * -1));
}

// ------------------------------------------------------------------ EventsChunk
void EventsChunk::read(BinaryReader& reader) {
    g_identifierCounter = 0;
    while (true) {
        if (!reader.hasMemory(4)) break;
        std::string identifier = reader.readAscii(4);
        if (identifier == "ER>>") {
            maxObjects = reader.readI16();
            maxObjectInfo = reader.readI16();
            numberOfPlayers = reader.readI16();
            for (int i = 0; i < 17; ++i) conditionsPerObject.push_back(reader.readI16());
            int16_t qualifierCount = reader.readI16();
            for (int16_t i = 0; i < qualifierCount; ++i) {
                Qualifier q;
                q.objectInfo = reader.readU16();
                q.type = reader.readI16();
                q.qualifier = q.objectInfo & 0x7FF;
                qualifiers.push_back(q);
            }
        } else if (identifier == "ERes") {
            if (Settings::android()) reader.skip(4);
            reader.readI32(); // size, unused
        } else if (identifier == "ERev") {
            int32_t size = reader.readI32();
            if (Settings::android()) size += 4;
            int64_t endPosition = reader.tell() + size;
            if (Settings::android()) reader.readI32();
            while (reader.tell() < endPosition && !reader.eof()) {
                EventGroup eg;
                eg.read(reader);
                groups.push_back(std::move(eg));
            }
        } else if (identifier == "ERop") {
            optionFlags = reader.readI32();
        } else if (identifier == "<<ER") {
            break;
        } else {
            Logger::warning("Unknown event section: " + identifier);
            break;
        }
    }
}

} // namespace ctfak
