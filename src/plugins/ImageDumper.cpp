#include "Dumpers.h"
#include "../io/PngWriter.h"
#include "../util/Config.h"
#include "../util/Utils.h"
#include "../core/Logger.h"
#include <cstdio>
#include <string>     // std::to_string (MSVC does not leak it transitively)

namespace ctfak {

static std::string dumpsBase(GameData& game) {
    return pathJoin(g_dumpsDir, sanitizeGameName(game.name.empty() ? "Unknown Game" : game.name));
}

// ------------------------------------------------------------------ ImageDumper
void ImageDumper::execute(GameData& game) {
    std::string dir = pathJoin(dumpsBase(game), "Images");
    createDirectories(dir);
    Logger::log("Dumping images...");
    size_t count = 0;
    for (auto& [handle, image] : game.images.items) {
        if (!image) continue;
        image->decode();
        std::string path = pathJoin(dir, std::to_string(handle) + ".png");
        if (PngWriter::writeFile(path, image->width, image->height, image->rgba)) ++count;
    }
    Logger::log("Image dumping done (" + std::to_string(count) + " images).");
}

// ------------------------------------------------------------------ AutoDumper
void AutoDumper::execute(GameData& game) {
    Logger::log("Dumping images...");
    ImageDumper().execute(game);
    Logger::log("Image dumping done");
    Logger::log("Dumping sounds...");
    SoundDumper().execute(game);
    Logger::log("Sound dumping done");
    Logger::log("Dumping packed data...");
    PackedDataDumper().execute(game);
    Logger::log("Packed Data dumping done");
}

} // namespace ctfak
