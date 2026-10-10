// Frame Replay (v2.66): composes every frame the way the Clickteam Fusion
// 2.5 engine draws it at frame start. Per frame:
//   1. fill with the frame background color;
//   2. draw the instances in LAYER order (stable within a layer), each with
//      its hotspot applied (x-hotX, y-hotY) at native size;
//   3. apply the instance's own ink effect (inkEffect 1 = semi-transparency
//      with inkEffectValue as the coefficient) and RGB coefficient (channel
//      tint) - the "dim/tinted" looks the older Frame Layout tool ignored;
//   4. Quick Backdrops: solid (fillType 0) or vertical/horizontal gradient
//      between shape.color1 and shape.color2.
// The first animation frame is the baseline (runtime animation is out of
// scope). Output: <dumpdir>/FrameReplay/frame_<i>_<name>.png per frame.
#include "FrameReplay.h"
#include "FrameLayout.h"
#include "Dumpers.h"
#include "../io/PngWriter.h"
#include "../util/Config.h"
#include "../util/Utils.h"
#include "../core/Logger.h"
#include "../format/GameData.h"
#include "../format/ImageBank.h"
#include "../format/ObjectInfo.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace ctfak {

namespace {

// src-over blend with the per-instance ink effect and RGB coefficient.
// alphaEff = the effective alpha: the image alpha, then the ink-effect
// coefficient (inkEffect 1) scaled by inkEffectValue/255. The channels are
// multiplied by rgbCoeff (0..255 => 0..1.0).
void blendCoeff(std::vector<uint8_t>& canvas, int cw, int ch,
                const ImageItem& img, int dstX, int dstY,
                uint8_t inkEffect, uint8_t inkEffectValue,
                const BinaryReader::Color& coeff) {
    if (img.width <= 0 || img.height <= 0) return;
    if (img.rgba.size() < static_cast<size_t>(img.width) * img.height * 4) return;
    const float cr = coeff.r / 255.0f;
    const float cg = coeff.g / 255.0f;
    const float cb = coeff.b / 255.0f;
    const bool semi = (inkEffect == 1);
    const float kCoeff = semi ? (inkEffectValue / 255.0f) : 1.0f;
    int x0 = std::max(dstX, 0), y0 = std::max(dstY, 0);
    int x1 = std::min(dstX + img.width, cw), y1 = std::min(dstY + img.height, ch);
    for (int y = y0; y < y1; ++y) {
        const uint8_t* srcRow = &img.rgba[static_cast<size_t>(y - dstY) * img.width * 4];
        uint8_t* dstRow = &canvas[static_cast<size_t>(y) * cw * 4];
        for (int x = x0; x < x1; ++x) {
            const uint8_t* sp = srcRow + static_cast<size_t>(x - dstX) * 4;
            float sa = (sp[3] / 255.0f) * kCoeff;
            if (sa <= 0.0f) continue;
            uint8_t* dp = dstRow + static_cast<size_t>(x) * 4;
            const float ia = 1.0f - sa;
            dp[0] = static_cast<uint8_t>(sp[0] * cr * sa + dp[0] * ia);
            dp[1] = static_cast<uint8_t>(sp[1] * cg * sa + dp[1] * ia);
            dp[2] = static_cast<uint8_t>(sp[2] * cb * sa + dp[2] * ia);
            dp[3] = 255;
        }
    }
}

void gradientRect(std::vector<uint8_t>& canvas, int cw, int ch,
                  int x0, int y0, int x1, int y1,
                  const BinaryReader::Color& c1, const BinaryReader::Color& c2,
                  bool vertical) {
    x0 = std::max(x0, 0); y0 = std::max(y0, 0);
    x1 = std::min(x1, cw - 1); y1 = std::min(y1, ch - 1);
    const int span = vertical ? std::max(1, y1 - y0) : std::max(1, x1 - x0);
    for (int y = y0; y <= y1; ++y) {
        uint8_t* row = &canvas[static_cast<size_t>(y) * cw * 4];
        for (int x = x0; x <= x1; ++x) {
            const float t = vertical ? (float)(y - y0) / span : (float)(x - x0) / span;
            uint8_t* p = row + static_cast<size_t>(x) * 4;
            p[0] = static_cast<uint8_t>(c1.r + (c2.r - c1.r) * t);
            p[1] = static_cast<uint8_t>(c1.g + (c2.g - c1.g) * t);
            p[2] = static_cast<uint8_t>(c1.b + (c2.b - c1.b) * t);
            p[3] = 255;
        }
    }
}

} // namespace

void FrameReplayTool::execute(GameData& game) {
    const std::string dir = pathJoin(g_dumpsDir,
                                     sanitizeGameName(game.name.empty()
                                                          ? "Unknown Game"
                                                          : game.name),
                                     "FrameReplay");
    createDirectories(dir);

    int done = 0;
    for (size_t fi = 0; fi < game.frames.size(); ++fi) {
        const Frame& frame = *game.frames[fi];
        const int W = std::max(frame.width, 1);
        const int H = std::max(frame.height, 1);
        std::vector<uint8_t> canvas(static_cast<size_t>(W) * H * 4, 0);

        // 1. the frame background color
        for (size_t i = 0; i < canvas.size(); i += 4) {
            canvas[i + 0] = frame.background.r;
            canvas[i + 1] = frame.background.g;
            canvas[i + 2] = frame.background.b;
            canvas[i + 3] = 255;
        }

        // 2. the instances in layer order (stable within a layer)
        std::vector<const ObjectInstance*> insts;
        for (const auto& inst : frame.objects) insts.push_back(&inst);
        std::stable_sort(insts.begin(), insts.end(),
                         [](const ObjectInstance* a, const ObjectInstance* b) {
                             return a->layer < b->layer;
                         });

        for (const ObjectInstance* inst : insts) {
            auto it = game.frameItems.find(inst->objectInfo);
            const ObjectInfo* obj = it != game.frameItems.end() ? it->second.get() : nullptr;
            if (!obj) continue;
            const auto& common = obj->common;

            if (obj->objectType == kObjQuickBackdrop && obj->quickBackdrop) {
                const auto& sh = obj->quickBackdrop->shape;
                const bool grad = sh.fillType != 0;
                if (grad)
                    gradientRect(canvas, W, H, inst->x, inst->y,
                                 inst->x + obj->quickBackdrop->width - 1,
                                 inst->y + obj->quickBackdrop->height - 1,
                                 sh.color1, sh.color2, true);
                else
                    gradientRect(canvas, W, H, inst->x, inst->y,
                                 inst->x + obj->quickBackdrop->width - 1,
                                 inst->y + obj->quickBackdrop->height - 1,
                                 sh.color1, sh.color1, false);
                continue;
            }

            const int32_t handle = resolveStartImage(*obj);
            if (handle < 0) continue;
            auto img = game.images.find(handle);
            if (!img || img->width <= 0 || img->height <= 0) continue;
            if (img->rgba.empty()) img->decode();

            uint8_t ink = obj->inkEffect;
            uint8_t inkV = obj->inkEffectValue;
            BinaryReader::Color coeff = obj->rgbCoeff;
            blendCoeff(canvas, W, H, *img,
                       inst->x - img->hotX, inst->y - img->hotY, ink, inkV, coeff);
        }

        const std::string base = "frame_" + std::to_string(fi) + "_" +
                                 (frame.name.empty() ? std::string("unnamed")
                                                     : clearName(frame.name));
        PngWriter::writeFile(pathJoin(dir, base + ".png"), W, H, canvas);
        Logger::log("FrameReplay: \"" +
                    (frame.name.empty() ? std::string("(unnamed)") : frame.name) +
                    "\" " + std::to_string(W) + "x" + std::to_string(H));
        done += 1;
    }
    Logger::log("Frame Replay: " + std::to_string(done) + " frames -> " + dir);
}

} // namespace ctfak
