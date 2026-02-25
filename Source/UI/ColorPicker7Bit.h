#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Model/Shape.h"
#include "../Model/Color.h"
#include "Theme.h"

namespace erae {

// ============================================================
// ColorPicker7Bit — HSV picker with 7-bit RGB output for Erae II
//
// Layout (vertical, fits in sidebar):
//   [Hue bar]         — horizontal, full width, 20px tall
//   [SV square]       — square, full width
//   [Preview swatch]  — current color, 30px tall
//   [RGB readout]     — "R:xxx G:xxx B:xxx" label
//   [Quick palette]   — 2 rows of 8 preset colors
// ============================================================
class ColorPicker7Bit : public juce::Component {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void colorChanged(Color7 newColor) = 0;
    };

    ColorPicker7Bit();

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

    Color7 getColor() const { return currentColor_; }
    void setColor(Color7 c);

    void addListener(Listener* l) { listeners_.push_back(l); }
    void removeListener(Listener* l) {
        listeners_.erase(std::remove(listeners_.begin(), listeners_.end(), l), listeners_.end());
    }

private:
    enum class DragTarget { None, Hue, SV, Palette };

    void updateFromHSV();
    void notifyListeners();
    void handleHueClick(float x);
    void handleSVClick(float x, float y);
    void handlePaletteClick(float x, float y);

    juce::Rectangle<int> hueBarBounds() const;
    juce::Rectangle<int> svSquareBounds() const;
    juce::Rectangle<int> previewBounds() const;
    juce::Rectangle<int> labelBounds() const;
    juce::Rectangle<int> paletteBounds() const;

    float hue_ = 200.0f;   // 0-360
    float sat_ = 0.85f;    // 0-1
    float val_ = 0.9f;     // 0-1
    Color7 currentColor_;
    DragTarget dragTarget_ = DragTarget::None;

    std::vector<Listener*> listeners_;

    // Quick palette (16 colors)
    static constexpr int kPaletteCols = 8;
    static constexpr int kPaletteRows = 2;
    Color7 palette_[kPaletteRows * kPaletteCols];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ColorPicker7Bit)
};

} // namespace erae
