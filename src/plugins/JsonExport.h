// JsonExportTool - dumps the full application structure (app info, objects,
// frames, instances, images metadata, sounds, banks) and the parsed event
// program to JSON files.
#pragma once

#include "Plugin.h"

namespace ctfak {

class JsonExportTool : public FusionTool {
public:
    const char* name() const override { return "Export Structure as JSON"; }
    void execute(GameData& game) override;
};

} // namespace ctfak
