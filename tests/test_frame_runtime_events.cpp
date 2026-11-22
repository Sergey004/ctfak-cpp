#include "test_framework.h"
#include "plugins/FrameRuntimeEvents.h"
#include "plugins/FrameRuntime.h"
#include "format/Events.h"
#include "format/Frame.h"
#include "format/GameData.h"
#include "format/ObjectInfo.h"
#include <memory>
#include <vector>

using namespace ctfak;

namespace {

ObjectInfo makeObject(int16_t objectType = 2, bool visible = true) {
    ObjectInfo obj;
    obj.handle = objectType;
    obj.objectType = objectType;
    obj.blend = 255;
    obj.rgbCoeff = {255, 255, 255, 255};
    obj.common = std::make_shared<ObjectCommon>();
    obj.common->flags.setBit("DoNotCreateAtStart", false);
    obj.common->newFlags.setBit("VisibleAtStart", visible);
    return obj;
}

std::shared_ptr<ObjectInfo> addObject(GameData& game, int objectInfo,
                                      int16_t objectType, bool visible = true,
                                      std::vector<int32_t> alterables = {}) {
    auto obj = std::make_shared<ObjectInfo>(makeObject(objectType, visible));
    obj->handle = static_cast<int16_t>(objectInfo);
    obj->common->alterableValues = std::move(alterables);
    game.frameItems[objectInfo] = obj;
    return obj;
}

ObjectInstance addInstance(Frame& frame, int handle, int objectInfo,
                           int x, int y, int layer) {
    ObjectInstance inst;
    inst.handle = static_cast<uint16_t>(handle);
    inst.objectInfo = static_cast<uint16_t>(objectInfo);
    inst.x = x;
    inst.y = y;
    inst.layer = static_cast<int16_t>(layer);
    frame.objects.push_back(inst);
    return frame.objects.back();
}

Condition makeCondition(int16_t objectType, int16_t num, uint16_t objectInfo,
                        std::vector<Parameter> params = {},
                        int8_t otherFlags = 0) {
    Condition cond;
    cond.objectType = objectType;
    cond.num = num;
    cond.objectInfo = objectInfo;
    cond.otherFlags = otherFlags;
    cond.parameters = std::move(params);
    return cond;
}

Action makeAction(int16_t objectType, int16_t num, uint16_t objectInfo,
                  std::vector<Parameter> params = {}) {
    Action action;
    action.objectType = objectType;
    action.num = num;
    action.objectInfo = objectInfo;
    action.parameters = std::move(params);
    return action;
}

Parameter expressionParameter(int16_t comparison,
                              std::vector<ExpressionItem> items) {
    Parameter p;
    p.kind = Parameter::Kind::Expression;
    p.code = 23;
    p.expression = std::make_shared<ExpressionParameter>();
    p.expression->comparsion = comparison;
    p.expression->items = std::move(items);
    return p;
}

ExpressionItem constant(int32_t value) {
    ExpressionItem item;
    item.kind = ExpressionItem::Kind::Long;
    item.objectType = -1;
    item.num = 0;
    item.longValue = value;
    return item;
}

ExpressionItem op(int opcode) {
    ExpressionItem item;
    item.kind = ExpressionItem::Kind::Operator;
    item.objectType = 0;
    item.num = static_cast<int16_t>(opcode);
    item.longValue = opcode;
    return item;
}

ExpressionItem parenOpen() {
    ExpressionItem item;
    item.kind = ExpressionItem::Kind::ParenOpen;
    item.objectType = -1;
    item.num = 1;
    return item;
}

ExpressionItem parenClose() {
    ExpressionItem item;
    item.kind = ExpressionItem::Kind::ParenClose;
    item.objectType = -1;
    item.num = -2;
    return item;
}

Parameter alterableParameter(int index) {
    Parameter p;
    p.kind = Parameter::Kind::AlterableValue;
    p.code = 50;
    p.value = index;
    return p;
}

Parameter shortParameter(int value) {
    Parameter p;
    p.kind = Parameter::Kind::Short;
    p.code = 3;
    p.value = value;
    return p;
}

Parameter positionParameter(int parentInfo, int x, int y, int layer) {
    Parameter p;
    p.kind = Parameter::Kind::Position;
    p.code = 16;
    p.position.parentInfo = static_cast<uint16_t>(parentInfo);
    p.position.x = static_cast<int16_t>(x);
    p.position.y = static_cast<int16_t>(y);
    p.position.layer = static_cast<int16_t>(layer);
    return p;
}

EventGroup makeGroup(std::vector<Condition> conditions,
                     std::vector<Action> actions) {
    EventGroup group;
    group.conditions = std::move(conditions);
    group.actions = std::move(actions);
    return group;
}

EventGroup startGroup(std::vector<Action> actions) {
    return makeGroup({makeCondition(-3, -1, 0)}, std::move(actions));
}

Frame makeFrame() {
    Frame frame;
    frame.width = 320;
    frame.height = 180;
    frame.background = {0, 0, 0, 255};
    return frame;
}

GameData makeGame() {
    GameData game;
    game.header = std::make_unique<AppHeader>();
    game.header->windowWidth = 320;
    game.header->windowHeight = 180;
    return game;
}

} // namespace

TEST(frame_events_start_group_sets_counter) {
    Frame frame = makeFrame();
    GameData game = makeGame();
    auto counter = addObject(game, 7, 7);
    counter->common->hasCounterConfig = true;
    counter->common->counterInitial = 5;
    addInstance(frame, 1, 7, 0, 0, 0);
    frame.events = std::make_shared<EventsChunk>();
    frame.events->groups.push_back(startGroup({
        makeAction(7, 80, 7, {expressionParameter(0, {constant(999)})})
    }));

    auto state = evaluateFrameStartEvents(frame, game);
    EXPECT_EQ(state.groupsExecuted, 1);
    EXPECT_EQ(state.counters.at(7), 999);
}

TEST(frame_events_counter_operations) {
    Frame frame = makeFrame();
    GameData game = makeGame();
    addObject(game, 7, 7);
    addInstance(frame, 1, 7, 0, 0, 0);
    frame.events = std::make_shared<EventsChunk>();
    frame.events->groups.push_back(startGroup({
        makeAction(7, 80, 7, {expressionParameter(0, {constant(10)})}),
        makeAction(7, 81, 7, {expressionParameter(0, {constant(4)})}),
        makeAction(7, 82, 7, {expressionParameter(0, {constant(2)})})
    }));

    auto state = evaluateFrameStartEvents(frame, game);
    EXPECT_EQ(state.counters.at(7), 12);
}

TEST(frame_events_counter_condition_gates_group) {
    Frame frame = makeFrame();
    GameData game = makeGame();
    addObject(game, 7, 7);
    auto active = addObject(game, 2, 2, true, {7});
    (void)active;
    addInstance(frame, 1, 7, 0, 0, 0);
    addInstance(frame, 2, 2, 10, 20, 0);
    frame.events = std::make_shared<EventsChunk>();
    frame.events->groups.push_back(makeGroup(
        {makeCondition(7, -81, 7, {expressionParameter(0, {constant(11)})})},
        {makeAction(2, 31, 2, {alterableParameter(0),
                               expressionParameter(0, {constant(99)})})}
    ));

    auto state = evaluateFrameStartEvents(frame, game);
    EXPECT_EQ(state.groupsFalse, 1);
    EXPECT_EQ(state.objects[1].alterables[0], 7);
}

TEST(frame_events_alterable_condition_and_set) {
    Frame frame = makeFrame();
    GameData game = makeGame();
    addObject(game, 2, 2, true, {1});
    addInstance(frame, 1, 2, 0, 0, 0);
    frame.events = std::make_shared<EventsChunk>();
    frame.events->groups.push_back(makeGroup(
        {makeCondition(2, -27, 2, {alterableParameter(0),
                                   expressionParameter(0, {constant(1)})})},
        {makeAction(2, 31, 2, {alterableParameter(0),
                               expressionParameter(0, {constant(9)})})}
    ));

    auto state = evaluateFrameStartEvents(frame, game);
    EXPECT_EQ(state.groupsExecuted, 1);
    EXPECT_EQ(state.objects[0].alterables[0], 9);
}

TEST(frame_events_show_hides_serialized_state) {
    Frame frame = makeFrame();
    GameData game = makeGame();
    addObject(game, 2, 2, true);
    addObject(game, 4, 2, false);
    addInstance(frame, 1, 2, 0, 0, 0);
    addInstance(frame, 2, 4, 0, 0, 0);
    frame.events = std::make_shared<EventsChunk>();
    frame.events->groups.push_back(startGroup({
        makeAction(2, 26, 2),
        makeAction(2, 27, 4)
    }));

    auto state = evaluateFrameStartEvents(frame, game);
    EXPECT_TRUE(!state.objects[0].visible);
    EXPECT_TRUE(state.objects[1].visible);
}

TEST(frame_events_do_not_create_stays_non_renderable) {
    Frame frame = makeFrame();
    GameData game = makeGame();
    auto obj = addObject(game, 2, 2, false);
    obj->common->flags.setBit("DoNotCreateAtStart", true);
    addInstance(frame, 1, 2, 0, 0, 0);
    frame.events = std::make_shared<EventsChunk>();
    frame.events->groups.push_back(startGroup({makeAction(2, 27, 2)}));

    auto state = evaluateFrameStartEvents(frame, game);
    EXPECT_TRUE(state.objects[0].visible);
    EXPECT_TRUE(!state.objects[0].created);
}

TEST(frame_events_relative_position) {
    Frame frame = makeFrame();
    GameData game = makeGame();
    addObject(game, 3, 2);
    addObject(game, 5, 2);
    addInstance(frame, 1, 3, 100, 50, 0);
    addInstance(frame, 2, 5, 0, 0, 1);
    frame.events = std::make_shared<EventsChunk>();
    frame.events->groups.push_back(startGroup({
        makeAction(2, 1, 5, {positionParameter(3, 5, -2, 1)})
    }));

    auto state = evaluateFrameStartEvents(frame, game);
    EXPECT_EQ(state.objects[1].x, 105);
    EXPECT_EQ(state.objects[1].y, 48);
    EXPECT_EQ(state.objects[1].layer, 1);
}

TEST(frame_events_animation_state_selects_frame) {
    Frame frame = makeFrame();
    GameData game = makeGame();
    auto obj = addObject(game, 2, 2);
    obj->common->animations.items[2][7].frames = {22};
    addInstance(frame, 1, 2, 0, 0, 0);
    frame.events = std::make_shared<EventsChunk>();
    frame.events->groups.push_back(startGroup({
        makeAction(2, 17, 2, {shortParameter(2)})
    }));

    auto state = evaluateFrameStartEvents(frame, game);
    EXPECT_EQ(state.objects[0].animation, 2);
    EXPECT_EQ(resolveRuntimeStartImage(*game.frameItems[2], 2, 7), 22);
}

TEST(frame_events_alpha_override) {
    Frame frame = makeFrame();
    GameData game = makeGame();
    addObject(game, 2, 2);
    addInstance(frame, 1, 2, 0, 0, 0);
    frame.events = std::make_shared<EventsChunk>();
    frame.events->groups.push_back(startGroup({
        makeAction(2, 65, 2, {expressionParameter(0, {constant(100)})})
    }));

    auto state = evaluateFrameStartEvents(frame, game);
    EXPECT_TRUE(state.objects[0].alphaOverride);
    EXPECT_EQ(state.objects[0].alphaCoefficient, 100);
}

TEST(frame_events_expression_operators_and_parentheses) {
    Frame frame = makeFrame();
    GameData game = makeGame();
    addObject(game, 7, 7);
    addInstance(frame, 1, 7, 0, 0, 0);
    frame.events = std::make_shared<EventsChunk>();
    frame.events->groups.push_back(startGroup({
        makeAction(7, 80, 7, {expressionParameter(0, {
            parenOpen(), constant(2), op(2), constant(3), parenClose(),
            op(8), constant(4)
        })})
    }));

    auto state = evaluateFrameStartEvents(frame, game);
    EXPECT_EQ(state.counters.at(7), 20);
}

TEST(frame_events_unsupported_group_does_not_partially_apply) {
    Frame frame = makeFrame();
    GameData game = makeGame();
    addObject(game, 2, 2, true, {1});
    addInstance(frame, 1, 2, 0, 0, 0);
    frame.events = std::make_shared<EventsChunk>();
    frame.events->groups.push_back(makeGroup(
        {makeCondition(-3, -1, 0)},
        {makeAction(2, 31, 2, {alterableParameter(0),
                               expressionParameter(0, {constant(8)})}),
         makeAction(2, 99, 2)}
    ));

    auto state = evaluateFrameStartEvents(frame, game);
    EXPECT_EQ(state.groupsSkippedUnsupported, 1);
    EXPECT_EQ(state.objects[0].alterables[0], 1);
    EXPECT_TRUE(state.notes.find("unsupported action (2,99)") != std::string::npos);
}

TEST(frame_events_not_condition_inverts_active_compare) {
    Frame frame = makeFrame();
    GameData game = makeGame();
    addObject(game, 2, 2, true, {0});
    addInstance(frame, 1, 2, 0, 0, 0);
    addInstance(frame, 2, 2, 10, 0, 0);
    frame.events = std::make_shared<EventsChunk>();
    frame.events->groups.push_back(makeGroup(
        {makeCondition(2, -27, 2, {alterableParameter(0),
                                   expressionParameter(0, {constant(999)})}, 1)},
        {makeAction(2, 31, 2, {alterableParameter(0),
                               expressionParameter(0, {constant(5)})})}
    ));

    auto state = evaluateFrameStartEvents(frame, game);
    EXPECT_EQ(state.groupsExecuted, 1);
    EXPECT_EQ(state.objects[0].alterables[0], 5);
    EXPECT_EQ(state.objects[1].alterables[0], 5);
}

// v2.98: input/timer/audio-state conditions are deterministically FALSE at
// the serialized first tick; negated variants are TRUE and the group runs.

TEST(frame_events_input_conditions_false_at_first_tick) {
    Frame frame = makeFrame();
    GameData game = makeGame();
    addObject(game, 2, 2, true, {1});
    addInstance(frame, 1, 2, 0, 0, 0);
    frame.events = std::make_shared<EventsChunk>();
    // key pressed (-6,-1), key released (-6,-2), mouse click on object
    // (-6,-7) and mouse over object (-6,-4): all false at tick 0
    frame.events->groups.push_back(makeGroup(
        {makeCondition(-6, -1, 0, {shortParameter(13)})},
        {makeAction(2, 31, 2, {alterableParameter(0),
                               expressionParameter(0, {constant(7)})})}
    ));
    frame.events->groups.push_back(makeGroup(
        {makeCondition(-6, -2, 0, {shortParameter(87)})},
        {makeAction(2, 31, 2, {alterableParameter(0),
                               expressionParameter(0, {constant(8)})})}
    ));
    frame.events->groups.push_back(makeGroup(
        {makeCondition(-6, -7, 0, {shortParameter(0)})},
        {makeAction(2, 31, 2, {alterableParameter(0),
                               expressionParameter(0, {constant(9)})})}
    ));
    frame.events->groups.push_back(makeGroup(
        {makeCondition(-6, -4, 0)},
        {makeAction(2, 31, 2, {alterableParameter(0),
                               expressionParameter(0, {constant(10)})})}
    ));

    auto state = evaluateFrameStartEvents(frame, game);
    EXPECT_EQ(state.groupsExecuted, 0);
    EXPECT_EQ(state.groupsFalse, 4);
    EXPECT_EQ(state.groupsSkippedUnsupported, 0);
    EXPECT_EQ(state.objects[0].alterables[0], 1);
}

TEST(frame_events_negated_input_condition_runs_group) {
    Frame frame = makeFrame();
    GameData game = makeGame();
    addObject(game, 2, 2, true, {1});
    addInstance(frame, 1, 2, 0, 0, 0);
    frame.events = std::make_shared<EventsChunk>();
    frame.events->groups.push_back(makeGroup(
        {makeCondition(-6, -2, 0, {shortParameter(87)}, 1)},  // NOT key released
        {makeAction(2, 31, 2, {alterableParameter(0),
                               expressionParameter(0, {constant(6)})})}
    ));

    auto state = evaluateFrameStartEvents(frame, game);
    EXPECT_EQ(state.groupsExecuted, 1);
    EXPECT_EQ(state.objects[0].alterables[0], 6);
}

TEST(frame_events_timer_conditions_false_at_first_tick) {
    Frame frame = makeFrame();
    GameData game = makeGame();
    addObject(game, 2, 2, true, {1});
    addInstance(frame, 1, 2, 0, 0, 0);
    frame.events = std::make_shared<EventsChunk>();
    // "every N units" (-4,-8) and "after N" (-4,-1): nothing elapsed at
    // the first tick
    frame.events->groups.push_back(makeGroup(
        {makeCondition(-4, -8, 0, {shortParameter(1000), shortParameter(0)})},
        {makeAction(2, 31, 2, {alterableParameter(0),
                               expressionParameter(0, {constant(3)})})}
    ));
    frame.events->groups.push_back(makeGroup(
        {makeCondition(-4, -1, 0, {shortParameter(120000)})},
        {makeAction(2, 31, 2, {alterableParameter(0),
                               expressionParameter(0, {constant(4)})})}
    ));

    auto state = evaluateFrameStartEvents(frame, game);
    EXPECT_EQ(state.groupsExecuted, 0);
    EXPECT_EQ(state.groupsFalse, 2);
    EXPECT_EQ(state.groupsSkippedUnsupported, 0);
    EXPECT_EQ(state.objects[0].alterables[0], 1);
}

TEST(frame_events_negated_timer_condition_runs_group) {
    Frame frame = makeFrame();
    GameData game = makeGame();
    addObject(game, 2, 2, true, {1});
    addInstance(frame, 1, 2, 0, 0, 0);
    frame.events = std::make_shared<EventsChunk>();
    frame.events->groups.push_back(makeGroup(
        {makeCondition(-4, -8, 0, {shortParameter(1000), shortParameter(0)}, 1)},
        {makeAction(2, 31, 2, {alterableParameter(0),
                               expressionParameter(0, {constant(2)})})}
    ));

    auto state = evaluateFrameStartEvents(frame, game);
    EXPECT_EQ(state.groupsExecuted, 1);
    EXPECT_EQ(state.objects[0].alterables[0], 2);
}

TEST(frame_events_speaker_and_player_conditions_false_at_first_tick) {
    Frame frame = makeFrame();
    GameData game = makeGame();
    addObject(game, 2, 2, true, {1});
    addInstance(frame, 1, 2, 0, 0, 0);
    frame.events = std::make_shared<EventsChunk>();
    frame.events->groups.push_back(makeGroup(
        {makeCondition(-2, -1, 0, {shortParameter(170)})},   // sample playing
        {makeAction(2, 31, 2, {alterableParameter(0),
                               expressionParameter(0, {constant(5)})})}
    ));
    frame.events->groups.push_back(makeGroup(
        {makeCondition(-7, -4, 0, {shortParameter(16)})},    // player input
        {makeAction(2, 31, 2, {alterableParameter(0),
                               expressionParameter(0, {constant(6)})})}
    ));

    auto state = evaluateFrameStartEvents(frame, game);
    EXPECT_EQ(state.groupsExecuted, 0);
    EXPECT_EQ(state.groupsFalse, 2);
    EXPECT_EQ(state.groupsSkippedUnsupported, 0);
    EXPECT_EQ(state.objects[0].alterables[0], 1);
}

TEST(frame_events_counter_without_config_defaults_zero_and_hides) {
    Frame frame = makeFrame();
    GameData game = makeGame();
    addObject(game, 48, 7);          // "viewing" counter — no counterConfig
    addObject(game, 149, 2);         // "Active 20" (Mangle)
    addInstance(frame, 1, 149, 637, 0, 0);
    frame.events = std::make_shared<EventsChunk>();
    frame.events->groups.push_back(makeGroup(
        {makeCondition(7, -81, 48, {expressionParameter(0, {constant(0)})})},
        {makeAction(2, 26, 149)}
    ));

    auto state = evaluateFrameStartEvents(frame, game);
    EXPECT_EQ(state.groupsExecuted, 1);
    EXPECT_TRUE(!state.objects[0].visible);
}

TEST(frame_events_chain_subjects_collected) {
    Frame frame = makeFrame();
    GameData game = makeGame();
    addObject(game, 2, 2, true, {1});
    addInstance(frame, 1, 2, 0, 0, 0);
    frame.events = std::make_shared<EventsChunk>();
    // "animation sequence 0 finished" — the object is a chain subject
    frame.events->groups.push_back(makeGroup(
        {makeCondition(2, -2, 2, {shortParameter(0)})},
        {makeAction(2, 24, 2)}
    ));
    frame.events->groups.push_back(makeGroup(
        {makeCondition(7, -81, 7, {expressionParameter(0, {constant(0)})})},
        {}
    ));

    auto state = evaluateFrameStartEvents(frame, game);
    EXPECT_TRUE(state.chainSubjects.count(2) != 0);
    EXPECT_TRUE(state.chainSubjects.count(7) == 0);
}

// v2.100: (2,-4) is the Active object's collision-with-object condition.
// FNAF4 positions its characters with it ('plushtrap' collides with the
// 'in chair'/'hall stage N' zone objects), so the evaluator computes a real
// AABB overlap from the serialized positions and the start images.

namespace {

Parameter objectParameter(uint16_t objectInfo) {
    Parameter p;
    p.kind = Parameter::Kind::Object;
    p.code = 1;
    p.objectInfo = objectInfo;
    return p;
}

void addBackdropImage(GameData& game, int objectInfo, int32_t handle,
                      int w, int h, int hotX, int hotY) {
    auto it = game.frameItems.find(objectInfo);
    if (it == game.frameItems.end()) return;
    auto backdrop = std::make_unique<Backdrop>();
    backdrop->image = handle;
    it->second->backdrop = std::move(backdrop);
    auto img = std::make_shared<ImageItem>();
    img->handle = handle;
    img->width = w;
    img->height = h;
    img->hotX = static_cast<int16_t>(hotX);
    img->hotY = static_cast<int16_t>(hotY);
    game.images.items[handle] = img;
}

} // namespace

TEST(frame_events_collision_condition_overlap_selects) {
    Frame frame = makeFrame();
    GameData game = makeGame();
    addObject(game, 2, 2, true, {0});          // the mover ('plushtrap')
    addObject(game, 3, 2, true);               // the zone ('in chair')
    addInstance(frame, 1, 2, 100, 100, 0);
    addInstance(frame, 2, 3, 105, 105, 0);
    addBackdropImage(game, 2, 10, 20, 20, 10, 10);
    addBackdropImage(game, 3, 11, 40, 40, 20, 20);
    frame.events = std::make_shared<EventsChunk>();
    frame.events->groups.push_back(makeGroup(
        {makeCondition(2, -4, 2, {objectParameter(3)})},
        {makeAction(2, 31, 2, {alterableParameter(0),
                               expressionParameter(0, {constant(7)})})}
    ));

    auto state = evaluateFrameStartEvents(frame, game);
    EXPECT_EQ(state.groupsExecuted, 1);
    EXPECT_EQ(state.objects[0].alterables[0], 7);
}

TEST(frame_events_collision_condition_no_overlap_skips) {
    Frame frame = makeFrame();
    GameData game = makeGame();
    addObject(game, 2, 2, true, {0});
    addObject(game, 3, 2, true);
    addInstance(frame, 1, 2, 100, 100, 0);
    addInstance(frame, 2, 3, 500, 500, 0);
    addBackdropImage(game, 2, 10, 20, 20, 10, 10);
    addBackdropImage(game, 3, 11, 40, 40, 20, 20);
    frame.events = std::make_shared<EventsChunk>();
    frame.events->groups.push_back(makeGroup(
        {makeCondition(2, -4, 2, {objectParameter(3)})},
        {makeAction(2, 31, 2, {alterableParameter(0),
                               expressionParameter(0, {constant(7)})})}
    ));

    auto state = evaluateFrameStartEvents(frame, game);
    EXPECT_EQ(state.groupsExecuted, 0);
    EXPECT_EQ(state.objects[0].alterables[0], 0);
}

TEST(frame_events_collision_condition_negated_runs_without_overlap) {
    Frame frame = makeFrame();
    GameData game = makeGame();
    addObject(game, 2, 2, true, {0});
    addObject(game, 3, 2, true);
    addInstance(frame, 1, 2, 100, 100, 0);
    addInstance(frame, 2, 3, 500, 500, 0);
    addBackdropImage(game, 2, 10, 20, 20, 10, 10);
    addBackdropImage(game, 3, 11, 40, 40, 20, 20);
    frame.events = std::make_shared<EventsChunk>();
    frame.events->groups.push_back(makeGroup(
        {makeCondition(2, -4, 2, {objectParameter(3)}, 1)},  // NOT colliding
        {makeAction(2, 31, 2, {alterableParameter(0),
                               expressionParameter(0, {constant(9)})})}
    ));

    auto state = evaluateFrameStartEvents(frame, game);
    EXPECT_EQ(state.groupsExecuted, 1);
    EXPECT_EQ(state.objects[0].alterables[0], 9);
}

// v2.101: (2,-16) is the Y-position compare (the mirror of the X compare
// -17) — FNAF3's 'hitbox' compares its Y against the player's Y +/- 384.

TEST(frame_events_y_compare_selects_by_y) {
    Frame frame = makeFrame();
    GameData game = makeGame();
    addObject(game, 2, 2, true, {0});
    addInstance(frame, 1, 2, 10, 100, 0);   // x 10, y 100
    addInstance(frame, 2, 2, 10, 500, 0);   // x 10, y 500
    frame.events = std::make_shared<EventsChunk>();
    frame.events->groups.push_back(makeGroup(
        {makeCondition(2, -16, 2, {expressionParameter(5, {constant(300)})})}, // Y > 300
        {makeAction(2, 31, 2, {alterableParameter(0),
                               expressionParameter(0, {constant(4)})})}
    ));

    auto state = evaluateFrameStartEvents(frame, game);
    EXPECT_EQ(state.groupsExecuted, 1);
    EXPECT_EQ(state.objects[0].alterables[0], 0);   // y=100 does not match
    EXPECT_EQ(state.objects[1].alterables[0], 4);   // y=500 matches
}

// v2.101: the "alterable value" expression operand (Object(info=N,
// list=I, num=80)) reads the alterable I of object N.

TEST(frame_events_expression_reads_alterable_operand) {
    Frame frame = makeFrame();
    GameData game = makeGame();
    addObject(game, 2, 2, true, {0});
    addObject(game, 3, 2, true, {11, 22});
    auto counter = addObject(game, 7, 7);
    counter->common->counterInitial = 22;   // the counter compares == 22
    counter->common->hasCounterConfig = true;
    addInstance(frame, 1, 2, 0, 0, 0);
    addInstance(frame, 2, 3, 0, 0, 0);
    frame.events = std::make_shared<EventsChunk>();
    ExpressionItem item;
    item.kind = ExpressionItem::Kind::Object;
    item.objectType = 2;
    item.num = 80;
    item.objectInfo = 3;
    item.objectInfoList = 1;              // alterable B of object 3
    frame.events->groups.push_back(makeGroup(
        {makeCondition(7, -81, 7, {expressionParameter(0, {item})})},
        {makeAction(2, 31, 2, {alterableParameter(0),
                               expressionParameter(0, {constant(1)})})}
    ));

    auto state = evaluateFrameStartEvents(frame, game);
    // the expression resolved to the alterable B (22) and matched the
    // counter — a failed operand would have skipped the group instead
    EXPECT_EQ(state.groupsExecuted, 1);
    EXPECT_EQ(state.groupsSkippedUnsupported, 0);
    EXPECT_EQ(state.objects[0].alterables[0], 1);
}
