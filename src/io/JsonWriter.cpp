#include "JsonWriter.h"
#include <cmath>
#include <cstdio>
#include <sstream>

namespace ctfak {

JsonPtr JsonValue::makeNull() { return std::make_shared<JsonValue>(); }
JsonPtr JsonValue::makeBool(bool v) {
    auto j = std::make_shared<JsonValue>();
    j->type = Type::Bool;
    j->boolValue = v;
    return j;
}
JsonPtr JsonValue::makeInt(int64_t v) {
    auto j = std::make_shared<JsonValue>();
    j->type = Type::Int;
    j->intValue = v;
    return j;
}
JsonPtr JsonValue::makeDouble(double v) {
    auto j = std::make_shared<JsonValue>();
    j->type = Type::Double;
    j->doubleValue = v;
    return j;
}
JsonPtr JsonValue::makeString(const std::string& v) {
    auto j = std::make_shared<JsonValue>();
    j->type = Type::String;
    j->stringValue = v;
    return j;
}
JsonPtr JsonValue::makeArray() {
    auto j = std::make_shared<JsonValue>();
    j->type = Type::Array;
    return j;
}
JsonPtr JsonValue::makeObject() {
    auto j = std::make_shared<JsonValue>();
    j->type = Type::Object;
    return j;
}

JsonPtr& JsonValue::operator[](const std::string& key) {
    for (auto& kv : members)
        if (kv.first == key) return kv.second;
    members.emplace_back(key, makeNull());
    return members.back().second;
}

void JsonValue::set(const std::string& key, JsonPtr v) {
    for (auto& kv : members) {
        if (kv.first == key) {
            kv.second = std::move(v);
            return;
        }
    }
    members.emplace_back(key, std::move(v));
}

void JsonValue::escape(const std::string& s, std::string& out) {
    for (unsigned char c : s) {
        switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
}

void JsonValue::serialize(std::string& out, int indent) const {
    std::string pad(indent * 2, ' ');
    std::string padInner((indent + 1) * 2, ' ');
    switch (type) {
        case Type::Null: out += "null"; break;
        case Type::Bool: out += boolValue ? "true" : "false"; break;
        case Type::Int: out += std::to_string(intValue); break;
        case Type::Double: {
            if (std::isfinite(doubleValue)) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%.17g", doubleValue);
                out += buf;
            } else {
                out += "null";
            }
            break;
        }
        case Type::String:
            out += '"';
            escape(stringValue, out);
            out += '"';
            break;
        case Type::Array: {
            out += "[";
            if (!array.empty()) {
                out += "\n";
                for (size_t i = 0; i < array.size(); ++i) {
                    out += padInner;
                    if (array[i]) array[i]->serialize(out, indent + 1);
                    else out += "null";
                    if (i + 1 < array.size()) out += ",";
                    out += "\n";
                }
                out += pad;
            }
            out += "]";
            break;
        }
        case Type::Object: {
            out += "{";
            if (!members.empty()) {
                out += "\n";
                for (size_t i = 0; i < members.size(); ++i) {
                    out += padInner;
                    out += '"';
                    escape(members[i].first, out);
                    out += "\": ";
                    if (members[i].second) members[i].second->serialize(out, indent + 1);
                    else out += "null";
                    if (i + 1 < members.size()) out += ",";
                    out += "\n";
                }
                out += pad;
            }
            out += "}";
            break;
        }
    }
}

} // namespace ctfak
