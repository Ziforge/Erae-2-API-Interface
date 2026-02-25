#include "EraeLookAndFeel.h"

namespace erae {

EraeLookAndFeel::EraeLookAndFeel()
{
    // Window
    setColour(juce::ResizableWindow::backgroundColourId, Theme::Colors::Background);

    // Buttons
    setColour(juce::TextButton::buttonColourId, Theme::Colors::ButtonBg);
    setColour(juce::TextButton::buttonOnColourId, Theme::Colors::ButtonActive);
    setColour(juce::TextButton::textColourOnId, Theme::Colors::TextBright);
    setColour(juce::TextButton::textColourOffId, Theme::Colors::Text);

    // ComboBox
    setColour(juce::ComboBox::backgroundColourId, Theme::Colors::ButtonBg);
    setColour(juce::ComboBox::textColourId, Theme::Colors::Text);
    setColour(juce::ComboBox::outlineColourId, Theme::Colors::Separator);
    setColour(juce::ComboBox::arrowColourId, Theme::Colors::TextDim);

    // Labels
    setColour(juce::Label::textColourId, Theme::Colors::Text);

    // Popup menus
    setColour(juce::PopupMenu::backgroundColourId, Theme::Colors::PopupBg);
    setColour(juce::PopupMenu::textColourId, Theme::Colors::Text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, Theme::Colors::Accent);
    setColour(juce::PopupMenu::highlightedTextColourId, Theme::Colors::TextBright);
    setColour(juce::PopupMenu::headerTextColourId, Theme::Colors::TextDim);

    // Sliders
    setColour(juce::Slider::backgroundColourId, Theme::Colors::SliderTrack);
    setColour(juce::Slider::trackColourId, Theme::Colors::Accent);
    setColour(juce::Slider::thumbColourId, Theme::Colors::SliderThumb);
    setColour(juce::Slider::textBoxTextColourId, Theme::Colors::Text);
    setColour(juce::Slider::textBoxBackgroundColourId, Theme::Colors::ButtonBg);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

    // Toggle buttons
    setColour(juce::ToggleButton::tickColourId, Theme::Colors::Accent);
    setColour(juce::ToggleButton::tickDisabledColourId, Theme::Colors::TextDim);

    // Scroll bars
    setColour(juce::ScrollBar::thumbColourId, Theme::Colors::TextDim);
}

void EraeLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                            juce::Button& button,
                                            const juce::Colour& backgroundColour,
                                            bool shouldDrawButtonAsHighlighted,
                                            bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    auto baseColour = backgroundColour;

    if (shouldDrawButtonAsDown)
        baseColour = baseColour.brighter(0.15f);
    else if (shouldDrawButtonAsHighlighted)
        baseColour = baseColour.brighter(0.08f);

    g.setColour(baseColour);
    g.fillRoundedRectangle(bounds, Theme::ButtonRadius);

    // Subtle border
    g.setColour(Theme::Colors::Separator);
    g.drawRoundedRectangle(bounds, Theme::ButtonRadius, 0.5f);
}

void EraeLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                      bool /*shouldDrawButtonAsHighlighted*/,
                                      bool /*shouldDrawButtonAsDown*/)
{
    auto font = juce::Font(Theme::FontToolbar);
    g.setFont(font);

    auto colour = button.findColour(button.getToggleState()
                                        ? juce::TextButton::textColourOnId
                                        : juce::TextButton::textColourOffId);
    g.setColour(colour);

    auto bounds = button.getLocalBounds();
    g.drawText(button.getButtonText(), bounds, juce::Justification::centred, false);
}

void EraeLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height,
                                    bool isButtonDown, int, int, int, int,
                                    juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float>(0, 0, (float)width, (float)height).reduced(0.5f);
    auto bgColour = box.findColour(juce::ComboBox::backgroundColourId);

    if (isButtonDown)
        bgColour = bgColour.brighter(0.1f);

    g.setColour(bgColour);
    g.fillRoundedRectangle(bounds, Theme::ButtonRadius);
    g.setColour(Theme::Colors::Separator);
    g.drawRoundedRectangle(bounds, Theme::ButtonRadius, 0.5f);

    // Arrow
    auto arrowZone = juce::Rectangle<float>((float)width - 20.0f, 0, 16.0f, (float)height);
    juce::Path arrow;
    arrow.addTriangle(arrowZone.getCentreX() - 3.0f, arrowZone.getCentreY() - 2.0f,
                      arrowZone.getCentreX() + 3.0f, arrowZone.getCentreY() - 2.0f,
                      arrowZone.getCentreX(), arrowZone.getCentreY() + 3.0f);
    g.setColour(Theme::Colors::TextDim);
    g.fillPath(arrow);
}

void EraeLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    auto bounds = juce::Rectangle<float>(0, 0, (float)width, (float)height);
    g.setColour(Theme::Colors::PopupBg);
    g.fillRoundedRectangle(bounds, Theme::PopupRadius);
    g.setColour(Theme::Colors::Separator);
    g.drawRoundedRectangle(bounds, Theme::PopupRadius, 0.5f);
}

void EraeLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                        bool isSeparator, bool isActive, bool isHighlighted,
                                        bool isTicked, bool /*hasSubMenu*/,
                                        const juce::String& text, const juce::String& shortcutKeyText,
                                        const juce::Drawable* /*icon*/, const juce::Colour* textColour)
{
    if (isSeparator) {
        auto sepArea = area.reduced(8, 0);
        g.setColour(Theme::Colors::Separator);
        g.fillRect(sepArea.getX(), sepArea.getCentreY(), sepArea.getWidth(), 1);
        return;
    }

    auto r = area.reduced(4, 1);

    if (isHighlighted && isActive) {
        g.setColour(Theme::Colors::Accent);
        g.fillRoundedRectangle(r.toFloat(), 3.0f);
    }

    auto col = textColour != nullptr ? *textColour
               : isHighlighted ? Theme::Colors::TextBright
               : isActive ? Theme::Colors::Text
               : Theme::Colors::TextDisabled;

    g.setColour(col);
    g.setFont(juce::Font(Theme::FontBase));

    auto textArea = r.reduced(8, 0);

    if (isTicked) {
        auto tickArea = textArea.removeFromLeft(16);
        g.setColour(Theme::Colors::Accent);
        g.setFont(juce::Font(Theme::FontBase, juce::Font::bold));
        g.drawText(juce::CharPointer_UTF8("\xe2\x9c\x93"), tickArea, juce::Justification::centred);
        g.setColour(col);
        g.setFont(juce::Font(Theme::FontBase));
    }

    g.drawText(text, textArea, juce::Justification::centredLeft, true);

    if (shortcutKeyText.isNotEmpty()) {
        g.setColour(Theme::Colors::TextDim);
        g.setFont(juce::Font(Theme::FontSmall));
        g.drawText(shortcutKeyText, textArea, juce::Justification::centredRight, true);
    }
}

int EraeLookAndFeel::getPopupMenuBorderSize()
{
    return 4;
}

void EraeLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float minSliderPos, float maxSliderPos,
                                        const juce::Slider::SliderStyle style,
                                        juce::Slider& slider)
{
    if (style == juce::Slider::LinearBar || style == juce::Slider::LinearBarVertical) {
        auto bounds = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height);

        // Background track
        g.setColour(Theme::Colors::SliderTrack);
        g.fillRoundedRectangle(bounds, 2.0f);

        // Filled portion
        float fillWidth = sliderPos - (float)x;
        if (fillWidth > 0) {
            g.setColour(Theme::Colors::Accent);
            g.fillRoundedRectangle(bounds.withWidth(fillWidth), 2.0f);
        }

        // Value text
        auto text = slider.getTextFromValue(slider.getValue());
        g.setColour(Theme::Colors::TextBright);
        g.setFont(Theme::FontBase);
        g.drawText(text, bounds.toNearestInt(), juce::Justification::centred, false);
    } else {
        juce::LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos,
                                                minSliderPos, maxSliderPos, style, slider);
    }
}

void EraeLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                        bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/)
{
    auto bounds = button.getLocalBounds().toFloat();
    float size = std::min(bounds.getWidth(), bounds.getHeight()) - 4.0f;
    auto tickBounds = juce::Rectangle<float>(2.0f, (bounds.getHeight() - size) / 2.0f, size, size);

    // Background
    g.setColour(button.getToggleState() ? Theme::Colors::Accent : Theme::Colors::SliderTrack);
    g.fillRoundedRectangle(tickBounds, 2.0f);

    // Checkmark
    if (button.getToggleState()) {
        juce::Path tick;
        float cx = tickBounds.getCentreX(), cy = tickBounds.getCentreY();
        float s = size * 0.25f;
        tick.startNewSubPath(cx - s, cy);
        tick.lineTo(cx - s * 0.3f, cy + s * 0.8f);
        tick.lineTo(cx + s, cy - s * 0.6f);
        g.setColour(Theme::Colors::TextBright);
        g.strokePath(tick, juce::PathStrokeType(1.8f));
    }

    // Border
    g.setColour(shouldDrawButtonAsHighlighted ? Theme::Colors::Accent : Theme::Colors::Separator);
    g.drawRoundedRectangle(tickBounds, 2.0f, 0.5f);
}

void EraeLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    g.fillAll(label.findColour(juce::Label::backgroundColourId));

    auto textArea = label.getBorderSize().subtractedFrom(label.getLocalBounds());
    auto font = label.getFont();
    g.setFont(font);
    g.setColour(label.findColour(juce::Label::textColourId));
    g.drawText(label.getText(), textArea, label.getJustificationType(), false);
}

void EraeLookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height)
{
    auto bounds = juce::Rectangle<float>(0, 0, (float)width, (float)height);

    // Dark background with slight transparency
    g.setColour(Theme::Colors::Toolbar.withAlpha(0.95f));
    g.fillRoundedRectangle(bounds, Theme::ButtonRadius);
    g.setColour(Theme::Colors::Separator);
    g.drawRoundedRectangle(bounds, Theme::ButtonRadius, 0.5f);

    g.setColour(Theme::Colors::Text);
    g.setFont(juce::Font(Theme::FontSmall));
    g.drawText(text, bounds.reduced(6, 2), juce::Justification::centredLeft, true);
}

juce::Font EraeLookAndFeel::getPopupMenuFont()
{
    return juce::Font(Theme::FontBase);
}

juce::Font EraeLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return juce::Font(Theme::FontBase);
}

void EraeLookAndFeel::drawScrollbar(juce::Graphics& g, juce::ScrollBar&,
                                     int x, int y, int width, int height,
                                     bool isScrollbarVertical,
                                     int thumbStartPosition, int thumbSize,
                                     bool isMouseOver, bool isMouseDown)
{
    // Track background (nearly invisible)
    g.setColour(Theme::Colors::Separator.withAlpha(0.3f));
    g.fillRect(x, y, width, height);

    // Thumb
    auto thumbColour = Theme::Colors::TextDim.withAlpha(isMouseDown ? 0.7f : isMouseOver ? 0.5f : 0.3f);
    g.setColour(thumbColour);

    if (isScrollbarVertical) {
        int thumbW = std::max(4, width - 4);
        int thumbX = x + (width - thumbW) / 2;
        g.fillRoundedRectangle((float)thumbX, (float)(y + thumbStartPosition),
                               (float)thumbW, (float)thumbSize, (float)thumbW / 2.0f);
    } else {
        int thumbH = std::max(4, height - 4);
        int thumbY = y + (height - thumbH) / 2;
        g.fillRoundedRectangle((float)(x + thumbStartPosition), (float)thumbY,
                               (float)thumbSize, (float)thumbH, (float)thumbH / 2.0f);
    }
}

} // namespace erae
