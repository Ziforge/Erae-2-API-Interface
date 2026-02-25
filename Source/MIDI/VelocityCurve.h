#pragma once

#include <cmath>
#include <string>

namespace erae {

enum class CurveType { Linear, Exponential, Logarithmic, SCurve };

inline CurveType curveFromString(const std::string& s)
{
    if (s == "exponential") return CurveType::Exponential;
    if (s == "logarithmic") return CurveType::Logarithmic;
    if (s == "s_curve")     return CurveType::SCurve;
    return CurveType::Linear;
}

inline std::string curveToString(CurveType c)
{
    switch (c) {
        case CurveType::Exponential: return "exponential";
        case CurveType::Logarithmic: return "logarithmic";
        case CurveType::SCurve:      return "s_curve";
        default:                     return "linear";
    }
}

// Apply curve to input in range [0, 1], returns [0, 1]
inline float applyCurve(float input, CurveType curve)
{
    input = std::max(0.0f, std::min(1.0f, input));

    switch (curve) {
        case CurveType::Linear:
            return input;

        case CurveType::Exponential:
            // x^3 — light taps = low, hard press = high
            return input * input * input;

        case CurveType::Logarithmic:
            // 1-(1-x)^3 — sensitive at low pressures
            { float inv = 1.0f - input;
              return 1.0f - inv * inv * inv; }

        case CurveType::SCurve:
            // Smoothstep: 3x^2 - 2x^3 — gentle at extremes, steep in middle
            return input * input * (3.0f - 2.0f * input);
    }
    return input;
}

} // namespace erae
