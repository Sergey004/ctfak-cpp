#include "EventsListing.h"
#include "Dumpers.h"
#include "../util/Config.h"
#include "../util/Utils.h"
#include "../core/Logger.h"
#include "../format/GameData.h"
#include "../format/Frame.h"
#include "../format/ObjectInfo.h"
#include <algorithm>
#include <cstdio>
#include <memory>

namespace ctfak {

// ---------------------------------------------------------------- helpers

namespace {

std::string asciiOnly(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (static_cast<unsigned char>(c) >= 32 && static_cast<unsigned char>(c) < 127) out += c;
        else if (c == '\n' || c == '\r' || c == '\t') out += ' ';
    }
    return out;
}

std::string quoted(const std::string& s) { return "\"" + asciiOnly(s) + "\""; }

// Fusion timer unit is 1/50 s (the editor shows 1"50 = one second).
std::string ticksToSeconds(int64_t ticks) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "~%.2fs", static_cast<double>(ticks) / 50.0);
    return buf;
}

} // namespace

const char* virtualKeyName(int code) {
    switch (code) {
        case 0x01: return "LButton";
        case 0x02: return "RButton";
        case 0x04: return "MButton";
        case 0x08: return "Backspace";
        case 0x09: return "Tab";
        case 0x0D: return "Enter";
        case 0x10: return "Shift";
        case 0x11: return "Ctrl";
        case 0x12: return "Alt";
        case 0x14: return "CapsLock";
        case 0x1B: return "Escape";
        case 0x20: return "Space";
        case 0x21: return "PageUp";
        case 0x22: return "PageDown";
        case 0x23: return "End";
        case 0x24: return "Home";
        case 0x25: return "Left";
        case 0x26: return "Up";
        case 0x27: return "Right";
        case 0x28: return "Down";
        case 0x2E: return "Delete";
        case 0x30: case 0x31: case 0x32: case 0x33: case 0x34:
        case 0x35: case 0x36: case 0x37: case 0x38: case 0x39: {
            static const char* digits[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
            return digits[code - 0x30];
        }
        case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
        case 0x48: case 0x49: case 0x4A: case 0x4B: case 0x4C: case 0x4D: case 0x4E:
        case 0x4F: case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55:
        case 0x56: case 0x57: case 0x58: case 0x59: case 0x5A: {
            static const char* letters[] = {"A", "B", "C", "D", "E", "F", "G", "H", "I",
                "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W",
                "X", "Y", "Z"};
            return letters[code - 0x41];
        }
        case 0x60: case 0x61: case 0x62: case 0x63: case 0x64:
        case 0x65: case 0x66: case 0x67: case 0x68: case 0x69: {
            static const char* numpad[] = {"Num0", "Num1", "Num2", "Num3", "Num4",
                "Num5", "Num6", "Num7", "Num8", "Num9"};
            return numpad[code - 0x60];
        }
        case 0x6A: return "NumMul";
        case 0x6B: return "NumPlus";
        case 0x6D: return "NumMinus";
        case 0x6E: return "NumDot";
        case 0x6F: return "NumDiv";
        default:
            if (code >= 0x70 && code <= 0x7B) { // F1..F12
                static const char* funcs[] = {"F1", "F2", "F3", "F4", "F5", "F6", "F7",
                    "F8", "F9", "F10", "F11", "F12"};
                return funcs[code - 0x70];
            }
            return nullptr;
    }
}

std::string eventObjectLabel(int16_t objectType, uint16_t objectInfo,
                             const ObjectNameResolver& resolver) {
    std::string type = objectTypeName(objectType);
    if (objectInfo != 0) {
        std::string name = resolver ? resolver(objectInfo) : std::string();
        if (!name.empty()) return quoted(name) + " (" + type + ")";
        return type + " objInfo=" + std::to_string(objectInfo);
    }
    return type;
}

std::string describeEventParameter(const Parameter& p, const ObjectNameResolver& resolver) {
    auto nameOf = [&](uint16_t info, int16_t type) {
        std::string name = resolver ? resolver(info) : std::string();
        if (!name.empty()) return quoted(name);
        return std::string(objectTypeName(type));
    };
    switch (p.kind) {
        case Parameter::Kind::Object:
            return "object " + nameOf(p.objectInfo, p.objectType) +
                   " (objInfo " + std::to_string(p.objectInfo) + ")";
        case Parameter::Kind::Time: {
            std::string s = "timer " + std::to_string(p.value) + " (" +
                            ticksToSeconds(p.value) + ")";
            if (p.value2 != 0) s += " loops " + std::to_string(p.value2);
            return s;
        }
        case Parameter::Kind::Every: {
            std::string s = "every " + std::to_string(p.value) + " (" +
                            ticksToSeconds(p.value) + ")";
            if (p.value2 != 0) s += " counter " + std::to_string(p.value2);
            return s;
        }
        case Parameter::Kind::Short:
        case Parameter::Kind::Int:
            return "value " + std::to_string(p.value);
        case Parameter::Kind::Sample: {
            std::string s = "sample #" + std::to_string(p.sampleHandle);
            if (!p.str.empty()) s += " " + quoted(p.str);
            return s;
        }
        case Parameter::Kind::Create:
            return "create x" + std::to_string(p.createInstances) + " " +
                   nameOf(p.createObjectInfo, 2);
        case Parameter::Kind::Key: {
            std::string s = "key " + std::to_string(p.value);
            if (const char* k = virtualKeyName(static_cast<int>(p.value))) s += " (" + std::string(k) + ")";
            return s;
        }
        case Parameter::Kind::Expression:
            if (p.expression)
                return "expr " + std::string(p.expression->operatorString()) + ": " +
                       p.expression->describe();
            return "expr ?";
        case Parameter::Kind::Position: {
            std::string s = "pos (" + std::to_string(p.position.x) + "," +
                            std::to_string(p.position.y) + ")";
            if (p.position.parentInfo != 0)
                s += " rel " + nameOf(p.position.parentInfo, p.position.typeParent);
            return s;
        }
        case Parameter::Kind::Shoot:
            return "shoot " + nameOf(p.shootObjectInfo, 2) + " speed " +
                   std::to_string(p.shootSpeed);
        case Parameter::Kind::Zone:
            return "zone (" + std::to_string(p.zone[0]) + "," + std::to_string(p.zone[1]) +
                   ")-(" + std::to_string(p.zone[2]) + "," + std::to_string(p.zone[3]) + ")";
        case Parameter::Kind::Colour: {
            char buf[48];
            std::snprintf(buf, sizeof(buf), "color RGB(%u,%u,%u)",
                          p.colour[0], p.colour[1], p.colour[2]);
            return buf;
        }
        case Parameter::Kind::Click: {
            std::string s = "click button " + std::to_string(p.value);
            if (p.value2 != 0) s += " (double)";
            return s;
        }
        case Parameter::Kind::Program:
            return "program " + quoted(p.programCommand.empty() ? p.str : p.programCommand);
        case Parameter::Kind::Filename:
        case Parameter::Kind::String:
            return quoted(p.str);
        case Parameter::Kind::GlobalValue:
            return "global[" + std::to_string(p.value) + "]";
        case Parameter::Kind::AlterableValue:
            return "alterable[" + std::to_string(p.value) + "]";
        case Parameter::Kind::TwoShorts:
            return "pair (" + std::to_string(p.value) + ", " + std::to_string(p.value2) + ")";
        case Parameter::Kind::Extension: {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "extension type=%d code=%d (%d bytes)",
                          p.extType, p.extCode, static_cast<int>(p.extData.size()));
            return buf;
        }
        case Parameter::Kind::Group: {
            std::string s = "group " + quoted(p.groupName);
            if (p.groupId != 0) s += " id " + std::to_string(p.groupId);
            if (p.groupFlags != 0) s += " flags " + std::to_string(p.groupFlags);
            return s;
        }
        case Parameter::Kind::GroupPointer: {
            std::string s = "group ptr " + std::to_string(p.groupPointer);
            if (p.groupPointerId != 0) s += " id " + std::to_string(p.groupPointerId);
            return s;
        }
        case Parameter::Kind::MultipleVariables: {
            std::string s = "vars[" + std::to_string(p.multivars.size()) + "]";
            for (size_t i = 0; i < p.multivars.size() && i < 8; ++i) {
                s += i == 0 ? " " : ", ";
                s += std::to_string(p.multivars[i].index) + "=" +
                     std::to_string(p.multivars[i].isDouble ? static_cast<int64_t>(p.multivars[i].doubleValue)
                                                            : p.multivars[i].intValue);
            }
            return s;
        }
        case Parameter::Kind::ChildEvent:
            return "child events (" + std::to_string(p.childCount) + " objects)";
        default: {
            char buf[48];
            std::snprintf(buf, sizeof(buf), "param code=%d", p.code);
            return buf;
        }
    }
}

// ------------------------------------------------------------------- the tool

namespace {

std::string flagsSuffix(int8_t flags, int8_t otherFlags) {
    if (flags == 0 && otherFlags == 0) return "";
    char buf[48];
    std::snprintf(buf, sizeof(buf), "  [flags=0x%02X other=0x%02X]",
                  static_cast<uint8_t>(flags), static_cast<uint8_t>(otherFlags));
    return buf;
}

std::string listingForFrame(const Frame& frame, int index, const ObjectNameResolver& resolver) {
    std::string out;
    out += "========================================================================\n";
    out += "Frame " + std::to_string(index) + " " + quoted(frame.name) +
           "   " + std::to_string(frame.width) + "x" + std::to_string(frame.height) +
           "   groups: " + std::to_string(frame.events->groups.size()) + "\n";
    out += "========================================================================\n";

    size_t gid = 0;
    for (const auto& g : frame.events->groups) {
        ++gid;
        std::string header = "[Group " + std::to_string(gid);
        if (g.identifier != 0) header += " / id " + std::to_string(g.identifier);
        header += "]";
        if (g.line > 0) header += "  line " + std::to_string(g.line);
        out += "\n" + header + "\n";
        for (const auto& c : g.conditions) {
            out += "  ON  " + eventObjectLabel(c.objectType, c.objectInfo, resolver) +
                   "  cond #" + std::to_string(c.num) +
                   flagsSuffix(c.flags, c.otherFlags) + "\n";
            for (const auto& p : c.parameters)
                out += "      - " + describeEventParameter(p, resolver) + "\n";
        }
        for (const auto& a : g.actions) {
            out += "  DO  " + eventObjectLabel(a.objectType, a.objectInfo, resolver) +
                   "  act #" + std::to_string(a.num) +
                   flagsSuffix(a.flags, a.otherFlags) + "\n";
            for (const auto& p : a.parameters)
                out += "      - " + describeEventParameter(p, resolver) + "\n";
        }
    }
    return out;
}

} // namespace

void EventsListing::execute(GameData& game) {
    std::string dir = pathJoin(g_dumpsDir,
                               sanitizeGameName(game.name.empty() ? "Unknown Game" : game.name),
                               "Events");
    createDirectories(dir);
    Logger::log("Listing events...");

    ObjectNameResolver resolver = [&game](int handle) -> std::string {
        auto it = game.frameItems.find(handle);
        return it != game.frameItems.end() ? it->second->name : std::string();
    };

    std::string all;
    size_t frames = 0, groups = 0;
    int index = 0;
    for (const auto& frame : game.frames) {
        if (!frame || !frame->events) { ++index; continue; }
        std::string listing = listingForFrame(*frame, index, resolver);
        std::string base = std::to_string(index) + "_" +
                           (frame->name.empty() ? "unnamed" : clearName(frame->name));
        writeFileBytes(pathJoin(dir, "frame_" + base + "_events.txt"),
                       std::vector<uint8_t>(listing.begin(), listing.end()));
        all += listing + "\n";
        groups += frame->events->groups.size();
        ++frames;
        ++index;
    }
    writeFileBytes(pathJoin(dir, "ALL_EVENTS.txt"),
                   std::vector<uint8_t>(all.begin(), all.end()));
    Logger::log("Events listing done (" + std::to_string(frames) + " frames, " +
                std::to_string(groups) + " groups) -> " + dir);
}

} // namespace ctfak
