// Simple string chunks: app name/author/copyright/filenames etc.
#pragma once

#include <string>

namespace ctfak {

class BinaryReader;

// Reads a NUL-terminated Yuniversal string.
std::string readStringChunk(BinaryReader& reader);

// App name chunk has special unicode auto-detection (ASCII length + 1 == chunk size?).
std::string readAppNameChunk(BinaryReader& reader, int64_t chunkSize);

} // namespace ctfak
