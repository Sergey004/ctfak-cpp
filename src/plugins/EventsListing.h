// Events Listing: writes the parsed event program (conditions/actions) of every
// frame as a human readable text listing in the style of the Fusion event grid:
//
//   [Group 12]  line 34
//     ON  Timer cond #-8
//         - timer 90 (~1.8s)
//     ON  "new game" (Active) cond #7
//         - click button 1
//     DO  "static" (Active) act #65
//         - expr ==: 50, System(type=0, num=2), ...
//
// The same information already exists in the JSON export; this tool renders it
// for humans. Output: <dumps>/<Game>/Events/frame_N_<name>_events.txt plus a
// combined ALL_EVENTS.txt.
#pragma once

#include "Plugin.h"
#include "../format/Events.h"
#include <functional>
#include <string>

namespace ctfak {

// Resolves an objectInfo handle to a display name (empty string if unknown).
using ObjectNameResolver = std::function<std::string(int)>;

// Human readable, single-line description of one event parameter
// (no trailing newline, ASCII only).
std::string describeEventParameter(const Parameter& p, const ObjectNameResolver& resolver);

// "\"name\" (Active)" / "Timer" / "System" label for the object a condition or
// action is bound to.
std::string eventObjectLabel(int16_t objectType, uint16_t objectInfo,
                             const ObjectNameResolver& resolver);

// Windows virtual-key name for Fusion key parameters ("Enter", "Space",
// "Left", "A"...); returns null for unmapped codes.
const char* virtualKeyName(int code);

class EventsListing : public FusionTool {
public:
    const char* name() const override { return "Events Listing"; }
    void execute(GameData& game) override;
};

} // namespace ctfak
