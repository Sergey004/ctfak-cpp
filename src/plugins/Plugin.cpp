#include "Plugin.h"
#include "Dumpers.h"
#include "ChunkTrace.h"
#include "JsonExport.h"
#include "FrameLayout.h"
#include "EventsListing.h"
#include "RecompPack.h"
#include <memory>

namespace ctfak {

std::vector<FusionTool*>& builtinTools() {
    static std::vector<FusionTool*> tools;
    static bool initialized = false;
    if (!initialized) {
        tools.push_back(new AutoDumper());
        tools.push_back(new ImageDumper());
        tools.push_back(new SoundDumper());
        tools.push_back(new PackedDataDumper());
        tools.push_back(new SortedImageDumper());
        tools.push_back(new ChunkTraceTool());
        tools.push_back(new JsonExportTool());
        tools.push_back(new FrameLayoutRenderer());
        tools.push_back(new EventsListing());
        tools.push_back(new RecompPack());   // tool #10: .pak repacker (was never registered)
        initialized = true;
    }
    return tools;
}

} // namespace ctfak
