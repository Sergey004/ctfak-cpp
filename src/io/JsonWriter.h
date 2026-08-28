// Small JSON document writer (no external dependencies).
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace ctfak {

class JsonValue;
using JsonPtr = std::shared_ptr<JsonValue>;

class JsonValue {
public:
    enum class Type { Null, Bool, Int, Double, String, Array, Object };

    Type type = Type::Null;
    bool boolValue = false;
    int64_t intValue = 0;
    double doubleValue = 0;
    std::string stringValue;
    std::vector<JsonPtr> array;
    std::vector<std::pair<std::string, JsonPtr>> members; // preserves insertion order

    static JsonPtr makeNull();
    static JsonPtr makeBool(bool v);
    static JsonPtr makeInt(int64_t v);
    static JsonPtr makeDouble(double v);
    static JsonPtr makeString(const std::string& v);
    static JsonPtr makeArray();
    static JsonPtr makeObject();

    JsonPtr& operator[](const std::string& key);              // object member access (creates)
    void set(const std::string& key, JsonPtr v);              // object member assign
    void push(JsonPtr v) { array.push_back(std::move(v)); }   // array append

    void serialize(std::string& out, int indent = 0) const;
    std::string dump() const { std::string s; serialize(s); return s; }

private:
    static void escape(const std::string& s, std::string& out);
};

} // namespace ctfak
