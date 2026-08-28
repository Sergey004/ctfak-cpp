#include "Dumpers.h"
#include "../io/WavWriter.h"
#include "../util/Config.h"
#include "../util/SoundExt.h"
#include "../util/Utils.h"
#include "../core/Logger.h"

namespace ctfak {

void SoundDumper::execute(GameData& game) {
    std::string dir = pathJoin(g_dumpsDir, sanitizeGameName(game.name.empty() ? "Unknown Game" : game.name),
                               "Sounds");
    createDirectories(dir);
    Logger::log("Dumping sounds...");

    if (game.sounds.items.empty()) {
        Logger::log("No sounds found.");
        return;
    }

    size_t count = 0;
    for (auto& snd : game.sounds.items) {
        if (!snd) continue;
        std::string name = clearName(snd->name);
        if (name.empty()) name = "sound_" + std::to_string(snd->handle);
        std::string ext = sniffSoundExtension(snd->data);
        std::string path = pathJoin(dir, name + ext);
        if (writeFileBytes(path, snd->data)) ++count;
    }

    // Old (1.5) builds store raw PCM headers that the reference wraps into WAVs.
    // Modern payloads already contain complete RIFF/OGG containers.
    Logger::log("Sound dumping done (" + std::to_string(count) + " sounds).");
}

void PackedDataDumper::execute(GameData& game) {
    std::string base = pathJoin(g_dumpsDir, sanitizeGameName(game.name.empty() ? "Unknown Game" : game.name),
                                "Packed Data");
    if (game.binaryFiles.files.empty() && game.packData.items.empty()) {
        Logger::log("No Packed Data found.");
        return;
    }
    Logger::log("Dumping packed data...");

    int packed = 0;

    std::string binaryDir = pathJoin(base, "Binary Data");
    for (const auto& file : game.binaryFiles.files) {
        createDirectories(binaryDir);
        std::string stem = fileStem(file.name + ".exe");
        if (writeFileBytes(pathJoin(binaryDir, stem), file.data)) ++packed;
    }

    static const char* const subdirs[] = {"Extensions", "Libraries", "Filters", "Movements"};
    for (const auto& pack : game.packData.items) {
        std::string ext = fileExtension(pack.packFilename);
        std::string dir = base;
        if (ext == ".mfx") dir = pathJoin(base, subdirs[0]);
        else if (ext == ".dll") dir = pathJoin(base, subdirs[1]);
        else if (ext == ".ift" || ext == ".sft") dir = pathJoin(base, subdirs[2]);
        else if (ext == ".mvx") dir = pathJoin(base, subdirs[3]);
        createDirectories(dir);
        std::string stem = fileStem(pack.packFilename + ".exe");
        if (writeFileBytes(pathJoin(dir, stem), pack.data)) ++packed;
    }

    Logger::log("Packed Data dumping done (" + std::to_string(packed) + " files).");
}

} // namespace ctfak
