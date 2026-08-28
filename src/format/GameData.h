// GameData - top-level CCN chunk stream orchestrator and data model.
#pragma once

#include "AppHeader.h"
#include "ExtendedHeader.h"
#include "Extensions.h"
#include "Frame.h"
#include "ImageBank.h"
#include "FontBank.h"
#include "ObjectInfo.h"
#include "PackData.h"
#include "Shaders.h"
#include "SoundBank.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ctfak {

struct GameData {
    int16_t runtimeVersion = 0;
    int16_t runtimeSubversion = 0;
    int32_t productVersion = 0;
    int32_t productBuild = 0;

    std::string name;
    std::string author;
    std::string copyright;
    std::string editorFilename;
    std::string targetFilename;
    bool exeOnly = false;

    std::unique_ptr<AppHeader> header;
    std::unique_ptr<ExtendedHeader> extHeader;

    FontBank fonts;
    SoundBank sounds;
    MusicBank music;
    ImageBank images;
    TrueTypeFonts trueTypeFonts;

    std::map<int, std::shared_ptr<ObjectInfo>> frameItems;
    std::vector<std::shared_ptr<Frame>> frames;
    std::vector<int16_t> frameHandles;
    Extensions extensions;
    PackData packData;
    Shaders shaders;
    GlobalValues globalValues;
    GlobalStrings globalStrings;
    BinaryFiles binaryFiles;

    // Source bookkeeping (set by the CLI layer).
    std::string sourcePath;
    int64_t dataOffset = 0; // start of the CCN stream inside the container

    void read(BinaryReader& reader);
};

} // namespace ctfak
