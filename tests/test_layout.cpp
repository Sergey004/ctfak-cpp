#include "test_framework.h"
#include "plugins/FrameLayout.h"
#include "plugins/Font5x7.h"
#include "format/ImageBank.h"
#include "format/ObjectInfo.h"
#include <cstring>
#include <vector>

using namespace ctfak;

namespace {

ImageItem makeImage(int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                    int hotX = 0, int hotY = 0) {
    ImageItem img;
    img.handle = 1;
    img.width = w;
    img.height = h;
    img.hotX = static_cast<int16_t>(hotX);
    img.hotY = static_cast<int16_t>(hotY);
    img.rgba.assign(static_cast<size_t>(w) * h * 4, 0);
    for (size_t i = 0; i < img.rgba.size(); i += 4) {
        img.rgba[i] = r;
        img.rgba[i + 1] = g;
        img.rgba[i + 2] = b;
        img.rgba[i + 3] = a;
    }
    return img;
}

std::vector<uint8_t> makeCanvas(int w, int h, uint8_t r, uint8_t g, uint8_t b) {
    std::vector<uint8_t> canvas(static_cast<size_t>(w) * h * 4);
    for (size_t i = 0; i < canvas.size(); i += 4) {
        canvas[i] = r;
        canvas[i + 1] = g;
        canvas[i + 2] = b;
        canvas[i + 3] = 255;
    }
    return canvas;
}

const uint8_t* pixel(std::vector<uint8_t>& c, int w, int x, int y) {
    return &c[(static_cast<size_t>(y) * w + x) * 4];
}

} // namespace

TEST(layout_blend_opaque_replaces) {
    const int W = 2, H = 1;
    auto canvas = makeCanvas(W, H, 0, 0, 255);
    auto img = makeImage(1, 1, 255, 0, 0, 255);
    canvasBlendImage(canvas, W, H, img, 0, 0);
    EXPECT_EQ(pixel(canvas, W, 0, 0)[0], 255);
    EXPECT_EQ(pixel(canvas, W, 0, 0)[1], 0);
    EXPECT_EQ(pixel(canvas, W, 0, 0)[2], 0);
    EXPECT_EQ(pixel(canvas, W, 0, 0)[3], 255);
    // second pixel untouched
    EXPECT_EQ(pixel(canvas, W, 1, 0)[2], 255);
}

TEST(layout_blend_semitransparent_averages) {
    const int W = 1, H = 1;
    auto canvas = makeCanvas(W, H, 0, 0, 0);
    // 50% white over black -> ~128
    auto img = makeImage(1, 1, 255, 255, 255, 128);
    canvasBlendImage(canvas, W, H, img, 0, 0);
    EXPECT_EQ(pixel(canvas, W, 0, 0)[0], static_cast<uint8_t>((255 * 128) / 255));
}

TEST(layout_blend_zero_alpha_noop) {
    const int W = 1, H = 1;
    auto canvas = makeCanvas(W, H, 10, 20, 30);
    auto img = makeImage(1, 1, 255, 255, 255, 0);
    canvasBlendImage(canvas, W, H, img, 0, 0);
    EXPECT_EQ(pixel(canvas, W, 0, 0)[0], 10);
    EXPECT_EQ(pixel(canvas, W, 0, 0)[1], 20);
    EXPECT_EQ(pixel(canvas, W, 0, 0)[2], 30);
}

TEST(layout_blend_clips_out_of_bounds) {
    const int W = 4, H = 4;
    auto canvas = makeCanvas(W, H, 0, 0, 0);
    auto img = makeImage(2, 2, 99, 99, 99, 255);
    // fully outside on every side - must not crash nor touch the canvas
    canvasBlendImage(canvas, W, H, img, -10, -10);
    canvasBlendImage(canvas, W, H, img, W + 5, 0);
    canvasBlendImage(canvas, W, H, img, 0, H + 5);
    for (size_t i = 0; i < canvas.size(); i += 4) {
        EXPECT_EQ(canvas[i], 0);
        EXPECT_EQ(canvas[i + 3], 255);
    }
    // partially overlapping: 1x1 visible
    canvasBlendImage(canvas, W, H, img, -1, -1);
    EXPECT_EQ(pixel(canvas, W, 0, 0)[0], 99);
}

TEST(layout_fill_and_outline_clipped) {
    const int W = 5, H = 5;
    auto canvas = makeCanvas(W, H, 0, 0, 0);
    canvasFillRect(canvas, W, H, -3, -3, 100, 100, 7, 8, 9);
    EXPECT_EQ(pixel(canvas, W, 0, 0)[0], 7);
    EXPECT_EQ(pixel(canvas, W, 4, 4)[0], 7);
    auto c2 = makeCanvas(W, H, 0, 0, 0);
    canvasRectOutline(c2, W, H, -2, -2, 2, 2, 1, 2, 3);
    EXPECT_EQ(pixel(c2, W, 0, 0)[0], 0);   // interior stays background
    EXPECT_EQ(pixel(c2, W, 2, 0)[0], 1);   // right edge
    EXPECT_EQ(pixel(c2, W, 2, 2)[0], 1);   // bottom-right corner
    EXPECT_EQ(pixel(c2, W, 0, 2)[0], 1);   // bottom edge
}

TEST(layout_draw_text_glyphs_and_ascii_filter) {
    const int W = 40, H = 12;
    auto canvas = makeCanvas(W, H, 0, 0, 0);
    canvasDrawText(canvas, W, H, 1, 1, "A", 255, 255, 0, 1);
    int lit = 0;
    for (size_t i = 0; i < canvas.size(); i += 4)
        if (canvas[i] == 255 && canvas[i + 1] == 255 && canvas[i + 2] == 0) ++lit;
    EXPECT_TRUE(lit > 0);
    // space and non-ASCII must draw nothing
    auto c2 = makeCanvas(W, H, 0, 0, 0);
    canvasDrawText(c2, W, H, 1, 1, " \xC3\xA9", 255, 255, 0, 1);
    for (size_t i = 0; i < c2.size(); i += 4)
        if (c2[i] == 255 && c2[i + 1] == 255 && c2[i + 2] == 0) {
            EXPECT_TRUE(false && "space/non-ascii drew a glyph");
            break;
        }
    // clipping: long text off the right edge must not crash
    auto c3 = makeCanvas(W, H, 0, 0, 0);
    canvasDrawText(c3, W, H, W - 3, H - 3, "VERY LONG LABEL 123456", 200, 200, 200, 1);
}

TEST(layout_font_table_shape) {
    // space must be blank; every other glyph must have at least one lit pixel
    for (int c = 0; c < 95; ++c) {
        int lit = 0;
        for (int col = 0; col < 5; ++col)
            lit += __builtin_popcount(kFont5x7[c][col]);
        if (c == 0) {
            EXPECT_EQ(lit, 0);
        } else {
            EXPECT_TRUE(lit > 0);
        }
    }
}

TEST(layout_draw_grid_and_cross) {
    const int W = 210, H = 110;
    auto canvas = makeCanvas(W, H, 0, 0, 0);
    canvasDrawGrid(canvas, W, H, 50, 100);
    // minor vertical line at x=50
    EXPECT_EQ(pixel(canvas, W, 50, 70)[0], 46);
    EXPECT_EQ(pixel(canvas, W, 50, 70)[2], 56);
    // major vertical line at x=100 brighter
    EXPECT_EQ(pixel(canvas, W, 100, 70)[0], 92);
    EXPECT_EQ(pixel(canvas, W, 100, 70)[2], 110);
    // no grid at x=30
    EXPECT_EQ(pixel(canvas, W, 30, 70)[0], 0);
    // minor horizontal line at y=50
    EXPECT_EQ(pixel(canvas, W, 70, 50)[0], 46);
    // crosshair: center + arms lit, diagonal not
    auto c2 = makeCanvas(20, 20, 0, 0, 0);
    canvasDrawCross(c2, 20, 20, 10, 10, 255, 0, 0);
    EXPECT_EQ(pixel(c2, 20, 10, 10)[0], 255);
    EXPECT_EQ(pixel(c2, 20, 10, 5)[0], 255);   // vertical arm
    EXPECT_EQ(pixel(c2, 20, 15, 10)[0], 255);  // horizontal arm
    EXPECT_EQ(pixel(c2, 20, 13, 13)[0], 0);    // diagonal untouched
    // off-canvas cross must not crash
    canvasDrawCross(c2, 20, 20, -50, -50, 0, 255, 0);
    canvasDrawCross(c2, 20, 20, 500, 500, 0, 255, 0);
}

TEST(layout_resolve_start_image_animation_first) {
    ObjectInfo oi;
    oi.backdrop = nullptr;
    oi.common = std::make_shared<ObjectCommon>();
    AnimationDirection dir;
    dir.frames = {7, 8, 9};
    oi.common->animations.items[0][0] = dir;
    EXPECT_EQ(resolveStartImage(oi), 7);

    // backdrop wins over animations
    ObjectInfo ob;
    ob.backdrop = std::make_unique<Backdrop>();
    ob.backdrop->image = 42;
    EXPECT_EQ(resolveStartImage(ob), 42);

    // counter frame as fallback
    ObjectInfo oc;
    oc.common = std::make_shared<ObjectCommon>();
    oc.common->counter = std::make_unique<SystemCounter>();
    oc.common->counter->frames = {77, 78};
    EXPECT_EQ(resolveStartImage(oc), 77);

    // nothing to show
    ObjectInfo on;
    EXPECT_EQ(resolveStartImage(on), -1);
}
