#pragma once

#include <string>

namespace erae {

enum class VisualStyle {
    Static,       // Current behavior — solid fill, no animation
    FillBar,      // Fills vertically or horizontally based on touch position
    PositionDot,  // Bright dot follows finger within shape bounds
    RadialArc,    // Arc sweeps from 0 based on touch position (like a knob)
    PressureGlow  // Color brightness pulses with pressure (z)
};

inline VisualStyle visualStyleFromString(const std::string& s)
{
    if (s == "fill_bar")      return VisualStyle::FillBar;
    if (s == "position_dot")  return VisualStyle::PositionDot;
    if (s == "radial_arc")    return VisualStyle::RadialArc;
    if (s == "pressure_glow") return VisualStyle::PressureGlow;
    return VisualStyle::Static;
}

inline std::string visualStyleToString(VisualStyle v)
{
    switch (v) {
        case VisualStyle::Static:       return "static";
        case VisualStyle::FillBar:      return "fill_bar";
        case VisualStyle::PositionDot:  return "position_dot";
        case VisualStyle::RadialArc:    return "radial_arc";
        case VisualStyle::PressureGlow: return "pressure_glow";
    }
    return "static";
}

} // namespace erae
