#include "AppHeader.h"
#include "../core/Settings.h"

namespace ctfak {

void AppHeader::read(BinaryReader& reader) {
    if (!Settings::old()) size = reader.readI32();
    flags.set(reader.readU16());
    newFlags.set(reader.readU16());
    graphicsMode = reader.readI16();
    otherFlags.set(reader.readU16());
    windowWidth = reader.readI16();
    windowHeight = reader.readI16();
    initialScore = static_cast<int32_t>(reader.readU32() ^ 0xFFFFFFFF);
    initialLives = static_cast<int32_t>(reader.readU32() ^ 0xFFFFFFFF);

    if (Settings::old()) {
        reader.skip(56);
    } else {
        controls.resize(4);
        for (auto& ctrl : controls) {
            ctrl.controlType = reader.readI16();
            ctrl.up = reader.readI16();
            ctrl.down = reader.readI16();
            ctrl.left = reader.readI16();
            ctrl.right = reader.readI16();
            ctrl.button1 = reader.readI16();
            ctrl.button2 = reader.readI16();
            ctrl.button3 = reader.readI16();
            ctrl.button4 = reader.readI16();
        }
    }

    borderColor = reader.readColor();
    numberOfFrames = reader.readI32();
    if (Settings::old()) return;
    frameRate = reader.readI32();
    windowsMenuIndex = reader.readI32();
}

} // namespace ctfak
