// Frame Runtime Renderer: a separate frame-start CTF runtime view.
// Renders the application viewport, not the full editor world. Event and
// animation ticks are deliberately not executed; unsupported cases are named
// in a per-frame report instead of being drawn with editor/debug primitives.
#pragma once

#include "Plugin.h"
#include "../core/BinaryReader.h"
#include <cstdint>
#include <vector>

namespace ctfak {

class Frame;
class ObjectInfo;
struct ObjectInstance;
struct ImageItem;
struct RuntimeObjectState;

struct RuntimePerspectiveGeometry {
    float objW, objH, orgX, orgY, winW, winH;
    float zoom, centerY, curve;
    int effect, direction;
};

RuntimePerspectiveGeometry runtimePerspectiveGeometry(int instanceX, int instanceY,
                                                       const ObjectInfo& obj,
                                                       int windowW, int windowH);
void runtimeApplyPerspective(std::vector<uint8_t>& canvas, int cw, int ch,
                             const RuntimePerspectiveGeometry& geometry);

// Small public surface kept for unit tests and future runtime render passes.
uint8_t runtimeObjectAlpha(const ObjectInfo& obj);
bool runtimeObjectVisibleAtStart(const ObjectInfo& obj);
bool runtimeLayerVisibleAtStart(const Frame& frame, int16_t layer);
int32_t resolveRuntimeStartImage(const ObjectInfo& obj);
int32_t resolveRuntimeStartImage(const ObjectInfo& obj, int animation, int direction);
BinaryReader::Color runtimeEffectCoeff(int16_t effect, BinaryReader::Color coeff);

// Src-over an image with the object's CTF tint and corrected alpha semantics.
// The optional evaluated state supplies an event-set alpha coefficient.
void runtimeBlendImage(std::vector<uint8_t>& canvas, int cw, int ch,
                       const ImageItem& img, int dstX, int dstY,
                       const ObjectInfo& obj);
void runtimeBlendImage(std::vector<uint8_t>& canvas, int cw, int ch,
                       const ImageItem& img, int dstX, int dstY,
                       const ObjectInfo& obj, const RuntimeObjectState* state);

// v2.98: TRUE when every sampled pixel is the same RGBA value — the "empty"
// placeholder frame of an idle animation chain (solid black attack states,
// flash covers). Decoded rgba required.
bool isSolidImage(const ImageItem& img);

class FrameRuntimeRenderer : public FusionTool {
public:
    const char* name() const override { return "Frame Runtime Renderer"; }
    void execute(GameData& game) override;
};

} // namespace ctfak
