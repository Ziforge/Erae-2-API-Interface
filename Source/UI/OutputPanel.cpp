#include "OutputPanel.h"

namespace erae {

static void styleLabel(juce::Label& label, bool header = false)
{
    if (header) {
        label.setFont(juce::Font(Theme::FontSection, juce::Font::bold));
        label.setColour(juce::Label::textColourId, Theme::Colors::TextDim);
    } else {
        label.setFont(juce::Font(Theme::FontBase));
        label.setColour(juce::Label::textColourId, Theme::Colors::TextDim);
    }
}

static void styleSlider(juce::Slider& slider, double min, double max, double def)
{
    slider.setRange(min, max, 1.0);
    slider.setValue(def, juce::dontSendNotification);
    slider.setSliderStyle(juce::Slider::LinearBar);
    slider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 40, 20);
    slider.setColour(juce::Slider::trackColourId, Theme::Colors::Accent);
    slider.setColour(juce::Slider::textBoxTextColourId, Theme::Colors::Text);
}

OutputPanel::OutputPanel()
{
    // CV section
    styleLabel(cvLabel_, true);
    addAndMakeVisible(cvLabel_);

    styleLabel(cvEnableLabel_);
    cvEnableToggle_.addListener(this);
    addAndMakeVisible(cvEnableLabel_);
    addAndMakeVisible(cvEnableToggle_);

    styleLabel(cvChannelLabel_);
    styleSlider(cvChannelSlider_, 0, 31, 0);
    cvChannelSlider_.addListener(this);
    addAndMakeVisible(cvChannelLabel_);
    addAndMakeVisible(cvChannelSlider_);

    // OSC section
    styleLabel(oscLabel_, true);
    addAndMakeVisible(oscLabel_);

    oscToggle_.onClick = [this] { notifyOSC(); };
    addAndMakeVisible(oscToggle_);

    oscHostLabel_.setFont(juce::Font(Theme::FontBase));
    oscHostLabel_.setColour(juce::Label::textColourId, Theme::Colors::TextDim);
    addAndMakeVisible(oscHostLabel_);

    oscHostEditor_.setFont(juce::Font(Theme::FontBase));
    oscHostEditor_.onReturnKey = [this] { notifyOSC(); };
    addAndMakeVisible(oscHostEditor_);

    oscPortLabel_.setFont(juce::Font(Theme::FontBase));
    oscPortLabel_.setColour(juce::Label::textColourId, Theme::Colors::TextDim);
    addAndMakeVisible(oscPortLabel_);

    oscPortSlider_.setRange(1024, 65535, 1);
    oscPortSlider_.setValue(9000, juce::dontSendNotification);
    oscPortSlider_.setSliderStyle(juce::Slider::LinearBar);
    oscPortSlider_.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 50, 20);
    oscPortSlider_.setColour(juce::Slider::trackColourId, Theme::Colors::Accent);
    oscPortSlider_.setColour(juce::Slider::textBoxTextColourId, Theme::Colors::Text);
    oscPortSlider_.onValueChange = [this] { notifyOSC(); };
    addAndMakeVisible(oscPortSlider_);
}

void OutputPanel::paint(juce::Graphics& g)
{
    // Separator between CV and OSC sections
    if (oscLabel_.isVisible()) {
        int lineY = oscLabel_.getY() - 4;
        g.setColour(Theme::Colors::Separator);
        g.fillRect(0, lineY, getWidth(), 1);
    }
}

void OutputPanel::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(6);
    int rowH = 26;
    int labelW = 74;
    int gap = 5;

    // CV section (only when shape is loaded)
    bool hasShape = (currentShape_ != nullptr);
    cvLabel_.setVisible(hasShape);
    cvEnableLabel_.setVisible(hasShape);
    cvEnableToggle_.setVisible(hasShape);

    if (hasShape) {
        cvLabel_.setBounds(area.removeFromTop(18));
        area.removeFromTop(3);
        {
            auto row = area.removeFromTop(rowH);
            cvEnableLabel_.setBounds(row.removeFromLeft(labelW));
            cvEnableToggle_.setBounds(row.removeFromLeft(rowH));
            area.removeFromTop(3);
        }
        bool showCVCh = cvEnableToggle_.getToggleState();
        cvChannelLabel_.setVisible(showCVCh);
        cvChannelSlider_.setVisible(showCVCh);
        if (showCVCh) {
            auto row = area.removeFromTop(rowH);
            cvChannelLabel_.setBounds(row.removeFromLeft(labelW));
            cvChannelSlider_.setBounds(row);
            area.removeFromTop(3);
        }
        area.removeFromTop(gap + 2);
    } else {
        cvChannelLabel_.setVisible(false);
        cvChannelSlider_.setVisible(false);
    }

    // OSC section (always visible)
    oscLabel_.setBounds(area.removeFromTop(18));
    area.removeFromTop(3);
    oscToggle_.setBounds(area.removeFromTop(22));
    area.removeFromTop(3);
    {
        auto row = area.removeFromTop(22);
        oscHostLabel_.setBounds(row.removeFromLeft(34));
        oscHostEditor_.setBounds(row);
        area.removeFromTop(3);
    }
    {
        auto row = area.removeFromTop(22);
        oscPortLabel_.setBounds(row.removeFromLeft(34));
        oscPortSlider_.setBounds(row);
    }
}

void OutputPanel::loadShape(Shape* shape)
{
    currentShape_ = shape;
    if (!shape) return;

    loading_ = true;

    auto getPBool = [&](const juce::String& key, bool def) -> bool {
        if (auto* obj = shape->behaviorParams.getDynamicObject())
            if (obj->hasProperty(key))
                return (bool)obj->getProperty(key);
        return def;
    };
    auto getP = [&](const juce::String& key, int def) -> int {
        if (auto* obj = shape->behaviorParams.getDynamicObject())
            if (obj->hasProperty(key))
                return (int)obj->getProperty(key);
        return def;
    };

    cvEnableToggle_.setToggleState(getPBool("cv_enabled", false), juce::dontSendNotification);
    cvChannelSlider_.setValue(getP("cv_channel", 0), juce::dontSendNotification);

    loading_ = false;
    resized();
}

void OutputPanel::clearShape()
{
    currentShape_ = nullptr;
    resized();
}

void OutputPanel::setOscState(bool enabled, const std::string& host, int port)
{
    oscToggle_.setToggleState(enabled, juce::dontSendNotification);
    oscHostEditor_.setText(juce::String(host));
    oscPortSlider_.setValue(port, juce::dontSendNotification);
}

void OutputPanel::sliderValueChanged(juce::Slider* slider)
{
    if (loading_ || !currentShape_) return;
    writeCVToShape();
    notifyCV();
}

void OutputPanel::buttonClicked(juce::Button* button)
{
    if (loading_ || !currentShape_) return;

    if (button == &cvEnableToggle_)
        resized();

    writeCVToShape();
    notifyCV();
}

void OutputPanel::writeCVToShape()
{
    if (!currentShape_) return;

    // Update cv_enabled and cv_channel in existing behaviorParams
    auto* obj = currentShape_->behaviorParams.getDynamicObject();
    if (!obj) {
        obj = new juce::DynamicObject();
        currentShape_->behaviorParams = juce::var(obj);
    }
    obj->setProperty("cv_enabled", cvEnableToggle_.getToggleState());
    obj->setProperty("cv_channel", (int)cvChannelSlider_.getValue());
}

void OutputPanel::notifyCV()
{
    if (!currentShape_) return;
    for (auto* l : listeners_)
        l->cvParamsChanged(currentShape_->id);
}

void OutputPanel::notifyOSC()
{
    for (auto* l : listeners_)
        l->oscSettingsChanged(oscToggle_.getToggleState(),
                              oscHostEditor_.getText().toStdString(),
                              (int)oscPortSlider_.getValue());
}

} // namespace erae
