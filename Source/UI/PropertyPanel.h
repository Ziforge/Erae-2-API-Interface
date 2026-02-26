#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Model/Shape.h"
#include "../Model/Behavior.h"
#include "../Model/VisualStyle.h"
#include "../Model/Layout.h"
#include "Theme.h"

namespace erae {

class PropertyPanel : public juce::Component,
                      private juce::ComboBox::Listener,
                      private juce::Slider::Listener,
                      private juce::Button::Listener {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void behaviorChanged(const std::string& shapeId) = 0;
    };

    PropertyPanel(Layout& layout);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void loadShape(Shape* shape);
    void clearShape();

    void addListener(Listener* l) { listeners_.push_back(l); }
    void removeListener(Listener* l) {
        listeners_.erase(std::remove(listeners_.begin(), listeners_.end(), l), listeners_.end());
    }

private:
    void comboBoxChanged(juce::ComboBox* box) override;
    void sliderValueChanged(juce::Slider* slider) override;
    void buttonClicked(juce::Button* button) override;

    void updateVisibility();
    void writeParamsToShape();
    void notifyListeners();

    Layout& layout_;
    Shape* currentShape_ = nullptr;
    bool loading_ = false;

    juce::Label behaviorLabel_  {"", "BEHAVIOR"};
    juce::ComboBox behaviorBox_;

    juce::Label noteLabel_      {"", "Note"};
    juce::Slider noteSlider_;
    juce::Label channelLabel_   {"", "Channel"};
    juce::Slider channelSlider_;
    juce::Label velocityLabel_  {"", "Velocity"};
    juce::Slider velocitySlider_;
    juce::Label ccLabel_        {"", "CC"};
    juce::Slider ccSlider_;
    juce::Label ccXLabel_       {"", "CC X"};
    juce::Slider ccXSlider_;
    juce::Label ccYLabel_       {"", "CC Y"};
    juce::Slider ccYSlider_;
    juce::Label horizLabel_     {"", "Horizontal"};
    juce::ToggleButton horizToggle_;
    juce::Label highresLabel_   {"", "Hi-Res 14b"};
    juce::ToggleButton highresToggle_;
    juce::Label slideCCLabel_   {"", "Slide CC"};
    juce::Slider slideCCSlider_;

    juce::Label mpeHint_        {"", "(MPE: pitch-X, slide-Y, pressure-Z)"};

    // Phase 2: Musical features
    juce::Label velCurveLabel_     {"", "Vel Curve"};
    juce::ComboBox velocityCurveBox_;
    juce::Label pressCurveLabel_   {"", "Press Curve"};
    juce::ComboBox pressureCurveBox_;
    juce::Label latchLabel_        {"", "Latch"};
    juce::ToggleButton latchToggle_;
    juce::Label scaleLabel_        {"", "Scale"};
    juce::ComboBox scaleBox_;
    juce::Label rootNoteLabel_     {"", "Root"};
    juce::Slider rootNoteSlider_;
    juce::Label pitchQuantLabel_   {"", "Quantize PB"};
    juce::ToggleButton pitchQuantizeToggle_;
    juce::Label glideLabel_        {"", "Glide"};
    juce::Slider glideSlider_;

    // Phase 4: CC ranges
    juce::Label ccMinLabel_    {"", "CC Min"};
    juce::Slider ccMinSlider_;
    juce::Label ccMaxLabel_    {"", "CC Max"};
    juce::Slider ccMaxSlider_;
    juce::Label ccXMinLabel_   {"", "X Min"};
    juce::Slider ccXMinSlider_;
    juce::Label ccXMaxLabel_   {"", "X Max"};
    juce::Slider ccXMaxSlider_;
    juce::Label ccYMinLabel_   {"", "Y Min"};
    juce::Slider ccYMinSlider_;
    juce::Label ccYMaxLabel_   {"", "Y Max"};
    juce::Slider ccYMaxSlider_;

    // CV output
    juce::Label cvLabel_        {"", "CV OUTPUT"};
    juce::Label cvEnableLabel_  {"", "CV Enabled"};
    juce::ToggleButton cvEnableToggle_;
    juce::Label cvChannelLabel_ {"", "CV Channel"};
    juce::Slider cvChannelSlider_;

    // Visual style controls
    juce::Label visualLabel_    {"", "VISUAL"};
    juce::ComboBox visualBox_;
    juce::Label fillHorizLabel_ {"", "Fill Horiz"};
    juce::ToggleButton fillHorizToggle_;

    std::vector<Listener*> listeners_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PropertyPanel)
};

} // namespace erae
