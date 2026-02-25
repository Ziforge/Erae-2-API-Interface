#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

namespace erae {

enum class ScaleType {
    Chromatic, Major, NaturalMinor, HarmonicMinor, Pentatonic,
    MinorPentatonic, WholeTone, Blues, Dorian, Mixolydian
};

inline ScaleType scaleFromString(const std::string& s)
{
    if (s == "major")            return ScaleType::Major;
    if (s == "natural_minor")    return ScaleType::NaturalMinor;
    if (s == "harmonic_minor")   return ScaleType::HarmonicMinor;
    if (s == "pentatonic")       return ScaleType::Pentatonic;
    if (s == "minor_pentatonic") return ScaleType::MinorPentatonic;
    if (s == "whole_tone")       return ScaleType::WholeTone;
    if (s == "blues")            return ScaleType::Blues;
    if (s == "dorian")           return ScaleType::Dorian;
    if (s == "mixolydian")       return ScaleType::Mixolydian;
    return ScaleType::Chromatic;
}

inline std::string scaleToString(ScaleType s)
{
    switch (s) {
        case ScaleType::Major:           return "major";
        case ScaleType::NaturalMinor:    return "natural_minor";
        case ScaleType::HarmonicMinor:   return "harmonic_minor";
        case ScaleType::Pentatonic:      return "pentatonic";
        case ScaleType::MinorPentatonic: return "minor_pentatonic";
        case ScaleType::WholeTone:       return "whole_tone";
        case ScaleType::Blues:           return "blues";
        case ScaleType::Dorian:          return "dorian";
        case ScaleType::Mixolydian:      return "mixolydian";
        default:                         return "chromatic";
    }
}

// Returns the semitone intervals for a given scale
inline std::vector<int> getScaleIntervals(ScaleType scale)
{
    switch (scale) {
        case ScaleType::Major:           return {0, 2, 4, 5, 7, 9, 11};
        case ScaleType::NaturalMinor:    return {0, 2, 3, 5, 7, 8, 10};
        case ScaleType::HarmonicMinor:   return {0, 2, 3, 5, 7, 8, 11};
        case ScaleType::Pentatonic:      return {0, 2, 4, 7, 9};
        case ScaleType::MinorPentatonic: return {0, 3, 5, 7, 10};
        case ScaleType::WholeTone:       return {0, 2, 4, 6, 8, 10};
        case ScaleType::Blues:           return {0, 3, 5, 6, 7, 10};
        case ScaleType::Dorian:          return {0, 2, 3, 5, 7, 9, 10};
        case ScaleType::Mixolydian:      return {0, 2, 4, 5, 7, 9, 10};
        default:                         return {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    }
}

// Quantize a MIDI note to the nearest scale degree
// rootNote: 0-11 (C=0, C#=1, ... B=11)
inline int quantizeNote(int note, int rootNote, ScaleType scale)
{
    if (scale == ScaleType::Chromatic) return note;

    auto intervals = getScaleIntervals(scale);
    int pc = ((note - rootNote) % 12 + 12) % 12; // pitch class relative to root
    int octave = (note - rootNote) / 12;
    if ((note - rootNote) < 0 && pc != 0) octave--;

    // Find nearest scale degree
    int bestInterval = 0;
    int bestDist = 12;
    for (int interval : intervals) {
        int dist = std::abs(pc - interval);
        if (dist > 6) dist = 12 - dist; // wrap around
        if (dist < bestDist) {
            bestDist = dist;
            bestInterval = interval;
        }
    }

    return rootNote + octave * 12 + bestInterval;
}

// Quantize pitch bend to snap to scale degrees
// pb: 0-16383 (center = 8192)
// baseNote: the MIDI note this pitch bend is relative to
// pbRange: pitch bend range in semitones
// glideAmount: 0.0 = hard snap, 1.0 = no quantization (smooth glide)
// Returns quantized pitch bend value 0-16383
inline int quantizePitchBend(int pb, int baseNote, int rootNote,
                              ScaleType scale, int pbRange, float glideAmount)
{
    if (scale == ScaleType::Chromatic || pbRange <= 0) return pb;

    // Convert pb to semitone offset from baseNote
    float semitones = (float)(pb - 8192) / 8192.0f * (float)pbRange;
    float targetNote = (float)baseNote + semitones;

    // Quantize the target note
    int quantized = quantizeNote((int)std::round(targetNote), rootNote, scale);
    float quantSemitones = (float)(quantized - baseNote);

    // Blend between quantized and original based on glide amount
    float finalSemitones = quantSemitones + (semitones - quantSemitones) * glideAmount;

    // Convert back to pitch bend
    int result = (int)(finalSemitones / (float)pbRange * 8192.0f) + 8192;
    return std::max(0, std::min(16383, result));
}

} // namespace erae
