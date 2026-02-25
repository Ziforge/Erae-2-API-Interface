#pragma once

#include <string>

namespace erae {

// Behavior types — how a shape responds to touch and generates MIDI
enum class BehaviorType {
    Trigger,      // Note on press, off on release (no slide)
    Momentary,    // Like trigger but sends pressure from Z while held
    NotePad,      // Full MPE (velocity, pitchbend from X slide, pressure from Z)
    XYController, // Two CCs from X/Y position within shape (no notes)
    Fader         // Single CC from position along one axis
};

inline BehaviorType behaviorFromString(const std::string& s)
{
    if (s == "trigger")       return BehaviorType::Trigger;
    if (s == "momentary")     return BehaviorType::Momentary;
    if (s == "note_pad")      return BehaviorType::NotePad;
    if (s == "xy_controller") return BehaviorType::XYController;
    if (s == "fader")         return BehaviorType::Fader;
    return BehaviorType::Trigger;
}

inline std::string behaviorToString(BehaviorType t)
{
    switch (t) {
        case BehaviorType::Trigger:      return "trigger";
        case BehaviorType::Momentary:    return "momentary";
        case BehaviorType::NotePad:      return "note_pad";
        case BehaviorType::XYController: return "xy_controller";
        case BehaviorType::Fader:        return "fader";
    }
    return "trigger";
}

} // namespace erae
