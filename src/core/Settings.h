// Global reader settings shared across the program (game type, build, unicode mode).
#pragma once

#include <cstdint>
#include <string>
#include <set>

namespace ctfak {

enum class GameType {
    Normal = 1,
    Mmf2 = 2,
    Mmf15 = 4,
    Android = 8,
    TwoFivePlus = 16,
    F3 = 32,
    Ios = 64
};

inline bool hasFlag(uint32_t value, GameType flag) {
    return (value & static_cast<uint32_t>(flag)) != 0;
}

struct Settings {
    static uint32_t gameType;   // bitfield of GameType
    static int build;           // product build number
    // NOTE: stored as int (not byte) intentionally. The reference declares its
    // counterpart as a byte, so its "== 284" comparison can never fire; an int
    // here keeps that identical runtime behavior while avoiding a compiler
    // dead-comparison warning. Valid file values are 0-255 either way.
    static int buildType;       // from extended header (0 if unknown)
    static bool unicode;        // wide strings vs ANSI
    static bool isMFA;
    static bool fusion3Seed;    // "seeded" Fusion 3 file encountered
    static std::set<std::string> parameters; // parsed CLI parameters (without dash)

    static bool old() { return hasFlag(gameType, GameType::Mmf15); }
    static bool twoFivePlus() { return hasFlag(gameType, GameType::TwoFivePlus); }
    static bool android() { return hasFlag(gameType, GameType::Android); }
    static bool f3() { return hasFlag(gameType, GameType::F3); }
    static bool ios() { return hasFlag(gameType, GameType::Ios); }

    static void setGameType(GameType t) { gameType = static_cast<uint32_t>(t); }
    static void addGameType(GameType t) { gameType |= static_cast<uint32_t>(t); }

    static bool hasParameter(const std::string& name);
    // Registers a raw parameter string such as "-noimg -nosounds excludeframe(2)".
    static void parseParameters(const std::string& raw);
};

} // namespace ctfak
