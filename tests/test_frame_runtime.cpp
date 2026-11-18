#include "test_framework.h"
#include "plugins/FrameRuntime.h"
#include "format/Frame.h"
#include "format/ImageBank.h"
#include "format/ObjectInfo.h"
#include <memory>
#include <vector>

using namespace ctfak;

namespace {

ObjectInfo makeObject() {
    ObjectInfo obj;
    obj.blend = 255;
    obj.rgbCoeff = {255, 255, 255, 255};
    obj.common = std::make_shared<ObjectCommon>();
    obj.common->newFlags.setBit("VisibleAtStart", true);
    return obj;
}

ImageItem whiteImage(int w = 1, int h = 1) {
    ImageItem img;
    img.width = w;
    img.height = h;
    img.rgba.assign(static_cast<size_t>(w) * h * 4, 255);
    return img;
}

std::vector<uint8_t> blackCanvas(int w, int h) {
    std::vector<uint8_t> canvas(static_cast<size_t>(w) * h * 4, 0);
    for (size_t i = 3; i < canvas.size(); i += 4) canvas[i] = 255;
    return canvas;
}

} // namespace

TEST(frame_runtime_alpha_uses_inverse_ctf_coefficient) {
    auto obj = makeObject();
    obj.inkEffect = 1;
    obj.inkEffectValue = 0;
    EXPECT_EQ(runtimeObjectAlpha(obj), 255);
    obj.inkEffectValue = 100;
    EXPECT_EQ(runtimeObjectAlpha(obj), 155);
    obj.inkEffectValue = 255;
    EXPECT_EQ(runtimeObjectAlpha(obj), 0);
    obj.inkEffect = 0;
    obj.blend = 173;
    EXPECT_EQ(runtimeObjectAlpha(obj), 173);
}

TEST(frame_runtime_visibility_flags) {
    auto obj = makeObject();
    EXPECT_TRUE(runtimeObjectVisibleAtStart(obj));
    obj.common->flags.setBit("DoNotCreateAtStart", true);
    EXPECT_TRUE(!runtimeObjectVisibleAtStart(obj));
    obj.common->flags.setBit("DoNotCreateAtStart", false);
    obj.common->newFlags.setBit("VisibleAtStart", false);
    EXPECT_TRUE(!runtimeObjectVisibleAtStart(obj));
    ObjectInfo backdrop;
    EXPECT_TRUE(runtimeObjectVisibleAtStart(backdrop));
}

TEST(frame_runtime_layer_visibility) {
    Frame frame;
    frame.layers.resize(1);
    frame.layers[0].flags.setBit("Visible", true);
    EXPECT_TRUE(runtimeLayerVisibleAtStart(frame, 0));
    frame.layers[0].flags.setBit("Visible", false);
    EXPECT_TRUE(!runtimeLayerVisibleAtStart(frame, 0));
    EXPECT_TRUE(runtimeLayerVisibleAtStart(frame, 3));
}

TEST(frame_runtime_start_direction_and_fallback) {
    auto obj = makeObject();
    obj.common->animations.items[0][2].frames = {11};
    obj.common->animations.items[0][7].frames = {22};
    Movement movement;
    movement.directionAtStart = 7;
    obj.common->movements.push_back(movement);
    EXPECT_EQ(resolveRuntimeStartImage(obj), 22);
    obj.common->movements[0].directionAtStart = 5;
    EXPECT_EQ(resolveRuntimeStartImage(obj), 11);
    obj.common->movements.clear();
    EXPECT_EQ(resolveRuntimeStartImage(obj), 11);
}

TEST(frame_runtime_counter_fallback) {
    auto obj = makeObject();
    obj.common->counter = std::make_unique<SystemCounter>();
    obj.common->counter->frames = {33, 34};
    EXPECT_EQ(resolveRuntimeStartImage(obj), 33);
}

TEST(frame_runtime_effect_zero_is_neutral_not_black) {
    const BinaryReader::Color black{0, 0, 0, 255};
    const auto neutral = runtimeEffectCoeff(0, black);
    EXPECT_EQ(neutral.r, 255);
    EXPECT_EQ(neutral.g, 255);
    EXPECT_EQ(neutral.b, 255);
    const auto active = runtimeEffectCoeff(1, black);
    EXPECT_EQ(active.r, 0);
    EXPECT_EQ(active.g, 0);
    EXPECT_EQ(active.b, 0);
}

TEST(frame_runtime_blend_alpha_and_clipping) {
    auto obj = makeObject();
    obj.inkEffect = 1;
    obj.inkEffectValue = 100;
    auto img = whiteImage();
    auto canvas = blackCanvas(1, 1);
    runtimeBlendImage(canvas, 1, 1, img, 0, 0, obj);
    EXPECT_EQ(canvas[0], 155);
    EXPECT_EQ(canvas[1], 155);
    EXPECT_EQ(canvas[2], 155);
    EXPECT_EQ(canvas[3], 255);

    obj.inkEffectValue = 255;
    auto before = blackCanvas(2, 2);
    auto clipped = before;
    runtimeBlendImage(clipped, 2, 2, img, -20, -20, obj);
    runtimeBlendImage(clipped, 2, 2, img, 20, 20, obj);
    EXPECT_TRUE(clipped == before);
}

TEST(frame_runtime_tint_is_applied) {
    auto obj = makeObject();
    obj.rgbCoeff = {255, 0, 0, 255};
    auto canvas = blackCanvas(1, 1);
    runtimeBlendImage(canvas, 1, 1, whiteImage(), 0, 0, obj);
    EXPECT_EQ(canvas[0], 255);
    EXPECT_EQ(canvas[1], 0);
    EXPECT_EQ(canvas[2], 0);
}
