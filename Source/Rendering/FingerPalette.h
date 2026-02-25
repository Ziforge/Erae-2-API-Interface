#pragma once

#include "../Model/Shape.h" // Color7

namespace erae {

namespace FingerPalette {

    // 10 distinct colors for per-finger display
    inline Color7 colorForFinger(int index)
    {
        static const Color7 palette[10] = {
            {127,   0,   0},  // red
            {  0, 127,   0},  // green
            {  0,   0, 127},  // blue
            {127, 127,   0},  // yellow
            {127,   0, 127},  // magenta
            {  0, 127, 127},  // cyan
            {127,  64,   0},  // orange
            { 80,   0, 127},  // purple
            {127, 127, 127},  // white
            { 64, 127,   0},  // lime
        };
        return palette[((index % 10) + 10) % 10];
    }

    inline juce::Colour juceColorForFinger(int index)
    {
        return colorForFinger(index).toJuceColour();
    }

} // namespace FingerPalette
} // namespace erae
