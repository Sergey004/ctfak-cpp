// Asset dumper tools: images (PNG), sounds, packed data, sorted images.
#pragma once

#include "Plugin.h"

namespace ctfak {

// "Dump Everything" - runs the three basic dumpers in sequence.
class AutoDumper : public FusionTool {
public:
    const char* name() const override { return "Dump Everything"; }
    void execute(GameData& game) override;
};

class ImageDumper : public FusionTool {
public:
    const char* name() const override { return "Image Dumper"; }
    void execute(GameData& game) override;
};

class SoundDumper : public FusionTool {
public:
    const char* name() const override { return "Sound Dumper"; }
    void execute(GameData& game) override;
};

class PackedDataDumper : public FusionTool {
public:
    const char* name() const override { return "Packed Data Dumper"; }
    void execute(GameData& game) override;
};

class SortedImageDumper : public FusionTool {
public:
    const char* name() const override { return "Sorted Image Dumper"; }
    void execute(GameData& game) override;

private:
    bool saveImagePng(const std::string& path, ImageItem& image);
};

} // namespace ctfak
