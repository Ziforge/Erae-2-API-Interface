#include "EffectPanel.h"

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

static void styleSlider(juce::Slider& slider, double min, double max, double def, double step = 1.0)
{
    slider.setRange(min, max, step);
    slider.setValue(def, juce::dontSendNotification);
    slider.setSliderStyle(juce::Slider::LinearBar);
    slider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 40, 20);
    slider.setColour(juce::Slider::trackColourId, Theme::Colors::Accent);
    slider.setColour(juce::Slider::textBoxTextColourId, Theme::Colors::Text);
}

EffectPanel::EffectPanel()
{
    // Effect type header + dropdown
    styleLabel(effectLabel_, true);
    addAndMakeVisible(effectLabel_);

    effectBox_.addItem("None",      1);
    effectBox_.addItem("Trail",     2);
    effectBox_.addItem("Ripple",    3);
    effectBox_.addItem("Particles", 4);
    effectBox_.addItem("Pulse",     5);
    effectBox_.addItem("Breathe",   6);
    effectBox_.addItem("Spin",      7);
    effectBox_.addItem("Orbit",     8);
    effectBox_.addItem("Boundary",  9);
    effectBox_.addItem("String",           10);
    effectBox_.addItem("Membrane",         11);
    effectBox_.addItem("Fluid",            12);
    effectBox_.addItem("Spring Lattice",   13);
    effectBox_.addItem("Pendulum",         14);
    effectBox_.addItem("Collision",        15);
    effectBox_.addItem("Tombolo",          16);
    effectBox_.addItem("Gravity Well",     17);
    effectBox_.addItem("Elastic Band",     18);
    effectBox_.addItem("Bow",              19);
    effectBox_.addItem("Wave Interference",20);
    effectBox_.addListener(this);
    addAndMakeVisible(effectBox_);

    // Parameters
    styleLabel(paramsLabel_, true);
    addAndMakeVisible(paramsLabel_);

    styleLabel(speedLabel_);
    styleSlider(speedSlider_, 0.1, 5.0, 1.0, 0.1);
    speedSlider_.addListener(this);
    addAndMakeVisible(speedLabel_);
    addAndMakeVisible(speedSlider_);

    styleLabel(intensityLabel_);
    styleSlider(intensitySlider_, 0.0, 1.0, 0.8, 0.05);
    intensitySlider_.addListener(this);
    addAndMakeVisible(intensityLabel_);
    addAndMakeVisible(intensitySlider_);

    styleLabel(decayLabel_);
    styleSlider(decaySlider_, 0.1, 2.0, 0.5, 0.1);
    decaySlider_.addListener(this);
    addAndMakeVisible(decayLabel_);
    addAndMakeVisible(decaySlider_);

    styleLabel(motionLabel_);
    motionToggle_.addListener(this);
    addAndMakeVisible(motionLabel_);
    addAndMakeVisible(motionToggle_);

    // Modulation
    styleLabel(modLabel_, true);
    addAndMakeVisible(modLabel_);

    styleLabel(targetLabel_);
    targetBox_.addItem("None",       1);
    targetBox_.addItem("MIDI CC",    2);
    targetBox_.addItem("Pitch Bend", 3);
    targetBox_.addItem("Pressure",   4);
    targetBox_.addItem("CV",         5);
    targetBox_.addItem("OSC",        6);
    targetBox_.addItem("MPE XYZ",    7);
    targetBox_.addListener(this);
    addAndMakeVisible(targetLabel_);
    addAndMakeVisible(targetBox_);

    styleLabel(ccLabel_);
    styleSlider(ccSlider_, 0, 127, 74);
    ccSlider_.addListener(this);
    addAndMakeVisible(ccLabel_);
    addAndMakeVisible(ccSlider_);

    styleLabel(channelLabel_);
    styleSlider(channelSlider_, 0, 15, 0);
    channelSlider_.addListener(this);
    addAndMakeVisible(channelLabel_);
    addAndMakeVisible(channelSlider_);

    styleLabel(cvChLabel_);
    styleSlider(cvChSlider_, 0, 31, 0);
    cvChSlider_.addListener(this);
    addAndMakeVisible(cvChLabel_);
    addAndMakeVisible(cvChSlider_);

    styleLabel(mpeChLabel_);
    styleSlider(mpeChSlider_, 1, 15, 1);
    mpeChSlider_.addListener(this);
    addAndMakeVisible(mpeChLabel_);
    addAndMakeVisible(mpeChSlider_);

    // No shape label
    noShapeLabel_.setFont(juce::Font(Theme::FontBase));
    noShapeLabel_.setColour(juce::Label::textColourId, Theme::Colors::TextDim);
    noShapeLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(noShapeLabel_);

    updateVisibility();
}

void EffectPanel::paint(juce::Graphics&) {}

void EffectPanel::resized()
{
    auto area = getLocalBounds().reduced(Theme::SpaceMD);
    int rowH = 26;
    int labelW = 74;

    if (!currentShape_) {
        noShapeLabel_.setBounds(area);
        return;
    }

    effectLabel_.setBounds(area.removeFromTop(18));
    area.removeFromTop(3);
    effectBox_.setBounds(area.removeFromTop(rowH));
    area.removeFromTop(Theme::SpaceLG);

    bool hasEffect = effectBox_.getSelectedId() > 1;

    if (hasEffect) {
        // Parameters section
        paramsLabel_.setBounds(area.removeFromTop(18));
        area.removeFromTop(3);

        {
            auto row = area.removeFromTop(rowH);
            speedLabel_.setBounds(row.removeFromLeft(labelW));
            speedSlider_.setBounds(row);
        }
        area.removeFromTop(3);
        {
            auto row = area.removeFromTop(rowH);
            intensityLabel_.setBounds(row.removeFromLeft(labelW));
            intensitySlider_.setBounds(row);
        }
        area.removeFromTop(3);
        {
            auto row = area.removeFromTop(rowH);
            decayLabel_.setBounds(row.removeFromLeft(labelW));
            decaySlider_.setBounds(row);
        }
        area.removeFromTop(3);
        {
            auto row = area.removeFromTop(rowH);
            motionLabel_.setBounds(row.removeFromLeft(labelW));
            motionToggle_.setBounds(row.removeFromLeft(rowH));
        }
        area.removeFromTop(Theme::SpaceLG);

        // Modulation section
        modLabel_.setBounds(area.removeFromTop(18));
        area.removeFromTop(3);
        {
            auto row = area.removeFromTop(rowH);
            targetLabel_.setBounds(row.removeFromLeft(labelW));
            targetBox_.setBounds(row);
        }
        area.removeFromTop(3);

        int targetId = targetBox_.getSelectedId();
        bool showCC  = (targetId == 2 || targetId == 6); // MidiCC or OSC
        bool showCh  = (targetId >= 2 && targetId <= 4); // CC, PB, Pressure
        bool showCV  = (targetId == 5 || targetId == 7); // CV or MPE
        bool showMPE = (targetId == 7); // MPE XYZ

        if (showCC) {
            auto row = area.removeFromTop(rowH);
            ccLabel_.setBounds(row.removeFromLeft(labelW));
            ccSlider_.setBounds(row);
            area.removeFromTop(3);
        }
        if (showCh) {
            auto row = area.removeFromTop(rowH);
            channelLabel_.setBounds(row.removeFromLeft(labelW));
            channelSlider_.setBounds(row);
            area.removeFromTop(3);
        }
        if (showCV) {
            auto row = area.removeFromTop(rowH);
            cvChLabel_.setBounds(row.removeFromLeft(labelW));
            cvChSlider_.setBounds(row);
            area.removeFromTop(3);
        }
        if (showMPE) {
            auto row = area.removeFromTop(rowH);
            mpeChLabel_.setBounds(row.removeFromLeft(labelW));
            mpeChSlider_.setBounds(row);
            area.removeFromTop(3);
        }
    }
}

void EffectPanel::loadShape(Shape* shape)
{
    currentShape_ = shape;
    if (!shape) { clearShape(); return; }

    loading_ = true;

    // Parse existing effect params
    EffectParams p;
    auto* obj = shape->behaviorParams.getDynamicObject();
    if (obj && obj->hasProperty("effect")) {
        auto effectVar = obj->getProperty("effect");
        auto* eff = effectVar.getDynamicObject();
        if (eff) {
            auto getStr = [&](const char* key, const char* def) -> std::string {
                if (eff->hasProperty(key))
                    return eff->getProperty(key).toString().toStdString();
                return def;
            };
            auto getF = [&](const char* key, float def) -> float {
                if (eff->hasProperty(key))
                    return (float)(double)eff->getProperty(key);
                return def;
            };
            auto getI = [&](const char* key, int def) -> int {
                if (eff->hasProperty(key))
                    return (int)eff->getProperty(key);
                return def;
            };
            auto getB = [&](const char* key, bool def) -> bool {
                if (eff->hasProperty(key))
                    return (bool)eff->getProperty(key);
                return def;
            };

            p.type           = effectFromString(getStr("type", "none"));
            p.speed          = getF("speed", 1.0f);
            p.intensity      = getF("intensity", 0.8f);
            p.decay          = getF("decay", 0.5f);
            p.motionReactive = getB("motion_reactive", false);
            p.modTarget      = modTargetFromString(getStr("mod_target", "none"));
            p.modCC          = getI("mod_cc", 74);
            p.modChannel     = getI("mod_channel", 0);
            p.modCVCh        = getI("mod_cv_ch", 0);
            p.mpeChannel     = getI("mpe_channel", 1);
        }
    }

    // Set UI values
    switch (p.type) {
        case TouchEffectType::None:      effectBox_.setSelectedId(1, juce::dontSendNotification); break;
        case TouchEffectType::Trail:     effectBox_.setSelectedId(2, juce::dontSendNotification); break;
        case TouchEffectType::Ripple:    effectBox_.setSelectedId(3, juce::dontSendNotification); break;
        case TouchEffectType::Particles: effectBox_.setSelectedId(4, juce::dontSendNotification); break;
        case TouchEffectType::Pulse:     effectBox_.setSelectedId(5, juce::dontSendNotification); break;
        case TouchEffectType::Breathe:   effectBox_.setSelectedId(6, juce::dontSendNotification); break;
        case TouchEffectType::Spin:      effectBox_.setSelectedId(7, juce::dontSendNotification); break;
        case TouchEffectType::Orbit:     effectBox_.setSelectedId(8, juce::dontSendNotification); break;
        case TouchEffectType::Boundary:         effectBox_.setSelectedId(9,  juce::dontSendNotification); break;
        case TouchEffectType::String:           effectBox_.setSelectedId(10, juce::dontSendNotification); break;
        case TouchEffectType::Membrane:         effectBox_.setSelectedId(11, juce::dontSendNotification); break;
        case TouchEffectType::Fluid:            effectBox_.setSelectedId(12, juce::dontSendNotification); break;
        case TouchEffectType::SpringLattice:    effectBox_.setSelectedId(13, juce::dontSendNotification); break;
        case TouchEffectType::Pendulum:         effectBox_.setSelectedId(14, juce::dontSendNotification); break;
        case TouchEffectType::Collision:        effectBox_.setSelectedId(15, juce::dontSendNotification); break;
        case TouchEffectType::Tombolo:          effectBox_.setSelectedId(16, juce::dontSendNotification); break;
        case TouchEffectType::GravityWell:      effectBox_.setSelectedId(17, juce::dontSendNotification); break;
        case TouchEffectType::ElasticBand:      effectBox_.setSelectedId(18, juce::dontSendNotification); break;
        case TouchEffectType::Bow:              effectBox_.setSelectedId(19, juce::dontSendNotification); break;
        case TouchEffectType::WaveInterference: effectBox_.setSelectedId(20, juce::dontSendNotification); break;
    }

    speedSlider_.setValue(p.speed, juce::dontSendNotification);
    intensitySlider_.setValue(p.intensity, juce::dontSendNotification);
    decaySlider_.setValue(p.decay, juce::dontSendNotification);
    motionToggle_.setToggleState(p.motionReactive, juce::dontSendNotification);

    switch (p.modTarget) {
        case ModTarget::None:      targetBox_.setSelectedId(1, juce::dontSendNotification); break;
        case ModTarget::MidiCC:    targetBox_.setSelectedId(2, juce::dontSendNotification); break;
        case ModTarget::PitchBend: targetBox_.setSelectedId(3, juce::dontSendNotification); break;
        case ModTarget::Pressure:  targetBox_.setSelectedId(4, juce::dontSendNotification); break;
        case ModTarget::CV:        targetBox_.setSelectedId(5, juce::dontSendNotification); break;
        case ModTarget::OSC:       targetBox_.setSelectedId(6, juce::dontSendNotification); break;
        case ModTarget::MPE:       targetBox_.setSelectedId(7, juce::dontSendNotification); break;
    }

    ccSlider_.setValue(p.modCC, juce::dontSendNotification);
    channelSlider_.setValue(p.modChannel, juce::dontSendNotification);
    cvChSlider_.setValue(p.modCVCh, juce::dontSendNotification);
    mpeChSlider_.setValue(p.mpeChannel, juce::dontSendNotification);

    loading_ = false;
    updateVisibility();
    resized();
}

void EffectPanel::clearShape()
{
    currentShape_ = nullptr;
    updateVisibility();
}

void EffectPanel::updateVisibility()
{
    bool hasShape = currentShape_ != nullptr;
    bool hasEffect = hasShape && effectBox_.getSelectedId() > 1;

    noShapeLabel_.setVisible(!hasShape);
    effectLabel_.setVisible(hasShape);
    effectBox_.setVisible(hasShape);

    paramsLabel_.setVisible(hasEffect);
    speedLabel_.setVisible(hasEffect);
    speedSlider_.setVisible(hasEffect);
    intensityLabel_.setVisible(hasEffect);
    intensitySlider_.setVisible(hasEffect);
    decayLabel_.setVisible(hasEffect);
    decaySlider_.setVisible(hasEffect);
    motionLabel_.setVisible(hasEffect);
    motionToggle_.setVisible(hasEffect);

    modLabel_.setVisible(hasEffect);
    targetLabel_.setVisible(hasEffect);
    targetBox_.setVisible(hasEffect);

    int targetId = hasEffect ? targetBox_.getSelectedId() : 0;
    bool showCC  = (targetId == 2 || targetId == 6);
    bool showCh  = (targetId >= 2 && targetId <= 4);
    bool showCV  = (targetId == 5 || targetId == 7); // CV or MPE (3 channels for X/Y/Z)
    bool showMPE = (targetId == 7);

    ccLabel_.setVisible(showCC);
    ccSlider_.setVisible(showCC);
    channelLabel_.setVisible(showCh);
    channelSlider_.setVisible(showCh);
    cvChLabel_.setVisible(showCV);
    cvChSlider_.setVisible(showCV);
    mpeChLabel_.setVisible(showMPE);
    mpeChSlider_.setVisible(showMPE);
}

void EffectPanel::writeParamsToShape()
{
    if (!currentShape_ || loading_) return;

    auto* obj = currentShape_->behaviorParams.getDynamicObject();
    if (!obj) {
        currentShape_->behaviorParams = juce::var(new juce::DynamicObject());
        obj = currentShape_->behaviorParams.getDynamicObject();
    }

    // Build effect sub-object
    auto* eff = new juce::DynamicObject();

    static const char* typeNames[] = {
        "none", "trail", "ripple", "particles", "pulse", "breathe", "spin", "orbit", "boundary",
        "string", "membrane", "fluid", "spring_lattice", "pendulum", "collision",
        "tombolo", "gravity_well", "elastic_band", "bow", "wave_interference"
    };
    int typeIdx = effectBox_.getSelectedId() - 1;
    eff->setProperty("type", typeIdx >= 0 && typeIdx < 20 ? typeNames[typeIdx] : "none");

    eff->setProperty("speed", speedSlider_.getValue());
    eff->setProperty("intensity", intensitySlider_.getValue());
    eff->setProperty("decay", decaySlider_.getValue());
    eff->setProperty("motion_reactive", motionToggle_.getToggleState());
    eff->setProperty("use_shape_color", true);

    static const char* targetNames[] = {"none", "midi_cc", "pitch_bend", "pressure", "cv", "osc", "mpe"};
    int targetIdx = targetBox_.getSelectedId() - 1;
    eff->setProperty("mod_target", targetIdx >= 0 && targetIdx < 7 ? targetNames[targetIdx] : "none");

    eff->setProperty("mod_cc", (int)ccSlider_.getValue());
    eff->setProperty("mod_channel", (int)channelSlider_.getValue());
    eff->setProperty("mod_cv_ch", (int)cvChSlider_.getValue());
    eff->setProperty("mpe_channel", (int)mpeChSlider_.getValue());

    obj->setProperty("effect", juce::var(eff));
}

void EffectPanel::notifyListeners()
{
    if (!currentShape_ || loading_) return;
    for (auto* l : listeners_)
        l->effectChanged(currentShape_->id);
}

void EffectPanel::comboBoxChanged(juce::ComboBox*)
{
    updateVisibility();
    resized();
    writeParamsToShape();
    notifyListeners();
}

void EffectPanel::sliderValueChanged(juce::Slider*)
{
    writeParamsToShape();
    notifyListeners();
}

void EffectPanel::buttonClicked(juce::Button*)
{
    writeParamsToShape();
    notifyListeners();
}

} // namespace erae
