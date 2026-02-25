#pragma once

#include "../Model/Shape.h"
#include "../Model/VisualStyle.h"
#include <juce_graphics/juce_graphics.h>
#include <vector>
#include <map>
#include <string>

namespace erae {

struct WidgetState {
    float normX = 0.5f;   // finger X normalized 0-1 within shape bbox
    float normY = 0.5f;   // finger Y normalized 0-1 within shape bbox
    float pressure = 0;   // z value 0-1
    bool active = false;   // any finger touching this shape?

    bool operator==(const WidgetState& o) const {
        return normX == o.normX && normY == o.normY && pressure == o.pressure && active == o.active;
    }
    bool operator!=(const WidgetState& o) const { return !(*this == o); }
};

namespace WidgetRenderer {

    // For Erae surface: returns per-pixel color commands
    struct PixelCommand { int x, y; Color7 color; };
    std::vector<PixelCommand> renderWidget(const Shape& shape, const WidgetState& state);

    // For GridCanvas: draws directly to JUCE Graphics within screen bounds
    void drawWidget(juce::Graphics& g, const Shape& shape, const WidgetState& state,
                    juce::Rectangle<float> screenBounds, float cellPx);
}

} // namespace erae
