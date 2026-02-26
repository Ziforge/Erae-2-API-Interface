#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Model/Shape.h"
#include "../Model/Behavior.h"
#include "Theme.h"

namespace erae {

class OutputPanel : public juce::Component,
                    private juce::Slider::Listener,
                    private juce::Button::Listener {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void cvParamsChanged(const std::string& shapeId) = 0;
        virtual void oscSettingsChanged(bool enabled, const std::string& host, int port) = 0;
    };

    OutputPanel();

    void paint(juce::Graphics& g) override;
    void resized() override;

    void loadShape(Shape* shape);
    void clearShape();

    // OSC state (set from processor on init)
    void setOscState(bool enabled, const std::string& host, int port);

    void addListener(Listener* l) { listeners_.push_back(l); }
    void removeListener(Listener* l) {
        listeners_.erase(std::remove(listeners_.begin(), listeners_.end(), l), listeners_.end());
    }

private:
    void sliderValueChanged(juce::Slider* slider) override;
    void buttonClicked(juce::Button* button) override;

    void writeCVToShape();
    void notifyCV();
    void notifyOSC();

    Shape* currentShape_ = nullptr;
    bool loading_ = false;

    // CV section (per-shape)
    juce::Label cvLabel_        {"", "CV OUTPUT"};
    juce::Label cvEnableLabel_  {"", "CV Enabled"};
    juce::ToggleButton cvEnableToggle_;
    juce::Label cvChannelLabel_ {"", "CV Channel"};
    juce::Slider cvChannelSlider_;

    // OSC section (global)
    juce::Label oscLabel_          {"", "OSC OUTPUT"};
    juce::ToggleButton oscToggle_  {"Enable"};
    juce::Label oscHostLabel_      {"", "Host"};
    juce::TextEditor oscHostEditor_;
    juce::Label oscPortLabel_      {"", "Port"};
    juce::Slider oscPortSlider_;

    std::vector<Listener*> listeners_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutputPanel)
};

} // namespace erae
