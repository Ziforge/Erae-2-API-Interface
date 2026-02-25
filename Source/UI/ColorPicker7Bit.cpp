#include "ColorPicker7Bit.h"
#include <cmath>

namespace erae {

ColorPicker7Bit::ColorPicker7Bit()
{
    // Initialize quick palette
    palette_[0]  = Palette::Red;
    palette_[1]  = Palette::Orange;
    palette_[2]  = Palette::Yellow;
    palette_[3]  = Palette::Green;
    palette_[4]  = Palette::Cyan;
    palette_[5]  = Palette::Blue;
    palette_[6]  = Palette::Purple;
    palette_[7]  = Palette::Magenta;
    palette_[8]  = Palette::White;
    palette_[9]  = Color7{100, 100, 100};
    palette_[10] = Palette::Gray;
    palette_[11] = Palette::DimWhite;
    palette_[12] = Color7{127, 50, 50};
    palette_[13] = Color7{50, 127, 50};
    palette_[14] = Color7{50, 50, 127};
    palette_[15] = Palette::Black;

    updateFromHSV();
}

// ============================================================
// Layout bounds
// ============================================================

juce::Rectangle<int> ColorPicker7Bit::hueBarBounds() const
{
    return {0, 0, getWidth(), 20};
}

juce::Rectangle<int> ColorPicker7Bit::svSquareBounds() const
{
    // Below the SV square: gap(4) + preview(28) + gap(2) + label(16) + gap(4) + palette(44) = 98
    // Above: hue(20) + gap(4) = 24. Total overhead = 24 + 98 = 122. Add 4px breathing room.
    int size = std::min(getWidth(), getHeight() - 126);
    if (size < 40) size = 40;
    return {0, 24, getWidth(), size};
}

juce::Rectangle<int> ColorPicker7Bit::previewBounds() const
{
    auto sv = svSquareBounds();
    return {0, sv.getBottom() + 4, getWidth(), 28};
}

juce::Rectangle<int> ColorPicker7Bit::labelBounds() const
{
    auto pv = previewBounds();
    return {0, pv.getBottom() + 2, getWidth(), 16};
}

juce::Rectangle<int> ColorPicker7Bit::paletteBounds() const
{
    auto lb = labelBounds();
    return {0, lb.getBottom() + 4, getWidth(), 44};
}

// ============================================================
// Paint
// ============================================================

void ColorPicker7Bit::paint(juce::Graphics& g)
{
    // Hue bar — rainbow gradient (rendered to Image for rounded clipping)
    auto hb = hueBarBounds();
    {
        juce::Image hueImg(juce::Image::RGB, std::max(1, hb.getWidth()), std::max(1, hb.getHeight()), false);
        for (int x = 0; x < hb.getWidth(); ++x) {
            float h = (float)x / hb.getWidth() * 360.0f;
            auto c = hsvToRgb7(h, 1.0f, 1.0f);
            auto jc = c.toJuceColour();
            for (int y = 0; y < hb.getHeight(); ++y)
                hueImg.setPixelAt(x, y, jc);
        }
        juce::Path clip;
        clip.addRoundedRectangle(hb.toFloat(), Theme::ButtonRadius);
        g.saveState();
        g.reduceClipRegion(clip);
        g.drawImageAt(hueImg, hb.getX(), hb.getY());
        g.restoreState();
    }
    // Hue cursor
    float hueCursorX = hb.getX() + (hue_ / 360.0f) * hb.getWidth();
    g.setColour(Theme::Colors::TextBright);
    g.drawLine(hueCursorX, (float)hb.getY(), hueCursorX, (float)hb.getBottom(), 2.0f);

    // SV square — saturation on X, value on Y (rendered to Image, rounded)
    auto sv = svSquareBounds();
    if (sv.getWidth() > 0 && sv.getHeight() > 0) {
        juce::Image svImg(juce::Image::RGB, sv.getWidth(), sv.getHeight(), false);
        for (int y = 0; y < sv.getHeight(); ++y) {
            float v = 1.0f - (float)y / sv.getHeight();
            for (int x = 0; x < sv.getWidth(); ++x) {
                float s = (float)x / sv.getWidth();
                auto c = hsvToRgb7(hue_, s, v);
                svImg.setPixelAt(x, y, c.toJuceColour());
            }
        }
        juce::Path clip;
        clip.addRoundedRectangle(sv.toFloat(), Theme::CornerRadius);
        g.saveState();
        g.reduceClipRegion(clip);
        g.drawImageAt(svImg, sv.getX(), sv.getY());
        g.restoreState();
    }
    // SV cursor (crosshair)
    float svCursorX = sv.getX() + sat_ * sv.getWidth();
    float svCursorY = sv.getY() + (1.0f - val_) * sv.getHeight();
    g.setColour(Theme::Colors::TextBright);
    g.drawEllipse(svCursorX - 5, svCursorY - 5, 10, 10, 1.5f);
    g.setColour(Theme::Colors::CanvasBg);
    g.drawEllipse(svCursorX - 4, svCursorY - 4, 8, 8, 1.0f);

    // Preview swatch
    auto pv = previewBounds();
    g.setColour(currentColor_.toJuceColour());
    g.fillRoundedRectangle(pv.toFloat(), Theme::ButtonRadius);
    g.setColour(Theme::Colors::Separator);
    g.drawRoundedRectangle(pv.toFloat(), Theme::ButtonRadius, 0.5f);

    // RGB label (fixed-width digits)
    auto lb = labelBounds();
    g.setColour(Theme::Colors::TextDim);
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), Theme::FontSmall, juce::Font::plain));
    auto rStr = juce::String(currentColor_.r).paddedLeft('0', 3);
    auto gStr = juce::String(currentColor_.g).paddedLeft('0', 3);
    auto bStr = juce::String(currentColor_.b).paddedLeft('0', 3);
    g.drawText("R:" + rStr + "  G:" + gStr + "  B:" + bStr,
               lb, juce::Justification::centred);

    // Quick palette
    auto pb = paletteBounds();
    float cellW = (float)pb.getWidth() / kPaletteCols;
    float cellH = (float)pb.getHeight() / kPaletteRows;
    for (int row = 0; row < kPaletteRows; ++row) {
        for (int col = 0; col < kPaletteCols; ++col) {
            auto& c = palette_[row * kPaletteCols + col];
            float cx = pb.getX() + col * cellW;
            float cy = pb.getY() + row * cellH;
            auto cellRect = juce::Rectangle<float>(cx + 1, cy + 1, cellW - 2, cellH - 2);
            g.setColour(c.toJuceColour());
            g.fillRoundedRectangle(cellRect, 2.0f);

            // Highlight if current color matches
            if (c == currentColor_) {
                g.setColour(Theme::Colors::TextBright);
                g.drawRoundedRectangle(juce::Rectangle<float>(cx, cy, cellW, cellH), 2.0f, 1.5f);
            }
        }
    }
}

void ColorPicker7Bit::resized() {}

// ============================================================
// Mouse interaction
// ============================================================

void ColorPicker7Bit::mouseDown(const juce::MouseEvent& e)
{
    auto pos = e.position;

    if (hueBarBounds().toFloat().contains(pos)) {
        dragTarget_ = DragTarget::Hue;
        handleHueClick(pos.x);
    }
    else if (svSquareBounds().toFloat().contains(pos)) {
        dragTarget_ = DragTarget::SV;
        handleSVClick(pos.x, pos.y);
    }
    else if (paletteBounds().toFloat().contains(pos)) {
        dragTarget_ = DragTarget::Palette;
        handlePaletteClick(pos.x, pos.y);
    }
}

void ColorPicker7Bit::mouseDrag(const juce::MouseEvent& e)
{
    auto pos = e.position;

    if (dragTarget_ == DragTarget::Hue)
        handleHueClick(pos.x);
    else if (dragTarget_ == DragTarget::SV)
        handleSVClick(pos.x, pos.y);
}

void ColorPicker7Bit::handleHueClick(float x)
{
    auto hb = hueBarBounds();
    hue_ = juce::jlimit(0.0f, 360.0f, (x - hb.getX()) / hb.getWidth() * 360.0f);
    updateFromHSV();
    repaint();
    notifyListeners();
}

void ColorPicker7Bit::handleSVClick(float x, float y)
{
    auto sv = svSquareBounds();
    sat_ = juce::jlimit(0.0f, 1.0f, (x - sv.getX()) / sv.getWidth());
    val_ = juce::jlimit(0.0f, 1.0f, 1.0f - (y - sv.getY()) / sv.getHeight());
    updateFromHSV();
    repaint();
    notifyListeners();
}

void ColorPicker7Bit::handlePaletteClick(float x, float y)
{
    auto pb = paletteBounds();
    float cellW = (float)pb.getWidth() / kPaletteCols;
    float cellH = (float)pb.getHeight() / kPaletteRows;
    int col = (int)((x - pb.getX()) / cellW);
    int row = (int)((y - pb.getY()) / cellH);
    col = juce::jlimit(0, kPaletteCols - 1, col);
    row = juce::jlimit(0, kPaletteRows - 1, row);

    setColor(palette_[row * kPaletteCols + col]);
}

// ============================================================
// Color management
// ============================================================

void ColorPicker7Bit::setColor(Color7 c)
{
    currentColor_ = c;
    // Reverse-map to HSV (approximate — good enough for UI)
    float r = c.r / 127.0f, g = c.g / 127.0f, b = c.b / 127.0f;
    float maxC = std::max({r, g, b});
    float minC = std::min({r, g, b});
    float delta = maxC - minC;

    val_ = maxC;
    sat_ = (maxC > 0) ? delta / maxC : 0;

    if (delta < 0.001f) {
        hue_ = 0;
    } else if (maxC == r) {
        hue_ = 60.0f * std::fmod((g - b) / delta, 6.0f);
    } else if (maxC == g) {
        hue_ = 60.0f * ((b - r) / delta + 2.0f);
    } else {
        hue_ = 60.0f * ((r - g) / delta + 4.0f);
    }
    if (hue_ < 0) hue_ += 360.0f;

    repaint();
    notifyListeners();
}

void ColorPicker7Bit::updateFromHSV()
{
    currentColor_ = hsvToRgb7(hue_, sat_, val_);
}

void ColorPicker7Bit::notifyListeners()
{
    for (auto* l : listeners_)
        l->colorChanged(currentColor_);
}

} // namespace erae
