#include "ExtendedHeader.h"
#include "../core/Settings.h"
#include "../core/Logger.h"

namespace ctfak {

void ExtendedHeader::read(BinaryReader& reader) {
    flags.set(reader.readU32());
    buildType = reader.readU8();
    Settings::buildType = buildType;
    reader.skip(3);
    compressionFlags.set(reader.readU32());
    screenRatio = reader.readI16();
    screenAngle = reader.readI16();
    reader.readI16(); // view flags
    reader.readI16(); // new flags

    switch (buildType) {
        case 12:
        case 34: Settings::addGameType(GameType::Android); break;
        case 13:
        case 14:
        case 15: Settings::addGameType(GameType::Ios); break;
        case 74:
        case 75:
        case 78: Settings::addGameType(GameType::F3); break;
        default: break;
    }
}

const char* ExtendedHeader::buildTypeName() const {
    switch (buildType) {
        case 0: return "Windows EXE Application";
        case 1: return "Windows Screen Saver";
        case 2: return "Sub-Application";
        case 3: return "Java Sub-Application";
        case 4: return "Java Application";
        case 5: return "Java Internet Applet";
        case 6: return "Java Web Start";
        case 7: return "Java for Mobile Devices";
        case 9: return "Java Mac Application";
        case 10: return "Adobe Flash";
        case 11: return "Java for BlackBerry";
        case 12: return "Android / OUYA Application";
        case 13: return "iOS Application";
        case 14: return "iOS Xcode Project";
        case 15: return "Final iOS Xcode Project";
        case 18: return "XNA Windows Project";
        case 19: return "XNA Xbox Project";
        case 20: return "XNA Phone Project";
        case 27: return "HTML5 Development";
        case 28: return "HTML5 Final Project";
        case 33: return "UWP Project";
        case 34: return "Android App Bundle";
        case 74: return "Nintendo Switch";
        case 75: return "Xbox One";
        case 78: return "Playstation";
        default: return "Unknown Build Type";
    }
}

} // namespace ctfak
