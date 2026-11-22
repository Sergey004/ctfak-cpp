// Frame-start CTF event evaluation for Frame Runtime Renderer.
// Executes only Start of Frame / always groups on the first tick: counters,
// alterables, visibility, position, animation and alpha. Timer ticks, input,
// movement and audio are deliberately not simulated; unsupported groups fail
// closed and are reported rather than partially modifying state.
#pragma once

#include "../core/BinaryReader.h"
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace ctfak {

class Frame;
class GameData;

struct RuntimeObjectState {
    int handle = 0;
    int objectInfo = 0;
    int layer = 0;
    int x = 0, y = 0;
    int animation = 0;
    int direction = -1;
    bool created = true;
    bool visible = true;
    bool alphaOverride = false;
    int alphaCoefficient = 0; // CTF coefficient: 0 opaque, 255 invisible
    std::vector<int32_t> alterables;
};

struct RuntimeFrameState {
    std::vector<RuntimeObjectState> objects;
    std::map<int32_t, int32_t> counters;
    // objectInfos whose animation is driven by "animation sequence finished"
    // chains (condition (2,-2)): their serialized editor image is a MID-CHAIN
    // frame, not the runtime entry state (see FrameRuntime.cpp).
    std::set<int> chainSubjects;
    int groupsExecuted = 0;
    int groupsFalse = 0;
    int groupsSkippedUnsupported = 0;
    std::string notes;
};

RuntimeFrameState evaluateFrameStartEvents(const Frame& frame,
                                            const GameData& game);

} // namespace ctfak
