#include "Logger.h"
#include <cstdio>
#include <ctime>
#include <vector>

namespace ctfak {

bool Logger::logToScreen = true;
bool Logger::verbose = false;

static FILE* g_logFile = nullptr;

void Logger::openFile(const std::string& path) {
    closeFile();
    g_logFile = fopen(path.c_str(), "wb");
}

void Logger::closeFile() {
    if (g_logFile) {
        fclose(g_logFile);
        g_logFile = nullptr;
    }
}

static const char* levelTag(LogLevel level) {
    switch (level) {
        case LogLevel::Warning: return "[WARN ] ";
        case LogLevel::Error:   return "[ERROR] ";
        case LogLevel::Debug:   return "[DEBUG] ";
        default:                return "";
    }
}

void Logger::log(const std::string& msg, LogLevel level) {
    char timeBuf[32];
    {
        std::time_t t = std::time(nullptr);
        std::tm tmv{};
#ifdef _WIN32
        localtime_s(&tmv, &t);
#else
        localtime_r(&t, &tmv);
#endif
        std::strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &tmv);
    }
    std::string line = std::string("[") + timeBuf + "] " + levelTag(level) + msg + "\n";
    if (logToScreen) {
        fputs(line.c_str(), stdout);
        fflush(stdout);
    }
    if (g_logFile) {
        fputs(line.c_str(), g_logFile);
        fflush(g_logFile);
    }
}

} // namespace ctfak
