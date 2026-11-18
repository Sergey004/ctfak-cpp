#include "FrameRuntime.h"
#include "Dumpers.h"
#include "../io/PngWriter.h"
#include "../util/Config.h"
#include "../util/Utils.h"
#include "../core/Logger.h"
#include "../format/GameData.h"
#include "../format/Events.h"
#include "../format/ImageBank.h"
#include "../format/ObjectInfo.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace ctfak {

namespace {

struct RuntimeStyle {
    float r, g, b, a;
};

RuntimeStyle objectStyle(const ObjectInfo& obj) {
    const auto& c = obj.rgbCoeff;
    return { c.r / 255.0f, c.g / 255.0f, c.b / 255.0f,
             runtimeObjectAlpha(obj) / 255.0f };
}

void blendPixel(uint8_t* dp, float sr, float sg, float sb, float sa) {
    if (sa <= 0.0f) return;
    if (sa > 1.0f) sa = 1.0f;
    const float da = dp[3] / 255.0f;
    const float outA = sa + da * (1.0f - sa);
    if (outA <= 0.0f) {
        dp[0] = dp[1] = dp[2] = dp[3] = 0;
        return;
    }
    const float dstWeight = da * (1.0f - sa);
    dp[0] = static_cast<uint8_t>(std::clamp((sr * sa + dp[0] * dstWeight) / outA,
                                            0.0f, 255.0f));
    dp[1] = static_cast<uint8_t>(std::clamp((sg * sa + dp[1] * dstWeight) / outA,
                                            0.0f, 255.0f));
    dp[2] = static_cast<uint8_t>(std::clamp((sb * sa + dp[2] * dstWeight) / outA,
                                            0.0f, 255.0f));
    dp[3] = static_cast<uint8_t>(std::clamp(outA * 255.0f, 0.0f, 255.0f));
}

void blendImageStyled(std::vector<uint8_t>& canvas, int cw, int ch,
                      const ImageItem& img, int dstX, int dstY,
                      const RuntimeStyle& style) {
    if (img.width <= 0 || img.height <= 0 || cw <= 0 || ch <= 0) return;
    if (img.rgba.size() < static_cast<size_t>(img.width) * img.height * 4) return;
    const int x0 = std::max(dstX, 0), y0 = std::max(dstY, 0);
    const int x1 = std::min(dstX + img.width, cw);
    const int y1 = std::min(dstY + img.height, ch);
    for (int y = y0; y < y1; ++y) {
        const uint8_t* srcRow = &img.rgba[static_cast<size_t>(y - dstY) * img.width * 4];
        uint8_t* dstRow = &canvas[static_cast<size_t>(y) * cw * 4];
        for (int x = x0; x < x1; ++x) {
            const uint8_t* sp = srcRow + static_cast<size_t>(x - dstX) * 4;
            const float sa = (sp[3] / 255.0f) * style.a;
            blendPixel(dstRow + static_cast<size_t>(x) * 4,
                       sp[0] * style.r, sp[1] * style.g, sp[2] * style.b, sa);
        }
    }
}

void blendRect(std::vector<uint8_t>& canvas, int cw, int ch,
               int x0, int y0, int x1, int y1,
               const BinaryReader::Color& color, const RuntimeStyle& style) {
    x0 = std::max(x0, 0); y0 = std::max(y0, 0);
    x1 = std::min(x1, cw - 1); y1 = std::min(y1, ch - 1);
    for (int y = y0; y <= y1; ++y) {
        uint8_t* row = &canvas[static_cast<size_t>(y) * cw * 4];
        for (int x = x0; x <= x1; ++x) {
            blendPixel(row + static_cast<size_t>(x) * 4,
                       color.r * style.r, color.g * style.g, color.b * style.b, style.a);
        }
    }
}

void blendGradient(std::vector<uint8_t>& canvas, int cw, int ch,
                   int x0, int y0, int x1, int y1,
                   const QuickBackdrop& qb, const RuntimeStyle& style) {
    const auto& sh = qb.shape;
    if (sh.fillType != 2) {
        blendRect(canvas, cw, ch, x0, y0, x1, y1, sh.color1, style);
        return;
    }
    const int left = std::max(x0, 0), top = std::max(y0, 0);
    const int right = std::min(x1, cw - 1), bottom = std::min(y1, ch - 1);
    const int width = std::max(1, x1 - x0), height = std::max(1, y1 - y0);
    for (int y = top; y <= bottom; ++y) {
        for (int x = left; x <= right; ++x) {
            const float tx = static_cast<float>(x - x0) / width;
            const float ty = static_cast<float>(y - y0) / height;
            float t = tx; // gradFlags 0: horizontal
            if (sh.gradFlags == 1) t = ty;                    // vertical
            else if (sh.gradFlags == 2) t = (tx + ty) * 0.5f; // diagonal down
            else if (sh.gradFlags == 3) t = (tx + 1.0f - ty) * 0.5f; // diagonal up
            t = std::clamp(t, 0.0f, 1.0f);
            BinaryReader::Color c;
            c.r = static_cast<uint8_t>(sh.color1.r + (sh.color2.r - sh.color1.r) * t);
            c.g = static_cast<uint8_t>(sh.color1.g + (sh.color2.g - sh.color1.g) * t);
            c.b = static_cast<uint8_t>(sh.color1.b + (sh.color2.b - sh.color1.b) * t);
            blendPixel(&canvas[(static_cast<size_t>(y) * cw + x) * 4],
                       c.r * style.r, c.g * style.g, c.b * style.b, style.a);
        }
    }
}

void applyEffectToLayer(std::vector<uint8_t>& canvas,
                        const BinaryReader::Color& coeff) {
    for (size_t i = 0; i < canvas.size(); i += 4) {
        canvas[i + 0] = static_cast<uint8_t>(canvas[i + 0] * coeff.r / 255);
        canvas[i + 1] = static_cast<uint8_t>(canvas[i + 1] * coeff.g / 255);
        canvas[i + 2] = static_cast<uint8_t>(canvas[i + 2] * coeff.b / 255);
    }
}

void compositeLayer(std::vector<uint8_t>& base,
                    const std::vector<uint8_t>& layer) {
    for (size_t i = 0; i < base.size(); i += 4) {
        blendPixel(&base[i], layer[i], layer[i + 1], layer[i + 2],
                   layer[i + 3] / 255.0f);
    }
}

uint8_t samplePerspectiveChannel(const std::vector<uint8_t>& source,
                                 int cw, int ch, float sx, float sy, int channel) {
    sx = std::clamp(sx, 0.0f, static_cast<float>(cw - 1));
    sy = std::clamp(sy, 0.0f, static_cast<float>(ch - 1));
    const int x0 = std::clamp(static_cast<int>(std::floor(sx)), 0, cw - 1);
    const int y0 = std::clamp(static_cast<int>(std::floor(sy)), 0, ch - 1);
    const int x1 = std::min(x0 + 1, cw - 1), y1 = std::min(y0 + 1, ch - 1);
    const float tx = std::clamp(sx - std::floor(sx), 0.0f, 1.0f);
    const float ty = std::clamp(sy - std::floor(sy), 0.0f, 1.0f);
    const auto at = [&](int x, int y) -> float {
        return source[(static_cast<size_t>(y) * cw + x) * 4 + channel];
    };
    const float top = at(x0, y0) + (at(x1, y0) - at(x0, y0)) * tx;
    const float bottom = at(x0, y1) + (at(x1, y1) - at(x0, y1)) * tx;
    return static_cast<uint8_t>(std::clamp(top + (bottom - top) * ty + 0.5f,
                                           0.0f, 255.0f));
}

struct RuntimeReport {
    int sprites = 0;
    int quickBackdrops = 0;
    int noCreate = 0;
    int invisibleObjects = 0;
    int invisibleLayers = 0;
    int missing = 0;
    int text = 0;
    int customShaders = 0;
    int perspectivePasses = 0;
};

} // namespace

uint8_t runtimeObjectAlpha(const ObjectInfo& obj) {
    // CTF stores semi-transparency, NOT opacity: 0=opaque, 255=invisible.
    return obj.inkEffect == 1 ? static_cast<uint8_t>(255 - obj.inkEffectValue)
                              : obj.blend;
}

bool runtimeObjectVisibleAtStart(const ObjectInfo& obj) {
    if (!obj.common) return true;
    return !obj.common->flags["DoNotCreateAtStart"] &&
           obj.common->newFlags["VisibleAtStart"];
}

bool runtimeLayerVisibleAtStart(const Frame& frame, int16_t layer) {
    if (layer < 0 || static_cast<size_t>(layer) >= frame.layers.size()) return true;
    return frame.layers[layer].flags["Visible"];
}

int32_t resolveRuntimeStartImage(const ObjectInfo& obj) {
    if (obj.backdrop) return obj.backdrop->image;
    if (!obj.common) return -1;
    const auto& anims = obj.common->animations.items;
    if (!anims.empty()) {
        const auto& dirs = anims.begin()->second;
        for (const auto& movement : obj.common->movements) {
            auto dir = dirs.find(movement.directionAtStart);
            if (dir != dirs.end() && !dir->second.frames.empty())
                return dir->second.frames.front();
        }
        if (!dirs.empty() && !dirs.begin()->second.frames.empty())
            return dirs.begin()->second.frames.front();
    }
    if (obj.common->counter && !obj.common->counter->frames.empty())
        return obj.common->counter->frames.front();
    return -1;
}

BinaryReader::Color runtimeEffectCoeff(int16_t effect, BinaryReader::Color coeff) {
    if (effect == 0) return {255, 255, 255, 255};
    return coeff;
}

void runtimeBlendImage(std::vector<uint8_t>& canvas, int cw, int ch,
                       const ImageItem& img, int dstX, int dstY,
                       const ObjectInfo& obj) {
    blendImageStyled(canvas, cw, ch, img, dstX, dstY, objectStyle(obj));
}

RuntimePerspectiveGeometry runtimePerspectiveGeometry(const ObjectInstance& inst,
                                                       int windowW, int windowH) {
    // FNAF1's serialized object is 1324x754 at (-22,-22). Deriving the size
    // from those margins yields the same values, and keeps other viewports
    // from inheriting a hardcoded 1280x720 rectangle.
    const float orgX = static_cast<float>(inst.x), orgY = static_cast<float>(inst.y);
    const float objW = static_cast<float>(windowW) - 2.0f * std::min(orgX, 0.0f);
    const float objH = static_cast<float>(windowH) - 2.0f * std::min(orgY, 0.0f);
    return { objW, objH, orgX, orgY,
             static_cast<float>(windowW), static_cast<float>(windowH),
             300.0f, orgY + objH * 0.5f, 4.0f };
}

void runtimeApplyPerspective(std::vector<uint8_t>& canvas, int cw, int ch,
                             const RuntimePerspectiveGeometry& g) {
    if (cw <= 0 || ch <= 0 || g.objW <= 0.0f || g.objH <= 0.0f ||
        g.winW <= 0.0f || g.winH <= 0.0f) return;
    const std::vector<uint8_t> source = canvas;
    const float u0 = -g.orgX / g.objW;
    const float u1 = (g.winW - g.orgX) / g.objW;
    const float v0 = -g.orgY / g.objH;
    const float v1 = (g.winH - g.orgY) / g.objH;
    const float pivot = (g.centerY - g.orgY) / g.objH;
    const float fB = 1.0f - g.zoom / g.objH;

    for (int y = 0; y < ch; ++y) {
        for (int x = 0; x < cw; ++x) {
            const float u = u0 + ((x + 0.5f) / cw) * (u1 - u0);
            const float v = v0 + ((y + 0.5f) / ch) * (v1 - v0);
            const float a = u - 0.5f;
            const float fC = std::max(0.02f, 1.0f + (fB - 1.0f) * g.curve * a * a);
            const float srcY = (v - pivot) * fC + pivot;
            const float winX = (u * g.objW + g.orgX) / g.winW;
            const float winY = (srcY * g.objH + g.orgY) / g.winH;
            const float sx = winX * cw - 0.5f;
            const float sy = winY * ch - 0.5f;
            uint8_t* dp = &canvas[(static_cast<size_t>(y) * cw + x) * 4];
            for (int c = 0; c < 4; ++c)
                dp[c] = samplePerspectiveChannel(source, cw, ch, sx, sy, c);
        }
    }
}

void FrameRuntimeRenderer::execute(GameData& game) {
    const std::string dir = pathJoin(g_dumpsDir,
                                     sanitizeGameName(game.name.empty()
                                                          ? "Unknown Game" : game.name),
                                     "FrameRuntime");
    createDirectories(dir);
    int completed = 0;

    for (size_t fi = 0; fi < game.frames.size(); ++fi) {
        if (!game.frames[fi]) continue;
        const Frame& frame = *game.frames[fi];
        const int W = std::max(game.header ? game.header->windowWidth : frame.width, 1);
        const int H = std::max(game.header ? game.header->windowHeight : frame.height, 1);
        std::vector<uint8_t> canvas(static_cast<size_t>(W) * H * 4);
        std::vector<uint8_t> layerCanvas(canvas.size());
        for (size_t i = 0; i < canvas.size(); i += 4) {
            canvas[i] = frame.background.r;
            canvas[i + 1] = frame.background.g;
            canvas[i + 2] = frame.background.b;
            canvas[i + 3] = 255;
        }

        std::vector<const ObjectInstance*> instances;
        instances.reserve(frame.objects.size());
        for (const auto& inst : frame.objects) instances.push_back(&inst);
        std::stable_sort(instances.begin(), instances.end(),
                         [](const ObjectInstance* a, const ObjectInstance* b) {
                             return a->layer < b->layer;
                         });

        RuntimeReport stats;
        std::string notes;
        int perspectiveLayer = -1;
        RuntimePerspectiveGeometry perspectiveGeom{};
        size_t cursor = 0;
        while (cursor < instances.size()) {
            const int16_t layerIndex = instances[cursor]->layer;
            size_t end = cursor + 1;
            while (end < instances.size() && instances[end]->layer == layerIndex) ++end;
            std::fill(layerCanvas.begin(), layerCanvas.end(), 0);

            const Layer* layer = (layerIndex >= 0 &&
                                  static_cast<size_t>(layerIndex) < frame.layers.size())
                                     ? &frame.layers[layerIndex] : nullptr;
            if (layer && layer->shaderData.hasShader) {
                notes += "layer " + std::to_string(layerIndex) + " custom HWA shader '" +
                         layer->shaderData.name + "' not executed\n";
                ++stats.customShaders;
            }

            for (size_t n = cursor; n < end; ++n) {
                const ObjectInstance& inst = *instances[n];
                auto it = game.frameItems.find(inst.objectInfo);
                const ObjectInfo* obj = it != game.frameItems.end() ? it->second.get() : nullptr;
                if (!obj) {
                    notes += "objInfo " + std::to_string(inst.objectInfo) +
                             " missing from frame item bank\n";
                    ++stats.missing;
                    continue;
                }
                if (obj->common && obj->common->flags["DoNotCreateAtStart"]) {
                    ++stats.noCreate;
                    continue;
                }
                if (!runtimeObjectVisibleAtStart(*obj)) {
                    ++stats.invisibleObjects;
                    continue;
                }
                if (!runtimeLayerVisibleAtStart(frame, inst.layer)) {
                    ++stats.invisibleLayers;
                    continue;
                }
                if (obj->objectType == kObjText || (obj->common && obj->common->text)) {
                    notes += "text object '" + obj->name + "' not rasterized (TTF runtime is out of scope)\n";
                    ++stats.text;
                    continue;
                }
                if (obj->shaderData.hasShader) {
                    notes += "object '" + obj->name + "' custom HWA shader '" +
                             obj->shaderData.name + "' not executed; base image shown\n";
                    ++stats.customShaders;
                }

                if (obj->objectType == kObjExtension && obj->name == "Perspective") {
                    perspectiveLayer = inst.layer;
                    perspectiveGeom = runtimePerspectiveGeometry(inst, W, H);
                    continue;
                }

                if (obj->objectType == kObjQuickBackdrop && obj->quickBackdrop) {
                    const auto& qb = *obj->quickBackdrop;
                    blendGradient(layerCanvas, W, H, inst.x, inst.y,
                                  inst.x + qb.width - 1, inst.y + qb.height - 1,
                                  qb, objectStyle(*obj));
                    ++stats.quickBackdrops;
                    continue;
                }

                const int32_t handle = resolveRuntimeStartImage(*obj);
                if (handle < 0) {
                    notes += "object '" + obj->name + "' (" +
                             std::string(objectTypeName(obj->objectType)) +
                             ") has no frame-start image\n";
                    ++stats.missing;
                    continue;
                }
                auto img = game.images.find(handle);
                if (!img || img->width <= 0 || img->height <= 0) {
                    notes += "object '" + obj->name + "' image handle " +
                             std::to_string(handle) + " is missing or empty\n";
                    ++stats.missing;
                    continue;
                }
                if (img->rgba.empty()) img->decode();
                runtimeBlendImage(layerCanvas, W, H, *img,
                                  inst.x - img->hotX, inst.y - img->hotY, *obj);
                ++stats.sprites;
            }

            if (layer) {
                const auto coeff = runtimeEffectCoeff(layer->effect, layer->rgbCoeff);
                if (layer->effect != 0) applyEffectToLayer(layerCanvas, coeff);
            }
            compositeLayer(canvas, layerCanvas);
            if (perspectiveLayer == layerIndex) {
                runtimeApplyPerspective(canvas, W, H, perspectiveGeom);
                notes += "Perspective applied after layer " + std::to_string(layerIndex) +
                         " (CPU parabola, zoom 300, curve 4)\n";
                ++stats.perspectivePasses;
                perspectiveLayer = -1;
            }
            cursor = end;
        }

        if (frame.effect != 0) {
            applyEffectToLayer(canvas, runtimeEffectCoeff(frame.effect, frame.rgbCoeff));
        }
        if (frame.shaderData.hasShader) {
            notes += "frame custom HWA shader '" + frame.shaderData.name +
                     "' not executed\n";
            ++stats.customShaders;
        }
        if (frame.events && !frame.events->groups.empty()) {
            notes += std::to_string(frame.events->groups.size()) +
                     " event groups not executed (frame-start view only)\n";
        }

        const std::string base = "frame_" + std::to_string(fi) + "_" +
                                 (frame.name.empty() ? std::string("unnamed")
                                                     : clearName(frame.name));
        PngWriter::writeFile(pathJoin(dir, base + ".png"), W, H, canvas);

        char summary[512];
        std::snprintf(summary, sizeof(summary),
                      "Frame Runtime Renderer — frame-start only\n"
                      "frame: %s\noriginal world: %dx%d\nviewport: %dx%d at (0,0)\n"
                      "sprites: %d\nquick backdrops: %d\n"
                      "not created: %d\ninvisible objects: %d\ninstances on invisible layers: %d\n"
                      "missing object/image: %d\ntext objects not rasterized: %d\ncustom shaders not executed: %d\nPerspective passes applied: %d\n\n",
                      frame.name.c_str(), frame.width, frame.height, W, H,
                      stats.sprites, stats.quickBackdrops, stats.noCreate,
                      stats.invisibleObjects, stats.invisibleLayers, stats.missing,
                      stats.text, stats.customShaders, stats.perspectivePasses);
        std::string report = summary + notes;
        writeFileBytes(pathJoin(dir, base + ".txt"),
                       std::vector<uint8_t>(report.begin(), report.end()));

        Logger::log("FrameRuntime: " + frame.name + " -> " + std::to_string(W) + "x" +
                    std::to_string(H) + ", " + std::to_string(stats.sprites) + " sprites, " +
                    std::to_string(stats.text) + " text, " +
                    std::to_string(stats.customShaders) + " custom shaders skipped");
        ++completed;
    }
    Logger::log("Frame Runtime Renderer: " + std::to_string(completed) +
                " frame-start viewports -> " + dir);
}

} // namespace ctfak
