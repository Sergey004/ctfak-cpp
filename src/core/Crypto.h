// State-chunk encryption/decryption for Clickteam Fusion 2.5 files.
// Implements the publicly documented key-derivation and stream-transform algorithms
// described in docs/FORMAT_NOTES.md section 3. Clean-room implementation.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ctfak {

class Crypto {
public:
    static constexpr uint8_t kMagicChar = 54;

    // Converts a unicode string into the byte sequence used for key material:
    // for each code unit emit the low byte if non-zero, then the high byte if non-zero.
    static std::vector<uint8_t> keyString(const std::string& utf8);

    // Builds the 256-byte decryption key from concatenated key strings.
    static std::vector<uint8_t> makeKeyCombined(std::vector<uint8_t> data);

    // Builds and installs the decryption table from three metadata strings.
    // order1/2/3 depend on the build (see GameData); strings are UTF-8.
    static void makeKey(const std::string& s1, const std::string& s2, const std::string& s3);

    // In-place stream transform (RC4-like PRGA over a copy of the KSA table).
    static void transformChunk(std::vector<uint8_t>& chunk);
    static void transformChunk(uint8_t* data, size_t size);

    // True when makeKey has successfully initialized the table.
    static bool valid() { return _valid; }

private:
    static void initDecryptionTable(const std::vector<uint8_t>& magicKey, uint8_t magicChar);
    static uint8_t rotl8(uint8_t v) { return static_cast<uint8_t>((v << 7) | (v >> 1)); }

    static std::vector<uint8_t> _table;
    static bool _valid;
};

} // namespace ctfak
