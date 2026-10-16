// Unit tests for the Events Listing helpers: parameter descriptions,
// object labels, virtual key names.
#include "test_framework.h"
#include "plugins/EventsListing.h"
#include "format/Events.h"
#include <functional>
#include <map>
#include <memory>
#include <string>

using namespace ctfak;

namespace {

const ObjectNameResolver& testResolver() {
    static std::map<int, std::string> names = {{2, "static"}, {5, "new game"}, {44, "Freddy"}};
    static ObjectNameResolver r = [](int h) -> std::string {
        auto it = names.find(h);
        return it != names.end() ? it->second : std::string();
    };
    return r;
}

Parameter makeParam(Parameter::Kind kind, int16_t code) {
    Parameter p;
    p.kind = kind;
    p.code = code;
    return p;
}

} // namespace

TEST(events_describe_time) {
    Parameter p = makeParam(Parameter::Kind::Time, 2);
    p.value = 90;
    // v2.72: CFT event timer parameters are MILLISECONDS (proof: F1 AI
    // intervals 4970/5010 = the datamined 4.97/5.01 s) — the old /50
    // annotation read them 20x too long.
    EXPECT_EQ(describeEventParameter(p, testResolver()), std::string("timer 90 (~0.09s)"));
    p.value2 = 3;
    EXPECT_EQ(describeEventParameter(p, testResolver()), std::string("timer 90 (~0.09s) loops 3"));
}

TEST(events_describe_every) {
    Parameter p = makeParam(Parameter::Kind::Every, 13);
    p.value = 50;
    EXPECT_EQ(describeEventParameter(p, testResolver()), std::string("every 50 (~0.05s)"));
}

TEST(events_describe_values) {
    Parameter p = makeParam(Parameter::Kind::Int, 25);
    p.value = -42;
    EXPECT_EQ(describeEventParameter(p, testResolver()), std::string("value -42"));

    Parameter g = makeParam(Parameter::Kind::GlobalValue, 49);
    g.value = 7;
    EXPECT_EQ(describeEventParameter(g, testResolver()), std::string("global[7]"));

    Parameter a = makeParam(Parameter::Kind::AlterableValue, 50);
    a.value = 11;
    EXPECT_EQ(describeEventParameter(a, testResolver()), std::string("alterable[11]"));
}

TEST(events_describe_key) {
    Parameter p = makeParam(Parameter::Kind::Key, 14);
    p.value = 13;
    EXPECT_EQ(describeEventParameter(p, testResolver()), std::string("key 13 (Enter)"));
    p.value = 32;
    EXPECT_EQ(describeEventParameter(p, testResolver()), std::string("key 32 (Space)"));
    p.value = 37;
    EXPECT_EQ(describeEventParameter(p, testResolver()), std::string("key 37 (Left)"));
    p.value = 200; // unmapped code -> number only
    EXPECT_EQ(describeEventParameter(p, testResolver()), std::string("key 200"));
}

TEST(events_describe_string_sample) {
    Parameter s = makeParam(Parameter::Kind::String, 41);
    s.str = "hello\nworld";
    EXPECT_EQ(describeEventParameter(s, testResolver()), std::string("\"hello world\""));

    Parameter sm = makeParam(Parameter::Kind::Sample, 6);
    sm.sampleHandle = 12;
    sm.str = "Buzz_Fan.wav";
    EXPECT_EQ(describeEventParameter(sm, testResolver()), std::string("sample #12 \"Buzz_Fan.wav\""));
}

TEST(events_describe_zone_colour_click) {
    Parameter z = makeParam(Parameter::Kind::Zone, 19);
    z.zone[0] = 10; z.zone[1] = 20; z.zone[2] = 30; z.zone[3] = 40;
    EXPECT_EQ(describeEventParameter(z, testResolver()), std::string("zone (10,20)-(30,40)"));

    Parameter c = makeParam(Parameter::Kind::Colour, 24);
    c.colour[0] = 255; c.colour[1] = 128; c.colour[2] = 0;
    EXPECT_EQ(describeEventParameter(c, testResolver()), std::string("color RGB(255,128,0)"));

    Parameter cl = makeParam(Parameter::Kind::Click, 32);
    cl.value = 1;
    EXPECT_EQ(describeEventParameter(cl, testResolver()), std::string("click button 1"));
    cl.value2 = 1;
    EXPECT_EQ(describeEventParameter(cl, testResolver()), std::string("click button 1 (double)"));
}

TEST(events_describe_object_expression) {
    Parameter o = makeParam(Parameter::Kind::Object, 1);
    o.objectInfo = 5;
    o.objectType = 2;
    EXPECT_EQ(describeEventParameter(o, testResolver()),
              std::string("object \"new game\" (objInfo 5)"));
    o.objectInfo = 77;
    EXPECT_EQ(describeEventParameter(o, testResolver()),
              std::string("object Active (objInfo 77)"));

    Parameter e = makeParam(Parameter::Kind::Expression, 22);
    e.expression = std::make_shared<ExpressionParameter>();
    ExpressionItem num;
    num.kind = ExpressionItem::Kind::Long;
    num.longValue = 100;
    e.expression->items.push_back(num);
    EXPECT_EQ(describeEventParameter(e, testResolver()), std::string("expr ==: 100"));
}

TEST(events_object_labels) {
    EXPECT_EQ(eventObjectLabel(-4, 0, testResolver()), std::string("Timer"));
    EXPECT_EQ(eventObjectLabel(-1, 0, testResolver()), std::string("System"));
    EXPECT_EQ(eventObjectLabel(2, 2, testResolver()), std::string("\"static\" (Active)"));
    EXPECT_EQ(eventObjectLabel(7, 99, testResolver()), std::string("Counter objInfo=99"));
}

TEST(events_virtual_key_names) {
    EXPECT_EQ(std::string(virtualKeyName(0x0D)), std::string("Enter"));
    EXPECT_EQ(std::string(virtualKeyName(0x1B)), std::string("Escape"));
    EXPECT_EQ(std::string(virtualKeyName(0x41)), std::string("A"));
    EXPECT_EQ(std::string(virtualKeyName(0x70)), std::string("F1"));
    EXPECT_EQ(std::string(virtualKeyName(0x7B)), std::string("F12"));
    EXPECT_EQ(std::string(virtualKeyName(0x25)), std::string("Left"));
    EXPECT_TRUE(virtualKeyName(9999) == nullptr);
}
