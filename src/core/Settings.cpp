#include "Settings.h"
#include <cctype>

namespace ctfak {

uint32_t Settings::gameType = static_cast<uint32_t>(GameType::Normal);
int Settings::build = 0;
int Settings::buildType = 0;
bool Settings::unicode = false;
bool Settings::isMFA = false;
bool Settings::fusion3Seed = false;
std::set<std::string> Settings::parameters;

bool Settings::hasParameter(const std::string& name) {
    return parameters.find(name) != parameters.end();
}

static std::string toLower(const std::string& s) {
    std::string out = s;
    for (auto& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

void Settings::parseParameters(const std::string& raw) {
    size_t i = 0;
    while (i < raw.size()) {
        while (i < raw.size() && std::isspace(static_cast<unsigned char>(raw[i]))) ++i;
        size_t start = i;
        while (i < raw.size() && !std::isspace(static_cast<unsigned char>(raw[i]))) ++i;
        if (i == start) break;
        std::string token = raw.substr(start, i - start);
        if (!token.empty() && token[0] == '-') token = token.substr(1);
        if (!token.empty()) parameters.insert(toLower(token));
    }
}

} // namespace ctfak
