#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Model/Shape.h"
#include "../Model/Behavior.h"
#include "../Model/VisualStyle.h"
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

    PropertyPanel();

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
    juce::Label slideCCLabel_   {"", "Slide CC"};
    juce::Slider slideCCSlider_;

    juce::Label mpeHint_        {"", "(MPE: pitch-X, slide-Y, pressure-Z)"};

    // Visual style controls
    juce::Label visualLabel_    {"", "VISUAL"};
    juce::ComboBox visualBox_;
    juce::Label fillHorizLabel_ {"", "Fill Horiz"};
    juce::ToggleButton fillHorizToggle_;

    std::vector<Listener*> listeners_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PropertyPanel)
};

} // namespace erae
