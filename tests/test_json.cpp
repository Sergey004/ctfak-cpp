#include "test_framework.h"
#include "io/JsonWriter.h"

TEST(json_scalars) {
    auto obj = ctfak::JsonValue::makeObject();
    obj->set("int", ctfak::JsonValue::makeInt(42));
    obj->set("neg", ctfak::JsonValue::makeInt(-7));
    obj->set("bool", ctfak::JsonValue::makeBool(true));
    obj->set("dbl", ctfak::JsonValue::makeDouble(1.5));
    obj->set("null", ctfak::JsonValue::makeNull());

    auto dumped = obj->dump();
    EXPECT_TRUE(dumped.find("\"int\": 42") != std::string::npos);
    EXPECT_TRUE(dumped.find("\"neg\": -7") != std::string::npos);
    EXPECT_TRUE(dumped.find("\"bool\": true") != std::string::npos);
    EXPECT_TRUE(dumped.find("\"dbl\": 1.5") != std::string::npos);
    EXPECT_TRUE(dumped.find("\"null\": null") != std::string::npos);
}

TEST(json_string_escaping) {
    auto obj = ctfak::JsonValue::makeObject();
    obj->set("s", ctfak::JsonValue::makeString(std::string("a\"b\\c\nd\te")));
    auto dumped = obj->dump();
    EXPECT_TRUE(dumped.find("\"s\": \"a\\\"b\\\\c\\nd\\te\"") != std::string::npos);
}

TEST(json_nested_arrays) {
    auto arr = ctfak::JsonValue::makeArray();
    for (int i = 0; i < 3; ++i) {
        auto inner = ctfak::JsonValue::makeObject();
        inner->set("v", ctfak::JsonValue::makeInt(i));
        arr->push(std::move(inner));
    }
    auto root = ctfak::JsonValue::makeObject();
    root->set("items", std::move(arr));
    auto dumped = root->dump();
    EXPECT_TRUE(dumped.find("\"v\": 0") != std::string::npos);
    EXPECT_TRUE(dumped.find("\"v\": 2") != std::string::npos);
}
