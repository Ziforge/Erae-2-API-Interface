#include "MidiPanel.h"
#include "../MIDI/VelocityCurve.h"
#include "../MIDI/ScaleQuantizer.h"

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

MidiPanel::MidiPanel(Layout& layout)
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

    // MIDI Learn button
    midiLearnBtn_.setTooltip("Capture note/CC from incoming MIDI");
    midiLearnBtn_.onClick = [this] {
        if (midiLearnBtn_.getButtonText() == "Cancel") {
            midiLearnBtn_.setButtonText("Learn");
            midiLearnBtn_.setColour(juce::TextButton::buttonColourId, Theme::Colors::ButtonBg);
            for (auto* l : listeners_) l->midiLearnCancelled();
        } else if (currentShape_) {
            midiLearnBtn_.setButtonText("Cancel");
            midiLearnBtn_.setColour(juce::TextButton::buttonColourId, Theme::Colors::Accent);
            for (auto* l : listeners_) l->midiLearnRequested(currentShape_->id);
        }
    };
    addAndMakeVisible(midiLearnBtn_);

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

    // Hi-Res 14-bit toggle
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

    // Velocity curve
    styleLabel(velCurveLabel_);
    velocityCurveBox_.addItem("Linear",      1);
    velocityCurveBox_.addItem("Exponential", 2);
    velocityCurveBox_.addItem("Logarithmic", 3);
    velocityCurveBox_.addItem("S-Curve",     4);
    velocityCurveBox_.addListener(this);
    addAndMakeVisible(velCurveLabel_);
    addAndMakeVisible(velocityCurveBox_);

    // Pressure curve
    styleLabel(pressCurveLabel_);
    pressureCurveBox_.addItem("Linear",      1);
    pressureCurveBox_.addItem("Exponential", 2);
    pressureCurveBox_.addItem("Logarithmic", 3);
    pressureCurveBox_.addItem("S-Curve",     4);
    pressureCurveBox_.addListener(this);
    addAndMakeVisible(pressCurveLabel_);
    addAndMakeVisible(pressureCurveBox_);

    // Latch toggle (Trigger only)
    styleLabel(latchLabel_);
    latchToggle_.addListener(this);
    addAndMakeVisible(latchLabel_);
    addAndMakeVisible(latchToggle_);

    // Scale
    styleLabel(scaleLabel_);
    scaleBox_.addItem("Chromatic",        1);
    scaleBox_.addItem("Major",            2);
    scaleBox_.addItem("Natural Minor",    3);
    scaleBox_.addItem("Harmonic Minor",   4);
    scaleBox_.addItem("Pentatonic",       5);
    scaleBox_.addItem("Minor Pentatonic", 6);
    scaleBox_.addItem("Whole Tone",       7);
    scaleBox_.addItem("Blues",            8);
    scaleBox_.addItem("Dorian",           9);
    scaleBox_.addItem("Mixolydian",       10);
    scaleBox_.addListener(this);
    addAndMakeVisible(scaleLabel_);
    addAndMakeVisible(scaleBox_);

    // Root note (0-11)
    styleLabel(rootNoteLabel_);
    styleSlider(rootNoteSlider_, 0, 11, 0);
    rootNoteSlider_.textFromValueFunction = [](double v) {
        const char* names[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        int idx = juce::jlimit(0, 11, (int)v);
        return juce::String(names[idx]);
    };
    rootNoteSlider_.addListener(this);
    addAndMakeVisible(rootNoteLabel_);
    addAndMakeVisible(rootNoteSlider_);

    // Pitch quantize toggle
    styleLabel(pitchQuantLabel_);
    pitchQuantizeToggle_.addListener(this);
    addAndMakeVisible(pitchQuantLabel_);
    addAndMakeVisible(pitchQuantizeToggle_);

    // Glide amount (0.0-1.0)
    styleLabel(glideLabel_);
    glideSlider_.setRange(0.0, 1.0, 0.01);
    glideSlider_.setValue(0.0, juce::dontSendNotification);
    glideSlider_.setSliderStyle(juce::Slider::LinearBar);
    glideSlider_.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 40, 20);
    glideSlider_.setColour(juce::Slider::trackColourId, Theme::Colors::Accent);
    glideSlider_.setColour(juce::Slider::textBoxTextColourId, Theme::Colors::Text);
    glideSlider_.addListener(this);
    addAndMakeVisible(glideLabel_);
    addAndMakeVisible(glideSlider_);

    // CC range sliders
    styleLabel(ccMinLabel_);
    styleSlider(ccMinSlider_, 0, 127, 0);
    ccMinSlider_.addListener(this);
    addAndMakeVisible(ccMinLabel_);
    addAndMakeVisible(ccMinSlider_);

    styleLabel(ccMaxLabel_);
    styleSlider(ccMaxSlider_, 0, 127, 127);
    ccMaxSlider_.addListener(this);
    addAndMakeVisible(ccMaxLabel_);
    addAndMakeVisible(ccMaxSlider_);

    styleLabel(ccXMinLabel_);
    styleSlider(ccXMinSlider_, 0, 127, 0);
    ccXMinSlider_.addListener(this);
    addAndMakeVisible(ccXMinLabel_);
    addAndMakeVisible(ccXMinSlider_);

    styleLabel(ccXMaxLabel_);
    styleSlider(ccXMaxSlider_, 0, 127, 127);
    ccXMaxSlider_.addListener(this);
    addAndMakeVisible(ccXMaxLabel_);
    addAndMakeVisible(ccXMaxSlider_);

    styleLabel(ccYMinLabel_);
    styleSlider(ccYMinSlider_, 0, 127, 0);
    ccYMinSlider_.addListener(this);
    addAndMakeVisible(ccYMinLabel_);
    addAndMakeVisible(ccYMinSlider_);

    styleLabel(ccYMaxLabel_);
    styleSlider(ccYMaxSlider_, 0, 127, 127);
    ccYMaxSlider_.addListener(this);
    addAndMakeVisible(ccYMaxLabel_);
    addAndMakeVisible(ccYMaxSlider_);

    updateVisibility();
}

void MidiPanel::paint(juce::Graphics& g)
{
    if (!currentShape_) return;

    g.setColour(Theme::Colors::Separator);
    int lineY1 = behaviorLabel_.getBottom() + 1;
    g.fillRect(0, lineY1, getWidth(), 1);
}

void MidiPanel::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(6);
    int rowH = 26;
    int labelW = 74;
    int gap = 5;

    behaviorLabel_.setBounds(area.removeFromTop(18));
    area.removeFromTop(3);
    behaviorBox_.setBounds(area.removeFromTop(rowH));
    area.removeFromTop(gap + 2);

    auto layoutRow = [&](juce::Label& label, juce::Component& control) {
        auto row = area.removeFromTop(rowH);
        label.setBounds(row.removeFromLeft(labelW));
        control.setBounds(row);
        area.removeFromTop(3);
    };

    layoutRow(noteLabel_, noteSlider_);
    layoutRow(channelLabel_, channelSlider_);
    midiLearnBtn_.setBounds(area.removeFromTop(rowH));
    area.removeFromTop(3);
    layoutRow(velocityLabel_, velocitySlider_);
    layoutRow(ccLabel_, ccSlider_);
    layoutRow(ccXLabel_, ccXSlider_);
    layoutRow(ccYLabel_, ccYSlider_);
    layoutRow(slideCCLabel_, slideCCSlider_);

    // Horizontal toggle row
    {
        auto row = area.removeFromTop(rowH);
        horizLabel_.setBounds(row.removeFromLeft(labelW));
        horizToggle_.setBounds(row.removeFromLeft(rowH));
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
    area.removeFromTop(gap);

    // Musical features
    layoutRow(velCurveLabel_, velocityCurveBox_);
    layoutRow(pressCurveLabel_, pressureCurveBox_);
    {
        auto row = area.removeFromTop(rowH);
        latchLabel_.setBounds(row.removeFromLeft(labelW));
        latchToggle_.setBounds(row.removeFromLeft(rowH));
        area.removeFromTop(3);
    }
    layoutRow(scaleLabel_, scaleBox_);
    layoutRow(rootNoteLabel_, rootNoteSlider_);
    {
        auto row = area.removeFromTop(rowH);
        pitchQuantLabel_.setBounds(row.removeFromLeft(labelW));
        pitchQuantizeToggle_.setBounds(row.removeFromLeft(rowH));
        area.removeFromTop(3);
    }
    layoutRow(glideLabel_, glideSlider_);

    // CC ranges
    layoutRow(ccMinLabel_, ccMinSlider_);
    layoutRow(ccMaxLabel_, ccMaxSlider_);
    layoutRow(ccXMinLabel_, ccXMinSlider_);
    layoutRow(ccXMaxLabel_, ccXMaxSlider_);
    layoutRow(ccYMinLabel_, ccYMinSlider_);
    layoutRow(ccYMaxLabel_, ccYMaxSlider_);
}

void MidiPanel::loadShape(Shape* shape)
{
    currentShape_ = shape;
    if (!shape) return;

    loading_ = true;

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
    auto getPFloat = [&](const juce::String& key, float def) -> float {
        if (auto* obj = shape->behaviorParams.getDynamicObject())
            if (obj->hasProperty(key))
                return (float)(double)obj->getProperty(key);
        return def;
    };
    auto getPString = [&](const juce::String& key, const std::string& def) -> std::string {
        if (auto* obj = shape->behaviorParams.getDynamicObject())
            if (obj->hasProperty(key))
                return obj->getProperty(key).toString().toStdString();
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
    double maxCC = hr ? 31.0 : 127.0;
    ccSlider_.setRange(0, maxCC, 1.0);
    ccXSlider_.setRange(0, maxCC, 1.0);
    ccYSlider_.setRange(0, maxCC, 1.0);

    // Musical features
    auto velCurveStr = getPString("velocity_curve", "linear");
    auto velCurve = curveFromString(velCurveStr);
    velocityCurveBox_.setSelectedId((int)velCurve + 1, juce::dontSendNotification);

    auto pressCurveStr = getPString("pressure_curve", "linear");
    auto pressCurve = curveFromString(pressCurveStr);
    pressureCurveBox_.setSelectedId((int)pressCurve + 1, juce::dontSendNotification);

    latchToggle_.setToggleState(getPBool("latch", false), juce::dontSendNotification);

    auto scaleStr = getPString("scale", "chromatic");
    auto scaleType = scaleFromString(scaleStr);
    int scaleId = 1;
    switch (scaleType) {
        case ScaleType::Chromatic:       scaleId = 1; break;
        case ScaleType::Major:           scaleId = 2; break;
        case ScaleType::NaturalMinor:    scaleId = 3; break;
        case ScaleType::HarmonicMinor:   scaleId = 4; break;
        case ScaleType::Pentatonic:      scaleId = 5; break;
        case ScaleType::MinorPentatonic: scaleId = 6; break;
        case ScaleType::WholeTone:       scaleId = 7; break;
        case ScaleType::Blues:           scaleId = 8; break;
        case ScaleType::Dorian:          scaleId = 9; break;
        case ScaleType::Mixolydian:      scaleId = 10; break;
    }
    scaleBox_.setSelectedId(scaleId, juce::dontSendNotification);
    rootNoteSlider_.setValue(getP("root_note", 0), juce::dontSendNotification);
    pitchQuantizeToggle_.setToggleState(getPBool("pitch_quantize", false), juce::dontSendNotification);
    glideSlider_.setValue(getPFloat("glide_amount", 0.0f), juce::dontSendNotification);

    // CC ranges
    ccMinSlider_.setValue(getP("cc_min", 0), juce::dontSendNotification);
    ccMaxSlider_.setValue(getP("cc_max", 127), juce::dontSendNotification);
    ccXMinSlider_.setValue(getP("cc_x_min", 0), juce::dontSendNotification);
    ccXMaxSlider_.setValue(getP("cc_x_max", 127), juce::dontSendNotification);
    ccYMinSlider_.setValue(getP("cc_y_min", 0), juce::dontSendNotification);
    ccYMaxSlider_.setValue(getP("cc_y_max", 127), juce::dontSendNotification);

    updateVisibility();
    loading_ = false;
}

void MidiPanel::clearShape()
{
    currentShape_ = nullptr;
}

void MidiPanel::comboBoxChanged(juce::ComboBox* box)
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

    updateVisibility();
    writeParamsToShape();
    notifyListeners();
}

void MidiPanel::sliderValueChanged(juce::Slider* slider)
{
    if (loading_ || !currentShape_) return;

    // Enforce CC min <= max
    if (slider == &ccMinSlider_ && ccMinSlider_.getValue() > ccMaxSlider_.getValue())
        ccMaxSlider_.setValue(ccMinSlider_.getValue(), juce::dontSendNotification);
    if (slider == &ccMaxSlider_ && ccMaxSlider_.getValue() < ccMinSlider_.getValue())
        ccMinSlider_.setValue(ccMaxSlider_.getValue(), juce::dontSendNotification);
    if (slider == &ccXMinSlider_ && ccXMinSlider_.getValue() > ccXMaxSlider_.getValue())
        ccXMaxSlider_.setValue(ccXMinSlider_.getValue(), juce::dontSendNotification);
    if (slider == &ccXMaxSlider_ && ccXMaxSlider_.getValue() < ccXMinSlider_.getValue())
        ccXMinSlider_.setValue(ccXMaxSlider_.getValue(), juce::dontSendNotification);
    if (slider == &ccYMinSlider_ && ccYMinSlider_.getValue() > ccYMaxSlider_.getValue())
        ccYMaxSlider_.setValue(ccYMinSlider_.getValue(), juce::dontSendNotification);
    if (slider == &ccYMaxSlider_ && ccYMaxSlider_.getValue() < ccYMinSlider_.getValue())
        ccYMinSlider_.setValue(ccYMaxSlider_.getValue(), juce::dontSendNotification);

    writeParamsToShape();
    notifyListeners();
}

void MidiPanel::buttonClicked(juce::Button* button)
{
    if (loading_ || !currentShape_) return;

    if (button == &highresToggle_) {
        double maxCC = highresToggle_.getToggleState() ? 31.0 : 127.0;
        ccSlider_.setRange(0, maxCC, 1.0);
        ccXSlider_.setRange(0, maxCC, 1.0);
        ccYSlider_.setRange(0, maxCC, 1.0);
    }

    if (button == &pitchQuantizeToggle_)
        updateVisibility();

    writeParamsToShape();
    notifyListeners();
}

void MidiPanel::updateVisibility()
{
    auto btype = currentShape_ ? behaviorFromString(currentShape_->behavior) : BehaviorType::Trigger;

    bool showNote     = (btype == BehaviorType::Trigger || btype == BehaviorType::Momentary || btype == BehaviorType::NotePad);
    bool showChannel  = (btype != BehaviorType::NotePad);
    bool showVelocity = (btype == BehaviorType::Trigger);
    bool showCC       = (btype == BehaviorType::Fader);
    bool showCCXY     = (btype == BehaviorType::XYController);
    bool showHoriz    = (btype == BehaviorType::Fader);
    bool showHighres  = (btype == BehaviorType::Fader || btype == BehaviorType::XYController);
    bool showSlideCC  = (btype == BehaviorType::NotePad);
    bool showMPEHint  = (btype == BehaviorType::NotePad);

    bool showVelCurve   = (btype == BehaviorType::Trigger || btype == BehaviorType::Momentary || btype == BehaviorType::NotePad);
    bool showPressCurve = (btype == BehaviorType::Momentary || btype == BehaviorType::NotePad);
    bool showLatch      = (btype == BehaviorType::Trigger);
    bool showScale      = (btype == BehaviorType::NotePad);
    bool showRootNote   = showScale && (scaleBox_.getSelectedId() > 1);
    bool showPitchQuant = (btype == BehaviorType::NotePad);
    bool showGlide      = showPitchQuant && pitchQuantizeToggle_.getToggleState();

    bool showCCRange    = (btype == BehaviorType::Fader);
    bool showCCXYRange  = (btype == BehaviorType::XYController);

    noteLabel_.setVisible(showNote);
    noteSlider_.setVisible(showNote);
    channelLabel_.setVisible(showChannel);
    channelSlider_.setVisible(showChannel);
    midiLearnBtn_.setVisible(showNote || showCC || showCCXY);
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

    velCurveLabel_.setVisible(showVelCurve);
    velocityCurveBox_.setVisible(showVelCurve);
    pressCurveLabel_.setVisible(showPressCurve);
    pressureCurveBox_.setVisible(showPressCurve);
    latchLabel_.setVisible(showLatch);
    latchToggle_.setVisible(showLatch);
    scaleLabel_.setVisible(showScale);
    scaleBox_.setVisible(showScale);
    rootNoteLabel_.setVisible(showRootNote);
    rootNoteSlider_.setVisible(showRootNote);
    pitchQuantLabel_.setVisible(showPitchQuant);
    pitchQuantizeToggle_.setVisible(showPitchQuant);
    glideLabel_.setVisible(showGlide);
    glideSlider_.setVisible(showGlide);

    ccMinLabel_.setVisible(showCCRange);
    ccMinSlider_.setVisible(showCCRange);
    ccMaxLabel_.setVisible(showCCRange);
    ccMaxSlider_.setVisible(showCCRange);
    ccXMinLabel_.setVisible(showCCXYRange);
    ccXMinSlider_.setVisible(showCCXYRange);
    ccXMaxLabel_.setVisible(showCCXYRange);
    ccXMaxSlider_.setVisible(showCCXYRange);
    ccYMinLabel_.setVisible(showCCXYRange);
    ccYMinSlider_.setVisible(showCCXYRange);
    ccYMaxLabel_.setVisible(showCCXYRange);
    ccYMaxSlider_.setVisible(showCCXYRange);
}

void MidiPanel::writeParamsToShape()
{
    if (!currentShape_) return;

    auto btype = behaviorFromString(currentShape_->behavior);
    auto* obj = new juce::DynamicObject();

    auto curveIdToString = [](int id) -> juce::String {
        switch (id) {
            case 2: return "exponential";
            case 3: return "logarithmic";
            case 4: return "s_curve";
            default: return "linear";
        }
    };

    switch (btype) {
        case BehaviorType::Trigger:
            obj->setProperty("note", (int)noteSlider_.getValue());
            obj->setProperty("channel", (int)channelSlider_.getValue());
            obj->setProperty("velocity", (int)velocitySlider_.getValue());
            obj->setProperty("velocity_curve", curveIdToString(velocityCurveBox_.getSelectedId()));
            obj->setProperty("latch", latchToggle_.getToggleState());
            break;
        case BehaviorType::Momentary:
            obj->setProperty("note", (int)noteSlider_.getValue());
            obj->setProperty("channel", (int)channelSlider_.getValue());
            obj->setProperty("velocity_curve", curveIdToString(velocityCurveBox_.getSelectedId()));
            obj->setProperty("pressure_curve", curveIdToString(pressureCurveBox_.getSelectedId()));
            break;
        case BehaviorType::NotePad: {
            obj->setProperty("note", (int)noteSlider_.getValue());
            obj->setProperty("slide_cc", (int)slideCCSlider_.getValue());
            obj->setProperty("velocity_curve", curveIdToString(velocityCurveBox_.getSelectedId()));
            obj->setProperty("pressure_curve", curveIdToString(pressureCurveBox_.getSelectedId()));
            auto scaleNames = std::vector<std::string>{
                "chromatic", "major", "natural_minor", "harmonic_minor",
                "pentatonic", "minor_pentatonic", "whole_tone", "blues",
                "dorian", "mixolydian"
            };
            int scaleIdx = scaleBox_.getSelectedId() - 1;
            if (scaleIdx >= 0 && scaleIdx < (int)scaleNames.size())
                obj->setProperty("scale", juce::String(scaleNames[scaleIdx]));
            obj->setProperty("root_note", (int)rootNoteSlider_.getValue());
            obj->setProperty("pitch_quantize", pitchQuantizeToggle_.getToggleState());
            obj->setProperty("glide_amount", glideSlider_.getValue());
            break;
        }
        case BehaviorType::XYController: {
            bool hr = highresToggle_.getToggleState();
            int maxCC = hr ? 31 : 127;
            obj->setProperty("cc_x", juce::jlimit(0, maxCC, (int)ccXSlider_.getValue()));
            obj->setProperty("cc_y", juce::jlimit(0, maxCC, (int)ccYSlider_.getValue()));
            obj->setProperty("channel", (int)channelSlider_.getValue());
            obj->setProperty("highres", hr);
            obj->setProperty("cc_x_min", (int)ccXMinSlider_.getValue());
            obj->setProperty("cc_x_max", (int)ccXMaxSlider_.getValue());
            obj->setProperty("cc_y_min", (int)ccYMinSlider_.getValue());
            obj->setProperty("cc_y_max", (int)ccYMaxSlider_.getValue());
            break;
        }
        case BehaviorType::Fader: {
            bool hr = highresToggle_.getToggleState();
            int maxCC = hr ? 31 : 127;
            obj->setProperty("cc", juce::jlimit(0, maxCC, (int)ccSlider_.getValue()));
            obj->setProperty("channel", (int)channelSlider_.getValue());
            obj->setProperty("horizontal", horizToggle_.getToggleState());
            obj->setProperty("highres", hr);
            obj->setProperty("cc_min", (int)ccMinSlider_.getValue());
            obj->setProperty("cc_max", (int)ccMaxSlider_.getValue());
            break;
        }
    }

    // Preserve CV params from existing behaviorParams
    if (auto* existing = currentShape_->behaviorParams.getDynamicObject()) {
        if (existing->hasProperty("cv_enabled"))
            obj->setProperty("cv_enabled", existing->getProperty("cv_enabled"));
        if (existing->hasProperty("cv_channel"))
            obj->setProperty("cv_channel", existing->getProperty("cv_channel"));
    }

    currentShape_->behaviorParams = juce::var(obj);
}

void MidiPanel::notifyListeners()
{
    if (!currentShape_) return;
    for (auto* l : listeners_)
        l->behaviorChanged(currentShape_->id);
}

void MidiPanel::setMidiLearnActive(bool active)
{
    if (active) {
        midiLearnBtn_.setButtonText("Cancel");
        midiLearnBtn_.setColour(juce::TextButton::buttonColourId, Theme::Colors::Accent);
    } else {
        midiLearnBtn_.setButtonText("Learn");
        midiLearnBtn_.setColour(juce::TextButton::buttonColourId, Theme::Colors::ButtonBg);
    }
}

void MidiPanel::applyMidiLearnResult(int note, int cc, int channel, bool isCC)
{
    if (!currentShape_) return;

    loading_ = true;
    channelSlider_.setValue(channel, juce::dontSendNotification);

    auto btype = behaviorFromString(currentShape_->behavior);
    if (isCC) {
        if (btype == BehaviorType::Fader) {
            ccSlider_.setValue(cc, juce::dontSendNotification);
        } else if (btype == BehaviorType::XYController) {
            ccXSlider_.setValue(cc, juce::dontSendNotification);
        }
    } else {
        noteSlider_.setValue(note, juce::dontSendNotification);
    }
    loading_ = false;

    setMidiLearnActive(false);
    writeParamsToShape();
    notifyListeners();
}

} // namespace erae
