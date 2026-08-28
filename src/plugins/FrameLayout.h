// Frame Layout Renderer: rebuilds how each frame looks at start by placing
// every object instance at its stored (x, y) position with hotspots applied.
// Produces three artifacts per frame:
//   frame_NN_<name>.png       - composite of all sprites at their positions
//   frame_NN_<name>_wire.png  - wireframe: outlines, internal object names,
//                               coordinates, and the dumped image file name
//                               (Images/<handle>.png) for every sprite
//   frame_NN_<name>.txt       - exact placement table (objInfo/img/dump file/x/y/...)
#pragma once

#include "Plugin.h"
#include <cstdint>
#include <string>
#include <vector>

namespace ctfak {

class Frame;
class ObjectInfo;
struct ImageItem;

// ---------------------------------------------------------------- canvas helpers
// All helpers operate on an opaque RGBA canvas (width*height*4, alpha kept at 255)
// and clip everything against the canvas bounds, so negative / oversized
// coordinates are safe.

// Src-over blend of a decoded RGBA image at (dstX, dstY). Hotspot is NOT applied
// here - pass already-adjusted coordinates.
void canvasBlendImage(std::vector<uint8_t>& canvas, int cw, int ch,
                      const ImageItem& img, int dstX, int dstY);

void canvasFillRect(std::vector<uint8_t>& canvas, int cw, int ch,
                    int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b);

void canvasRectOutline(std::vector<uint8_t>& canvas, int cw, int ch,
                       int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b);

// Draws ASCII text with the embedded 5x7 font (black shadow + main color).
// scale >= 1; non-ASCII bytes advance one cell without drawing.
void canvasDrawText(std::vector<uint8_t>& canvas, int cw, int ch,
                    int x, int y, const std::string& text,
                    uint8_t r, uint8_t g, uint8_t b, int scale = 1);

// Coordinate grid for wireframes: minor lines every `minor` px, brighter major
// lines every `major` px, ruler tick labels on the top/left edges (5x7 font)
// and a "canvas WxH" caption in the bottom-right corner.
void canvasDrawGrid(std::vector<uint8_t>& canvas, int cw, int ch,
                    int minor = 100, int major = 500);

// Small crosshair marking an exact anchor point (clipped).
void canvasDrawCross(std::vector<uint8_t>& canvas, int cw, int ch,
                     int x, int y, uint8_t r, uint8_t g, uint8_t b);

// Which image handle an object displays at frame start (-1 if none):
// backdrop image, else first animation frame, else counter frame.
int32_t resolveStartImage(const ObjectInfo& oi);

// ------------------------------------------------------------------- the tool
class FrameLayoutRenderer : public FusionTool {
public:
    const char* name() const override { return "Frame Layout Renderer"; }
    void execute(GameData& game) override;
};

} // namespace ctfak
