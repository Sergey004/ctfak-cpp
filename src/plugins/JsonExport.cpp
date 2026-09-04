#include "JsonExport.h"
#include "Dumpers.h"
#include "../format/Events.h"
#include "../io/JsonWriter.h"
#include "../util/Config.h"
#include "../util/Utils.h"
#include "../core/Logger.h"
#include "../core/Settings.h"

namespace ctfak {

static JsonPtr colorJson(const BinaryReader::Color& c) {
    auto j = JsonValue::makeArray();
    j->push(JsonValue::makeInt(c.r));
    j->push(JsonValue::makeInt(c.g));
    j->push(JsonValue::makeInt(c.b));
    j->push(JsonValue::makeInt(c.a));
    return j;
}

static JsonPtr bitDictJson(const BitDict& flags) {
    auto j = JsonValue::makeObject();
    for (size_t i = 0; i < flags.names().size(); ++i) {
        const std::string& n = flags.names()[i];
        if (n.empty()) continue;
        bool named = n[0] >= 'A' && n[0] <= 'Z';
        if (!named) continue;
        j->members.push_back({n, JsonValue::makeBool(flags[i])});
    }
    return j;
}

static JsonPtr parameterJson(const Parameter& p) {
    auto j = JsonValue::makeObject();
    j->set("code", JsonValue::makeInt(p.code));
    j->set("type", JsonValue::makeString(p.kindName()));
    switch (p.kind) {
        case Parameter::Kind::Object:
            j->set("objectInfo", JsonValue::makeInt(p.objectInfo));
            j->set("objectInfoList", JsonValue::makeInt(p.objectInfoList));
            j->set("objectType", JsonValue::makeInt(p.objectType));
            break;
        case Parameter::Kind::Time:
            j->set("timer", JsonValue::makeInt(p.value));
            j->set("loops", JsonValue::makeInt(p.value2));
            break;
        case Parameter::Kind::Short:
        case Parameter::Kind::Int:
        case Parameter::Kind::Key:
        case Parameter::Kind::GlobalValue:
        case Parameter::Kind::AlterableValue:
            j->set("value", JsonValue::makeInt(p.value));
            break;
        case Parameter::Kind::TwoShorts:
            j->set("value1", JsonValue::makeInt(p.value));
            j->set("value2", JsonValue::makeInt(p.value2));
            break;
        case Parameter::Kind::Every:
            j->set("delayMs", JsonValue::makeInt(p.value));
            j->set("counter", JsonValue::makeInt(p.value2));
            break;
        case Parameter::Kind::Sample:
            j->set("handle", JsonValue::makeInt(p.sampleHandle));
            j->set("flags", JsonValue::makeInt(p.sampleFlags));
            j->set("name", JsonValue::makeString(p.str));
            break;
        case Parameter::Kind::Create:
            j->set("x", JsonValue::makeInt(p.position.x));
            j->set("y", JsonValue::makeInt(p.position.y));
            j->set("objectInstances", JsonValue::makeInt(p.createInstances));
            j->set("objectInfo", JsonValue::makeInt(p.createObjectInfo));
            break;
        case Parameter::Kind::Shoot:
            j->set("x", JsonValue::makeInt(p.position.x));
            j->set("y", JsonValue::makeInt(p.position.y));
            j->set("objectInstance", JsonValue::makeInt(p.shootInstance));
            j->set("objectInfo", JsonValue::makeInt(p.shootObjectInfo));
            j->set("speed", JsonValue::makeInt(p.shootSpeed));
            break;
        case Parameter::Kind::Position: {
            auto pos = JsonValue::makeObject();
            pos->set("x", JsonValue::makeInt(p.position.x));
            pos->set("y", JsonValue::makeInt(p.position.y));
            pos->set("typeParent", JsonValue::makeInt(p.position.typeParent));
            pos->set("objectInfoList", JsonValue::makeInt(p.position.objectInfoList));
            pos->set("layer", JsonValue::makeInt(p.position.layer));
            j->set("position", std::move(pos));
            break;
        }
        case Parameter::Kind::Zone:
            j->set("x1", JsonValue::makeInt(p.zone[0]));
            j->set("y1", JsonValue::makeInt(p.zone[1]));
            j->set("x2", JsonValue::makeInt(p.zone[2]));
            j->set("y2", JsonValue::makeInt(p.zone[3]));
            break;
        case Parameter::Kind::Colour: {
            auto c = JsonValue::makeObject();
            c->set("r", JsonValue::makeInt(p.colour[0]));
            c->set("g", JsonValue::makeInt(p.colour[1]));
            c->set("b", JsonValue::makeInt(p.colour[2]));
            j->set("color", std::move(c));
            break;
        }
        case Parameter::Kind::Click:
            j->set("button", JsonValue::makeInt(p.value));
            j->set("isDouble", JsonValue::makeBool(p.value2 != 0));
            break;
        case Parameter::Kind::Program:
            j->set("filename", JsonValue::makeString(p.str));
            j->set("command", JsonValue::makeString(p.programCommand));
            break;
        case Parameter::Kind::Filename:
        case Parameter::Kind::String:
            j->set("value", JsonValue::makeString(p.str));
            break;
        case Parameter::Kind::Group:
            j->set("flags", JsonValue::makeInt(p.groupFlags));
            j->set("id", JsonValue::makeInt(p.groupId));
            j->set("name", JsonValue::makeString(p.groupName));
            break;
        case Parameter::Kind::GroupPointer:
            j->set("pointer", JsonValue::makeInt(p.groupPointer));
            j->set("id", JsonValue::makeInt(p.groupPointerId));
            break;
        case Parameter::Kind::Extension:
            j->set("extType", JsonValue::makeInt(p.extType));
            j->set("extCode", JsonValue::makeInt(p.extCode));
            j->set("dataSize", JsonValue::makeInt(p.extData.size()));
            break;
        case Parameter::Kind::MultipleVariables: {
            auto mv = JsonValue::makeObject();
            mv->set("flags", JsonValue::makeInt(p.mvFlags));
            mv->set("flagMasks", JsonValue::makeInt(p.mvFlagMasks));
            mv->set("flagValues", JsonValue::makeInt(p.mvFlagValues));
            auto values = JsonValue::makeArray();
            for (const auto& v : p.multivars) {
                auto item = JsonValue::makeObject();
                item->set("index", JsonValue::makeInt(v.index));
                item->set("op", JsonValue::makeInt(v.op));
                item->set("isDouble", JsonValue::makeBool(v.isDouble));
                item->set("value", v.isDouble ? JsonValue::makeDouble(v.doubleValue)
                                             : JsonValue::makeInt(v.intValue));
                values->push(std::move(item));
            }
            mv->set("values", std::move(values));
            j->set("multipleVariables", std::move(mv));
            break;
        }
        case Parameter::Kind::ChildEvent: {
            auto children = JsonValue::makeArray();
            for (int16_t v : p.childObjects) children->push(JsonValue::makeInt(v));
            j->set("count", JsonValue::makeInt(p.childCount));
            j->set("objectInfos", std::move(children));
            break;
        }
        case Parameter::Kind::Expression: {
            auto expr = JsonValue::makeObject();
            expr->set("comparsion", JsonValue::makeString(p.expression ? p.expression->operatorString() : "?"));
            auto tokens = JsonValue::makeArray();
            if (p.expression)
                for (const auto& item : p.expression->items) tokens->push(JsonValue::makeString(item.describe()));
            expr->set("tokens", std::move(tokens));
            if (p.expression) expr->set("raw", JsonValue::makeString(p.expression->describe()));
            j->set("expression", std::move(expr));
            break;
        }
        default:
            break;
    }
    return j;
}

// Frame-navigation ("storyboard") action label; nullptr when not a nav action.
// FNaF drives frame jumps through the Game object (objectType -3); System (-1)
// is kept for completeness.
static const char* storyboardActionName(int16_t objectType, int16_t num) {
    if (objectType == -3) { // Game / storyboard controls
        switch (num) {
            case 0: return "Next frame";
            case 2: return "Jump to frame";
            case 4: return "End application";
            case 8: return "Jump to frame (expression)";
            default: return nullptr;
        }
    } else if (objectType == -1) { // System
        switch (num) {
            case 1: return "Previous frame";
            case 2: return "Next frame";
            case 3: return "Jump to frame";
            case 4: return "Restart current frame";
            case 5: return "Restart application";
            default: return nullptr;
        }
    }
    return nullptr;
}

static bool literalJumpAction(int16_t objectType, int16_t num) {
    return (objectType == -3 && num == 2) || (objectType == -1 && num == 3);
}

static JsonPtr conditionJson(const Condition& c) {
    auto j = JsonValue::makeObject();
    j->set("objectType", JsonValue::makeInt(c.objectType));
    j->set("num", JsonValue::makeInt(c.num));
    j->set("objectInfo", JsonValue::makeInt(c.objectInfo));
    j->set("objectInfoList", JsonValue::makeInt(c.objectInfoList));
    j->set("flags", JsonValue::makeInt(c.flags));
    j->set("otherFlags", JsonValue::makeInt(c.otherFlags));
    j->set("defType", JsonValue::makeInt(c.defType));
    j->set("identifier", JsonValue::makeInt(c.identifier));
    auto params = JsonValue::makeArray();
    for (const auto& p : c.parameters) params->push(parameterJson(p));
    j->set("parameters", std::move(params));
    return j;
}

static JsonPtr actionJson(const Action& a, const std::vector<std::string>* frameNames) {
    auto j = JsonValue::makeObject();
    j->set("objectType", JsonValue::makeInt(a.objectType));
    j->set("num", JsonValue::makeInt(a.num));
    j->set("objectInfo", JsonValue::makeInt(a.objectInfo));
    j->set("objectInfoList", JsonValue::makeInt(a.objectInfoList));
    j->set("flags", JsonValue::makeInt(a.flags));
    j->set("otherFlags", JsonValue::makeInt(a.otherFlags));
    j->set("defType", JsonValue::makeInt(a.defType));
    if (const char* nm = storyboardActionName(a.objectType, a.num))
        j->set("actionName", JsonValue::makeString(nm));
    if (literalJumpAction(a.objectType, a.num) && !a.parameters.empty()) {
        const Parameter& fp = a.parameters[0];
        if (fp.kind == Parameter::Kind::Short || fp.kind == Parameter::Kind::Int) {
            int tgt = static_cast<int>(fp.value);
            j->set("targetFrame", JsonValue::makeInt(tgt));
            if (frameNames && tgt >= 0 && tgt < static_cast<int>(frameNames->size()) &&
                !(*frameNames)[tgt].empty())
                j->set("targetFrameName", JsonValue::makeString((*frameNames)[tgt]));
        }
    }
    auto params = JsonValue::makeArray();
    for (const auto& p : a.parameters) params->push(parameterJson(p));
    j->set("parameters", std::move(params));
    return j;
}

static JsonPtr objectJson(const ObjectInfo& oi) {
    auto j = JsonValue::makeObject();
    j->set("handle", JsonValue::makeInt(oi.handle));
    j->set("objectType", JsonValue::makeInt(oi.objectType));
    j->set("type", JsonValue::makeString(objectTypeName(oi.objectType)));
    j->set("name", JsonValue::makeString(oi.name));
    j->set("flags", JsonValue::makeInt(oi.flags));
    j->set("inkEffect", JsonValue::makeInt(oi.inkEffect));
    j->set("inkEffectValue", JsonValue::makeInt(oi.inkEffectValue));
    j->set("blend", JsonValue::makeInt(oi.blend));
    j->set("rgbCoeff", colorJson(oi.rgbCoeff));
    if (oi.shaderData.hasShader) {
        auto sh = JsonValue::makeObject();
        sh->set("name", JsonValue::makeString(oi.shaderData.name));
        sh->set("handle", JsonValue::makeInt(oi.shaderData.handle));
        auto params = JsonValue::makeArray();
        for (const auto& p : oi.shaderData.parameters) {
            auto pj = JsonValue::makeObject();
            pj->set("name", JsonValue::makeString(p.name));
            pj->set("type", JsonValue::makeInt(p.type));
            pj->set("value", JsonValue::makeDouble(p.value));
            params->push(std::move(pj));
        }
        sh->set("parameters", std::move(params));
        j->set("shader", std::move(sh));
    }
    if (oi.backdrop) {
        auto b = JsonValue::makeObject();
        b->set("obstacleType", JsonValue::makeInt(oi.backdrop->obstacleType));
        b->set("collisionType", JsonValue::makeInt(oi.backdrop->collisionType));
        b->set("width", JsonValue::makeInt(oi.backdrop->width));
        b->set("height", JsonValue::makeInt(oi.backdrop->height));
        b->set("image", JsonValue::makeInt(oi.backdrop->image));
        j->set("backdrop", std::move(b));
    }
    if (oi.quickBackdrop) {
        auto b = JsonValue::makeObject();
        b->set("obstacleType", JsonValue::makeInt(oi.quickBackdrop->obstacleType));
        b->set("collisionType", JsonValue::makeInt(oi.quickBackdrop->collisionType));
        b->set("width", JsonValue::makeInt(oi.quickBackdrop->width));
        b->set("height", JsonValue::makeInt(oi.quickBackdrop->height));
        b->set("image", JsonValue::makeInt(oi.quickBackdrop->shape.image));
        j->set("quickBackdrop", std::move(b));
    }
    if (oi.common) {
        const ObjectCommon& c = *oi.common;
        auto common = JsonValue::makeObject();
        common->set("identifier", JsonValue::makeString(c.identifier));
        common->set("flags", bitDictJson(c.flags));
        common->set("newFlags", bitDictJson(c.newFlags));
        common->set("preferences", bitDictJson(c.preferences));
        common->set("backColor", colorJson(c.backColor));

        auto quals = JsonValue::makeArray();
        for (int16_t q : c.qualifiers) quals->push(JsonValue::makeInt(q));
        common->set("qualifiers", std::move(quals));

        if (!c.alterableValues.empty()) {
            auto vals = JsonValue::makeArray();
            for (int32_t v : c.alterableValues) vals->push(JsonValue::makeInt(v));
            common->set("alterableValues", std::move(vals));
        }
        if (!c.alterableStrings.empty()) {
            auto strs = JsonValue::makeArray();
            for (const auto& s : c.alterableStrings) strs->push(JsonValue::makeString(s));
            common->set("alterableStrings", std::move(strs));
        }
        if (!c.movements.empty()) {
            auto movs = JsonValue::makeArray();
            for (const auto& m : c.movements) {
                auto mj = JsonValue::makeObject();
                mj->set("player", JsonValue::makeInt(m.player));
                mj->set("type", JsonValue::makeInt(m.type));
                mj->set("name", JsonValue::makeString(m.typeName));
                mj->set("movingAtStart", JsonValue::makeBool(m.movingAtStart != 0));
                mj->set("directionAtStart", JsonValue::makeInt(m.directionAtStart));
                if (!m.fields.empty()) {
                    auto fields = JsonValue::makeObject();
                    for (const auto& [k, v] : m.fields) fields->members.push_back({k, JsonValue::makeDouble(v)});
                    mj->set("fields", std::move(fields));
                }
                if (!m.pathSteps.empty()) {
                    auto steps = JsonValue::makeArray();
                    for (const auto& s : m.pathSteps) {
                        auto sj = JsonValue::makeObject();
                        sj->set("x", JsonValue::makeInt(s.destX));
                        sj->set("y", JsonValue::makeInt(s.destY));
                        sj->set("speed", JsonValue::makeInt(s.speed));
                        sj->set("direction", JsonValue::makeInt(s.direction));
                        steps->push(std::move(sj));
                    }
                    mj->set("pathSteps", std::move(steps));
                }
                movs->push(std::move(mj));
            }
            common->set("movements", std::move(movs));
        }
        if (!c.animations.items.empty()) {
            auto anims = JsonValue::makeObject();
            for (const auto& [animKey, dirs] : c.animations.items) {
                if (dirs.empty()) continue;
                auto anim = JsonValue::makeObject();
                for (const auto& [dirKey, dir] : dirs) {
                    if (dir.frames.empty()) continue;
                    auto dj = JsonValue::makeObject();
                    dj->set("minSpeed", JsonValue::makeInt(dir.minSpeed));
                    dj->set("maxSpeed", JsonValue::makeInt(dir.maxSpeed));
                    dj->set("repeat", JsonValue::makeInt(dir.repeat));
                    dj->set("backTo", JsonValue::makeInt(dir.backTo));
                    auto frames = JsonValue::makeArray();
                    for (uint16_t f : dir.frames) frames->push(JsonValue::makeInt(f));
                    dj->set("frames", std::move(frames));
                    anim->members.push_back({std::to_string(dirKey), std::move(dj)});
                }
                anims->members.push_back({std::to_string(animKey), std::move(anim)});
            }
            common->set("animations", std::move(anims));
        }
        if (c.counter) {
            auto cnt = JsonValue::makeObject();
            cnt->set("displayType", JsonValue::makeInt(c.counter->displayType));
            cnt->set("player", JsonValue::makeInt(c.counter->player));
            auto frames = JsonValue::makeArray();
            for (uint16_t f : c.counter->frames) frames->push(JsonValue::makeInt(f));
            cnt->set("frames", std::move(frames));
            common->set("counter", std::move(cnt));
        }
        if (c.text) {
            auto text = JsonValue::makeObject();
            text->set("width", JsonValue::makeInt(c.text->width));
            text->set("height", JsonValue::makeInt(c.text->height));
            auto paras = JsonValue::makeArray();
            for (const auto& p : c.text->paragraphs) {
                auto pj = JsonValue::makeObject();
                pj->set("font", JsonValue::makeInt(p.font));
                pj->set("value", JsonValue::makeString(p.value));
                pj->set("flags", bitDictJson(p.flags));
                pj->set("color", colorJson(p.color));
                paras->push(std::move(pj));
            }
            text->set("paragraphs", std::move(paras));
            common->set("text", std::move(text));
        }
        if (c.subApplication) {
            auto sub = JsonValue::makeObject();
            sub->set("cx", JsonValue::makeInt(c.subApplication->cx));
            sub->set("cy", JsonValue::makeInt(c.subApplication->cy));
            sub->set("startFrame", JsonValue::makeInt(c.subApplication->startFrame));
            sub->set("options", JsonValue::makeInt(c.subApplication->options));
            common->set("subApplication", std::move(sub));
        }
        j->set("common", std::move(common));
    }
    return j;
}

static JsonPtr frameJson(const Frame& frame, const std::vector<std::string>& frameNames) {
    auto j = JsonValue::makeObject();
    j->set("name", JsonValue::makeString(frame.name));
    j->set("width", JsonValue::makeInt(frame.width));
    j->set("height", JsonValue::makeInt(frame.height));
    j->set("background", colorJson(frame.background));
    j->set("flags", bitDictJson(frame.flags));

    auto fadeJson = [](const Transition& t) {
        auto fj = JsonValue::makeObject();
        fj->set("module", JsonValue::makeString(t.module));
        fj->set("name", JsonValue::makeString(t.name));
        fj->set("duration", JsonValue::makeInt(t.duration));
        fj->set("flags", JsonValue::makeInt(t.flags));
        fj->set("color", colorJson(t.color));
        if (!t.moduleFile.empty()) fj->set("moduleFile", JsonValue::makeString(t.moduleFile));
        return fj;
    };
    if (frame.fadeIn) j->set("fadeIn", fadeJson(*frame.fadeIn));
    if (frame.fadeOut) j->set("fadeOut", fadeJson(*frame.fadeOut));

    auto instances = JsonValue::makeArray();
    for (const auto& inst : frame.objects) {
        auto ij = JsonValue::makeObject();
        ij->set("handle", JsonValue::makeInt(inst.handle));
        ij->set("objectInfo", JsonValue::makeInt(inst.objectInfo));
        ij->set("x", JsonValue::makeInt(inst.x));
        ij->set("y", JsonValue::makeInt(inst.y));
        ij->set("layer", JsonValue::makeInt(inst.layer));
        ij->set("parentType", JsonValue::makeInt(inst.parentType));
        ij->set("parentHandle", JsonValue::makeInt(inst.parentHandle));
        instances->push(std::move(ij));
    }
    j->set("instances", std::move(instances));

    if (!frame.layers.empty()) {
        auto layers = JsonValue::makeArray();
        for (const auto& layer : frame.layers) {
            auto lj = JsonValue::makeObject();
            lj->set("name", JsonValue::makeString(layer.name));
            lj->set("flags", bitDictJson(layer.flags));
            lj->set("xCoefficient", JsonValue::makeDouble(layer.xCoeff));
            lj->set("yCoefficient", JsonValue::makeDouble(layer.yCoeff));
            layers->push(std::move(lj));
        }
        j->set("layers", std::move(layers));
    }

    if (frame.events) {
        auto ev = JsonValue::makeObject();
        ev->set("maxObjects", JsonValue::makeInt(frame.events->maxObjects));
        ev->set("maxObjectInfo", JsonValue::makeInt(frame.events->maxObjectInfo));
        ev->set("numberOfPlayers", JsonValue::makeInt(frame.events->numberOfPlayers));
        ev->set("optionFlags", JsonValue::makeInt(frame.events->optionFlags));

        auto groups = JsonValue::makeArray();
        int groupIdx = 0;
        for (const auto& group : frame.events->groups) {
            auto gj = JsonValue::makeObject();
            gj->set("index", JsonValue::makeInt(groupIdx++));
            gj->set("flags", JsonValue::makeInt(group.flags));
            gj->set("isRestricted", JsonValue::makeInt(group.isRestricted));
            gj->set("restrictCpt", JsonValue::makeInt(group.restrictCpt));
            auto conds = JsonValue::makeArray();
            for (const auto& c : group.conditions) conds->push(conditionJson(c));
            gj->set("conditions", std::move(conds));
            auto acts = JsonValue::makeArray();
            for (const auto& a : group.actions) acts->push(actionJson(a, &frameNames));
            gj->set("actions", std::move(acts));
            groups->push(std::move(gj));
        }
        ev->set("groups", std::move(groups));
        j->set("events", std::move(ev));
    }
    return j;
}

void JsonExportTool::execute(GameData& game) {
    std::string dir = pathJoin(g_dumpsDir, sanitizeGameName(game.name.empty() ? "Unknown Game" : game.name),
                               "JSON");
    createDirectories(dir);
    Logger::log("Exporting structure as JSON...");

    // ---- application.json
    auto app = JsonValue::makeObject();
    app->set("name", JsonValue::makeString(game.name));
    app->set("author", JsonValue::makeString(game.author));
    app->set("copyright", JsonValue::makeString(game.copyright));
    app->set("editorFilename", JsonValue::makeString(game.editorFilename));
    app->set("targetFilename", JsonValue::makeString(game.targetFilename));
    app->set("productVersion", JsonValue::makeInt(game.productVersion));
    app->set("productBuild", JsonValue::makeInt(game.productBuild));
    app->set("exeOnly", JsonValue::makeBool(game.exeOnly));
    if (game.header) {
        auto h = JsonValue::makeObject();
        h->set("windowWidth", JsonValue::makeInt(game.header->windowWidth));
        h->set("windowHeight", JsonValue::makeInt(game.header->windowHeight));
        h->set("numberOfFrames", JsonValue::makeInt(game.header->numberOfFrames));
        h->set("frameRate", JsonValue::makeInt(game.header->frameRate));
        h->set("initialScore", JsonValue::makeInt(game.header->initialScore));
        h->set("initialLives", JsonValue::makeInt(game.header->initialLives));
        h->set("flags", bitDictJson(game.header->flags));
        h->set("newFlags", bitDictJson(game.header->newFlags));
        h->set("otherFlags", bitDictJson(game.header->otherFlags));
        app->set("header", std::move(h));
    }
    if (game.extHeader) {
        auto h = JsonValue::makeObject();
        h->set("buildType", JsonValue::makeInt(game.extHeader->buildType));
        h->set("buildTypeName", JsonValue::makeString(game.extHeader->buildTypeName()));
        app->set("extendedHeader", std::move(h));
    }

    auto globals = JsonValue::makeObject();
    auto gv = JsonValue::makeArray();
    for (const auto& v : game.globalValues.items)
        gv->push(v.isFloat ? JsonValue::makeDouble(v.floatValue) : JsonValue::makeInt(v.intValue));
    globals->set("values", std::move(gv));
    auto gs = JsonValue::makeArray();
    for (const auto& s : game.globalStrings.items) gs->push(JsonValue::makeString(s));
    globals->set("strings", std::move(gs));
    app->set("globals", std::move(globals));

    if (!game.extensions.items.empty()) {
        auto exts = JsonValue::makeArray();
        for (const auto& e : game.extensions.items) {
            auto ej = JsonValue::makeObject();
            ej->set("handle", JsonValue::makeInt(e.handle));
            ej->set("name", JsonValue::makeString(e.name));
            ej->set("subType", JsonValue::makeString(e.subType));
            ej->set("magic", JsonValue::makeInt(e.magicNumber));
            exts->push(std::move(ej));
        }
        app->set("extensions", std::move(exts));
    }

    auto objects = JsonValue::makeArray();
    for (const auto& [handle, info] : game.frameItems)
        if (info) objects->push(objectJson(*info));
    app->set("objects", std::move(objects));

    std::vector<std::string> frameNames;
    for (const auto& f : game.frames) frameNames.push_back(f ? f->name : std::string());

    auto frames = JsonValue::makeArray();
    for (const auto& frame : game.frames)
        if (frame) frames->push(frameJson(*frame, frameNames));
    app->set("frames", std::move(frames));

    Logger::debug("serializing application.json...");
    std::string appDump = app->dump();
    Logger::debug("app dump size=" + std::to_string(appDump.size()));
    writeFileBytes(pathJoin(dir, "application.json"),
                   std::vector<uint8_t>(appDump.begin(), appDump.end()));
    Logger::debug("application.json written");

    // ---- banks.json (metadata only)
    auto banks = JsonValue::makeObject();
    auto imgs = JsonValue::makeArray();
    for (const auto& [handle, image] : game.images.items) {
        if (!image) continue;
        auto ij = JsonValue::makeObject();
        ij->set("handle", JsonValue::makeInt(handle));
        ij->set("width", JsonValue::makeInt(image->width));
        ij->set("height", JsonValue::makeInt(image->height));
        ij->set("graphicMode", JsonValue::makeInt(image->graphicMode));
        ij->set("flags", JsonValue::makeInt(image->flags.raw()));
        ij->set("hotspotX", JsonValue::makeInt(image->hotX));
        ij->set("hotspotY", JsonValue::makeInt(image->hotY));
        imgs->push(std::move(ij));
    }
    banks->set("images", std::move(imgs));

    auto snds = JsonValue::makeArray();
    for (const auto& snd : game.sounds.items) {
        if (!snd) continue;
        auto sj = JsonValue::makeObject();
        sj->set("handle", JsonValue::makeInt(snd->handle));
        sj->set("name", JsonValue::makeString(snd->name));
        sj->set("flags", JsonValue::makeInt(snd->flags));
        sj->set("size", JsonValue::makeInt(snd->data.size()));
        snds->push(std::move(sj));
    }
    banks->set("sounds", std::move(snds));

    if (!game.packData.items.empty()) {
        auto packs = JsonValue::makeArray();
        for (const auto& p : game.packData.items) {
            auto pj = JsonValue::makeObject();
            pj->set("name", JsonValue::makeString(p.packFilename));
            pj->set("compressed", JsonValue::makeBool(p.compressed));
            pj->set("size", JsonValue::makeInt(p.data.size()));
            packs->push(std::move(pj));
        }
        banks->set("packData", std::move(packs));
    }
    Logger::debug("serializing banks.json...");
    std::string banksDump = banks->dump();
    Logger::debug("banks dump size=" + std::to_string(banksDump.size()));
    writeFileBytes(pathJoin(dir, "banks.json"),
                   std::vector<uint8_t>(banksDump.begin(), banksDump.end()));

    Logger::log("JSON export done.");
}

} // namespace ctfak
