// ChunkTraceTool - re-walks the source file and writes every chunk's processed
// payload to CHUNK_TRACE/<exeName>/<chunkName>-<index>.bin (mirrors -trace_chunks).
#pragma once

#include "Plugin.h"

namespace ctfak {

class ChunkTraceTool : public FusionTool {
public:
    const char* name() const override { return "Chunk Trace"; }
    void execute(GameData& game) override;
};

} // namespace ctfak
