#include "Dumpers.h"
#include "../io/PngWriter.h"
#include "../util/Config.h"
#include "../util/Utils.h"
#include "../core/Logger.h"
#include "../core/Settings.h"
#include <set>

namespace ctfak {

bool SortedImageDumper::saveImagePng(const std::string& path, ImageItem& image) {
    image.decode();
    return PngWriter::writeFile(path, image.width, image.height, image.rgba);
}

void SortedImageDumper::execute(GameData& game) {
    std::string base = pathJoin(g_dumpsDir, sanitizeGameName(game.name.empty() ? "Unknown Game" : game.name),
                                "Sorted Images");
    createDirectories(base);
    Logger::log("Dumping sorted images...");

    bool dumpStrings = Settings::hasParameter("sorteddumpstrings");
    std::set<int32_t> usedHandles;
    size_t objectsDone = 0;
    size_t maxDone = 0;
    for (auto& frame : game.frames)
        if (frame) maxDone += frame->objects.size();

    for (size_t frameIdx = 0; frameIdx < game.frames.size(); ++frameIdx) {
        auto& frame = game.frames[frameIdx];
        if (!frame) continue;
        std::string frameFolder =
            pathJoin(base, "[" + std::to_string(frameIdx) + "] " + clearName(frame->name.empty()
                                                                                     ? "frame " + std::to_string(frameIdx)
                                                                                     : frame->name));
        createDirectories(frameFolder);
        std::string unsorted = pathJoin(frameFolder, "[UNSORTED]");
        createDirectories(unsorted);

        for (auto& inst : frame->objects) {
            auto it = game.frameItems.find(static_cast<int>(inst.objectInfo));
            if (it == game.frameItems.end() || !it->second) {
                ++objectsDone;
                continue;
            }
            ObjectInfo& oi = *it->second;
            std::string objName = clearName(oi.name.empty() ? "object " + std::to_string(oi.handle) : oi.name);
            std::string objectFolder = pathJoin(frameFolder, objName);
            createDirectories(objectFolder);

            auto saveTo = [&](int32_t handle, const std::string& path) {
                auto img = game.images.find(handle);
                if (!img) return false;
                usedHandles.insert(handle);
                return saveImagePng(path, **&img);
            };

            if (oi.backdrop) {
                saveTo(oi.backdrop->image, pathJoin(objectFolder, objName + ".png"));
                saveTo(oi.backdrop->image, pathJoin(unsorted, objName + ".png"));
            } else if (oi.quickBackdrop) {
                saveTo(oi.quickBackdrop->shape.image, pathJoin(objectFolder, objName + ".png"));
                saveTo(oi.quickBackdrop->shape.image, pathJoin(unsorted, objName + ".png"));
            } else if (oi.common) {
                ObjectCommon& common = *oi.common;
                bool isSprite = common.identifier == "SPRI" ||
                                oi.objectType == kObjActive;
                bool isCounter = (common.identifier == "CNTR" || oi.objectType == kObjCounter);
                bool isText = (common.identifier.rfind("TEXT", 0) == 0 ||
                               common.identifier == "TE" || oi.objectType == kObjText);

                if (isSprite && !common.animations.items.empty()) {
                    size_t animCount = 0;
                    for (auto& [animKey, dirs] : common.animations.items)
                        if (!dirs.empty()) ++animCount;
                    for (auto& [animKey, dirs] : common.animations.items) {
                        if (dirs.empty()) continue;
                        std::string animationFolder = animCount > 0
                                                          ? pathJoin(objectFolder, "Animation " + std::to_string(animKey))
                                                          : objectFolder;
                        createDirectories(animationFolder);
                        size_t dirCount = 0;
                        for (auto& [dirKey, dir] : dirs)
                            if (!dir.frames.empty()) ++dirCount;
                        for (auto& [dirKey, dir] : dirs) {
                            if (dir.frames.empty()) continue;
                            std::string directionFolder =
                                dirCount > 1
                                    ? pathJoin(animationFolder, "Direction " + std::to_string(dirKey))
                                    : (dirCount <= 1 && animCount > 1 ? animationFolder : objectFolder);
                            createDirectories(directionFolder);
                            for (size_t i = 0; i < dir.frames.size(); ++i) {
                                saveTo(dir.frames[i], pathJoin(directionFolder, "_" + std::to_string(i) + ".png"));
                                saveTo(dir.frames[i],
                                       pathJoin(unsorted, objName + "_" + std::to_string(animKey) + "-" +
                                                              std::to_string(dirKey) + "_" + std::to_string(i) + ".png"));
                            }
                        }
                    }
                } else if (isCounter && common.counter &&
                           (common.counter->displayType == 1 || common.counter->displayType == 4 ||
                            common.counter->displayType == 50)) {
                    for (uint16_t handle : common.counter->frames) {
                        saveTo(handle, pathJoin(objectFolder, std::to_string(handle) + ".png"));
                        saveTo(handle, pathJoin(unsorted, objName + "_" + std::to_string(handle) + ".png"));
                    }
                } else if (isText && dumpStrings && common.text) {
                    std::string paragraphs;
                    for (auto& p : common.text->paragraphs) {
                        paragraphs += p.value;
                        paragraphs += "\r\n";
                    }
                    writeFileBytes(pathJoin(objectFolder, "Paragraphs.txt"),
                                   std::vector<uint8_t>(paragraphs.begin(), paragraphs.end()));
                    writeFileBytes(pathJoin(unsorted, objName + ".txt"),
                                   std::vector<uint8_t>(paragraphs.begin(), paragraphs.end()));
                }
            }
            ++objectsDone;
        }
        int percent = maxDone ? static_cast<int>(objectsDone * 100.0 / maxDone) : 100;
        Logger::log("Frame " + std::to_string(frameIdx) + " done (" + std::to_string(percent) + "%)");
    }

    // Lost and Found: images not referenced by any dumped object.
    std::string lostFound = pathJoin(base, "[~Lost and Found~]");
    createDirectories(lostFound);
    size_t lost = 0;
    for (auto& [handle, image] : game.images.items) {
        if (!image || usedHandles.count(handle)) continue;
        std::string path = pathJoin(lostFound, std::to_string(handle) + ".png");
        if (saveImagePng(path, *image)) ++lost;
    }
    Logger::log("Lost and Found: " + std::to_string(lost) + " images.");
}

} // namespace ctfak
