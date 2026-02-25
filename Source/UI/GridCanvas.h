#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Model/Layout.h"
#include "../Model/Color.h"
#include "../Rendering/WidgetRenderer.h"
#include "Theme.h"
#include <set>
#include <map>

namespace erae {

enum class ToolMode { Select, Paint, Erase, DrawRect, DrawCircle, DrawHex };

// Handle positions for selected shape resize
enum class HandlePos { None, TopLeft, Top, TopRight, Right, BottomRight, Bottom, BottomLeft, Left };

// ============================================================
// GridCanvas — full-featured shape editor canvas
// ============================================================
class GridCanvas : public juce::Component,
                   public Layout::Listener {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void selectionChanged(const std::string& shapeId) = 0;
        virtual void toolModeChanged(ToolMode) {}
    };

    GridCanvas(Layout& layout);
    ~GridCanvas() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    bool keyPressed(const juce::KeyPress& key) override;

    void layoutChanged() override;

    // Coordinate conversions
    juce::Point<float> screenToGrid(juce::Point<float> screen) const;
    juce::Point<float> gridToScreen(juce::Point<float> grid) const;

    // Zoom
    float getZoom() const { return zoom_; }
    void setZoom(float z);
    void zoomToFit();

    // Tool
    ToolMode getToolMode() const { return toolMode_; }
    void setToolMode(ToolMode mode);

    // Paint settings
    void setPaintColor(Color7 c) { paintColor_ = c; }
    Color7 getPaintColor() const { return paintColor_; }
    void setBrushSize(int size) { brushSize_ = juce::jlimit(1, 5, size); }
    int getBrushSize() const { return brushSize_; }

    // Selection
    std::string getSelectedId() const { return selectedId_; }
    void setSelectedId(const std::string& id);
    void deleteSelected();
    void duplicateSelected();

    // Finger overlay — call from editor timer
    struct FingerDot {
        float x, y, z;
        bool operator==(const FingerDot& o) const { return x == o.x && y == o.y && z == o.z; }
        bool operator!=(const FingerDot& o) const { return !(*this == o); }
    };
    void setFingers(const std::map<uint64_t, FingerDot>& fingers);
    void setWidgetStates(const std::map<std::string, WidgetState>& states);

    void addListener(Listener* l) { canvasListeners_.push_back(l); }
    void removeListener(Listener* l) {
        canvasListeners_.erase(std::remove(canvasListeners_.begin(), canvasListeners_.end(), l),
                               canvasListeners_.end());
    }

private:
    void drawGrid(juce::Graphics& g);
    void drawShapes(juce::Graphics& g);
    void drawShape(juce::Graphics& g, const Shape& shape);
    void drawHoverHighlight(juce::Graphics& g);
    void drawSelection(juce::Graphics& g);
    void drawCreationPreview(juce::Graphics& g);
    void drawCursor(juce::Graphics& g);
    void drawFingerOverlay(juce::Graphics& g);
    void drawCoordinateReadout(juce::Graphics& g);

    juce::Rectangle<float> gridCellToScreen(float gx, float gy, float gw = 1.0f, float gh = 1.0f) const;
    juce::Path hexPath(float cx, float cy, float radius) const;

    // Paint/erase
    void paintAtScreen(juce::Point<float> screenPos);
    void eraseAtScreen(juce::Point<float> screenPos);
    void paintPixel(int gx, int gy);
    void erasePixel(int gx, int gy);
    static std::string pixelId(int gx, int gy) { return "px_" + std::to_string(gx) + "_" + std::to_string(gy); }

    // Selection handles
    HandlePos hitTestHandle(juce::Point<float> screenPos) const;
    juce::Rectangle<float> getHandleRect(HandlePos pos) const;
    std::vector<HandlePos> allHandles() const;
    juce::Rectangle<float> selectedBBoxScreen() const;

    // Shape creation
    void finishCreation();
    std::string nextShapeId();

    // Grid snap
    float snapToGrid(float v) const { return std::round(v); }

    Layout& layout_;
    float zoom_ = Theme::DefaultZoom;
    juce::Point<float> panOffset_ {0, 0};

    ToolMode toolMode_ = ToolMode::Select;
    Color7 paintColor_ {0, 80, 127};
    int brushSize_ = 1;

    // Pan state
    bool panning_ = false;
    juce::Point<float> panStart_, panOffsetStart_;

    // Paint state
    bool painting_ = false;
    std::set<std::pair<int,int>> strokeCells_;

    // Selection state
    std::string selectedId_;
    bool draggingShape_ = false;
    HandlePos draggingHandle_ = HandlePos::None;
    juce::Point<float> dragStartGrid_;
    float dragStartX_, dragStartY_, dragStartW_, dragStartH_, dragStartR_;

    // Creation state (DrawRect/Circle/Hex)
    bool creating_ = false;
    juce::Point<float> createStartGrid_;
    juce::Point<float> createEndGrid_;

    // Cursor & hover
    juce::Point<float> cursorGrid_ {-1, -1};
    std::string hoveredId_;

    // ID counter
    int shapeCounter_ = 0;

    std::vector<Listener*> canvasListeners_;

    // Finger overlay
    std::map<uint64_t, FingerDot> fingers_;

    // Widget states for visual rendering
    std::map<std::string, WidgetState> widgetStates_;

    static constexpr float HandleSize = 8.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GridCanvas)
};

} // namespace erae
