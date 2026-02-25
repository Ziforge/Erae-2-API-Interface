#include "PropertyPanel.h"

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

PropertyPanel::PropertyPanel(Layout& layout)
    : layout_(layout)
{
    styleLabel(behaviorLabel_, true);
    addAndMakeVisible(behaviorLabel_);

    behaviorBox_.addItem("Trigger",       1);
    behaviorBox_.addItem("Momentary",     2);
    behaviorBox_.addItem("NotePad (MPE)", 3);
    behaviorBox_.addItem("XY Controller", 4);
    behaviorBox_.addItem("Fader",         5);
    behaviorBox_.addListener(this);
    addAndMakeVisible(behaviorBox_);

    // Note slider (0-127)
    styleLabel(noteLabel_);
    styleSlider(noteSlider_, 0, 127, 60);
    noteSlider_.addListener(this);
    addAndMakeVisible(noteLabel_);
    addAndMakeVisible(noteSlider_);

    // Channel slider (0-15)
    styleLabel(channelLabel_);
    styleSlider(channelSlider_, 0, 15, 0);
    channelSlider_.addListener(this);
    addAndMakeVisible(channelLabel_);
    addAndMakeVisible(channelSlider_);

    // Velocity slider (-1 to 127, -1 = auto)
    styleLabel(velocityLabel_);
    styleSlider(velocitySlider_, -1, 127, -1);
    velocitySlider_.setTextValueSuffix("");
    velocitySlider_.textFromValueFunction = [](double v) {
        return (int)v < 0 ? juce::String("Auto") : juce::String((int)v);
    };
    velocitySlider_.addListener(this);
    addAndMakeVisible(velocityLabel_);
    addAndMakeVisible(velocitySlider_);

    // CC slider (0-127)
    styleLabel(ccLabel_);
    styleSlider(ccSlider_, 0, 127, 1);
    ccSlider_.addListener(this);
    addAndMakeVisible(ccLabel_);
    addAndMakeVisible(ccSlider_);

    // CC X slider (0-127)
    styleLabel(ccXLabel_);
    styleSlider(ccXSlider_, 0, 127, 1);
    ccXSlider_.addListener(this);
    addAndMakeVisible(ccXLabel_);
    addAndMakeVisible(ccXSlider_);

    // CC Y slider (0-127)
    styleLabel(ccYLabel_);
    styleSlider(ccYSlider_, 0, 127, 2);
    ccYSlider_.addListener(this);
    addAndMakeVisible(ccYLabel_);
    addAndMakeVisible(ccYSlider_);

    // Horizontal toggle
    styleLabel(horizLabel_);
    horizToggle_.addListener(this);
    addAndMakeVisible(horizLabel_);
    addAndMakeVisible(horizToggle_);

    // Hi-Res 14-bit toggle (for Fader and XY Controller)
    styleLabel(highresLabel_);
    highresToggle_.addListener(this);
    addAndMakeVisible(highresLabel_);
    addAndMakeVisible(highresToggle_);

    // Slide CC slider (0-127, default 74 for MPE Y-axis)
    styleLabel(slideCCLabel_);
    styleSlider(slideCCSlider_, 0, 127, 74);
    slideCCSlider_.addListener(this);
    addAndMakeVisible(slideCCLabel_);
    addAndMakeVisible(slideCCSlider_);

    // MPE hint
    mpeHint_.setFont(juce::Font(Theme::FontSmall, juce::Font::italic));
    mpeHint_.setColour(juce::Label::textColourId, Theme::Colors::TextDim);
    addAndMakeVisible(mpeHint_);

    // Visual style
    styleLabel(visualLabel_, true);
    addAndMakeVisible(visualLabel_);

    visualBox_.addItem("Static",        1);
    visualBox_.addItem("Fill Bar",      2);
    visualBox_.addItem("Position Dot",  3);
    visualBox_.addItem("Radial Arc",    4);
    visualBox_.addItem("Pressure Glow", 5);
    visualBox_.addListener(this);
    addAndMakeVisible(visualBox_);

    // Fill horizontal toggle (for FillBar visual)
    styleLabel(fillHorizLabel_);
    fillHorizToggle_.addListener(this);
    addAndMakeVisible(fillHorizLabel_);
    addAndMakeVisible(fillHorizToggle_);

    updateVisibility();
}

void PropertyPanel::paint(juce::Graphics& g)
{
    if (!currentShape_) return;

    // Top separator
    g.setColour(Theme::Colors::Separator);
    g.fillRect(0, 0, getWidth(), 1);

    // Line under BEHAVIOR header
    int lineY1 = behaviorLabel_.getBottom() + 1;
    g.setColour(Theme::Colors::Separator);
    g.fillRect(0, lineY1, getWidth(), 1);

    // Line under VISUAL header (if visible)
    if (visualLabel_.isVisible()) {
        int lineY2 = visualLabel_.getBottom() + 1;
        g.fillRect(0, lineY2, getWidth(), 1);
    }
}

void PropertyPanel::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(6); // breathing room after separator
    int rowH = 26;
    int labelW = 74;
    int gap = 5;

    // Behavior header + combobox
    behaviorLabel_.setBounds(area.removeFromTop(18));
    area.removeFromTop(3);
    behaviorBox_.setBounds(area.removeFromTop(rowH));
    area.removeFromTop(gap + 2);

    // Parameter rows — each is label + slider side by side
    auto layoutRow = [&](juce::Label& label, juce::Component& control) {
        auto row = area.removeFromTop(rowH);
        label.setBounds(row.removeFromLeft(labelW));
        control.setBounds(row);
        area.removeFromTop(3);
    };

    layoutRow(noteLabel_, noteSlider_);
    layoutRow(channelLabel_, channelSlider_);
    layoutRow(velocityLabel_, velocitySlider_);
    layoutRow(ccLabel_, ccSlider_);
    layoutRow(ccXLabel_, ccXSlider_);
    layoutRow(ccYLabel_, ccYSlider_);
    layoutRow(slideCCLabel_, slideCCSlider_);

    // Horizontal toggle row
    {
        auto row = area.removeFromTop(rowH);
        horizLabel_.setBounds(row.removeFromLeft(labelW));
        horizToggle_.setBounds(row.removeFromLeft(rowH)); // square toggle
        area.removeFromTop(3);
    }

    // Hi-Res 14-bit toggle row
    {
        auto row = area.removeFromTop(rowH);
        highresLabel_.setBounds(row.removeFromLeft(labelW));
        highresToggle_.setBounds(row.removeFromLeft(rowH));
        area.removeFromTop(3);
    }

    // MPE hint
    mpeHint_.setBounds(area.removeFromTop(16));
    area.removeFromTop(gap + 2);

    // Visual style section
    visualLabel_.setBounds(area.removeFromTop(18));
    area.removeFromTop(3);
    visualBox_.setBounds(area.removeFromTop(rowH));
    area.removeFromTop(gap);

    // Fill horizontal toggle
    {
        auto row = area.removeFromTop(rowH);
        fillHorizLabel_.setBounds(row.removeFromLeft(labelW));
        fillHorizToggle_.setBounds(row.removeFromLeft(rowH));
        area.removeFromTop(3);
    }
}

void PropertyPanel::loadShape(Shape* shape)
{
    currentShape_ = shape;
    if (!shape) {
        setVisible(false);
        return;
    }

    loading_ = true;
    setVisible(true);

    auto btype = behaviorFromString(shape->behavior);
    switch (btype) {
        case BehaviorType::Trigger:      behaviorBox_.setSelectedId(1, juce::dontSendNotification); break;
        case BehaviorType::Momentary:    behaviorBox_.setSelectedId(2, juce::dontSendNotification); break;
        case BehaviorType::NotePad:      behaviorBox_.setSelectedId(3, juce::dontSendNotification); break;
        case BehaviorType::XYController: behaviorBox_.setSelectedId(4, juce::dontSendNotification); break;
        case BehaviorType::Fader:        behaviorBox_.setSelectedId(5, juce::dontSendNotification); break;
    }

    auto getP = [&](const juce::String& key, int def) -> int {
        if (auto* obj = shape->behaviorParams.getDynamicObject())
            if (obj->hasProperty(key))
                return (int)obj->getProperty(key);
        return def;
    };
    auto getPBool = [&](const juce::String& key, bool def) -> bool {
        if (auto* obj = shape->behaviorParams.getDynamicObject())
            if (obj->hasProperty(key))
                return (bool)obj->getProperty(key);
        return def;
    };

    noteSlider_.setValue(getP("note", 60), juce::dontSendNotification);
    channelSlider_.setValue(getP("channel", 0), juce::dontSendNotification);
    velocitySlider_.setValue(getP("velocity", -1), juce::dontSendNotification);
    ccSlider_.setValue(getP("cc", 1), juce::dontSendNotification);
    ccXSlider_.setValue(getP("cc_x", 1), juce::dontSendNotification);
    ccYSlider_.setValue(getP("cc_y", 2), juce::dontSendNotification);
    slideCCSlider_.setValue(getP("slide_cc", 74), juce::dontSendNotification);
    horizToggle_.setToggleState(getPBool("horizontal", false), juce::dontSendNotification);
    bool hr = getPBool("highres", false);
    highresToggle_.setToggleState(hr, juce::dontSendNotification);
    // Set CC slider ranges based on hi-res state
    double maxCC = hr ? 31.0 : 127.0;
    ccSlider_.setRange(0, maxCC, 1.0);
    ccXSlider_.setRange(0, maxCC, 1.0);
    ccYSlider_.setRange(0, maxCC, 1.0);

    // Visual style
    auto vstyle = visualStyleFromString(shape->visualStyle);
    switch (vstyle) {
        case VisualStyle::Static:       visualBox_.setSelectedId(1, juce::dontSendNotification); break;
        case VisualStyle::FillBar:      visualBox_.setSelectedId(2, juce::dontSendNotification); break;
        case VisualStyle::PositionDot:  visualBox_.setSelectedId(3, juce::dontSendNotification); break;
        case VisualStyle::RadialArc:    visualBox_.setSelectedId(4, juce::dontSendNotification); break;
        case VisualStyle::PressureGlow: visualBox_.setSelectedId(5, juce::dontSendNotification); break;
    }

    // Visual params
    auto getVP = [&](const juce::String& key, bool def) -> bool {
        if (auto* obj = shape->visualParams.getDynamicObject())
            if (obj->hasProperty(key))
                return (bool)obj->getProperty(key);
        return def;
    };
    fillHorizToggle_.setToggleState(getVP("fill_horizontal", false), juce::dontSendNotification);

    updateVisibility();
    loading_ = false;
}

void PropertyPanel::clearShape()
{
    currentShape_ = nullptr;
    setVisible(false);
}

void PropertyPanel::comboBoxChanged(juce::ComboBox* box)
{
    if (loading_ || !currentShape_) return;

    if (box == &behaviorBox_) {
        int id = behaviorBox_.getSelectedId();
        BehaviorType btype;
        switch (id) {
            case 1: btype = BehaviorType::Trigger; break;
            case 2: btype = BehaviorType::Momentary; break;
            case 3: btype = BehaviorType::NotePad; break;
            case 4: btype = BehaviorType::XYController; break;
            case 5: btype = BehaviorType::Fader; break;
            default: return;
        }
        currentShape_->behavior = behaviorToString(btype);

        // Auto-assign unique note/CC for the new behavior type
        if (btype == BehaviorType::Trigger || btype == BehaviorType::Momentary || btype == BehaviorType::NotePad) {
            int note = layout_.nextAvailableNote(60);
            noteSlider_.setValue(note, juce::dontSendNotification);
        }
        if (btype == BehaviorType::Fader) {
            int cc = layout_.nextAvailableCC(1);
            ccSlider_.setValue(cc, juce::dontSendNotification);
        }
        if (btype == BehaviorType::XYController) {
            int ccX = layout_.nextAvailableCC(1);
            ccXSlider_.setValue(ccX, juce::dontSendNotification);
            int ccY = layout_.nextAvailableCC(ccX + 1);
            ccYSlider_.setValue(ccY, juce::dontSendNotification);
        }
    }
    else if (box == &visualBox_) {
        int id = visualBox_.getSelectedId();
        VisualStyle vstyle;
        switch (id) {
            case 1: vstyle = VisualStyle::Static; break;
            case 2: vstyle = VisualStyle::FillBar; break;
            case 3: vstyle = VisualStyle::PositionDot; break;
            case 4: vstyle = VisualStyle::RadialArc; break;
            case 5: vstyle = VisualStyle::PressureGlow; break;
            default: return;
        }
        currentShape_->visualStyle = visualStyleToString(vstyle);
    }
    else {
        return;
    }

    updateVisibility();
    writeParamsToShape();
    notifyListeners();
}

void PropertyPanel::sliderValueChanged(juce::Slider*)
{
    if (loading_ || !currentShape_) return;
    writeParamsToShape();
    notifyListeners();
}

void PropertyPanel::buttonClicked(juce::Button* button)
{
    if (loading_ || !currentShape_) return;

    // When hi-res toggled, update CC slider ranges (0-31 for 14-bit, 0-127 for 7-bit)
    if (button == &highresToggle_) {
        double maxCC = highresToggle_.getToggleState() ? 31.0 : 127.0;
        ccSlider_.setRange(0, maxCC, 1.0);
        ccXSlider_.setRange(0, maxCC, 1.0);
        ccYSlider_.setRange(0, maxCC, 1.0);
    }

    writeParamsToShape();
    notifyListeners();
}

void PropertyPanel::updateVisibility()
{
    auto btype = currentShape_ ? behaviorFromString(currentShape_->behavior) : BehaviorType::Trigger;

    bool showNote     = (btype == BehaviorType::Trigger || btype == BehaviorType::Momentary || btype == BehaviorType::NotePad);
    bool showChannel  = (btype != BehaviorType::NotePad); // MPE allocates channels dynamically
    bool showVelocity = (btype == BehaviorType::Trigger);
    bool showCC       = (btype == BehaviorType::Fader);
    bool showCCXY     = (btype == BehaviorType::XYController);
    bool showHoriz    = (btype == BehaviorType::Fader);
    bool showHighres  = (btype == BehaviorType::Fader || btype == BehaviorType::XYController);
    bool showSlideCC  = (btype == BehaviorType::NotePad);
    bool showMPEHint  = (btype == BehaviorType::NotePad);

    noteLabel_.setVisible(showNote);
    noteSlider_.setVisible(showNote);
    channelLabel_.setVisible(showChannel);
    channelSlider_.setVisible(showChannel);
    velocityLabel_.setVisible(showVelocity);
    velocitySlider_.setVisible(showVelocity);
    ccLabel_.setVisible(showCC);
    ccSlider_.setVisible(showCC);
    ccXLabel_.setVisible(showCCXY);
    ccXSlider_.setVisible(showCCXY);
    ccYLabel_.setVisible(showCCXY);
    ccYSlider_.setVisible(showCCXY);
    horizLabel_.setVisible(showHoriz);
    horizToggle_.setVisible(showHoriz);
    highresLabel_.setVisible(showHighres);
    highresToggle_.setVisible(showHighres);
    slideCCLabel_.setVisible(showSlideCC);
    slideCCSlider_.setVisible(showSlideCC);
    mpeHint_.setVisible(showMPEHint);

    // Visual style controls — always visible when a shape is selected
    bool hasShape = (currentShape_ != nullptr);
    visualLabel_.setVisible(hasShape);
    visualBox_.setVisible(hasShape);

    auto vstyle = currentShape_ ? visualStyleFromString(currentShape_->visualStyle) : VisualStyle::Static;
    bool showFillHoriz = hasShape && (vstyle == VisualStyle::FillBar);
    fillHorizLabel_.setVisible(showFillHoriz);
    fillHorizToggle_.setVisible(showFillHoriz);
}

void PropertyPanel::writeParamsToShape()
{
    if (!currentShape_) return;

    auto btype = behaviorFromString(currentShape_->behavior);
    auto* obj = new juce::DynamicObject();

    switch (btype) {
        case BehaviorType::Trigger:
            obj->setProperty("note", (int)noteSlider_.getValue());
            obj->setProperty("channel", (int)channelSlider_.getValue());
            obj->setProperty("velocity", (int)velocitySlider_.getValue());
            break;
        case BehaviorType::Momentary:
            obj->setProperty("note", (int)noteSlider_.getValue());
            obj->setProperty("channel", (int)channelSlider_.getValue());
            break;
        case BehaviorType::NotePad:
            obj->setProperty("note", (int)noteSlider_.getValue());
            obj->setProperty("slide_cc", (int)slideCCSlider_.getValue());
            // No channel — MPE allocates dynamically via MPEAllocator
            break;
        case BehaviorType::XYController: {
            bool hr = highresToggle_.getToggleState();
            int maxCC = hr ? 31 : 127;
            obj->setProperty("cc_x", juce::jlimit(0, maxCC, (int)ccXSlider_.getValue()));
            obj->setProperty("cc_y", juce::jlimit(0, maxCC, (int)ccYSlider_.getValue()));
            obj->setProperty("channel", (int)channelSlider_.getValue());
            obj->setProperty("highres", hr);
            break;
        }
        case BehaviorType::Fader: {
            bool hr = highresToggle_.getToggleState();
            int maxCC = hr ? 31 : 127;
            obj->setProperty("cc", juce::jlimit(0, maxCC, (int)ccSlider_.getValue()));
            obj->setProperty("channel", (int)channelSlider_.getValue());
            obj->setProperty("horizontal", horizToggle_.getToggleState());
            obj->setProperty("highres", hr);
            break;
        }
    }

    currentShape_->behaviorParams = juce::var(obj);

    // Write visual params
    auto vstyle = visualStyleFromString(currentShape_->visualStyle);
    auto* vobj = new juce::DynamicObject();
    if (vstyle == VisualStyle::FillBar)
        vobj->setProperty("fill_horizontal", fillHorizToggle_.getToggleState());
    currentShape_->visualParams = juce::var(vobj);
}

void PropertyPanel::notifyListeners()
{
    if (!currentShape_) return;
    for (auto* l : listeners_)
        l->behaviorChanged(currentShape_->id);
}

} // namespace erae
