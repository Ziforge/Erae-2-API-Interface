#pragma once

#include "TouchEffect.h"
#include "../Model/Color.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <map>
#include <string>

namespace erae {

namespace EffectRenderer {

    // Pixel command for LED framebuffer overlay
    struct EffectPixel {
        int x, y;
        Color7 color;
        float alpha; // 0-1 blend factor
    };

    // Generate LED framebuffer pixels for all active effects
    std::vector<EffectPixel> renderEffects(
        const std::map<std::string, ShapeEffectState>& states,
        const std::map<std::string, EffectParams>& params,
        const std::map<std::string, const Shape*>& shapes);

    // Draw effects on the GridCanvas (screen-space)
    void drawEffects(
        juce::Graphics& g,
        const std::map<std::string, ShapeEffectState>& states,
        const std::map<std::string, EffectParams>& params,
        const std::map<std::string, const Shape*>& shapes,
        std::function<juce::Point<float>(juce::Point<float>)> gridToScreen,
        float cellPx);

} // namespace EffectRenderer
} // namespace erae
