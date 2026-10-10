// ctfak-cpp - clean-room Clickteam Fusion Army Knife reimplementation in C++.
// Interactive CLI mirroring the reference workflow: path -> parameters ->
// reader selection -> read -> tool selection loop.
#include "core/Logger.h"
#include "core/Settings.h"
#include "format/ExeFile.h"
#include "format/GameData.h"
#include "io/PngWriter.h"
#include "plugins/Plugin.h"
#include "util/ApkReader.h"
#include "util/Config.h"
#include "util/Utils.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
#endif

using namespace ctfak;

static void printBanner() {
    std::cout << "\n";
    std::cout << "  ____ ____   ___  ____   ____ _   _    _    ____  _  _______ ____\n";
    std::cout << " / ___|  _ \\ / _ \\|  _ \\ / ___| | | |  / \\  |  _ \\| |/ / ____|  _ \\\n";
    std::cout << "| |   | |_) | | | | |_) | |   | |_| | / _ \\ | |_) | ' /|  _| | |_) |\n";
    std::cout << "| |___|  _ <| |_| |  _ <| |___|  _  |/ ___ \\|  _ <| . \\| |___|  _ <\n";
    std::cout << " \\____|_| \\_\\\\___/|_| \\_\\\\____|_| |_/_/   \\_\\_| \\_\\_|\\_\\_____|_| \\_\\\n";
    std::cout << "\n";
    std::cout << "  Clickteam Fusion Army Knife - clean-room C++ reimplementation\n";
    std::cout << "\n";
}

static std::string argAfter(const std::vector<std::string>& args, const std::string& name) {
    for (size_t i = 0; i + 1 < args.size(); ++i)
        if (args[i] == name) return args[i + 1];
    return "";
}

static bool loadGame(const std::string& path, GameData& game) {
    std::vector<uint8_t> file;
    if (!readFileBytes(path, file)) {
        std::cout << "ERROR: File not found\n";
        return false;
    }
    std::string ext = fileExtension(path);

    if (ext == ".apk") {
        Logger::log("Extracting APK container...");
        ZipReader zip;
        if (!zip.open(file)) {
            Logger::error("Cannot read APK (not a zip?)");
            return false;
        }
        std::vector<uint8_t>* ccn = nullptr;
        std::string ccnName;
        for (const auto& entry : zip.entries()) {
            if (entry.name.find("application.ccn") != std::string::npos ||
                (ccn == nullptr && fileExtension(entry.name) == ".ccn")) {
                static std::vector<uint8_t> ccnData;
                if (zip.extract(entry, ccnData)) {
                    ccn = &ccnData;
                    ccnName = entry.name;
                }
            }
        }
        if (!ccn) {
            Logger::error("No CCN data inside APK");
            return false;
        }
        Settings::setGameType(GameType::Android);

        // Sounds live as loose files (s*.ogg/wav/mp3).
        for (const auto& entry : zip.entries()) {
            std::string e = fileExtension(entry.name);
            std::string stem = fileStem(entry.name);
            if (stem.size() > 1 && stem[0] == 's' &&
                (e == ".ogg" || e == ".wav" || e == ".mp3")) {
                std::vector<uint8_t> data;
                if (!zip.extract(entry, data)) continue;
                auto item = std::make_shared<SoundItem>();
                item->handle = static_cast<uint32_t>(atoi(stem.c_str() + 1));
                item->name = stem;
                item->data = std::move(data);
                game.sounds.items.push_back(std::move(item));
            }
        }

        game.sourcePath = path;
        int64_t start = 0;
        if (ccn->size() >= 4 && memcmp(ccn->data(), "\x77\x77\x77\x77", 4) == 0) start = 32;
        game.dataOffset = start;
        BinaryReader reader(ccn->data(), ccn->size());
        reader.seek(start);
        game.read(reader);
        return true;
    }

    if (ext == ".exe") {
        BinaryReader reader(file.data(), file.size());
        int64_t entry = calculateEntryPoint(reader);
        reader.seek(entry);
        uint16_t first = reader.peekU16();
        if (first == 0x7777) {
            Settings::setGameType(GameType::Normal);
        } else {
            Settings::setGameType(GameType::Mmf15);
            Settings::unicode = false;
            Logger::log("1.5 game detected. First short: " + std::to_string(first));
        }
        if (first == 0x7777) {
            game.packData.read(reader);
            game.dataOffset = reader.tell();
        } else {
            reader.seek(entry);
            game.dataOffset = entry;
        }
        game.sourcePath = path;
        game.read(reader);
        if (getenv("CTFAK_DUMP_HANDLES")) {
            std::cout << "=== storyboard frameHandles (" << game.frameHandles.size() << ") ===\n";
            for (size_t i = 0; i < game.frameHandles.size(); ++i) {
                int16_t h = game.frameHandles[i];
                int fileIdx = (h >= 0 && (size_t)h < game.frames.size()) ? h : -1;
                std::string nm = (fileIdx >= 0) ? game.frames[fileIdx]->name : std::string("?");
                std::cout << "slot " << i << " -> " << h << " -> file[" << fileIdx << "] \"" << nm << "\"\n";
            }
            std::cout << "=== file order frames ===\n";
            for (size_t fi = 0; fi < game.frames.size(); ++fi)
                std::cout << "file[" << fi << "] \"" << (game.frames[fi] ? game.frames[fi]->name : std::string("?")) << "\"\n";
        }
        if (getenv("CTFAK_DUMP_EXT")) {
            // v2.66: the per-object extension payloads (the Perspective
            // extension's serialized EDATA lives here) + the counter glyph
            // bindings (the CNTR raw head — where a counter's picture list
            // hides when the JSON shows frames: []).
            std::cout << "=== object extensions ===\n";
            for (const auto& kv : game.frameItems) {
                const auto& oi = kv.second;
                if (!oi || !oi->common) continue;
                if (!oi->common->extension.data.empty()) {
                    std::cout << "obj " << kv.first << " \"" << oi->name << "\" extId="
                              << oi->common->extension.id << " size="
                              << oi->common->extension.data.size() << " :";
                    char hex[8];
                    for (size_t b = 0; b < oi->common->extension.data.size() && b < 96; ++b) {
                        std::snprintf(hex, sizeof(hex), " %02X", oi->common->extension.data[b]);
                        std::cout << hex;
                    }
                    std::cout << "\n";
                }
                if (oi->common->counter) {
                    std::cout << "obj " << kv.first << " \"" << oi->name << "\" CNTR id=\""
                              << oi->common->identifier << "\" dt="
                              << oi->common->counter->displayType << " frames=";
                    for (uint16_t f : oi->common->counter->frames) std::cout << f << ",";
                    std::cout << " head:";
                    char hex[8];
                    for (size_t b = 0; b < oi->common->counter->rawHead.size(); ++b) {
                        std::snprintf(hex, sizeof(hex), " %02X", oi->common->counter->rawHead[b]);
                        std::cout << hex;
                    }
                    std::cout << "\n";
                }
            }
        }
        return true;
    }

    // .ccn / .dat / .bin / anything else: raw CCN stream.
    int64_t start = 0;
    if (file.size() >= 4 && memcmp(file.data(), "\x77\x77\x77\x77", 4) == 0) start = 32;
    game.dataOffset = start;
    game.sourcePath = path;
    BinaryReader reader(file.data(), file.size());
    reader.seek(start);
    game.read(reader);
    if (getenv("CTFAK_DUMP_HANDLES")) {
        std::cout << "=== storyboard frameHandles (" << game.frameHandles.size() << ") ===\n";
        for (size_t i = 0; i < game.frameHandles.size(); ++i) {
            int16_t h = game.frameHandles[i];
            int fileIdx = (h >= 0 && (size_t)h < game.frames.size()) ? h : -1;
            std::string nm = (fileIdx >= 0) ? game.frames[fileIdx]->name : std::string("?");
            std::cout << "slot " << i << " -> " << h << " -> file[" << fileIdx << "] \"" << nm << "\"\n";
        }
        std::cout << "=== file order frames ===\n";
        for (size_t fi = 0; fi < game.frames.size(); ++fi)
            std::cout << "file[" << fi << "] \"" << (game.frames[fi] ? game.frames[fi]->name : std::string("?")) << "\"\n";
    }
    return true;
}

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);

    // Work relative to the executable directory, mirroring the reference CLI.
    {
        std::string self = argv[0];
        size_t slash = self.find_last_of("/\\");
        std::string dir = (slash == std::string::npos) ? "." : self.substr(0, slash);
#ifdef _WIN32
        SetCurrentDirectoryA(dir.c_str());
#else
        if (chdir(dir.c_str()) != 0) {
        }
#endif
    }

    Logger::openFile("Latest.log");
    createDirectories("Plugins");
    createDirectories(g_dumpsDir);
    printBanner();

    std::string path;
    std::string parameters;
    std::vector<uint8_t> fileCheck;

    bool askForPath = true;
    while (askForPath) {
        path.clear();
        parameters.clear();
        if (args.size() == 1) {
            path = args[0];
        } else if (argAfter(args, "-path").empty() &&
                   std::find(args.begin(), args.end(), "-path") == args.end()) {
            std::cout << "Game path: ";
            std::string line;
            if (!std::getline(std::cin, line)) {
                // EOF on a closed stdin: leave instead of spinning forever.
                std::cout << "\nERROR: EOF on stdin\n";
                Logger::closeFile();
                return 1;
            }
            // Trim quotes.
            if (line.size() >= 2 && line.front() == '"' && line.back() == '"')
                line = line.substr(1, line.size() - 2);
            path = line;
        } else {
            path = argAfter(args, "-path");
        }

        // Mirrors the reference CLI: the "Parameters:" prompt only appears when
        // the path was also prompted for. With -path on the command line,
        // parameters come from -parameters or default to empty.
        if (args.size() != 1) {
            bool hasPathArg =
                std::find(args.begin(), args.end(), "-path") != args.end();
            auto it = std::find(args.begin(), args.end(), "-parameters");
            if (it != args.end() && it + 1 != args.end()) {
                parameters = *(it + 1);
            } else if (!hasPathArg) {
                std::cout << "Parameters: ";
                if (!std::getline(std::cin, parameters)) parameters.clear();
            }
        }

        if (!readFileBytes(path, fileCheck)) {
            std::cout << "ERROR: File not found\n";
            continue;
        }
        askForPath = false;
    }

    Settings::parseParameters(parameters);
    if (Settings::hasParameter("verbose")) Logger::verbose = true;
    if (Settings::hasParameter("log")) Logger::logToScreen = true;
    {
        std::string d = argAfter(args, "-dumps");
        if (!d.empty()) g_dumpsDir = d;
    }

    auto started = std::chrono::steady_clock::now();
    GameData game;
    if (!loadGame(path, game)) {
        std::cout << "ERROR: Failed to read game\n";
        return 1;
    }
    auto finished = std::chrono::steady_clock::now();
    double seconds = std::chrono::duration_cast<std::chrono::duration<double>>(finished - started).count();
    std::cout << "Reading finished in " << seconds << " seconds\n\n";

    // Tool selection loop.
    while (true) {
        std::cout << "Game Information:\n";
        std::cout << "Game Name: " << game.name << "\n";
        std::cout << "Author: " << game.author << "\n";
        std::cout << "Number of frames: " << game.frames.size() << "\n";
        std::cout << "Fusion Build: " << game.productBuild << "\n\n";

        auto& tools = builtinTools();
        std::cout << tools.size() << " tool(s) available\n\n";

        std::string toolArg = argAfter(args, "-tool");
        if (toolArg.empty()) {
            std::cout << "Select tool: \n";
            std::cout << "0. Exit ctfak-cpp\n";
            for (size_t i = 0; i < tools.size(); ++i)
                std::cout << i + 1 << ". " << tools[i]->name() << "\n";
            std::string line;
            std::getline(std::cin, line);
            int selection = atoi(line.c_str());
            if (selection == 0) break;
            if (selection < 1 || selection > static_cast<int>(tools.size())) continue;

            std::cout << "Selected tool: " << tools[selection - 1]->name() << ". Executing\n";
            auto t0 = std::chrono::steady_clock::now();
            try {
                Settings::build = game.productBuild;
                tools[selection - 1]->execute(game);
            } catch (const std::exception& e) {
                Logger::error(e.what());
            }
            auto t1 = std::chrono::steady_clock::now();
            double s = std::chrono::duration_cast<std::chrono::duration<double>>(t1 - t0).count();
            std::cout << "Execution of " << tools[selection - 1]->name() << " finished in " << s
                      << " seconds\n";
        } else {
            for (size_t i = 0; i + 1 < args.size(); ++i) {
                if (args[i] != "-tool") continue;
                const std::string& wanted = args[i + 1];
                for (auto* tool : tools) {
                    if (wanted == tool->name()) {
                        auto t0 = std::chrono::steady_clock::now();
                        try {
                            Settings::build = game.productBuild;
                            tool->execute(game);
                        } catch (const std::exception& e) {
                            Logger::error(e.what());
                        }
                        auto t1 = std::chrono::steady_clock::now();
                        double s =
                            std::chrono::duration_cast<std::chrono::duration<double>>(t1 - t0).count();
                        std::cout << "Execution of " << tool->name() << " finished in " << s
                                  << " seconds\n";
                    }
                }
            }
        }

        if (std::find(args.begin(), args.end(), "-closeonfinish") != args.end()) break;
    }

    Logger::closeFile();
    return 0;
}
