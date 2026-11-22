#include "FrameRuntimeEvents.h"
#include "FrameRuntime.h"
#include "../format/Events.h"
#include "../format/GameData.h"
#include "../format/ObjectInfo.h"
#include <algorithm>
#include <cmath>
#include <set>

namespace ctfak {

namespace {

struct ExprResult {
    bool ok = false;
    double value = 0.0;
    std::string error;
};

// Known at this stage from real FNAF dumps: op 2 is addition and op 8 is
// multiplication. Do not guess other opcodes; an unknown token fails closed.
bool operatorPrecedence(int opcode, int& precedence) {
    if (opcode == 2) { precedence = 1; return true; }
    if (opcode == 8) { precedence = 2; return true; }
    return false;
}

const RuntimeObjectState* findState(const std::vector<RuntimeObjectState>& states,
                                     int objectInfo) {
    for (const auto& state : states)
        if (state.objectInfo == objectInfo) return &state;
    return nullptr;
}

int32_t alterableValue(const RuntimeObjectState* state, const ObjectInfo* obj, int index) {
    if (state && index >= 0 && static_cast<size_t>(index) < state->alterables.size())
        return state->alterables[index];
    if (obj && obj->common && index >= 0 &&
        static_cast<size_t>(index) < obj->common->alterableValues.size())
        return obj->common->alterableValues[index];
    return 0;
}

ExprResult evaluateItems(const std::vector<ExpressionItem>& items,
                         const std::vector<RuntimeObjectState>& states,
                         const GameData& game) {
    ExprResult result;
    std::vector<double> output;
    struct Op { int opcode; int precedence; };
    std::vector<Op> ops;
    bool expectOperand = true;

    auto applyTop = [&]() -> bool {
        if (ops.empty() || output.size() < 2) return false;
        const double right = output.back(); output.pop_back();
        const double left = output.back(); output.pop_back();
        const int opcode = ops.back().opcode; ops.pop_back();
        output.push_back(opcode == 2 ? left + right : left * right);
        return true;
    };

    for (const auto& item : items) {
        if (item.kind == ExpressionItem::Kind::ParenOpen) {
            ops.push_back({0, 0});
            expectOperand = true;
            continue;
        }
        if (item.kind == ExpressionItem::Kind::ParenClose) {
            while (!ops.empty() && ops.back().opcode != 0) {
                if (!applyTop()) { result.error = "malformed expression"; return result; }
            }
            if (ops.empty()) { result.error = "unbalanced parentheses"; return result; }
            ops.pop_back();
            expectOperand = false;
            continue;
        }
        if (item.kind == ExpressionItem::Kind::Operator) {
            int precedence = 0;
            if (!operatorPrecedence(static_cast<int>(item.longValue), precedence)) {
                result.error = "unsupported expression operator " + std::to_string(item.longValue);
                return result;
            }
            while (!ops.empty() && ops.back().opcode != 0 &&
                   ops.back().precedence >= precedence) {
                if (!applyTop()) { result.error = "malformed expression"; return result; }
            }
            ops.push_back({static_cast<int>(item.longValue), precedence});
            expectOperand = true;
            continue;
        }

        if (!expectOperand) { result.error = "malformed expression"; return result; }
        double value = 0.0;
        if (item.kind == ExpressionItem::Kind::Long) {
            value = item.longValue;
        } else if (item.kind == ExpressionItem::Kind::Double) {
            value = item.doubleValue;
        } else if (item.kind == ExpressionItem::Kind::Global) {
            const size_t index = static_cast<size_t>(std::max(0, item.globalValue));
            if (index >= game.globalValues.items.size()) {
                result.error = "global value " + std::to_string(item.globalValue) + " is missing";
                return result;
            }
            const auto& global = game.globalValues.items[index];
            value = global.isFloat ? global.floatValue : global.intValue;
        } else if (item.kind == ExpressionItem::Kind::Object) {
            // v2.101: the "alterable value" operand (num 80) — the list
            // field carries the alterable INDEX (0 = A, 6 = G, ...). It is
            // by far the most common expression operand in the dumps
            // (3867 references across the eight games), and the FNAF4 AI
            // formula reads Object(info=56, list=0, num=80) as the AI
            // level. The other object slots (81/82/1/11/...) stay
            // unsupported — their semantics are not proven, so they fail
            // closed.
            if (item.num == 80) {
                const ObjectInfo* obj = nullptr;
                auto it = game.frameItems.find(item.objectInfo);
                if (it != game.frameItems.end()) obj = it->second.get();
                const RuntimeObjectState* state = findState(states, item.objectInfo);
                if (!state && !obj) {
                    result.error = "expression object " + std::to_string(item.objectInfo) +
                                   " is missing";
                    return result;
                }
                value = alterableValue(state, obj, item.objectInfoList);
            } else {
                result.error = "unsupported expression operand " + item.describe();
                return result;
            }
        } else if (item.kind == ExpressionItem::Kind::Extension) {
            const ObjectInfo* obj = nullptr;
            auto it = game.frameItems.find(item.objectInfo);
            if (it != game.frameItems.end()) obj = it->second.get();
            const RuntimeObjectState* state = findState(states, item.objectInfo);
            if (!state && !obj) {
                result.error = "expression object " + std::to_string(item.objectInfo) +
                               " is missing";
                return result;
            }
            value = alterableValue(state, obj, item.extensionValue);
        } else {
            result.error = "unsupported expression operand " + item.describe();
            return result;
        }
        output.push_back(value);
        expectOperand = false;
    }

    while (!ops.empty()) {
        if (ops.back().opcode == 0) { result.error = "unbalanced parentheses"; return result; }
        if (!applyTop()) { result.error = "malformed expression"; return result; }
    }
    if (output.size() != 1 || expectOperand) {
        result.error = "malformed expression";
        return result;
    }
    result.ok = true;
    result.value = output.front();
    return result;
}

ExprResult evaluateExpression(const Parameter& param,
                              const std::vector<RuntimeObjectState>& states,
                              const GameData& game) {
    if (param.kind != Parameter::Kind::Expression || !param.expression) {
        return ExprResult{false, 0.0, "parameter is not an expression"};
    }
    return evaluateItems(param.expression->items, states, game);
}

bool expressionIsSupported(const Parameter& param, const GameData& game) {
    if (param.kind != Parameter::Kind::Expression || !param.expression) return false;
    for (const auto& item : param.expression->items) {
        if (item.kind == ExpressionItem::Kind::Operator &&
            item.longValue != 2 && item.longValue != 8) return false;
        if (item.kind == ExpressionItem::Kind::String ||
            item.kind == ExpressionItem::Kind::None) return false;
        if (item.kind == ExpressionItem::Kind::Object) {
            // v2.101: only the alterable-value slot (num 80) is proven
            if (item.num != 80) return false;
            if (game.frameItems.find(item.objectInfo) == game.frameItems.end()) return false;
        }
        if (item.kind == ExpressionItem::Kind::Extension) {
            if (game.frameItems.find(item.objectInfo) == game.frameItems.end()) return false;
        }
        if (item.kind == ExpressionItem::Kind::Global) {
            if (item.globalValue < 0 ||
                static_cast<size_t>(item.globalValue) >= game.globalValues.items.size())
                return false;
        }
    }
    return true;
}

bool compare(double left, int16_t op, double right) {
    switch (op) {
        case 0: return left == right;
        case 1: return left != right;
        case 2: return left <= right;
        case 3: return left < right;
        case 4: return left >= right;
        case 5: return left > right;
        default: return false;
    }
}

struct ConditionResult {
    bool supported = false;
    bool value = false;
};

ConditionResult evaluateCondition(const Condition& cond,
                                  const std::vector<RuntimeObjectState>& states,
                                  const std::map<int32_t, int32_t>& counters,
                                  const GameData& game,
                                  std::map<int, std::set<int>>& selection,
                                  std::string& note) {
    ConditionResult result;
    if (cond.objectInfoList != 0) {
        note += "unsupported qualifier condition (" +
                std::to_string(cond.objectType) + "," + std::to_string(cond.num) + ")\n";
        return result;
    }

    const bool negate = (cond.otherFlags & 1) != 0;
    const auto objectIndices = [&](int objectInfo) {
        std::set<int> indices;
        for (size_t i = 0; i < states.size(); ++i)
            if (states[i].objectInfo == objectInfo) indices.insert(static_cast<int>(i));
        return indices;
    };

    if (cond.objectType == -3 && cond.num == -1 && cond.parameters.empty()) {
        result.supported = true; result.value = true; return result;
    }
    if (cond.objectType == -1 && cond.parameters.empty() &&
        (cond.num == -7 || cond.num == -6 || cond.num == -1)) {
        result.supported = true; result.value = true; return result;
    }
    if (cond.objectType == 7 && cond.num == -81 && cond.parameters.size() == 1) {
        ExprResult expr = evaluateExpression(cond.parameters[0], states, game);
        if (!expr.ok) {
            note += "counter condition expression: " + expr.error + "\n";
            return result;
        }
        auto counter = counters.find(cond.objectInfo);
        const int32_t value = counter == counters.end() ? 0 : counter->second;
        result.supported = true;
        result.value = compare(value, cond.parameters[0].expression->comparsion, expr.value);
        if (negate) result.value = !result.value;
        return result;
    }
    if (cond.objectType == 2 && cond.num == -27 && cond.parameters.size() == 2 &&
        cond.parameters[0].kind == Parameter::Kind::AlterableValue) {
        ExprResult expr = evaluateExpression(cond.parameters[1], states, game);
        if (!expr.ok) {
            note += "alterable condition expression: " + expr.error + "\n";
            return result;
        }
        auto indices = objectIndices(cond.objectInfo);
        std::set<int> matched, unmatched;
        for (int index : indices) {
            const RuntimeObjectState& state = states[index];
            const double left = alterableValue(&state, nullptr,
                                               static_cast<int>(cond.parameters[0].value));
            if (compare(left, cond.parameters[1].expression->comparsion, expr.value))
                matched.insert(index);
            else
                unmatched.insert(index);
        }
        const bool any = negate ? !unmatched.empty() : !matched.empty();
        if (any) {
            for (int index : (negate ? unmatched : matched))
                selection[cond.objectInfo].insert(index);
        }
        result.supported = true;
        result.value = any;
        return result;
    }
    // v2.100: (2,-4) is the Active object's "collision with another
    // object" condition; the single Object parameter names the target.
    // FNAF4 builds its whole position system on it — 'plushtrap' collides
    // with the 'in chair'/'hall stage N' zone objects (all tiny hot-spot
    // images), and FNAF2's 'your view' collides with 'paper pals' as the
    // camera-visibility test. Unlike the input/timer conditions, the
    // serialized first tick HAS positions, so this is evaluated faithfully:
    // an AABB overlap between the owner instances and the target instances,
    // using each state's x/y and the start image's size/hotspot (the same
    // anchor the renderer draws at). No overlap -> false (negated -> true),
    // matching objects form the selection like the other object conditions.
    if (cond.objectType == 2 && cond.num == -4 && cond.parameters.size() == 1 &&
        cond.parameters[0].kind == Parameter::Kind::Object) {
        const int32_t targetInfo = cond.parameters[0].objectInfo;
        struct Box { int x0, y0, x1, y1; };
        auto boxFor = [&](const RuntimeObjectState& st) -> Box {
            int w = 0, h = 0, hx = 0, hy = 0;
            const auto it = game.frameItems.find(st.objectInfo);
            if (it != game.frameItems.end()) {
                const int32_t handle = resolveRuntimeStartImage(
                    *it->second, st.animation, st.direction);
                const auto img = game.images.find(handle);
                if (img) {
                    w = img->width; h = img->height;
                    hx = img->hotX; hy = img->hotY;
                }
            }
            if (w <= 0 || h <= 0) return {st.x, st.y, st.x, st.y}; // point test
            return {st.x - hx, st.y - hy,
                    st.x - hx + w - 1, st.y - hy + h - 1};
        };
        auto overlaps = [](const Box& a, const Box& b) {
            return a.x0 <= b.x1 && b.x0 <= a.x1 &&
                   a.y0 <= b.y1 && b.y0 <= a.y1;
        };
        const auto targets = objectIndices(targetInfo);
        auto indices = objectIndices(cond.objectInfo);
        std::set<int> matched, unmatched;
        for (int index : indices) {
            const Box owner = boxFor(states[index]);
            bool hit = false;
            for (int t : targets) {
                if (overlaps(owner, boxFor(states[t]))) { hit = true; break; }
            }
            (hit ? matched : unmatched).insert(index);
        }
        const bool any = negate ? !unmatched.empty() : !matched.empty();
        if (any) {
            for (int index : (negate ? unmatched : matched))
                selection[cond.objectInfo].insert(index);
        }
        result.supported = true;
        result.value = any;
        return result;
    }
    // v2.98 FIX: (2,-2) is "animation N has finished playing", NOT
    // "animation == N". Evidence from the dumps: FNAF2 office group 149
    // chains "flip animation" finished 0 -> act #24 (next animation) +
    // restore the last camera, and FNAF1's door groups chain "finished
    // closing (12) -> set animation 13 (closed)" — classic finish-to-advance
    // steps, all of which fire MID-NIGHT, not at the frame's first tick.
    // The old "animation == N" reading evaluated TRUE whenever the object's
    // start animation matched (FNAF2: "flip animation" starts at 0 -> the
    // flip chain ran at tick 0 -> viewing became 9 -> the office Mangle
    // stayed visible against the real game). At the serialized first tick
    // NOTHING has played yet, so the faithful answer is FALSE (negated ->
    // TRUE) and nothing is selected.
    if (cond.objectType == 2 && cond.num == -2) {
        result.supported = true;
        result.value = negate;
        return result;
    }
    if (cond.objectType == 2 && cond.num == -17 && cond.parameters.size() == 1) {
        ExprResult expr = evaluateExpression(cond.parameters[0], states, game);
        if (!expr.ok) {
            note += "X condition expression: " + expr.error + "\n";
            return result;
        }
        auto indices = objectIndices(cond.objectInfo);
        std::set<int> matched, unmatched;
        for (int index : indices) {
            if (compare(states[index].x, cond.parameters[0].expression->comparsion, expr.value))
                matched.insert(index);
            else
                unmatched.insert(index);
        }
        const bool any = negate ? !unmatched.empty() : !matched.empty();
        if (any) {
            for (int index : (negate ? unmatched : matched))
                selection[cond.objectInfo].insert(index);
        }
        result.supported = true;
        result.value = any;
        return result;
    }
    // v2.101: (2,-16) is the Y-position compare — the mirror of the X
    // compare (-17). Evidence from the dumps: FNAF3's 'hitbox' compares
    // its Y against the player's Y +/- 384 (a vertical tolerance around
    // the player), 'top jaw' tests Y 0/-710, the blueprint-mode 'scroll'
    // tests the Y band 540..1620 and the FNaF World 'man' 200..600.
    if (cond.objectType == 2 && cond.num == -16 && cond.parameters.size() == 1) {
        ExprResult expr = evaluateExpression(cond.parameters[0], states, game);
        if (!expr.ok) {
            note += "Y condition expression: " + expr.error + "\n";
            return result;
        }
        auto indices = objectIndices(cond.objectInfo);
        std::set<int> matched, unmatched;
        for (int index : indices) {
            if (compare(states[index].y, cond.parameters[0].expression->comparsion, expr.value))
                matched.insert(index);
            else
                unmatched.insert(index);
        }
        const bool any = negate ? !unmatched.empty() : !matched.empty();
        if (any) {
            for (int index : (negate ? unmatched : matched))
                selection[cond.objectInfo].insert(index);
        }
        result.supported = true;
        result.value = any;
        return result;
    }
    if (cond.objectType == 2 && cond.num == -29 && cond.parameters.empty()) {
        auto indices = objectIndices(cond.objectInfo);
        std::set<int> matched, unmatched;
        for (int index : indices) {
            if (states[index].visible) matched.insert(index);
            else unmatched.insert(index);
        }
        const bool any = negate ? !unmatched.empty() : !matched.empty();
        if (any) {
            for (int index : (negate ? unmatched : matched))
                selection[cond.objectInfo].insert(index);
        }
        result.supported = true;
        result.value = any;
        return result;
    }
    if (cond.objectType == -1 && cond.num == -3 && cond.parameters.size() == 2) {
        ExprResult left = evaluateExpression(cond.parameters[0], states, game);
        ExprResult right = evaluateExpression(cond.parameters[1], states, game);
        if (!left.ok) { note += "system compare left: " + left.error + "\n"; return result; }
        if (!right.ok) { note += "system compare right: " + right.error + "\n"; return result; }
        result.supported = true;
        result.value = compare(left.value, cond.parameters[0].expression->comparsion,
                               right.value);
        if (negate) result.value = !result.value;
        return result;
    }

    // v2.98: input, timer and audio-state conditions are DETERMINISTIC at
    // the serialized first tick: no key/click is in flight, no mouse is
    // over an object, no timer interval has elapsed yet, no player control
    // is held and a visual renderer tracks no audio state. The faithful
    // answer is FALSE (negated -> TRUE) and nothing gets selected.
    // Pairs identified from real dump parameters (EventsListing + reports):
    //   -6 Mouse&Keyboard: -1 key pressed, -2 key released, -3 key down,
    //      -4 mouse over object, -5 click on background, -7 click on
    //      object, -8 mouse button down
    //   -4 Timer: -1/-2 "after N" one-shot, -4/-7/-8 "every N" — neither
    //      has elapsed at the first tick
    //   -7 Player: -2/-3/-4/-6 control/input state
    //   -2 Speaker: -1/-3 sample/channel playing state
    static const int16_t kFalseAtFirstTick[][2] = {
        {-6, -1}, {-6, -2}, {-6, -3}, {-6, -4}, {-6, -5}, {-6, -7}, {-6, -8},
        {-4, -1}, {-4, -2}, {-4, -4}, {-4, -7}, {-4, -8},
        {-7, -2}, {-7, -3}, {-7, -4}, {-7, -6},
        {-2, -1}, {-2, -3},
    };
    for (const auto& pair : kFalseAtFirstTick) {
        if (cond.objectType == pair[0] && cond.num == pair[1]) {
            result.supported = true;
            result.value = negate;
            return result;
        }
    }

    note += "unsupported condition (" + std::to_string(cond.objectType) + "," +
            std::to_string(cond.num) + ")\n";
    return result;
}

enum class ActionClass { Apply, NoOp, Unsupported };

ActionClass classifyAction(const Action& action) {
    const int type = action.objectType, num = action.num;
    if (type == 7 && (num == 80 || num == 81 || num == 82 || num == 26 || num == 27))
        return ActionClass::Apply;
    if (type == 2 && (num == 17 || num == 24 || num == 26 || num == 27 ||
                      num == 31 || num == 32 || num == 33 || num == 65 ||
                      num == 1 || num == 2 || num == 3))
        return ActionClass::Apply;
    if (type == -5 && num == 0) return ActionClass::Apply;
    if (type == -3 && (num == 0 || num == 2 || num == 4 || num == 8))
        return ActionClass::NoOp;
    if (type == -2 && (num == 1 || num == 11 || num == 12 || num == 17))
        return ActionClass::NoOp;
    if (type == 33 && (num == 80 || num == 81 || num == 86 || num == 87))
        return ActionClass::NoOp;
    if (type == 3 && (num == 88 || num == 83)) return ActionClass::NoOp;
    return ActionClass::Unsupported;
}

void appendNoOpNote(std::string& notes, const Action& action) {
    const char* label = "action";
    if (action.objectType == -3) label = "frame navigation";
    else if (action.objectType == -2) label = "audio";
    else if (action.objectType == 33) label = "INI storage";
    else if (action.objectType == 3) label = "text";
    notes += std::string(label) + " not rendered: action (" +
             std::to_string(action.objectType) + "," + std::to_string(action.num) + ")\n";
}

} // namespace

RuntimeFrameState evaluateFrameStartEvents(const Frame& frame,
                                            const GameData& game) {
    RuntimeFrameState state;
    int nextHandle = 1;

    // Seed one state per serialized instance. Events address instances through
    // objectInfo, so later actions can affect every instance of that object.
    for (size_t i = 0; i < frame.objects.size(); ++i) {
        const ObjectInstance& inst = frame.objects[i];
        auto it = game.frameItems.find(inst.objectInfo);
        const ObjectInfo* obj = it != game.frameItems.end() ? it->second.get() : nullptr;
        RuntimeObjectState s;
        s.handle = inst.handle ? inst.handle : static_cast<int>(i + 1);
        nextHandle = std::max(nextHandle, s.handle + 1);
        s.objectInfo = inst.objectInfo;
        s.layer = inst.layer;
        s.x = inst.x;
        s.y = inst.y;
        if (obj) {
            s.created = runtimeObjectVisibleAtStart(*obj);
            s.visible = s.created;
            s.alphaCoefficient = runtimeObjectAlpha(*obj);
            if (obj->common) {
                s.alterables = obj->common->alterableValues;
                for (const auto& movement : obj->common->movements)
                    s.direction = movement.directionAtStart;
            }
        } else {
            s.created = false;
            s.visible = false;
            state.notes += "objInfo " + std::to_string(inst.objectInfo) +
                           " missing from frame item bank\n";
        }
        state.objects.push_back(std::move(s));
    }

    // Counter config lives after the system-counter object in ObjectCommon.
    for (const auto& [objectInfo, obj] : game.frameItems) {
        if (obj && obj->objectType == kObjCounter && obj->common &&
            obj->common->hasCounterConfig) {
            state.counters[objectInfo] = obj->common->counterInitial;
        }
    }

    if (!frame.events) return state;

    // v2.98: collect the animation-chain subjects — objects whose frames are
    // advanced by "animation sequence finished" chains (condition (2,-2)).
    // Their serialized editor image is a MID-CHAIN snapshot (FNAF2's monitor
    // flip, FNAF4's attack placeholders), not the runtime entry state.
    for (const auto& group : frame.events->groups)
        for (const auto& condition : group.conditions)
            if (condition.objectType == 2 && condition.num == -2 &&
                condition.objectInfo > 0)
                state.chainSubjects.insert(condition.objectInfo);

    for (size_t groupIndex = 0; groupIndex < frame.events->groups.size(); ++groupIndex) {
        const EventGroup& group = frame.events->groups[groupIndex];
        std::map<int, std::set<int>> selection;
        bool conditionsTrue = true;
        bool supported = true;
        std::string groupNotes;
        const std::string groupLabel = "group " + std::to_string(groupIndex + 1) + ": ";

        for (const auto& condition : group.conditions) {
            ConditionResult result = evaluateCondition(condition, state.objects,
                                                       state.counters, game,
                                                       selection, groupNotes);
            if (!result.supported) {
                supported = false;
                conditionsTrue = false;
                break;
            }
            if (!result.value) {
                conditionsTrue = false;
                break;
            }
        }

        if (!supported) {
            ++state.groupsSkippedUnsupported;
            state.notes += groupLabel + "skipped\n" + groupNotes;
            continue;
        }
        if (!conditionsTrue) {
            ++state.groupsFalse;
            continue;
        }

        bool actionsSupported = true;
        for (const auto& action : group.actions) {
            const ActionClass kind = classifyAction(action);
            if (kind == ActionClass::Unsupported) {
                groupNotes += "unsupported action (" + std::to_string(action.objectType) +
                              "," + std::to_string(action.num) + ")\n";
                actionsSupported = false;
            } else if (kind == ActionClass::NoOp) {
                appendNoOpNote(groupNotes, action);
            } else if (action.objectType == 7 &&
                       (action.num == 80 || action.num == 81 || action.num == 82)) {
                if (!expressionIsSupported(action.parameters.empty()
                                               ? Parameter()
                                               : action.parameters[0], game)) {
                    groupNotes += "counter action expression unsupported\n";
                    actionsSupported = false;
                }
            } else if (action.objectType == 2 &&
                       (action.num == 31 || action.num == 32 || action.num == 33 ||
                        action.num == 65 || action.num == 2 || action.num == 3)) {
                const Parameter* expression = nullptr;
                if (action.num == 31 || action.num == 32 || action.num == 33) {
                    if (action.parameters.size() != 2 ||
                        action.parameters[0].kind != Parameter::Kind::AlterableValue) {
                        groupNotes += "malformed alterable action\n";
                        actionsSupported = false;
                    } else {
                        expression = &action.parameters[1];
                    }
                } else {
                    if (action.parameters.size() != 1) {
                        groupNotes += "malformed object action\n";
                        actionsSupported = false;
                    } else {
                        expression = &action.parameters[0];
                    }
                }
                if (expression && !expressionIsSupported(*expression, game)) {
                    groupNotes += "object action expression unsupported\n";
                    actionsSupported = false;
                }
            } else if (action.objectType == 2 && action.num == 1) {
                if (action.parameters.size() != 1 ||
                    action.parameters[0].kind != Parameter::Kind::Position) {
                    groupNotes += "malformed position action\n";
                    actionsSupported = false;
                }
            } else if (action.objectType == -5 && action.num == 0) {
                if (action.parameters.size() != 1 ||
                    action.parameters[0].kind != Parameter::Kind::Create ||
                    game.frameItems.find(action.parameters[0].createObjectInfo) ==
                        game.frameItems.end()) {
                    groupNotes += "unsupported create action\n";
                    actionsSupported = false;
                }
            }
        }

        if (!actionsSupported) {
            ++state.groupsSkippedUnsupported;
            state.notes += groupLabel + "skipped\n" + groupNotes;
            continue;
        }

        // Apply in action order. Selection applies only to object-backed
        // actions; counter actions address their counter objectInfo directly.
        for (const auto& action : group.actions) {
            const ActionClass kind = classifyAction(action);
            if (kind == ActionClass::NoOp) continue;
            const int type = action.objectType, num = action.num;

            std::vector<int> targets;
            if (type == 2 || type == 7) {
                if (selection.count(action.objectInfo)) {
                    targets.assign(selection[action.objectInfo].begin(),
                                   selection[action.objectInfo].end());
                } else {
                    for (size_t i = 0; i < state.objects.size(); ++i)
                        if (state.objects[i].objectInfo == action.objectInfo)
                            targets.push_back(static_cast<int>(i));
                }
            }

            if (type == 7 && (num == 80 || num == 81 || num == 82)) {
                ExprResult value = evaluateExpression(action.parameters[0],
                                                       state.objects, game);
                if (!value.ok) continue;
                int32_t& counter = state.counters[action.objectInfo];
                if (num == 80) counter = static_cast<int32_t>(value.value);
                else if (num == 81) counter += static_cast<int32_t>(value.value);
                else counter -= static_cast<int32_t>(value.value);
                continue;
            }
            if (type == 7 && (num == 26 || num == 27)) {
                for (int index : targets) state.objects[index].visible = (num == 27);
                continue;
            }
            if (type != 2) continue;

            if (num == 17 || num == 24) {
                for (int index : targets) {
                    if (num == 17) {
                        state.objects[index].animation =
                            action.parameters.empty()
                                ? 0 : static_cast<int>(action.parameters[0].value);
                    } else {
                        ++state.objects[index].animation;
                    }
                }
            } else if (num == 26 || num == 27) {
                for (int index : targets) state.objects[index].visible = (num == 27);
            } else if (num == 31 || num == 32 || num == 33) {
                const int index = static_cast<int>(action.parameters[0].value);
                ExprResult value = evaluateExpression(action.parameters[1],
                                                       state.objects, game);
                for (int target : targets) {
                    auto& alterables = state.objects[target].alterables;
                    if (index >= 0) {
                        if (static_cast<size_t>(index) >= alterables.size())
                            alterables.resize(static_cast<size_t>(index) + 1, 0);
                        if (num == 31) alterables[index] = static_cast<int32_t>(value.value);
                        else if (num == 32) alterables[index] += static_cast<int32_t>(value.value);
                        else alterables[index] -= static_cast<int32_t>(value.value);
                    }
                }
            } else if (num == 65) {
                ExprResult value = evaluateExpression(action.parameters[0],
                                                       state.objects, game);
                for (int target : targets) {
                    state.objects[target].alphaOverride = true;
                    state.objects[target].alphaCoefficient =
                        std::clamp(static_cast<int>(std::lround(value.value)), 0, 255);
                }
            } else if (num == 1) {
                const auto& position = action.parameters[0].position;
                int baseX = 0, baseY = 0;
                if (position.parentInfo != 0) {
                    const RuntimeObjectState* parent = findState(state.objects,
                                                                  position.parentInfo);
                    if (parent) { baseX = parent->x; baseY = parent->y; }
                }
                for (int target : targets) {
                    state.objects[target].x = baseX + position.x;
                    state.objects[target].y = baseY + position.y;
                    if (position.layer >= 0) state.objects[target].layer = position.layer;
                }
            } else if (num == 2 || num == 3) {
                ExprResult value = evaluateExpression(action.parameters[0],
                                                       state.objects, game);
                for (int target : targets) {
                    if (num == 2) state.objects[target].x = static_cast<int>(value.value);
                    else state.objects[target].y = static_cast<int>(value.value);
                }
            }
        }

        // A create action must be applied after the group's state reads, as in
        // the original event order. Its target instances become renderable.
        for (const auto& action : group.actions) {
            if (action.objectType != -5 || action.num != 0) continue;
            const auto& create = action.parameters[0];
            int baseX = create.position.x, baseY = create.position.y;
            if (create.position.parentInfo != 0) {
                const RuntimeObjectState* parent = findState(state.objects,
                                                              create.position.parentInfo);
                if (parent) {
                    baseX = parent->x + create.position.x;
                    baseY = parent->y + create.position.y;
                }
            }
            for (uint16_t n = 0; n < create.createInstances; ++n) {
                RuntimeObjectState created;
                created.handle = nextHandle++;
                created.objectInfo = create.createObjectInfo;
                created.layer = create.position.layer;
                created.x = baseX;
                created.y = baseY;
                created.created = true;
                created.visible = true;
                auto it = game.frameItems.find(created.objectInfo);
                if (it != game.frameItems.end() && it->second) {
                    created.alphaCoefficient = runtimeObjectAlpha(*it->second);
                    if (it->second->common) {
                        created.alterables = it->second->common->alterableValues;
                        for (const auto& movement : it->second->common->movements)
                            created.direction = movement.directionAtStart;
                    }
                }
                state.objects.push_back(std::move(created));
            }
        }

        if (!groupNotes.empty()) state.notes += groupLabel + groupNotes;
        ++state.groupsExecuted;
    }

    return state;
}

} // namespace ctfak
