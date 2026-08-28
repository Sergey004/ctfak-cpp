// Fusion tool ("plugin") interface, mirroring the reference plugin model.
#pragma once

#include "../format/GameData.h"
#include <string>
#include <vector>

namespace ctfak {

class FusionTool {
public:
    virtual ~FusionTool() = default;
    virtual const char* name() const = 0;
    virtual void execute(GameData& game) = 0;
};

// Registry of built-in tools (constructed once and reused).
std::vector<FusionTool*>& builtinTools();

} // namespace ctfak
