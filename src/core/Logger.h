// Minimal logging facility: timestamped console output mirrored to Latest.log.
#pragma once

#include <string>

namespace ctfak {

enum class LogLevel { Info, Warning, Error, Debug };

class Logger {
public:
    static bool logToScreen;
    static bool verbose; // extra chunk-level logging (enabled by -verbose parameter)

    static void openFile(const std::string& path);
    static void log(const std::string& msg, LogLevel level = LogLevel::Info);
    static void warning(const std::string& msg) { log(msg, LogLevel::Warning); }
    static void error(const std::string& msg) { log(msg, LogLevel::Error); }
    static void debug(const std::string& msg) { if (verbose) log(msg, LogLevel::Debug); }
    static void closeFile();
};

} // namespace ctfak
