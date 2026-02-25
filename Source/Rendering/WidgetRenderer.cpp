#include "WidgetRenderer.h"
#include <cmath>
#include <algorithm>

namespace erae {
namespace WidgetRenderer {

// ============================================================
// Color interpolation helpers
// ============================================================

static Color7 lerpColor(Color7 a, Color7 b, float t)
{
    t = std::max(0.0f, std::min(1.0f, t));
    return {
        (int)(a.r + (b.r - a.r) * t),
        (int)(a.g + (b.g - a.g) * t),
        (int)(a.b + (b.b - a.b) * t)
    };
}

static juce::Colour lerpJuceColour(juce::Colour a, juce::Colour b, float t)
{
    t = std::max(0.0f, std::min(1.0f, t));
    return juce::Colour(
        (juce::uint8)(a.getRed()   + (b.getRed()   - a.getRed())   * t),
        (juce::uint8)(a.getGreen() + (b.getGreen() - a.getGreen()) * t),
        (juce::uint8)(a.getBlue()  + (b.getBlue()  - a.getBlue())  * t));
}

// ============================================================
// Visual param helpers
// ============================================================

static bool getFillHorizontal(const Shape& shape)
{
    if (auto* obj = shape.visualParams.getDynamicObject())
        if (obj->hasProperty("fill_horizontal"))
            return (bool)obj->getProperty("fill_horizontal");
    return false;
}

static int getDotSize(const Shape& shape)
{
    if (auto* obj = shape.visualParams.getDynamicObject())
        if (obj->hasProperty("dot_size"))
            return std::max(1, std::min(3, (int)obj->getProperty("dot_size")));
    return 1;
}

// ============================================================
// Erae surface rendering (pixel commands)
// ============================================================

static std::vector<PixelCommand> renderStatic(const Shape& shape)
{
    std::vector<PixelCommand> cmds;
    auto pixels = shape.gridPixels();
    for (auto& [px, py] : pixels) {
        if (px >= 0 && px < 42 && py >= 0 && py < 24)
            cmds.push_back({px, py, shape.color});
    }
    return cmds;
}

static std::vector<PixelCommand> renderFillBar(const Shape& shape, const WidgetState& state)
{
    std::vector<PixelCommand> cmds;
    auto bb = shape.bbox();
    float bbW = bb.xMax - bb.xMin;
    float bbH = bb.yMax - bb.yMin;
    bool horiz = getFillHorizontal(shape);
    auto pixels = shape.gridPixels();

    for (auto& [px, py] : pixels) {
        if (px < 0 || px >= 42 || py < 0 || py >= 24) continue;

        bool filled = false;
        if (state.active) {
            if (horiz) {
                float relX = (bbW > 0) ? (px + 0.5f - bb.xMin) / bbW : 0;
                filled = relX <= state.normX;
            } else {
                // Fill from bottom up: normY=0 is top (finger at top = high value = full fill)
                // Pixel is filled if it's below the fill line: relY >= normY
                float relY = (bbH > 0) ? (py + 0.5f - bb.yMin) / bbH : 0;
                filled = relY >= state.normY;
            }
        }
        cmds.push_back({px, py, filled ? shape.colorActive : shape.color});
    }
    return cmds;
}

static std::vector<PixelCommand> renderPositionDot(const Shape& shape, const WidgetState& state)
{
    std::vector<PixelCommand> cmds;
    auto bb = shape.bbox();
    float bbW = bb.xMax - bb.xMin;
    float bbH = bb.yMax - bb.yMin;
    int dotSize = getDotSize(shape);
    int dotHalf = dotSize / 2;

    // Compute dot center in grid coords
    int dotCX = (int)(bb.xMin + state.normX * bbW);
    int dotCY = (int)(bb.yMin + state.normY * bbH);

    auto pixels = shape.gridPixels();
    for (auto& [px, py] : pixels) {
        if (px < 0 || px >= 42 || py < 0 || py >= 24) continue;

        bool isDot = state.active &&
                     px >= dotCX - dotHalf && px <= dotCX + dotHalf &&
                     py >= dotCY - dotHalf && py <= dotCY + dotHalf;
        cmds.push_back({px, py, isDot ? shape.colorActive : shape.color});
    }
    return cmds;
}

static std::vector<PixelCommand> renderRadialArc(const Shape& shape, const WidgetState& state)
{
    std::vector<PixelCommand> cmds;
    auto bb = shape.bbox();
    float cx = (bb.xMin + bb.xMax) / 2.0f;
    float cy = (bb.yMin + bb.yMax) / 2.0f;

    // Finger angle: compute from normalized position to actual grid position
    float fingerGX = bb.xMin + state.normX * (bb.xMax - bb.xMin);
    float fingerGY = bb.yMin + state.normY * (bb.yMax - bb.yMin);
    // Angle from center, measured clockwise from top (12 o'clock = 0)
    float fingerAngle = std::atan2(fingerGX - cx, -(fingerGY - cy)); // clockwise from top
    if (fingerAngle < 0) fingerAngle += 2.0f * 3.14159265f;

    auto pixels = shape.gridPixels();
    for (auto& [px, py] : pixels) {
        if (px < 0 || px >= 42 || py < 0 || py >= 24) continue;

        bool inArc = false;
        if (state.active) {
            float pixAngle = std::atan2(px + 0.5f - cx, -(py + 0.5f - cy));
            if (pixAngle < 0) pixAngle += 2.0f * 3.14159265f;
            inArc = pixAngle <= fingerAngle;
        }
        cmds.push_back({px, py, inArc ? shape.colorActive : shape.color});
    }
    return cmds;
}

static std::vector<PixelCommand> renderPressureGlow(const Shape& shape, const WidgetState& state)
{
    std::vector<PixelCommand> cmds;
    Color7 col = state.active ? lerpColor(shape.color, shape.colorActive, state.pressure) : shape.color;
    auto pixels = shape.gridPixels();
    for (auto& [px, py] : pixels) {
        if (px >= 0 && px < 42 && py >= 0 && py < 24)
            cmds.push_back({px, py, col});
    }
    return cmds;
}

std::vector<PixelCommand> renderWidget(const Shape& shape, const WidgetState& state)
{
    auto style = visualStyleFromString(shape.visualStyle);
    switch (style) {
        case VisualStyle::FillBar:      return renderFillBar(shape, state);
        case VisualStyle::PositionDot:  return renderPositionDot(shape, state);
        case VisualStyle::RadialArc:    return renderRadialArc(shape, state);
        case VisualStyle::PressureGlow: return renderPressureGlow(shape, state);
        case VisualStyle::Static:
        default:                        return renderStatic(shape);
    }
}

// ============================================================
// JUCE Graphics rendering (for GridCanvas)
// ============================================================

static void drawFillBarJuce(juce::Graphics& g, const Shape& shape, const WidgetState& state,
                            juce::Rectangle<float> bounds)
{
    auto baseCol = shape.color.toJuceColour();
    auto activeCol = shape.colorActive.toJuceColour();
    bool horiz = getFillHorizontal(shape);

    g.setColour(baseCol);
    g.fillRect(bounds);

    if (state.active) {
        juce::Rectangle<float> filled;
        if (horiz) {
            float fillW = bounds.getWidth() * state.normX;
            filled = bounds.withWidth(fillW);
        } else {
            // normY=0 (finger at top) = full fill from bottom; normY=1 = empty
            float fillH = bounds.getHeight() * (1.0f - state.normY);
            filled = bounds.withTop(bounds.getBottom() - fillH);
        }
        g.setColour(activeCol);
        g.fillRect(filled);
    }
}

static void drawPositionDotJuce(juce::Graphics& g, const Shape& shape, const WidgetState& state,
                                juce::Rectangle<float> bounds, float cellPx)
{
    auto baseCol = shape.color.toJuceColour();
    auto activeCol = shape.colorActive.toJuceColour();

    g.setColour(baseCol);
    g.fillRect(bounds);

    if (state.active) {
        int dotSize = getDotSize(shape);
        float dotPx = dotSize * cellPx;
        float dotX = bounds.getX() + state.normX * bounds.getWidth() - dotPx / 2;
        float dotY = bounds.getY() + state.normY * bounds.getHeight() - dotPx / 2;

        g.setColour(activeCol);
        g.fillEllipse(dotX, dotY, dotPx, dotPx);
    }
}

static void drawRadialArcJuce(juce::Graphics& g, const Shape& shape, const WidgetState& state,
                               juce::Rectangle<float> bounds)
{
    auto baseCol = shape.color.toJuceColour();
    auto activeCol = shape.colorActive.toJuceColour();

    g.setColour(baseCol);
    g.fillRect(bounds);

    if (state.active) {
        float cx = bounds.getCentreX();
        float cy = bounds.getCentreY();
        float radius = std::min(bounds.getWidth(), bounds.getHeight()) / 2.0f;

        // Finger angle clockwise from top
        float fx = (state.normX - 0.5f) * bounds.getWidth();
        float fy = (state.normY - 0.5f) * bounds.getHeight();
        float angleRad = std::atan2(fx, -fy); // clockwise from top
        if (angleRad < 0) angleRad += 2.0f * 3.14159265f;
        // JUCE arc: angles measured clockwise from 12 o'clock
        // Path addPieSegment uses radians from 3 o'clock, so adjust
        juce::Path arc;
        // startAngle in radians from 3 o'clock position, going clockwise
        float startRad = -3.14159265f / 2.0f; // 12 o'clock in JUCE coords
        float endRad = startRad + angleRad;
        arc.addPieSegment(cx - radius, cy - radius, radius * 2, radius * 2,
                          startRad, endRad, 0.0f);

        g.setColour(activeCol);
        g.fillPath(arc);
    }
}

static void drawPressureGlowJuce(juce::Graphics& g, const Shape& shape, const WidgetState& state,
                                  juce::Rectangle<float> bounds)
{
    auto baseCol = shape.color.toJuceColour();
    auto activeCol = shape.colorActive.toJuceColour();
    auto col = state.active ? lerpJuceColour(baseCol, activeCol, state.pressure) : baseCol;
    g.setColour(col);
    g.fillRect(bounds);
}

void drawWidget(juce::Graphics& g, const Shape& shape, const WidgetState& state,
                juce::Rectangle<float> screenBounds, float cellPx)
{
    auto style = visualStyleFromString(shape.visualStyle);
    switch (style) {
        case VisualStyle::FillBar:
            drawFillBarJuce(g, shape, state, screenBounds);
            break;
        case VisualStyle::PositionDot:
            drawPositionDotJuce(g, shape, state, screenBounds, cellPx);
            break;
        case VisualStyle::RadialArc:
            drawRadialArcJuce(g, shape, state, screenBounds);
            break;
        case VisualStyle::PressureGlow:
            drawPressureGlowJuce(g, shape, state, screenBounds);
            break;
        case VisualStyle::Static:
        default:
            // Static: don't override — let the normal drawShape handle it
            break;
    }
}

} // namespace WidgetRenderer
} // namespace erae
