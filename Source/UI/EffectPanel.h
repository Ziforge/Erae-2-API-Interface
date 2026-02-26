#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Model/Shape.h"
#include "../Effects/TouchEffect.h"
#include "Theme.h"

namespace erae {

class EffectPanel : public juce::Component,
                    private juce::ComboBox::Listener,
                    private juce::Slider::Listener,
                    private juce::Button::Listener {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void effectChanged(const std::string& shapeId) = 0;
    };

    EffectPanel();

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

    // Effect type
    juce::Label effectLabel_   {"", "TOUCH EFFECT"};
    juce::ComboBox effectBox_;

    // Parameters
    juce::Label paramsLabel_   {"", "PARAMETERS"};
    juce::Label speedLabel_    {"", "Speed"};
    juce::Slider speedSlider_;
    juce::Label intensityLabel_{"", "Intensity"};
    juce::Slider intensitySlider_;
    juce::Label decayLabel_    {"", "Decay"};
    juce::Slider decaySlider_;
    juce::Label motionLabel_   {"", "Motion React"};
    juce::ToggleButton motionToggle_;

    // Modulation
    juce::Label modLabel_      {"", "MODULATION"};
    juce::Label targetLabel_   {"", "Target"};
    juce::ComboBox targetBox_;
    juce::Label ccLabel_       {"", "CC"};
    juce::Slider ccSlider_;
    juce::Label channelLabel_  {"", "Channel"};
    juce::Slider channelSlider_;
    juce::Label cvChLabel_     {"", "CV Ch"};
    juce::Slider cvChSlider_;
    juce::Label mpeChLabel_    {"", "MPE Ch"};
    juce::Slider mpeChSlider_;

    // No shape selected
    juce::Label noShapeLabel_  {"", "No shape selected"};

    std::vector<Listener*> listeners_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectPanel)
};

} // namespace erae
