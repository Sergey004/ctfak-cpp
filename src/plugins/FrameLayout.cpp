#include "FrameLayout.h"
#include "Font5x7.h"
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

// ---------------------------------------------------------------- canvas helpers

void canvasBlendImage(std::vector<uint8_t>& canvas, int cw, int ch,
                      const ImageItem& img, int dstX, int dstY) {
    if (img.width <= 0 || img.height <= 0) return;
    if (img.rgba.size() < static_cast<size_t>(img.width) * img.height * 4) return;
    int x0 = std::max(dstX, 0), y0 = std::max(dstY, 0);
    int x1 = std::min(dstX + img.width, cw), y1 = std::min(dstY + img.height, ch);
    for (int y = y0; y < y1; ++y) {
        const uint8_t* srcRow = &img.rgba[static_cast<size_t>(y - dstY) * img.width * 4];
        uint8_t* dstRow = &canvas[static_cast<size_t>(y) * cw * 4];
        for (int x = x0; x < x1; ++x) {
            const uint8_t* sp = srcRow + static_cast<size_t>(x - dstX) * 4;
            uint8_t sa = sp[3];
            if (sa == 0) continue;
            uint8_t* dp = dstRow + static_cast<size_t>(x) * 4;
            if (sa == 255) {
                dp[0] = sp[0]; dp[1] = sp[1]; dp[2] = sp[2];
            } else {
                uint32_t ia = 255u - sa;
                dp[0] = static_cast<uint8_t>((sp[0] * sa + dp[0] * ia) / 255);
                dp[1] = static_cast<uint8_t>((sp[1] * sa + dp[1] * ia) / 255);
                dp[2] = static_cast<uint8_t>((sp[2] * sa + dp[2] * ia) / 255);
            }
            dp[3] = 255;
        }
    }
}

void canvasFillRect(std::vector<uint8_t>& canvas, int cw, int ch,
                    int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b) {
    x0 = std::max(x0, 0); y0 = std::max(y0, 0);
    x1 = std::min(x1, cw - 1); y1 = std::min(y1, ch - 1);
    for (int y = y0; y <= y1; ++y) {
        uint8_t* row = &canvas[static_cast<size_t>(y) * cw * 4];
        for (int x = x0; x <= x1; ++x) {
            uint8_t* p = row + static_cast<size_t>(x) * 4;
            p[0] = r; p[1] = g; p[2] = b; p[3] = 255;
        }
    }
}

void canvasRectOutline(std::vector<uint8_t>& canvas, int cw, int ch,
                       int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b) {
    if (x0 > x1) std::swap(x0, x1);
    if (y0 > y1) std::swap(y0, y1);
    canvasFillRect(canvas, cw, ch, x0, y0, x1, y0, r, g, b);
    canvasFillRect(canvas, cw, ch, x0, y1, x1, y1, r, g, b);
    canvasFillRect(canvas, cw, ch, x0, y0, x0, y1, r, g, b);
    canvasFillRect(canvas, cw, ch, x1, y0, x1, y1, r, g, b);
}

static void drawGlyph(std::vector<uint8_t>& canvas, int cw, int ch,
                      int x, int y, char c, uint8_t r, uint8_t g, uint8_t b, int scale) {
    if (c < 32 || c > 126) return;
    const uint8_t* glyph = kFont5x7[c - 32];
    for (int col = 0; col < 5; ++col) {
        uint8_t bits = glyph[col];
        if (!bits) continue;
        for (int row = 0; row < 7; ++row) {
            if (!((bits >> row) & 1)) continue;
            canvasFillRect(canvas, cw, ch, x + col * scale, y + row * scale,
                           x + col * scale + scale - 1, y + row * scale + scale - 1,
                           r, g, b);
        }
    }
}

void canvasDrawText(std::vector<uint8_t>& canvas, int cw, int ch,
                    int x, int y, const std::string& text,
                    uint8_t r, uint8_t g, uint8_t b, int scale) {
    if (scale < 1) scale = 1;
    // shadow pass for readability, then the main pass
    for (int pass = 0; pass < 2; ++pass) {
        int cx = x + (pass == 0 ? scale : 0);
        int cy = y + (pass == 0 ? scale : 0);
        for (char c : text) {
            if (c == '\n') { cy += 8 * scale; cx = x + (pass == 0 ? scale : 0); continue; }
            drawGlyph(canvas, cw, ch, cx, cy, c,
                      pass == 0 ? 0 : r, pass == 0 ? 0 : g, pass == 0 ? 0 : b, scale);
            cx += 6 * scale;
        }
    }
}

int32_t resolveStartImage(const ObjectInfo& oi) {
    if (oi.backdrop) return oi.backdrop->image;
    if (!oi.common) return -1;
    const auto& anims = oi.common->animations.items;
    if (!anims.empty()) {
        const auto& dirs = anims.begin()->second;
        if (!dirs.empty()) {
            const auto& frames = dirs.begin()->second.frames;
            if (!frames.empty()) return frames.front();
        }
    }
    if (oi.common->counter && !oi.common->counter->frames.empty())
        return oi.common->counter->frames.front();
    return -1;
}

void canvasDrawGrid(std::vector<uint8_t>& canvas, int cw, int ch, int minor, int major) {
    if (minor < 1 || major < minor) return;
    for (int gx = 0; gx < cw; gx += minor) {
        bool majorLine = gx % major == 0;
        if (majorLine)
            canvasFillRect(canvas, cw, ch, gx, 0, gx, ch - 1, 92, 92, 110);
        else
            canvasFillRect(canvas, cw, ch, gx, 0, gx, ch - 1, 46, 46, 56);
        if (majorLine && gx > 0)
            canvasDrawText(canvas, cw, ch, gx + 3, 3, std::to_string(gx), 168, 168, 195);
    }
    for (int gy = 0; gy < ch; gy += minor) {
        bool majorLine = gy % major == 0;
        if (majorLine)
            canvasFillRect(canvas, cw, ch, 0, gy, cw - 1, gy, 92, 92, 110);
        else
            canvasFillRect(canvas, cw, ch, 0, gy, cw - 1, gy, 46, 46, 56);
        if (majorLine && gy > 0)
            canvasDrawText(canvas, cw, ch, 3, gy + 3, std::to_string(gy), 168, 168, 195);
    }
    canvasDrawText(canvas, cw, ch, 3, 3, "0|0", 168, 168, 195);
    std::string caption = "canvas " + std::to_string(cw) + "x" + std::to_string(ch);
    int captionX = std::max(3, cw - static_cast<int>(caption.size()) * 6 - 4);
    canvasDrawText(canvas, cw, ch, captionX, std::max(3, ch - 11), caption, 168, 168, 195);
}

void canvasDrawCross(std::vector<uint8_t>& canvas, int cw, int ch,
                     int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    canvasFillRect(canvas, cw, ch, x - 6, y - 1, x + 6, y + 1, r, g, b);
    canvasFillRect(canvas, cw, ch, x - 1, y - 6, x + 1, y + 6, r, g, b);
}

// ------------------------------------------------------------------- the tool

namespace {

std::string asciiOnly(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (static_cast<unsigned char>(c) >= 32 && static_cast<unsigned char>(c) < 127) out += c;
        else if (c == '\n' || c == '\r' || c == '\t') out += ' ';
    }
    return out;
}

std::string truncateColumn(const std::string& s, size_t width) {
    std::string a = asciiOnly(s);
    if (a.size() > width) a.resize(width);
    return a;
}

struct RenderStats {
    int placed = 0;      // sprites composited
    int texts = 0;       // text objects (drawn in wireframe only)
    int rects = 0;       // quick backdrops (solid fill)
    int noImage = 0;     // objects with no displayable start image
    int missing = 0;     // start image not found in the bank
};

RenderStats renderFrame(GameData& game, const Frame& frame, int frameIndex,
                        const std::string& dir) {
    const int W = std::max(frame.width, 1);
    const int H = std::max(frame.height, 1);

    std::vector<uint8_t> composite(static_cast<size_t>(W) * H * 4);
    std::vector<uint8_t> wire(static_cast<size_t>(W) * H * 4);
    for (size_t i = 0; i < composite.size(); i += 4) {
        composite[i] = frame.background.r;
        composite[i + 1] = frame.background.g;
        composite[i + 2] = frame.background.b;
        composite[i + 3] = 255;
        wire[i] = 26; wire[i + 1] = 26; wire[i + 2] = 30; wire[i + 3] = 255;
    }
    canvasDrawGrid(wire, W, H);

    // stable ordering: layer first, authored order kept inside a layer (z-order)
    std::vector<const ObjectInstance*> insts;
    insts.reserve(frame.objects.size());
    for (const auto& inst : frame.objects) insts.push_back(&inst);
    std::stable_sort(insts.begin(), insts.end(),
                     [](const ObjectInstance* a, const ObjectInstance* b) {
                         return a->layer < b->layer;
                     });

    std::string table;
    table += "Frame: \"" + asciiOnly(frame.name) + "\"  " + std::to_string(W) + "x" +
             std::to_string(H) + "  instances: " + std::to_string(insts.size()) + "\n\n";
    char row[256];
    std::snprintf(row, sizeof(row),
                  "%7s  %-28s %-10s %5s %-11s %6s %6s %5s %5s %6s %6s %5s\n",
                  "objInfo", "object name", "type", "img", "dump file", "x", "y",
                  "hotX", "hotY", "left", "top", "layer");
    table += row;
    size_t hdrLen = std::strlen(row);
    table.append(hdrLen > 2 ? hdrLen - 1 : hdrLen, '-');
    table += "\n";

    RenderStats st;
    for (const ObjectInstance* inst : insts) {
        auto it = game.frameItems.find(inst->objectInfo);
        const ObjectInfo* obj = it != game.frameItems.end() ? it->second.get() : nullptr;
        std::string name = obj ? obj->name : "?";
        std::string type = obj ? objectTypeName(obj->objectType) : "?";
        int32_t imgHandle = -1;
        int hotX = 0, hotY = 0;

        if (obj) {
            if (obj->objectType == kObjQuickBackdrop && obj->quickBackdrop) {
                // solid approximation of the quick backdrop rectangle
                const auto& qb = obj->quickBackdrop;
                const auto& c = qb->shape.color1;
                canvasFillRect(composite, W, H, inst->x, inst->y,
                               inst->x + qb->width - 1, inst->y + qb->height - 1,
                               c.r, c.g, c.b);
                canvasRectOutline(wire, W, H, inst->x, inst->y,
                                  inst->x + qb->width - 1, inst->y + qb->height - 1,
                                  200, 80, 255);
                ++st.rects;
            } else if (obj->objectType == kObjText) {
                if (obj->common && obj->common->text &&
                    !obj->common->text->paragraphs.empty()) {
                    std::string txt = asciiOnly(obj->common->text->paragraphs.front().value);
                    canvasFillRect(wire, W, H, inst->x, inst->y,
                                   std::min(inst->x + 600, W - 1),
                                   std::min(inst->y + 8 * 2 + 3, H - 1), 12, 40, 12);
                    canvasDrawText(wire, W, H, inst->x + 2, inst->y + 2,
                                   "\"" + txt + "\"", 60, 255, 60, 2);
                }
                // text objects carry the same name/coordinate label, above the box
                char tlabel[160];
                std::snprintf(tlabel, sizeof(tlabel), "[%s] X%d|Y%d",
                              truncateColumn(name, 20).c_str(), inst->x, inst->y);
                canvasDrawText(wire, W, H, std::min(inst->x + 2, W - 1),
                               std::max(2, inst->y - 18), tlabel, 255, 220, 60, 2);
                ++st.texts;
            } else {
                imgHandle = resolveStartImage(*obj);
                if (imgHandle >= 0) {
                    auto img = game.images.find(imgHandle);
                    if (img && img->width > 0 && img->height > 0) {
                        if (img->rgba.empty()) img->decode();
                        hotX = img->hotX; hotY = img->hotY;
                        canvasBlendImage(composite, W, H, *img,
                                         inst->x - hotX, inst->y - hotY);
                        ++st.placed;
                    } else {
                        ++st.missing;
                    }
                } else {
                    ++st.noImage;
                }
            }
        } else {
            ++st.missing;
        }

        // wireframe annotation for every visible-ish instance
        if (obj && obj->objectType != kObjText) {
            int sx = inst->x - hotX, sy = inst->y - hotY;
            int sw = 1, sh = 1;
            if (obj->objectType == kObjQuickBackdrop && obj->quickBackdrop) {
                sx = inst->x; sy = inst->y;
                sw = obj->quickBackdrop->width;
                sh = obj->quickBackdrop->height;
            } else if (imgHandle >= 0) {
                auto img = game.images.find(imgHandle);
                if (img) { sw = img->width; sh = img->height; }
            }
            int x0 = std::max(sx, 0), y0 = std::max(sy, 0);
            int x1 = std::min(sx + std::max(sw, 1), W - 1);
            int y1 = std::min(sy + std::max(sh, 1), H - 1);
            if (x1 > x0 && y1 > y0) {
                canvasRectOutline(wire, W, H, x0, y0, x1, y1, 230, 60, 60);
                canvasDrawCross(wire, W, H, inst->x, inst->y, 90, 200, 255);
                char label[160];
                std::snprintf(label, sizeof(label), "[%s] X%d|Y%d",
                              truncateColumn(name, 20).c_str(), inst->x, inst->y);
                int lx = std::min(x0 + 2, W - 1), ly = std::min(y0 + 2, H - 1);
                canvasDrawText(wire, W, H, lx, ly, label, 255, 220, 60, 2);
                // second line: the file name the Image Dumper gives this sprite
                // (Images/<handle>.png) or why nothing is composited
                char sub[48];
                if (obj->objectType == kObjQuickBackdrop)
                    std::snprintf(sub, sizeof(sub), "solid %dx%d", sw, sh);
                else if (imgHandle >= 0)
                    std::snprintf(sub, sizeof(sub), "img: %d.png", imgHandle);
                else
                    std::snprintf(sub, sizeof(sub), "%s", objectTypeName(obj->objectType));
                canvasDrawText(wire, W, H, lx, ly + 16, sub, 90, 200, 255, 2);
            }
        }

        char dumpName[32] = "-";
        if (imgHandle >= 0) std::snprintf(dumpName, sizeof(dumpName), "%d.png", imgHandle);
        std::snprintf(row, sizeof(row),
                      "%7d  %-28s %-10s %5d %-11s %6d %6d %5d %5d %6d %6d %5d\n",
                      inst->objectInfo, truncateColumn(name, 28).c_str(), type.c_str(),
                      imgHandle, dumpName, inst->x, inst->y, hotX, hotY,
                      inst->x - hotX, inst->y - hotY, inst->layer);
        table += row;
    }

    std::string base = std::to_string(frameIndex) + "_" +
                       (frame.name.empty() ? "unnamed" : clearName(frame.name));
    std::string pngPath = pathJoin(dir, "frame_" + base + ".png");
    PngWriter::writeFile(pngPath, W, H, composite);
    PngWriter::writeFile(pathJoin(dir, "frame_" + base + "_wire.png"), W, H, wire);
    writeFileBytes(pathJoin(dir, "frame_" + base + ".txt"),
                   std::vector<uint8_t>(table.begin(), table.end()));

    std::string label = frame.name.empty() ? "(unnamed)" : frame.name;
    Logger::log("Frame \"" + label + "\": " + std::to_string(insts.size()) +
                " instances -> " + std::to_string(st.placed) + " sprites, " +
                std::to_string(st.rects) + " quick backdrops, " + std::to_string(st.texts) +
                " texts, " + std::to_string(st.noImage) + " without image, " +
                std::to_string(st.missing) + " missing");
    return st;
}

} // namespace

void FrameLayoutRenderer::execute(GameData& game) {
    std::string dir = pathJoin(g_dumpsDir,
                               sanitizeGameName(game.name.empty() ? "Unknown Game" : game.name),
                               "FrameLayout");
    createDirectories(dir);
    Logger::log("Rendering frame layouts...");
    size_t frames = 0;
    RenderStats total;
    int index = 0;
    for (const auto& frame : game.frames) {
        if (!frame) continue;
        RenderStats st = renderFrame(game, *frame, index++, dir);
        total.placed += st.placed;
        total.texts += st.texts;
        total.rects += st.rects;
        total.noImage += st.noImage;
        total.missing += st.missing;
        ++frames;
    }
    Logger::log("Frame layout rendering done (" + std::to_string(frames) +
                " frames, " + std::to_string(total.placed) + " sprites placed, " +
                std::to_string(total.missing) + " missing images).");
}

} // namespace ctfak
