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

struct RuntimePerspectiveGeometry {
    float objW, objH, orgX, orgY, winW, winH;
    float zoom, centerY, curve;
};

RuntimePerspectiveGeometry runtimePerspectiveGeometry(const ObjectInstance& inst,
                                                       int windowW, int windowH);
void runtimeApplyPerspective(std::vector<uint8_t>& canvas, int cw, int ch,
                             const RuntimePerspectiveGeometry& geometry);

// Small public surface kept for unit tests and future runtime render passes.
uint8_t runtimeObjectAlpha(const ObjectInfo& obj);
bool runtimeObjectVisibleAtStart(const ObjectInfo& obj);
bool runtimeLayerVisibleAtStart(const Frame& frame, int16_t layer);
int32_t resolveRuntimeStartImage(const ObjectInfo& obj);
BinaryReader::Color runtimeEffectCoeff(int16_t effect, BinaryReader::Color coeff);

// Src-over an image with the object's CTF tint and corrected alpha semantics.
void runtimeBlendImage(std::vector<uint8_t>& canvas, int cw, int ch,
                       const ImageItem& img, int dstX, int dstY,
                       const ObjectInfo& obj);

class FrameRuntimeRenderer : public FusionTool {
public:
    const char* name() const override { return "Frame Runtime Renderer"; }
    void execute(GameData& game) override;
};

} // namespace ctfak
