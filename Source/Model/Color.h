#pragma once

#include "Shape.h"  // for Color7
#include <algorithm>
#include <cmath>

namespace erae {

// ============================================================
// HSV → 7-bit RGB conversion (ported from color.py)
// ============================================================
inline Color7 hsvToRgb7(float h, float s, float v)
{
    h = std::fmod(h, 360.0f);
    if (h < 0) h += 360.0f;
    float c = v * s;
    float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;

    float r1, g1, b1;
    if (h < 60)       { r1 = c; g1 = x; b1 = 0; }
    else if (h < 120) { r1 = x; g1 = c; b1 = 0; }
    else if (h < 180) { r1 = 0; g1 = c; b1 = x; }
    else if (h < 240) { r1 = 0; g1 = x; b1 = c; }
    else if (h < 300) { r1 = x; g1 = 0; b1 = c; }
    else              { r1 = c; g1 = 0; b1 = x; }

    return {
        (int)((r1 + m) * 127),
        (int)((g1 + m) * 127),
        (int)((b1 + m) * 127)
    };
}

// Chromatic rainbow color for a MIDI note (pitch class 0-11)
inline Color7 noteColor(int note)
{
    int pc = note % 12;
    return hsvToRgb7((float)(pc * 30), 0.85f, 0.9f);
}

// Dim a color by a factor
inline Color7 dim(Color7 c, float factor = 0.4f)
{
    return {(int)(c.r * factor), (int)(c.g * factor), (int)(c.b * factor)};
}

// Brighten a color, clamped to 127
inline Color7 brighten(Color7 c, float factor = 1.5f)
{
    return {
        std::min(127, (int)(c.r * factor)),
        std::min(127, (int)(c.g * factor)),
        std::min(127, (int)(c.b * factor))
    };
}

// ============================================================
// Palette constants (7-bit RGB)
// ============================================================
namespace Palette {
    inline constexpr Color7 Black    {0, 0, 0};
    inline constexpr Color7 White    {127, 127, 127};
    inline constexpr Color7 Red      {127, 0, 0};
    inline constexpr Color7 Green    {0, 127, 0};
    inline constexpr Color7 Blue     {0, 0, 127};
    inline constexpr Color7 Yellow   {127, 127, 0};
    inline constexpr Color7 Cyan     {0, 127, 127};
    inline constexpr Color7 Magenta  {127, 0, 127};
    inline constexpr Color7 Orange   {127, 64, 0};
    inline constexpr Color7 Purple   {80, 0, 127};
    inline constexpr Color7 Gray     {40, 40, 40};
    inline constexpr Color7 DimWhite {20, 20, 20};
}

} // namespace erae
