#include "ExeFile.h"
#include "../core/Logger.h"

namespace ctfak {

int64_t calculateEntryPoint(BinaryReader& reader) {
    reader.seek(0);
    if (reader.size() < 64) return 0;
    std::string mz = reader.readAscii(2);
    if (mz != "MZ") Logger::error("Invalid executable signature");

    reader.seek(60);
    uint16_t hdrOffset = reader.readU16();
    reader.seek(hdrOffset);
    reader.readAscii(2);        // "PE" signature
    reader.skip(4);             // "\0\0" + Machine
    uint16_t numSections = reader.readU16();
    reader.skip(16);            // COFF fields
    reader.skip(28 + 68);       // optional header (standard + windows-specific)
    reader.skip(16 * 8);        // data directories

    int64_t position = 0;
    for (int i = 0; i < numSections; ++i) {
        int64_t entry = reader.tell();
        std::string name = reader.readAscii(8);
        if (name == ".extra") {
            reader.seek(entry + 20); // Name(8)+VirtualSize(4)+VirtualAddress(4)+SizeOfRawData(4)
            position = reader.readU32();
            break;
        }
        if (i == numSections - 1) {
            reader.seek(entry + 16); // SizeOfRawData / PointerToRawData
            uint32_t secSize = reader.readU32();
            uint32_t secAddress = reader.readU32();
            position = static_cast<int64_t>(secAddress) + secSize;
            break;
        }
        reader.seek(entry + 40);
    }
    return position;
}

} // namespace ctfak
