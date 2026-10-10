// Frame Replay: composes every frame the way the Clickteam Fusion 2.5
// engine draws it at start - the frame background color, then the instances
// in layer order (stable within a layer), each with its hotspot applied and
// its own ink effect (transparency) and RGB coefficient (tint) applied.
// Output: <dumpdir>/FrameReplay/frame_<idx>_<name>.png per frame.
// The baseline is static (first animation frame); runtime animation/event
// effects are out of scope - this is the "what the engine sees" reference.
#pragma once

#include "Plugin.h"

namespace ctfak {

class FrameReplayTool : public FusionTool {
public:
    const char* name() const override { return "Frame Replay"; }
    void execute(GameData& game) override;
};

} // namespace ctfak
