// Small shared helpers (row padding, name sanitizing, file IO).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ctfak {

// Row padding in PIXELS by default (mirrors the reference GetPadding):
// pad = bytes - (width*pointSize) % bytes; if pad == bytes return 0;
// else ceil(pad / pointSize). modular variant returns raw byte padding:
// (bytes - width*pointSize % bytes) % bytes.
int getPadding(int width, int pointSize, int bytes = 2, bool modular = false);

// Removes characters that are invalid in file names.
std::string clearName(const std::string& name);

std::string fileStem(const std::string& path);   // name without extension
std::string fileExtension(const std::string& path); // includes dot, lowercase
std::string sanitizeGameName(const std::string& name);

bool readFileBytes(const std::string& path, std::vector<uint8_t>& out);
bool writeFileBytes(const std::string& path, const std::vector<uint8_t>& data);
void createDirectories(const std::string& path);
// join a/b/c with native separators (variadic)
std::string pathJoin(const std::string& a, const std::string& b);

template <typename... Rest>
inline std::string pathJoin(const std::string& a, const std::string& b, Rest... rest) {
    return pathJoin(pathJoin(a, b), rest...);
}

} // namespace ctfak
