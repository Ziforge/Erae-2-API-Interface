#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Model/Layout.h"
#include "../Model/Color.h"
#include "../Core/UndoManager.h"
#include "../Core/SelectionManager.h"
#include "../Rendering/WidgetRenderer.h"
#include "../Rendering/FingerPalette.h"
#include "Theme.h"
#include <set>
#include <map>

namespace erae {

enum class ToolMode { Select, Paint, Erase, DrawRect, DrawCircle, DrawHex, DrawPoly, DrawPixel, EditShape };

// Handle positions for selected shape resize
enum class HandlePos { None, TopLeft, Top, TopRight, Right, BottomRight, Bottom, BottomLeft, Left };

// ============================================================
// GridCanvas — full-featured shape editor canvas
// ============================================================
class GridCanvas : public juce::Component,
                   public Layout::Listener,
                   public SelectionManager::Listener {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void selectionChanged() = 0;
        virtual void toolModeChanged(ToolMode) {}
        virtual void copyRequested() {}
        virtual void cutRequested() {}
        virtual void pasteRequested() {}
        virtual void designModeChanged(bool /*active*/) {}
        virtual void designFinished(std::set<std::pair<int,int>> /*cells*/) {}
    };

    GridCanvas(Layout& layout, UndoManager& undoManager, SelectionManager& selectionManager);
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
    void selectionChanged() override;

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

    // Selection (delegated to SelectionManager)
    std::string getSelectedId() const { return selMgr_.getSingleSelectedId(); }
    void setSelectedId(const std::string& id);
    void deleteSelected();
    void duplicateSelected();

    // Re-point to a different layout (for multi-page)
    void setLayout(Layout& newLayout);

    // Finger overlay — call from editor timer
    struct FingerDot {
        float x, y, z;
        bool operator==(const FingerDot& o) const { return x == o.x && y == o.y && z == o.z; }
        bool operator!=(const FingerDot& o) const { return !(*this == o); }
    };
    void setFingers(const std::map<uint64_t, FingerDot>& fingers);
    void setWidgetStates(const std::map<std::string, WidgetState>& states);
    void setHighlightedShapes(const std::set<std::string>& ids);
    void setPerFingerColors(bool enabled) { perFingerColors_ = enabled; }

    // Poly/pixel creation (public for toolbar Done button)
    void finishPolygonCreation();
    void cancelPolygonCreation();
    void finishPixelCreation();
    void cancelPixelCreation();
    bool isCreatingPixelShape() const { return creatingPixelShape_; }
    bool isCreatingPoly() const { return creatingPoly_; }

    // Edit-shape mode
    void enterEditMode(const std::string& shapeId, bool resizeOnly = false);
    void exitEditMode(bool commit = true);
    bool isEditingShape() const { return !editingShapeId_.empty(); }

    // Design-shape mode (full-canvas shape designer)
    void enterDesignMode(const std::string& shapeId = "");
    void exitDesignMode(bool save);
    bool isDesigning() const { return designMode_; }
    const std::set<std::pair<int,int>>& getDesignCells() const { return designCells_; }
    bool getDesignSymmetryH() const { return designSymmetryH_; }
    bool getDesignSymmetryV() const { return designSymmetryV_; }
    void setDesignSymmetryH(bool v) { designSymmetryH_ = v; repaint(); }
    void setDesignSymmetryV(bool v) { designSymmetryV_ = v; repaint(); }

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
    void drawPolygonCreationPreview(juce::Graphics& g);
    void drawPixelCreationPreview(juce::Graphics& g);
    void drawCursor(juce::Graphics& g);
    void drawEditModeOverlay(juce::Graphics& g);
    void drawDesignModeOverlay(juce::Graphics& g);
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

    // Selection handles (operates on primary selected shape)
    HandlePos hitTestHandle(juce::Point<float> screenPos) const;
    juce::Rectangle<float> getHandleRect(HandlePos pos) const;
    std::vector<HandlePos> allHandles() const;
    juce::Rectangle<float> selectedBBoxScreen() const;

    // Shape creation
    void finishCreation();
    void undoPixelStroke();
    std::string nextShapeId();

    // Edit-shape helpers
    void syncEditCellsToShape();
    void editAddCell(int cx, int cy);
    void editRemoveCell(int cx, int cy);
    HandlePos editHitTestHandle(juce::Point<float> screenPos) const;
    juce::Rectangle<float> editBBoxScreen() const;

    // Design-mode helpers
    void designAddCell(int cx, int cy);
    void designRemoveCell(int cx, int cy);
    void designStampCells(const std::vector<std::pair<int,int>>& cells);
    HandlePos designHitTestHandle(juce::Point<float> screenPos) const;
    juce::Rectangle<float> designBBoxScreen() const;

    // Grid snap
    float snapToGrid(float v) const { return std::round(v); }

    Layout* layout_;
    UndoManager& undoMgr_;
    SelectionManager& selMgr_;

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
    bool draggingShape_ = false;
    HandlePos draggingHandle_ = HandlePos::None;
    juce::Point<float> dragStartGrid_;
    float dragStartX_, dragStartY_, dragStartW_, dragStartH_, dragStartR_;
    int currentDragId_ = 0; // for undo coalescing

    // Multi-shape drag state
    struct DragOrigin { float x, y; };
    std::map<std::string, DragOrigin> dragOrigins_;

    // Creation state (DrawRect/Circle/Hex)
    bool creating_ = false;
    juce::Point<float> createStartGrid_;
    juce::Point<float> createEndGrid_;

    // Polygon creation state
    bool creatingPoly_ = false;
    std::vector<juce::Point<float>> polyVertices_;
    juce::Point<float> polyRubberBand_;

    // Pixel shape creation state
    bool creatingPixelShape_ = false;
    std::set<std::pair<int,int>> pixelCells_;
    std::vector<std::vector<std::pair<int,int>>> pixelStrokeHistory_;
    std::set<std::pair<int,int>> currentStroke_;
    bool pixelErasing_ = false;

    // Cursor & hover
    juce::Point<float> cursorGrid_ {-1, -1};
    std::string hoveredId_;

    // ID counter
    int shapeCounter_ = 0;
    int dragIdCounter_ = 0;

    std::vector<Listener*> canvasListeners_;

    // Finger overlay
    std::map<uint64_t, FingerDot> fingers_;

    // Widget states for visual rendering
    std::map<std::string, WidgetState> widgetStates_;

    // DAW feedback highlights
    std::set<std::string> highlightedShapes_;
    bool perFingerColors_ = true;

    // Design-shape mode state
    bool designMode_ = false;
    std::set<std::pair<int,int>> designCells_;        // accumulated design cells (absolute grid coords)
    std::string designOrigShapeId_;                    // if editing existing shape
    bool designSymmetryH_ = false;
    bool designSymmetryV_ = false;
    HandlePos designDraggingHandle_ = HandlePos::None;
    std::set<std::pair<int,int>> designDragStartCells_;
    float designDragStartX_, designDragStartY_, designDragStartW_, designDragStartH_;
    std::vector<std::set<std::pair<int,int>>> designSnapshots_; // for per-stroke undo
    ToolMode designPrevToolMode_ = ToolMode::Paint;    // tool mode before entering design

    // Edit-shape mode state
    std::string editingShapeId_;
    std::unique_ptr<Shape> editOrigShape_;        // clone of shape before editing
    std::set<std::pair<int,int>> editCells_;       // absolute grid coords of current cells
    bool editConverted_ = false;                   // true if we auto-converted to PixelShape
    bool editResizeOnly_ = false;                  // true = only handle resize, no cell painting
    HandlePos editDraggingHandle_ = HandlePos::None;
    std::set<std::pair<int,int>> editDragStartCells_;         // cells snapshot at resize drag start
    std::vector<std::set<std::pair<int,int>>> editSnapshots_; // cell snapshots for per-stroke undo
    bool editSymmetryH_ = false;                   // horizontal mirror painting
    bool editSymmetryV_ = false;                   // vertical mirror painting

    static constexpr float HandleSize = 8.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GridCanvas)
};

} // namespace erae
