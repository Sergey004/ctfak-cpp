// App header chunk (8739) and player controls.
#pragma once

#include "../core/BinaryReader.h"
#include "../core/BitDict.h"
#include <vector>

namespace ctfak {

struct PlayerControl {
    int16_t controlType = 0;
    int16_t up = 0, down = 0, left = 0, right = 0;
    int16_t button1 = 0, button2 = 0, button3 = 0, button4 = 0;
};

struct AppHeader {
    int32_t size = 0;
    BitDict flags = {"HeadingMaximized", "NoHeading", "FitInsideBars", "MachineIndependentSpeed",
                     "ResizeDisplay", "MusicOn", "SoundOn", "DontDisplayMenu", "MenuBar",
                     "MaximizedOnBoot", "MultiSamples", "ChangeResolutionMode",
                     "SwitchToFromFullscreen", "Protected", "Copyright", "OneFile"};
    BitDict newFlags = {"SamplesOverFrames", "RelocFiles", "RunFrame", "PlaySamplesWhenUnfocused",
                        "NoMinimizeBox", "NoMaximizeBox", "NoThickFrame", "DoNotCenterFrame",
                        "IgnoreInputOnScreensaver", "DisableClose", "HiddenAtStart", "VisualThemes",
                        "VSync", "RunWhenMinimized", "MDI", "RunWhileResizing"};
    BitDict otherFlags = {"DebuggerShortcuts", "Unknown1", "Unknown2", "DontShareSubData",
                          "Unknown3", "Unknown4", "Unknown5", "ShowDebugger", "Unknown6",
                          "Unknown7", "Unknown8", "Unknown9", "Unknown10", "Unknown11",
                          "Direct3D9or11", "Direct3D8or11"};
    int16_t graphicsMode = 0;
    int32_t windowWidth = 0, windowHeight = 0;
    int32_t initialScore = 0, initialLives = 0;
    std::vector<PlayerControl> controls;
    BinaryReader::Color borderColor;
    int32_t numberOfFrames = 0;
    int32_t frameRate = 0;
    int32_t windowsMenuIndex = 0;

    void read(BinaryReader& reader);
};

} // namespace ctfak
