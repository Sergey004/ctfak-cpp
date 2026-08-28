// Extended header chunk (8773): build type detection, compression flags.
#pragma once

#include "../core/BinaryReader.h"
#include "../core/BitDict.h"

namespace ctfak {

struct ExtendedHeader {
    BitDict flags = {"KeepScreenRatio", "1", "AntiAliasingWhenResizing", "2", "3",
                     "RightToLeftReading", "4", "RightToLeftLayout", "5", "6", "7", "8", "9", "10",
                     "11", "12", "13", "14", "15", "16", "17", "18", "DontOptimizeStrings", "19",
                     "20", "21", "DontIgnoreDestroy", "DisableIME", "ReduceCPUUsage", "22",
                     "PremultipliedAlpha", "OptimizePlaySample"};
    BitDict compressionFlags = {"CompressionLevelMax", "CompressSounds", "IncludeExternalFiles",
                                "NoAutoImageFilters", "NoAutoSoundFilters", "1", "2", "3",
                                "DontDisplayBuildWarning", "OptimizeImageSize"};
    uint8_t buildType = 0;
    int16_t screenRatio = 0;
    int16_t screenAngle = 0;

    void read(BinaryReader& reader);
    const char* buildTypeName() const;
};

} // namespace ctfak
