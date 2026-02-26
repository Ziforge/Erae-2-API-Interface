#include "GridCanvas.h"
#include "../Model/Behavior.h"
#include "../Core/LayoutActions.h"
#include <cmath>
#include <algorithm>

namespace erae {

GridCanvas::GridCanvas(Layout& layout, UndoManager& undoManager, SelectionManager& selectionManager)
    : layout_(&layout), undoMgr_(undoManager), selMgr_(selectionManager)
{
    layout_->addListener(this);
    selMgr_.addListener(this);
    setOpaque(true);
    setWantsKeyboardFocus(true);
}

GridCanvas::~GridCanvas()
{
    layout_->removeListener(this);
    selMgr_.removeListener(this);
}

void GridCanvas::setLayout(Layout& newLayout)
{
    layout_->removeListener(this);
    layout_ = &newLayout;
    layout_->addListener(this);
    selMgr_.clear();
    repaint();
}

// ============================================================
// Tool mode
// ============================================================

void GridCanvas::setToolMode(ToolMode mode)
{
    // Cancel any in-progress creation when switching tools
    cancelPolygonCreation();
    cancelPixelCreation();

    // Exit edit mode if switching away
    if (!editingShapeId_.empty() && mode != ToolMode::EditShape)
        exitEditMode(true);

    toolMode_ = mode;
    creating_ = false;
    painting_ = false;

    switch (mode) {
        case ToolMode::Select:     setMouseCursor(juce::MouseCursor::NormalCursor); break;
        case ToolMode::Paint:
        case ToolMode::Erase:      setMouseCursor(juce::MouseCursor::CrosshairCursor); break;
        case ToolMode::DrawRect:
        case ToolMode::DrawCircle:
        case ToolMode::DrawHex:
        case ToolMode::DrawPoly:
        case ToolMode::DrawPixel:
        case ToolMode::EditShape:  setMouseCursor(juce::MouseCursor::CrosshairCursor); break;
    }
    repaint();
}

// ============================================================
// Selection (delegates to SelectionManager)
// ============================================================

void GridCanvas::setSelectedId(const std::string& id)
{
    if (id.empty())
        selMgr_.clear();
    else
        selMgr_.select(id);
}

void GridCanvas::selectionChanged()
{
    for (auto* l : canvasListeners_)
        l->selectionChanged();
    repaint();
}

void GridCanvas::deleteSelected()
{
    auto& ids = selMgr_.getSelectedIds();
    if (ids.empty()) return;

    if (ids.size() == 1) {
        undoMgr_.perform(std::make_unique<RemoveShapeAction>(*layout_, *ids.begin()));
    } else {
        undoMgr_.perform(std::make_unique<RemoveMultipleAction>(*layout_, ids));
    }
    selMgr_.clear();
}

void GridCanvas::duplicateSelected()
{
    auto& ids = selMgr_.getSelectedIds();
    if (ids.empty()) return;

    std::set<std::string> newIds;
    for (auto& id : ids) {
        auto* s = layout_->getShape(id);
        if (!s) continue;

        auto dup = s->clone();
        dup->id = nextShapeId();
        dup->x += 1.0f;
        dup->y += 1.0f;

        // Auto-assign unique note/CC so duplicates don't clash
        auto btype = behaviorFromString(dup->behavior);
        auto* obj = dup->behaviorParams.getDynamicObject();
        if (obj) {
            if (btype == BehaviorType::Trigger || btype == BehaviorType::Momentary || btype == BehaviorType::NotePad) {
                int oldNote = obj->hasProperty("note") ? (int)obj->getProperty("note") : 60;
                obj->setProperty("note", layout_->nextAvailableNote(oldNote + 1));
            }
            if (btype == BehaviorType::Fader) {
                int oldCC = obj->hasProperty("cc") ? (int)obj->getProperty("cc") : 1;
                obj->setProperty("cc", layout_->nextAvailableCC(oldCC + 1));
            }
            if (btype == BehaviorType::XYController) {
                int oldX = obj->hasProperty("cc_x") ? (int)obj->getProperty("cc_x") : 1;
                int newX = layout_->nextAvailableCC(oldX + 1);
                obj->setProperty("cc_x", newX);
                obj->setProperty("cc_y", layout_->nextAvailableCC(newX + 1));
            }
        }

        newIds.insert(dup->id);
        undoMgr_.perform(std::make_unique<AddShapeAction>(*layout_, std::move(dup)));
    }

    // Select the duplicated shapes
    selMgr_.clear();
    for (auto& id : newIds)
        selMgr_.addToSelection(id);
}

std::string GridCanvas::nextShapeId()
{
    return "shape_" + std::to_string(++shapeCounter_);
}

// ============================================================
// Coordinates
// ============================================================

juce::Point<float> GridCanvas::screenToGrid(juce::Point<float> screen) const
{
    float cellPx = Theme::CellSize * zoom_;
    return {(screen.x - panOffset_.x) / cellPx, (screen.y - panOffset_.y) / cellPx};
}

juce::Point<float> GridCanvas::gridToScreen(juce::Point<float> grid) const
{
    float cellPx = Theme::CellSize * zoom_;
    return {grid.x * cellPx + panOffset_.x, grid.y * cellPx + panOffset_.y};
}

juce::Rectangle<float> GridCanvas::gridCellToScreen(float gx, float gy, float gw, float gh) const
{
    auto tl = gridToScreen({gx, gy});
    auto br = gridToScreen({gx + gw, gy + gh});
    return {tl.x, tl.y, br.x - tl.x, br.y - tl.y};
}

// ============================================================
// Zoom
// ============================================================

void GridCanvas::setZoom(float z)
{
    zoom_ = juce::jlimit(Theme::MinZoom, Theme::MaxZoom, z);
    repaint();
}

void GridCanvas::zoomToFit()
{
    if (getWidth() <= 0 || getHeight() <= 0) return;
    float zx = getWidth() / Theme::CanvasW;
    float zy = getHeight() / Theme::CanvasH;
    zoom_ = juce::jlimit(Theme::MinZoom, Theme::MaxZoom, std::min(zx, zy) * 0.95f);
    float gridScreenW = Theme::CanvasW * zoom_;
    float gridScreenH = Theme::CanvasH * zoom_;
    panOffset_ = {(getWidth() - gridScreenW) / 2.0f, (getHeight() - gridScreenH) / 2.0f};
    repaint();
}

// ============================================================
// Paint / Erase
// ============================================================

void GridCanvas::paintAtScreen(juce::Point<float> screenPos)
{
    auto gp = screenToGrid(screenPos);
    int cx = (int)std::floor(gp.x), cy = (int)std::floor(gp.y);
    int half = brushSize_ / 2;
    for (int dy = -half; dy < brushSize_ - half; ++dy)
        for (int dx = -half; dx < brushSize_ - half; ++dx)
            paintPixel(cx + dx, cy + dy);
}

void GridCanvas::eraseAtScreen(juce::Point<float> screenPos)
{
    auto gp = screenToGrid(screenPos);
    int cx = (int)std::floor(gp.x), cy = (int)std::floor(gp.y);
    int half = brushSize_ / 2;
    for (int dy = -half; dy < brushSize_ - half; ++dy)
        for (int dx = -half; dx < brushSize_ - half; ++dx)
            erasePixel(cx + dx, cy + dy);
}

void GridCanvas::paintPixel(int gx, int gy)
{
    if (gx < 0 || gx >= Theme::GridW || gy < 0 || gy >= Theme::GridH) return;
    if (strokeCells_.count({gx, gy})) return;
    strokeCells_.insert({gx, gy});

    auto id = pixelId(gx, gy);
    if (auto* existing = layout_->getShape(id)) {
        existing->color = paintColor_;
        existing->colorActive = brighten(paintColor_);
        repaint();
        return;
    }
    auto shape = std::make_unique<RectShape>(id, (float)gx, (float)gy, 1.0f, 1.0f);
    shape->color = paintColor_;
    shape->colorActive = brighten(paintColor_);
    shape->behavior = "trigger";
    auto* obj = new juce::DynamicObject();
    obj->setProperty("note", layout_->nextAvailableNote(60));
    obj->setProperty("channel", 0);
    obj->setProperty("velocity", -1);
    shape->behaviorParams = juce::var(obj);
    // Paint pixels bypass undo for performance — too many per stroke
    layout_->addShape(std::move(shape));
}

void GridCanvas::erasePixel(int gx, int gy)
{
    if (gx < 0 || gx >= Theme::GridW || gy < 0 || gy >= Theme::GridH) return;
    if (strokeCells_.count({gx, gy})) return;
    strokeCells_.insert({gx, gy});
    layout_->removeShape(pixelId(gx, gy));
    if (auto* hit = layout_->hitTest(gx + 0.5f, gy + 0.5f))
        layout_->removeShape(hit->id);
}

// ============================================================
// Selection handles
// ============================================================

juce::Rectangle<float> GridCanvas::selectedBBoxScreen() const
{
    // Use the primary (single) selection for handles
    auto singleId = selMgr_.getSingleSelectedId();
    if (singleId.empty()) return {};
    auto* s = layout_->getShape(singleId);
    if (!s) return {};
    auto b = s->bbox();
    return gridCellToScreen(b.xMin, b.yMin, b.xMax - b.xMin, b.yMax - b.yMin);
}

std::vector<HandlePos> GridCanvas::allHandles() const
{
    return {HandlePos::TopLeft, HandlePos::Top, HandlePos::TopRight,
            HandlePos::Right, HandlePos::BottomRight, HandlePos::Bottom,
            HandlePos::BottomLeft, HandlePos::Left};
}

juce::Rectangle<float> GridCanvas::getHandleRect(HandlePos pos) const
{
    auto r = selectedBBoxScreen();
    if (r.isEmpty()) return {};
    float hs = HandleSize;
    float hh = hs / 2;

    switch (pos) {
        case HandlePos::TopLeft:     return {r.getX() - hh, r.getY() - hh, hs, hs};
        case HandlePos::Top:         return {r.getCentreX() - hh, r.getY() - hh, hs, hs};
        case HandlePos::TopRight:    return {r.getRight() - hh, r.getY() - hh, hs, hs};
        case HandlePos::Right:       return {r.getRight() - hh, r.getCentreY() - hh, hs, hs};
        case HandlePos::BottomRight: return {r.getRight() - hh, r.getBottom() - hh, hs, hs};
        case HandlePos::Bottom:      return {r.getCentreX() - hh, r.getBottom() - hh, hs, hs};
        case HandlePos::BottomLeft:  return {r.getX() - hh, r.getBottom() - hh, hs, hs};
        case HandlePos::Left:        return {r.getX() - hh, r.getCentreY() - hh, hs, hs};
        default: return {};
    }
}

HandlePos GridCanvas::hitTestHandle(juce::Point<float> screenPos) const
{
    if (selMgr_.count() != 1) return HandlePos::None;
    for (auto hp : allHandles()) {
        if (getHandleRect(hp).expanded(2).contains(screenPos))
            return hp;
    }
    return HandlePos::None;
}

// ============================================================
// Shape creation
// ============================================================

void GridCanvas::finishCreation()
{
    float x0 = snapToGrid(std::min(createStartGrid_.x, createEndGrid_.x));
    float y0 = snapToGrid(std::min(createStartGrid_.y, createEndGrid_.y));
    float x1 = snapToGrid(std::max(createStartGrid_.x, createEndGrid_.x));
    float y1 = snapToGrid(std::max(createStartGrid_.y, createEndGrid_.y));

    float w = x1 - x0, h = y1 - y0;
    if (w < 0.5f && h < 0.5f) { creating_ = false; return; }

    auto id = nextShapeId();
    std::unique_ptr<Shape> shape;

    switch (toolMode_) {
        case ToolMode::DrawRect: {
            if (w < 0.5f) w = 1;
            if (h < 0.5f) h = 1;
            shape = std::make_unique<RectShape>(id, x0, y0, w, h);
            break;
        }
        case ToolMode::DrawCircle: {
            float cx = (x0 + x1) / 2.0f, cy = (y0 + y1) / 2.0f;
            float r = std::max(w, h) / 2.0f;
            if (r < 0.5f) r = 0.5f;
            shape = std::make_unique<CircleShape>(id, cx, cy, r);
            break;
        }
        case ToolMode::DrawHex: {
            float cx = (x0 + x1) / 2.0f, cy = (y0 + y1) / 2.0f;
            float r = std::max(w, h) / 2.0f;
            if (r < 0.5f) r = 0.5f;
            shape = std::make_unique<HexShape>(id, cx, cy, r);
            break;
        }
        default: break;
    }

    if (shape) {
        shape->color = paintColor_;
        shape->colorActive = brighten(paintColor_);
        shape->behavior = "trigger";
        auto* obj = new juce::DynamicObject();
        obj->setProperty("note", layout_->nextAvailableNote(60));
        obj->setProperty("channel", 0);
        obj->setProperty("velocity", -1);
        shape->behaviorParams = juce::var(obj);
        undoMgr_.perform(std::make_unique<AddShapeAction>(*layout_, std::move(shape)));
        selMgr_.select(id);
    }
    creating_ = false;
}

// ============================================================
// Polygon creation
// ============================================================

void GridCanvas::finishPolygonCreation()
{
    if (polyVertices_.size() < 3) { cancelPolygonCreation(); return; }

    // Find origin (min x, min y)
    float minX = polyVertices_[0].x, minY = polyVertices_[0].y;
    for (auto& v : polyVertices_) {
        minX = std::min(minX, v.x);
        minY = std::min(minY, v.y);
    }

    // Build relative vertices
    std::vector<std::pair<float,float>> relVerts;
    for (auto& v : polyVertices_)
        relVerts.push_back({v.x - minX, v.y - minY});

    auto id = nextShapeId();
    auto shape = std::make_unique<PolygonShape>(id, minX, minY, std::move(relVerts));
    shape->color = paintColor_;
    shape->colorActive = brighten(paintColor_);
    shape->behavior = "trigger";
    auto* obj = new juce::DynamicObject();
    obj->setProperty("note", layout_->nextAvailableNote(60));
    obj->setProperty("channel", 0);
    obj->setProperty("velocity", -1);
    shape->behaviorParams = juce::var(obj);
    undoMgr_.perform(std::make_unique<AddShapeAction>(*layout_, std::move(shape)));
    selMgr_.select(id);

    polyVertices_.clear();
    creatingPoly_ = false;
    repaint();
}

void GridCanvas::cancelPolygonCreation()
{
    polyVertices_.clear();
    creatingPoly_ = false;
}

// ============================================================
// Pixel shape creation
// ============================================================

void GridCanvas::finishPixelCreation()
{
    if (pixelCells_.empty()) { cancelPixelCreation(); return; }

    // Find origin (min x, min y)
    auto it = pixelCells_.begin();
    int minX = it->first, minY = it->second;
    for (auto& [cx, cy] : pixelCells_) {
        minX = std::min(minX, cx);
        minY = std::min(minY, cy);
    }

    // Build relative cells
    std::vector<std::pair<int,int>> relCells;
    for (auto& [cx, cy] : pixelCells_)
        relCells.push_back({cx - minX, cy - minY});

    auto id = nextShapeId();
    auto shape = std::make_unique<PixelShape>(id, (float)minX, (float)minY, std::move(relCells));
    shape->color = paintColor_;
    shape->colorActive = brighten(paintColor_);
    shape->behavior = "trigger";
    auto* obj = new juce::DynamicObject();
    obj->setProperty("note", layout_->nextAvailableNote(60));
    obj->setProperty("channel", 0);
    obj->setProperty("velocity", -1);
    shape->behaviorParams = juce::var(obj);
    undoMgr_.perform(std::make_unique<AddShapeAction>(*layout_, std::move(shape)));
    selMgr_.select(id);

    pixelCells_.clear();
    pixelStrokeHistory_.clear();
    currentStroke_.clear();
    creatingPixelShape_ = false;
    repaint();
}

void GridCanvas::cancelPixelCreation()
{
    pixelCells_.clear();
    pixelStrokeHistory_.clear();
    currentStroke_.clear();
    creatingPixelShape_ = false;
}

void GridCanvas::undoPixelStroke()
{
    if (pixelStrokeHistory_.empty()) return;
    pixelStrokeHistory_.pop_back();
    // Rebuild pixelCells_ from remaining strokes
    pixelCells_.clear();
    for (auto& stroke : pixelStrokeHistory_)
        for (auto& cell : stroke)
            pixelCells_.insert(cell);
    repaint();
}

// ============================================================
// Edit-shape mode
// ============================================================

void GridCanvas::enterEditMode(const std::string& shapeId)
{
    auto* s = layout_->getShape(shapeId);
    if (!s) return;

    editingShapeId_ = shapeId;
    editOrigShape_ = s->clone();
    editConverted_ = false;
    editSymmetryH_ = false;
    editSymmetryV_ = false;

    // Load current pixels as absolute grid coords
    editCells_.clear();
    for (auto& [cx, cy] : s->gridPixels())
        editCells_.insert({cx, cy});

    // Save initial state as first snapshot
    editSnapshots_.clear();
    editSnapshots_.push_back(editCells_);

    toolMode_ = ToolMode::EditShape;
    selMgr_.select(shapeId);
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
    repaint();
}

void GridCanvas::exitEditMode(bool commit)
{
    if (editingShapeId_.empty()) return;

    if (commit && editOrigShape_) {
        // Check if cells actually changed
        std::set<std::pair<int,int>> origPixels;
        for (auto& p : editOrigShape_->gridPixels())
            origPixels.insert(p);

        if (editCells_ != origPixels) {
            if (editCells_.empty()) {
                // All cells erased → revert to original
                layout_->replaceShape(editingShapeId_, editOrigShape_->clone());
            } else {
                // Build final PixelShape from edited cells
                auto it = editCells_.begin();
                int minX = it->first, minY = it->second;
                for (auto& [cx, cy] : editCells_) {
                    minX = std::min(minX, cx);
                    minY = std::min(minY, cy);
                }
                std::vector<std::pair<int,int>> relCells;
                for (auto& [cx, cy] : editCells_)
                    relCells.push_back({cx - minX, cy - minY});

                auto newShape = std::make_unique<PixelShape>(editingShapeId_, (float)minX, (float)minY, std::move(relCells));
                // Preserve visual properties from original
                newShape->color = editOrigShape_->color;
                newShape->colorActive = editOrigShape_->colorActive;
                newShape->behavior = editOrigShape_->behavior;
                newShape->behaviorParams = editOrigShape_->behaviorParams;
                newShape->zOrder = editOrigShape_->zOrder;
                newShape->visualStyle = editOrigShape_->visualStyle;
                newShape->visualParams = editOrigShape_->visualParams;

                undoMgr_.perform(std::make_unique<EditShapeAction>(
                    *layout_, editOrigShape_->clone(), std::move(newShape)));
            }
        }
    } else if (!commit) {
        // Revert: restore original shape
        if (editOrigShape_)
            layout_->replaceShape(editingShapeId_, editOrigShape_->clone());
    }

    editingShapeId_.clear();
    editOrigShape_.reset();
    editCells_.clear();
    editConverted_ = false;
    editDraggingHandle_ = HandlePos::None;
    editSnapshots_.clear();
    editSymmetryH_ = false;
    editSymmetryV_ = false;

    toolMode_ = ToolMode::Select;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

void GridCanvas::syncEditCellsToShape()
{
    if (editingShapeId_.empty() || editCells_.empty()) return;

    // Compute origin and relative cells
    auto it = editCells_.begin();
    int minX = it->first, minY = it->second;
    for (auto& [cx, cy] : editCells_) {
        minX = std::min(minX, cx);
        minY = std::min(minY, cy);
    }
    std::vector<std::pair<int,int>> relCells;
    for (auto& [cx, cy] : editCells_)
        relCells.push_back({cx - minX, cy - minY});

    auto* current = layout_->getShape(editingShapeId_);
    if (!current) return;

    // If not already a PixelShape, convert it
    if (current->type != ShapeType::Pixel) {
        auto newShape = std::make_unique<PixelShape>(editingShapeId_, (float)minX, (float)minY, std::move(relCells));
        newShape->color = current->color;
        newShape->colorActive = current->colorActive;
        newShape->behavior = current->behavior;
        newShape->behaviorParams = current->behaviorParams;
        newShape->zOrder = current->zOrder;
        newShape->visualStyle = current->visualStyle;
        newShape->visualParams = current->visualParams;
        layout_->replaceShape(editingShapeId_, std::move(newShape));
        editConverted_ = true;
    } else {
        // Update existing PixelShape in-place
        auto* pix = static_cast<PixelShape*>(current);
        pix->x = (float)minX;
        pix->y = (float)minY;
        pix->relCells = std::move(relCells);
        layout_->notifyListeners();
    }
}

void GridCanvas::editAddCell(int cx, int cy)
{
    if (cx < 0 || cx >= Theme::GridW || cy < 0 || cy >= Theme::GridH) return;
    editCells_.insert({cx, cy});

    if (!editCells_.empty() && (editSymmetryH_ || editSymmetryV_)) {
        // Compute bounding box center for mirror axis
        auto eit = editCells_.begin();
        int minX = eit->first, maxX = minX, minY = eit->second, maxY = minY;
        for (auto& [x, y] : editCells_) {
            minX = std::min(minX, x); maxX = std::max(maxX, x);
            minY = std::min(minY, y); maxY = std::max(maxY, y);
        }
        if (editSymmetryH_) {
            int mx = minX + maxX - cx;
            if (mx >= 0 && mx < Theme::GridW)
                editCells_.insert({mx, cy});
        }
        if (editSymmetryV_) {
            int my = minY + maxY - cy;
            if (my >= 0 && my < Theme::GridH)
                editCells_.insert({cx, my});
        }
        if (editSymmetryH_ && editSymmetryV_) {
            int mx = minX + maxX - cx;
            int my = minY + maxY - cy;
            if (mx >= 0 && mx < Theme::GridW && my >= 0 && my < Theme::GridH)
                editCells_.insert({mx, my});
        }
    }
}

void GridCanvas::editRemoveCell(int cx, int cy)
{
    editCells_.erase({cx, cy});

    if (!editCells_.empty() && (editSymmetryH_ || editSymmetryV_)) {
        auto eit = editCells_.begin();
        int minX = eit->first, maxX = minX, minY = eit->second, maxY = minY;
        for (auto& [x, y] : editCells_) {
            minX = std::min(minX, x); maxX = std::max(maxX, x);
            minY = std::min(minY, y); maxY = std::max(maxY, y);
        }
        if (editSymmetryH_)
            editCells_.erase({minX + maxX - cx, cy});
        if (editSymmetryV_)
            editCells_.erase({cx, minY + maxY - cy});
        if (editSymmetryH_ && editSymmetryV_)
            editCells_.erase({minX + maxX - cx, minY + maxY - cy});
    }
}

juce::Rectangle<float> GridCanvas::editBBoxScreen() const
{
    if (editCells_.empty()) return {};
    auto it = editCells_.begin();
    int minX = it->first, minY = it->second;
    int maxX = minX, maxY = minY;
    for (auto& [cx, cy] : editCells_) {
        minX = std::min(minX, cx);
        minY = std::min(minY, cy);
        maxX = std::max(maxX, cx);
        maxY = std::max(maxY, cy);
    }
    return gridCellToScreen((float)minX, (float)minY, (float)(maxX - minX + 1), (float)(maxY - minY + 1));
}

HandlePos GridCanvas::editHitTestHandle(juce::Point<float> screenPos) const
{
    auto bb = editBBoxScreen();
    if (bb.isEmpty()) return HandlePos::None;

    float hs = HandleSize;
    float hh = hs / 2;

    struct { HandlePos pos; float hx, hy; } handles[] = {
        {HandlePos::TopLeft,     bb.getX(),       bb.getY()},
        {HandlePos::Top,         bb.getCentreX(), bb.getY()},
        {HandlePos::TopRight,    bb.getRight(),   bb.getY()},
        {HandlePos::Right,       bb.getRight(),   bb.getCentreY()},
        {HandlePos::BottomRight, bb.getRight(),   bb.getBottom()},
        {HandlePos::Bottom,      bb.getCentreX(), bb.getBottom()},
        {HandlePos::BottomLeft,  bb.getX(),       bb.getBottom()},
        {HandlePos::Left,        bb.getX(),       bb.getCentreY()},
    };

    for (auto& h : handles) {
        auto hr = juce::Rectangle<float>(h.hx - hh, h.hy - hh, hs, hs).expanded(2);
        if (hr.contains(screenPos))
            return h.pos;
    }
    return HandlePos::None;
}

// ============================================================
// Rendering
// ============================================================

void GridCanvas::paint(juce::Graphics& g)
{
    g.fillAll(Theme::Colors::CanvasBg);
    drawGrid(g);
    drawShapes(g);
    drawHoverHighlight(g);
    drawFingerOverlay(g);
    drawSelection(g);
    drawCreationPreview(g);
    drawPolygonCreationPreview(g);
    drawPixelCreationPreview(g);
    drawEditModeOverlay(g);
    drawCursor(g);
    drawCoordinateReadout(g);
}

void GridCanvas::drawGrid(juce::Graphics& g)
{
    float cellPx = Theme::CellSize * zoom_;
    auto fullGrid = gridCellToScreen(0, 0, (float)Theme::GridW, (float)Theme::GridH);

    for (int gx = 1; gx < Theme::GridW; ++gx) {
        float sx = panOffset_.x + gx * cellPx;
        g.setColour((gx % 6 == 0) ? Theme::Colors::GridMajor : Theme::Colors::GridLine);
        g.drawLine(sx, fullGrid.getY(), sx, fullGrid.getBottom(), Theme::GridLineWidth);
    }
    for (int gy = 1; gy < Theme::GridH; ++gy) {
        float sy = panOffset_.y + gy * cellPx;
        g.setColour((gy % 6 == 0) ? Theme::Colors::GridMajor : Theme::Colors::GridLine);
        g.drawLine(fullGrid.getX(), sy, fullGrid.getRight(), sy, Theme::GridLineWidth);
    }

    g.setColour(Theme::Colors::GridBorder);
    g.drawRect(fullGrid, Theme::GridBorderWidth);
}

void GridCanvas::drawShapes(juce::Graphics& g)
{
    for (auto& shape : layout_->shapes())
        drawShape(g, *shape);
}

void GridCanvas::drawShape(juce::Graphics& g, const Shape& shape)
{
    auto col = shape.color.toJuceColour();
    auto style = visualStyleFromString(shape.visualStyle);

    WidgetState wstate;
    auto wit = widgetStates_.find(shape.id);
    if (wit != widgetStates_.end())
        wstate = wit->second;

    bool useWidget = (style != VisualStyle::Static);

    auto pixels = shape.gridPixels();

    if (useWidget) {
        auto cmds = WidgetRenderer::renderWidget(shape, wstate);
        for (auto& cmd : cmds) {
            auto cellRect = gridCellToScreen((float)cmd.x, (float)cmd.y, 1.0f, 1.0f);
            g.setColour(cmd.color.toJuceColour());
            g.fillRect(cellRect);
        }
    } else {
        g.setColour(col);
        for (auto& [px, py] : pixels) {
            auto cellRect = gridCellToScreen((float)px, (float)py, 1.0f, 1.0f);
            g.fillRect(cellRect);
        }
    }

    if (pixels.size() > 1) {
        auto borderCol = col.brighter(0.2f).withAlpha(0.5f);
        g.setColour(borderCol);
        auto bb = shape.bbox();
        auto screenBB = gridCellToScreen(bb.xMin, bb.yMin, bb.xMax - bb.xMin, bb.yMax - bb.yMin);
        g.drawRect(screenBB, 0.5f);
    }

    auto bb = shape.bbox();
    auto screenBB = gridCellToScreen(bb.xMin, bb.yMin, bb.xMax - bb.xMin, bb.yMax - bb.yMin);
    float minLabelSize = 40.0f;
    if (screenBB.getWidth() > minLabelSize && screenBB.getHeight() > 18.0f) {
        juce::String label;
        auto btype = behaviorFromString(shape.behavior);
        switch (btype) {
            case BehaviorType::Trigger:      label = "TRIG"; break;
            case BehaviorType::Momentary:    label = "MOM"; break;
            case BehaviorType::NotePad:      label = "MPE"; break;
            case BehaviorType::XYController: label = "XY"; break;
            case BehaviorType::Fader:        label = "FAD"; break;
        }

        float lum = col.getFloatRed() * 0.299f + col.getFloatGreen() * 0.587f + col.getFloatBlue() * 0.114f;
        auto textCol = (lum > 0.4f) ? juce::Colour(0x99000000) : juce::Colour(0x99ffffff);

        float fontSize = juce::jlimit(8.0f, 11.0f, screenBB.getHeight() * 0.35f);
        g.setFont(juce::Font(fontSize, juce::Font::bold));
        g.setColour(textCol);
        g.drawText(label, screenBB.toNearestInt(), juce::Justification::centred, false);
    }
}

void GridCanvas::drawHoverHighlight(juce::Graphics& g)
{
    if (hoveredId_.empty() || selMgr_.isSelected(hoveredId_)) return;
    if (toolMode_ != ToolMode::Select) return;

    auto* s = layout_->getShape(hoveredId_);
    if (!s) return;

    auto b = s->bbox();
    auto r = gridCellToScreen(b.xMin, b.yMin, b.xMax - b.xMin, b.yMax - b.yMin);

    g.setColour(Theme::Colors::AccentGlow);
    g.fillRect(r.expanded(2));
    g.setColour(Theme::Colors::Accent.withAlpha(0.5f));
    g.drawRect(r, 1.0f);
}

void GridCanvas::drawCoordinateReadout(juce::Graphics& g)
{
    if (cursorGrid_.x < 0 || cursorGrid_.y < 0) return;
    int gx = (int)std::floor(cursorGrid_.x);
    int gy = (int)std::floor(cursorGrid_.y);
    if (gx < 0 || gx >= Theme::GridW || gy < 0 || gy >= Theme::GridH) return;

    auto text = juce::String(gx) + ", " + juce::String(gy);
    g.setFont(juce::Font(Theme::FontSmall));
    g.setColour(Theme::Colors::TextDim.withAlpha(0.6f));
    g.drawText(text, 6, getHeight() - 18, 60, 16, juce::Justification::centredLeft, false);
}

void GridCanvas::drawSelection(juce::Graphics& g)
{
    auto& ids = selMgr_.getSelectedIds();
    if (ids.empty()) return;

    for (auto& id : ids) {
        auto* s = layout_->getShape(id);
        if (!s) continue;

        auto b = s->bbox();
        auto r = gridCellToScreen(b.xMin, b.yMin, b.xMax - b.xMin, b.yMax - b.yMin);

        g.setColour(Theme::Colors::SelectionFill);
        g.fillRect(r);
        g.setColour(Theme::Colors::Selection);
        g.drawRect(r, 1.5f);
    }

    // Draw handles only for single selection (not in edit mode — edit mode draws its own)
    if (ids.size() == 1 && editingShapeId_.empty()) {
        for (auto hp : allHandles()) {
            auto hr = getHandleRect(hp);
            g.setColour(Theme::Colors::HandleFill);
            g.fillRoundedRectangle(hr, 2.0f);
            g.setColour(Theme::Colors::HandleBorder);
            g.drawRoundedRectangle(hr, 2.0f, 1.0f);
        }
    }
}

void GridCanvas::drawCreationPreview(juce::Graphics& g)
{
    if (!creating_) return;

    float x0 = snapToGrid(std::min(createStartGrid_.x, createEndGrid_.x));
    float y0 = snapToGrid(std::min(createStartGrid_.y, createEndGrid_.y));
    float x1 = snapToGrid(std::max(createStartGrid_.x, createEndGrid_.x));
    float y1 = snapToGrid(std::max(createStartGrid_.y, createEndGrid_.y));
    float w = x1 - x0, h = y1 - y0;

    auto col = paintColor_.toJuceColour().withAlpha(0.35f);
    auto borderCol = Theme::Colors::Accent.withAlpha(0.9f);

    std::unique_ptr<Shape> tempShape;
    switch (toolMode_) {
        case ToolMode::DrawRect: {
            if (w < 0.5f) w = 1;
            if (h < 0.5f) h = 1;
            tempShape = std::make_unique<RectShape>("_preview", x0, y0, w, h);
            break;
        }
        case ToolMode::DrawCircle: {
            float cx = (x0 + x1) / 2.0f, cy = (y0 + y1) / 2.0f;
            float r = std::max(w, h) / 2.0f;
            if (r < 0.5f) r = 0.5f;
            tempShape = std::make_unique<CircleShape>("_preview", cx, cy, r);
            break;
        }
        case ToolMode::DrawHex: {
            float cx = (x0 + x1) / 2.0f, cy = (y0 + y1) / 2.0f;
            float r = std::max(w, h) / 2.0f;
            if (r < 0.5f) r = 0.5f;
            tempShape = std::make_unique<HexShape>("_preview", cx, cy, r);
            break;
        }
        default: break;
    }

    if (tempShape) {
        auto pixels = tempShape->gridPixels();
        g.setColour(col);
        for (auto& [px, py] : pixels) {
            auto cellRect = gridCellToScreen((float)px, (float)py, 1.0f, 1.0f);
            g.fillRect(cellRect);
        }
        g.setColour(borderCol);
        auto bb = tempShape->bbox();
        auto screenBB = gridCellToScreen(bb.xMin, bb.yMin, bb.xMax - bb.xMin, bb.yMax - bb.yMin);
        g.drawRect(screenBB, 1.5f);
    }
}

void GridCanvas::drawPolygonCreationPreview(juce::Graphics& g)
{
    if (!creatingPoly_ || polyVertices_.empty()) return;

    auto col = paintColor_.toJuceColour().withAlpha(0.35f);
    auto borderCol = Theme::Colors::Accent.withAlpha(0.9f);

    // Build path from vertices
    juce::Path path;
    auto firstScreen = gridToScreen(polyVertices_[0]);
    path.startNewSubPath(firstScreen.x, firstScreen.y);
    for (size_t i = 1; i < polyVertices_.size(); ++i) {
        auto p = gridToScreen(polyVertices_[i]);
        path.lineTo(p.x, p.y);
    }

    // If 3+ vertices, close and fill
    if (polyVertices_.size() >= 3) {
        juce::Path fillPath(path);
        fillPath.closeSubPath();
        g.setColour(col);
        g.fillPath(fillPath);
    }

    // Draw solid edges
    g.setColour(borderCol);
    g.strokePath(path, juce::PathStrokeType(1.5f));

    // Dashed rubber-band from last vertex to cursor
    auto lastScreen = gridToScreen(polyVertices_.back());
    auto cursorScreen = gridToScreen(polyRubberBand_);
    juce::Path rubberLine;
    rubberLine.startNewSubPath(lastScreen.x, lastScreen.y);
    rubberLine.lineTo(cursorScreen.x, cursorScreen.y);

    float dashes[] = {4.0f, 4.0f};
    juce::PathStrokeType dashStroke(1.0f);
    juce::Path dashedPath;
    dashStroke.createDashedStroke(dashedPath, rubberLine, dashes, 2);
    g.setColour(borderCol.withAlpha(0.6f));
    g.fillPath(dashedPath);

    // Vertex dots
    g.setColour(Theme::Colors::HandleFill);
    for (auto& v : polyVertices_) {
        auto sp = gridToScreen(v);
        g.fillEllipse(sp.x - 3, sp.y - 3, 6, 6);
    }
    g.setColour(Theme::Colors::HandleBorder);
    for (auto& v : polyVertices_) {
        auto sp = gridToScreen(v);
        g.drawEllipse(sp.x - 3, sp.y - 3, 6, 6, 1.0f);
    }
}

void GridCanvas::drawPixelCreationPreview(juce::Graphics& g)
{
    if (!creatingPixelShape_ && pixelCells_.empty()) return;

    auto col = paintColor_.toJuceColour().withAlpha(0.35f);
    g.setColour(col);
    for (auto& [cx, cy] : pixelCells_) {
        auto cellRect = gridCellToScreen((float)cx, (float)cy, 1.0f, 1.0f);
        g.fillRect(cellRect);
    }

    // Border around each cell
    auto borderCol = Theme::Colors::Accent.withAlpha(0.4f);
    g.setColour(borderCol);
    for (auto& [cx, cy] : pixelCells_) {
        auto cellRect = gridCellToScreen((float)cx, (float)cy, 1.0f, 1.0f);
        g.drawRect(cellRect, 0.5f);
    }
}

void GridCanvas::drawEditModeOverlay(juce::Graphics& g)
{
    if (editingShapeId_.empty()) return;

    // Draw each cell with a highlighted border
    auto cellCol = juce::Colour(100, 200, 255).withAlpha(0.15f);
    auto borderCol = Theme::Colors::Accent.withAlpha(0.6f);

    for (auto& [cx, cy] : editCells_) {
        auto cellRect = gridCellToScreen((float)cx, (float)cy, 1.0f, 1.0f);
        g.setColour(cellCol);
        g.fillRect(cellRect);
        g.setColour(borderCol);
        g.drawRect(cellRect, 0.5f);
    }

    // Draw bounding box with resize handles
    auto bb = editBBoxScreen();
    if (!bb.isEmpty()) {
        g.setColour(Theme::Colors::Selection);
        g.drawRect(bb, 1.5f);

        // Draw 8 resize handles
        float hs = HandleSize;
        float hh = hs / 2;
        auto drawHandle = [&](float hx, float hy) {
            auto hr = juce::Rectangle<float>(hx - hh, hy - hh, hs, hs);
            g.setColour(Theme::Colors::HandleFill);
            g.fillRoundedRectangle(hr, 2.0f);
            g.setColour(Theme::Colors::HandleBorder);
            g.drawRoundedRectangle(hr, 2.0f, 1.0f);
        };
        drawHandle(bb.getX(), bb.getY());                    // TopLeft
        drawHandle(bb.getCentreX(), bb.getY());              // Top
        drawHandle(bb.getRight(), bb.getY());                // TopRight
        drawHandle(bb.getRight(), bb.getCentreY());          // Right
        drawHandle(bb.getRight(), bb.getBottom());           // BottomRight
        drawHandle(bb.getCentreX(), bb.getBottom());         // Bottom
        drawHandle(bb.getX(), bb.getBottom());               // BottomLeft
        drawHandle(bb.getX(), bb.getCentreY());              // Left
    }

    // Symmetry axis indicators
    if (editSymmetryH_ || editSymmetryV_) {
        auto bb = editBBoxScreen();
        if (!bb.isEmpty()) {
            float dashes[] = {6.0f, 4.0f};
            juce::PathStrokeType dashStroke(1.0f);
            g.setColour(juce::Colour(255, 200, 50).withAlpha(0.5f));
            if (editSymmetryH_) {
                juce::Path hLine;
                hLine.startNewSubPath(bb.getCentreX(), bb.getY() - 8);
                hLine.lineTo(bb.getCentreX(), bb.getBottom() + 8);
                juce::Path dashedH;
                dashStroke.createDashedStroke(dashedH, hLine, dashes, 2);
                g.fillPath(dashedH);
            }
            if (editSymmetryV_) {
                juce::Path vLine;
                vLine.startNewSubPath(bb.getX() - 8, bb.getCentreY());
                vLine.lineTo(bb.getRight() + 8, bb.getCentreY());
                juce::Path dashedV;
                dashStroke.createDashedStroke(dashedV, vLine, dashes, 2);
                g.fillPath(dashedV);
            }
        }
    }

    // "Edit Mode" indicator text
    juce::String editLabel = "EDIT SHAPE (ESC to finish)";
    if (editSymmetryH_ || editSymmetryV_) {
        editLabel += "  Mirror:";
        if (editSymmetryH_) editLabel += " X";
        if (editSymmetryV_) editLabel += " Y";
    }
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.setColour(Theme::Colors::Accent.withAlpha(0.8f));
    g.drawText(editLabel, 6, 4, 320, 16, juce::Justification::centredLeft, false);
}

void GridCanvas::drawCursor(juce::Graphics& g)
{
    if (toolMode_ != ToolMode::Paint && toolMode_ != ToolMode::Erase
        && toolMode_ != ToolMode::DrawPixel && toolMode_ != ToolMode::EditShape) return;
    if (cursorGrid_.x < 0 || cursorGrid_.y < 0) return;

    int cx = (int)std::floor(cursorGrid_.x);
    int cy = (int)std::floor(cursorGrid_.y);
    int bs = (toolMode_ == ToolMode::EditShape) ? 1 : brushSize_;
    int half = bs / 2;

    auto cursorCol = (toolMode_ == ToolMode::Paint || toolMode_ == ToolMode::DrawPixel
                      || toolMode_ == ToolMode::EditShape)
                     ? paintColor_.toJuceColour().withAlpha(0.3f)
                     : Theme::Colors::Error.withAlpha(0.25f);

    for (int dy = -half; dy < bs - half; ++dy) {
        for (int dx = -half; dx < bs - half; ++dx) {
            int px = cx + dx, py = cy + dy;
            if (px < 0 || px >= Theme::GridW || py < 0 || py >= Theme::GridH) continue;
            auto r = gridCellToScreen((float)px, (float)py);
            g.setColour(cursorCol);
            g.fillRect(r);
            g.setColour(cursorCol.withAlpha(0.8f));
            g.drawRect(r, 1.0f);
        }
    }
}

juce::Path GridCanvas::hexPath(float cx, float cy, float radius) const
{
    juce::Path path;
    for (int i = 0; i < 6; ++i) {
        float angle = (float)(i * 60) * (3.14159265f / 180.0f);
        float gx = cx + radius * std::cos(angle);
        float gy = cy + radius * std::sin(angle);
        auto p = gridToScreen({gx, gy});
        if (i == 0) path.startNewSubPath(p.x, p.y);
        else        path.lineTo(p.x, p.y);
    }
    path.closeSubPath();
    return path;
}

// ============================================================
// Mouse handling
// ============================================================

void GridCanvas::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();

    // Middle-click or Ctrl+click: pan (any tool)
    if (e.mods.isMiddleButtonDown() ||
        (e.mods.isLeftButtonDown() && e.mods.isCtrlDown() && !e.mods.isShiftDown())) {
        panning_ = true;
        panStart_ = e.position;
        panOffsetStart_ = panOffset_;
        return;
    }

    auto gridPos = screenToGrid(e.position);

    if (e.mods.isLeftButtonDown()) {
        switch (toolMode_) {
            // ---- SELECT ----
            case ToolMode::Select: {
                // Check if clicking a resize handle (single selection only)
                if (selMgr_.count() == 1) {
                    auto hp = hitTestHandle(e.position);
                    if (hp != HandlePos::None) {
                        draggingHandle_ = hp;
                        dragStartGrid_ = gridPos;
                        currentDragId_ = ++dragIdCounter_;
                        auto singleId = selMgr_.getSingleSelectedId();
                        auto* s = layout_->getShape(singleId);
                        if (s) {
                            auto b = s->bbox();
                            dragStartX_ = b.xMin; dragStartY_ = b.yMin;
                            dragStartW_ = b.xMax - b.xMin; dragStartH_ = b.yMax - b.yMin;
                            if (s->type == ShapeType::Circle)
                                dragStartR_ = static_cast<CircleShape*>(s)->radius;
                            else if (s->type == ShapeType::Hex)
                                dragStartR_ = static_cast<HexShape*>(s)->radius;
                        }
                        return;
                    }
                }

                // Check if clicking on a shape
                auto* hit = layout_->hitTest(gridPos.x, gridPos.y);
                if (hit) {
                    if (e.mods.isShiftDown()) {
                        // Shift+click: toggle in multi-selection
                        selMgr_.toggleSelection(hit->id);
                    } else if (!selMgr_.isSelected(hit->id)) {
                        // Click on unselected shape: select it alone
                        selMgr_.select(hit->id);
                    }
                    // Start drag for all selected
                    draggingShape_ = true;
                    dragStartGrid_ = gridPos;
                    currentDragId_ = ++dragIdCounter_;

                    // Record origins for all selected shapes
                    dragOrigins_.clear();
                    for (auto& id : selMgr_.getSelectedIds()) {
                        auto* s = layout_->getShape(id);
                        if (s) dragOrigins_[id] = {s->x, s->y};
                    }
                    // For single-selection handle compat
                    if (selMgr_.count() == 1) {
                        auto* s = layout_->getShape(selMgr_.getSingleSelectedId());
                        if (s) { dragStartX_ = s->x; dragStartY_ = s->y; }
                    }
                } else {
                    if (!e.mods.isShiftDown())
                        selMgr_.clear();
                }
                break;
            }

            // ---- PAINT / ERASE ----
            case ToolMode::Paint:
                painting_ = true;
                strokeCells_.clear();
                paintAtScreen(e.position);
                break;
            case ToolMode::Erase:
                painting_ = true;
                strokeCells_.clear();
                eraseAtScreen(e.position);
                break;

            // ---- DRAW SHAPE ----
            case ToolMode::DrawRect:
            case ToolMode::DrawCircle:
            case ToolMode::DrawHex:
                creating_ = true;
                createStartGrid_ = gridPos;
                createEndGrid_ = gridPos;
                break;

            // ---- DRAW POLYGON ----
            case ToolMode::DrawPoly: {
                auto snapped = juce::Point<float>(snapToGrid(gridPos.x), snapToGrid(gridPos.y));
                // Double-click with 3+ vertices → finish
                if (e.getNumberOfClicks() >= 2 && polyVertices_.size() >= 3) {
                    finishPolygonCreation();
                } else {
                    polyVertices_.push_back(snapped);
                    polyRubberBand_ = snapped;
                    creatingPoly_ = true;
                    repaint();
                }
                break;
            }

            // ---- DRAW PIXEL ----
            case ToolMode::DrawPixel: {
                creatingPixelShape_ = true;
                pixelErasing_ = false;
                currentStroke_.clear();
                // Paint cell at cursor
                int cx = (int)std::floor(gridPos.x), cy = (int)std::floor(gridPos.y);
                int half = brushSize_ / 2;
                for (int dy = -half; dy < brushSize_ - half; ++dy) {
                    for (int dx = -half; dx < brushSize_ - half; ++dx) {
                        int px = cx + dx, py = cy + dy;
                        if (px >= 0 && px < Theme::GridW && py >= 0 && py < Theme::GridH) {
                            std::pair<int,int> cell = {px, py};
                            pixelCells_.insert(cell);
                            currentStroke_.insert(cell);
                        }
                    }
                }
                repaint();
                break;
            }

            // ---- EDIT SHAPE ----
            case ToolMode::EditShape: {
                if (editingShapeId_.empty()) break;

                // Check resize handles first
                auto hp = editHitTestHandle(e.position);
                if (hp != HandlePos::None) {
                    editDraggingHandle_ = hp;
                    dragStartGrid_ = gridPos;
                    currentDragId_ = ++dragIdCounter_;
                    // Use edit cells bounding box
                    if (!editCells_.empty()) {
                        auto eit = editCells_.begin();
                        int minX = eit->first, minY = eit->second;
                        int maxX = minX, maxY = minY;
                        for (auto& [cx, cy] : editCells_) {
                            minX = std::min(minX, cx);
                            minY = std::min(minY, cy);
                            maxX = std::max(maxX, cx);
                            maxY = std::max(maxY, cy);
                        }
                        dragStartX_ = (float)minX; dragStartY_ = (float)minY;
                        dragStartW_ = (float)(maxX - minX + 1); dragStartH_ = (float)(maxY - minY + 1);
                    }
                    break;
                }

                // Check if click is far outside the edit bbox → exit
                {
                    auto ebb = editBBoxScreen();
                    if (!ebb.isEmpty() && !ebb.expanded(Theme::CellSize * zoom_ * 3).contains(e.position)) {
                        exitEditMode(true);
                        break;
                    }
                }

                // Left-click: add cell (with symmetry)
                int ecx = (int)std::floor(gridPos.x), ecy = (int)std::floor(gridPos.y);
                if (ecx >= 0 && ecx < Theme::GridW && ecy >= 0 && ecy < Theme::GridH) {
                    editAddCell(ecx, ecy);
                    syncEditCellsToShape();
                }
                break;
            }
        }
    }
    // Right-click erase in EditShape mode (with symmetry)
    else if (e.mods.isRightButtonDown() && toolMode_ == ToolMode::EditShape) {
        if (!editingShapeId_.empty()) {
            int ecx = (int)std::floor(gridPos.x), ecy = (int)std::floor(gridPos.y);
            editRemoveCell(ecx, ecy);
            syncEditCellsToShape();
        }
    }
    // Right-click in Select mode → context menu
    else if (e.mods.isRightButtonDown() && toolMode_ == ToolMode::Select) {
        auto* hit = layout_->hitTest(gridPos.x, gridPos.y);
        if (hit) {
            auto shapeId = hit->id;
            juce::PopupMenu menu;
            menu.addItem(1, "Edit Shape");
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(
                juce::Rectangle<int>((int)e.getScreenX(), (int)e.getScreenY(), 1, 1)),
                [this, shapeId](int result) {
                    if (result == 1)
                        enterEditMode(shapeId);
                });
        }
    }
    // Right-click erase in paint mode
    else if (e.mods.isRightButtonDown() && toolMode_ == ToolMode::Paint) {
        painting_ = true;
        strokeCells_.clear();
        eraseAtScreen(e.position);
    }
    // Right-click erase in DrawPixel mode
    else if (e.mods.isRightButtonDown() && toolMode_ == ToolMode::DrawPixel) {
        pixelErasing_ = true;
        currentStroke_.clear();
        int cx = (int)std::floor(gridPos.x), cy = (int)std::floor(gridPos.y);
        int half = brushSize_ / 2;
        for (int dy = -half; dy < brushSize_ - half; ++dy) {
            for (int dx = -half; dx < brushSize_ - half; ++dx) {
                int px = cx + dx, py = cy + dy;
                std::pair<int,int> cell = {px, py};
                pixelCells_.erase(cell);
                currentStroke_.insert(cell);
            }
        }
        repaint();
    }
}

void GridCanvas::mouseDrag(const juce::MouseEvent& e)
{
    if (panning_) {
        panOffset_ = panOffsetStart_ + (e.position - panStart_);
        repaint();
        return;
    }

    auto gridPos = screenToGrid(e.position);
    cursorGrid_ = gridPos;

    // Edit-shape handle resize drag
    if (editDraggingHandle_ != HandlePos::None && !editingShapeId_.empty()) {
        float dx = gridPos.x - dragStartGrid_.x;
        float dy = gridPos.y - dragStartGrid_.y;

        // Compute new bounding box from handle drag
        float nx = dragStartX_, ny = dragStartY_;
        float nw = dragStartW_, nh = dragStartH_;

        switch (editDraggingHandle_) {
            case HandlePos::TopLeft:     nx += dx; ny += dy; nw -= dx; nh -= dy; break;
            case HandlePos::Top:         ny += dy; nh -= dy; break;
            case HandlePos::TopRight:    ny += dy; nw += dx; nh -= dy; break;
            case HandlePos::Right:       nw += dx; break;
            case HandlePos::BottomRight: nw += dx; nh += dy; break;
            case HandlePos::Bottom:      nh += dy; break;
            case HandlePos::BottomLeft:  nx += dx; nw -= dx; nh += dy; break;
            case HandlePos::Left:        nx += dx; nw -= dx; break;
            default: break;
        }

        // Snap and enforce minimums
        nx = snapToGrid(nx); ny = snapToGrid(ny);
        nw = snapToGrid(nw); nh = snapToGrid(nh);
        if (nw < 1.0f) nw = 1.0f;
        if (nh < 1.0f) nh = 1.0f;

        // Scale editCells_ to fit the new bounding box
        if (!editCells_.empty()) {
            auto oit = editCells_.begin();
            int oldMinX = oit->first, oldMinY = oit->second;
            int oldMaxX = oldMinX, oldMaxY = oldMinY;
            for (auto& [cx, cy] : editCells_) {
                oldMinX = std::min(oldMinX, cx);
                oldMinY = std::min(oldMinY, cy);
                oldMaxX = std::max(oldMaxX, cx);
                oldMaxY = std::max(oldMaxY, cy);
            }
            float oldW = (float)(oldMaxX - oldMinX + 1);
            float oldH = (float)(oldMaxY - oldMinY + 1);

            if (oldW > 0 && oldH > 0) {
                float scaleX = nw / oldW;
                float scaleY = nh / oldH;

                std::set<std::pair<int,int>> newCells;
                for (auto& [cx, cy] : editCells_) {
                    float relX = (float)(cx - oldMinX);
                    float relY = (float)(cy - oldMinY);
                    int newCX = (int)nx + (int)std::floor(relX * scaleX);
                    int newCY = (int)ny + (int)std::floor(relY * scaleY);
                    if (newCX >= 0 && newCX < Theme::GridW && newCY >= 0 && newCY < Theme::GridH)
                        newCells.insert({newCX, newCY});
                }
                editCells_ = std::move(newCells);
                syncEditCellsToShape();
            }
        }
        repaint();
        return;
    }

    // Handle resize drag (single selection only)
    if (draggingHandle_ != HandlePos::None && selMgr_.count() == 1) {
        auto singleId = selMgr_.getSingleSelectedId();
        auto* s = layout_->getShape(singleId);
        if (!s) return;

        float dx = gridPos.x - dragStartGrid_.x;
        float dy = gridPos.y - dragStartGrid_.y;

        if (s->type == ShapeType::Rect) {
            float nx = dragStartX_, ny = dragStartY_;
            float nw = dragStartW_, nh = dragStartH_;

            switch (draggingHandle_) {
                case HandlePos::TopLeft:     nx += dx; ny += dy; nw -= dx; nh -= dy; break;
                case HandlePos::Top:         ny += dy; nh -= dy; break;
                case HandlePos::TopRight:    ny += dy; nw += dx; nh -= dy; break;
                case HandlePos::Right:       nw += dx; break;
                case HandlePos::BottomRight: nw += dx; nh += dy; break;
                case HandlePos::Bottom:      nh += dy; break;
                case HandlePos::BottomLeft:  nx += dx; nw -= dx; nh += dy; break;
                case HandlePos::Left:        nx += dx; nw -= dx; break;
                default: break;
            }
            if (nw < 1.0f) { nw = 1.0f; }
            if (nh < 1.0f) { nh = 1.0f; }
            undoMgr_.perform(std::make_unique<ResizeRectAction>(
                *layout_, singleId, snapToGrid(nx), snapToGrid(ny),
                snapToGrid(nw), snapToGrid(nh), currentDragId_));
        }
        else if (s->type == ShapeType::Circle) {
            float dist = std::sqrt(dx * dx + dy * dy);
            float newR = dragStartR_ + dist * ((dx + dy > 0) ? 1.0f : -1.0f);
            newR = std::max(0.5f, snapToGrid(newR * 2.0f) / 2.0f);
            undoMgr_.perform(std::make_unique<ResizeCircleAction>(
                *layout_, singleId, s->x, s->y, newR, currentDragId_));
        }
        else if (s->type == ShapeType::Hex) {
            float dist = std::sqrt(dx * dx + dy * dy);
            float newR = dragStartR_ + dist * ((dx + dy > 0) ? 1.0f : -1.0f);
            newR = std::max(0.5f, snapToGrid(newR * 2.0f) / 2.0f);
            undoMgr_.perform(std::make_unique<ResizeHexAction>(
                *layout_, singleId, s->x, s->y, newR, currentDragId_));
        }
        repaint();
        return;
    }

    // Shape drag (move) — applies to all selected shapes
    if (draggingShape_ && !selMgr_.isEmpty()) {
        float dx = gridPos.x - dragStartGrid_.x;
        float dy = gridPos.y - dragStartGrid_.y;

        if (selMgr_.count() == 1) {
            auto singleId = selMgr_.getSingleSelectedId();
            float newX = snapToGrid(dragOrigins_[singleId].x + dx);
            float newY = snapToGrid(dragOrigins_[singleId].y + dy);
            undoMgr_.perform(std::make_unique<MoveShapeAction>(
                *layout_, singleId, newX, newY, currentDragId_));
        } else {
            std::vector<MoveMultipleAction::ShapePos> moves;
            for (auto& [id, origin] : dragOrigins_) {
                float newX = snapToGrid(origin.x + dx);
                float newY = snapToGrid(origin.y + dy);
                moves.push_back({id, origin.x, origin.y, newX, newY});
            }
            undoMgr_.perform(std::make_unique<MoveMultipleAction>(*layout_, std::move(moves), currentDragId_));
        }
        return;
    }

    // Painting
    if (painting_) {
        if (e.mods.isLeftButtonDown() && toolMode_ == ToolMode::Paint)
            paintAtScreen(e.position);
        else if (e.mods.isLeftButtonDown() && toolMode_ == ToolMode::Erase)
            eraseAtScreen(e.position);
        else if (e.mods.isRightButtonDown())
            eraseAtScreen(e.position);
    }

    // Creating shape
    if (creating_)
        createEndGrid_ = gridPos;

    // Polygon rubber-band
    if (creatingPoly_)
        polyRubberBand_ = juce::Point<float>(snapToGrid(gridPos.x), snapToGrid(gridPos.y));

    // Edit-shape painting/erasing during drag (with symmetry)
    if (toolMode_ == ToolMode::EditShape && !editingShapeId_.empty() && editDraggingHandle_ == HandlePos::None) {
        int ecx = (int)std::floor(gridPos.x), ecy = (int)std::floor(gridPos.y);
        if (ecx >= 0 && ecx < Theme::GridW && ecy >= 0 && ecy < Theme::GridH) {
            if (e.mods.isLeftButtonDown()) {
                editAddCell(ecx, ecy);
                syncEditCellsToShape();
            } else if (e.mods.isRightButtonDown()) {
                editRemoveCell(ecx, ecy);
                syncEditCellsToShape();
            }
        }
    }

    // Pixel painting during drag
    if (toolMode_ == ToolMode::DrawPixel && (e.mods.isLeftButtonDown() || e.mods.isRightButtonDown())) {
        int cx = (int)std::floor(gridPos.x), cy = (int)std::floor(gridPos.y);
        int half = brushSize_ / 2;
        for (int dy = -half; dy < brushSize_ - half; ++dy) {
            for (int dx = -half; dx < brushSize_ - half; ++dx) {
                int px = cx + dx, py = cy + dy;
                if (px < 0 || px >= Theme::GridW || py < 0 || py >= Theme::GridH) continue;
                std::pair<int,int> cell = {px, py};
                if (pixelErasing_) {
                    pixelCells_.erase(cell);
                } else {
                    pixelCells_.insert(cell);
                }
                currentStroke_.insert(cell);
            }
        }
    }

    repaint();
}

void GridCanvas::mouseUp(const juce::MouseEvent&)
{
    panning_ = false;
    painting_ = false;
    draggingShape_ = false;
    draggingHandle_ = HandlePos::None;
    editDraggingHandle_ = HandlePos::None;
    strokeCells_.clear();
    dragOrigins_.clear();

    if (creating_)
        finishCreation();

    // Save pixel stroke to history
    if (toolMode_ == ToolMode::DrawPixel && !currentStroke_.empty()) {
        std::vector<std::pair<int,int>> strokeVec(currentStroke_.begin(), currentStroke_.end());
        if (pixelErasing_) {
            // For erase strokes, record a special erase entry
            // We just rebuild from history, so record the full cell set as a checkpoint
            pixelStrokeHistory_.clear();
            std::vector<std::pair<int,int>> checkpoint(pixelCells_.begin(), pixelCells_.end());
            pixelStrokeHistory_.push_back(std::move(checkpoint));
        } else {
            pixelStrokeHistory_.push_back(std::move(strokeVec));
        }
        currentStroke_.clear();
        pixelErasing_ = false;
    }

    // Save edit-shape snapshot for per-stroke undo
    if (toolMode_ == ToolMode::EditShape && !editingShapeId_.empty()) {
        if (editSnapshots_.empty() || editCells_ != editSnapshots_.back())
            editSnapshots_.push_back(editCells_);
    }
}

void GridCanvas::mouseMove(const juce::MouseEvent& e)
{
    cursorGrid_ = screenToGrid(e.position);

    if (toolMode_ == ToolMode::Select) {
        auto* hit = layout_->hitTest(cursorGrid_.x, cursorGrid_.y);
        auto newHovered = hit ? hit->id : std::string();
        if (newHovered != hoveredId_) {
            hoveredId_ = newHovered;
            repaint();
        }

        if (selMgr_.count() == 1) {
            auto hp = hitTestHandle(e.position);
            switch (hp) {
                case HandlePos::TopLeft:
                case HandlePos::BottomRight:
                    setMouseCursor(juce::MouseCursor::TopLeftCornerResizeCursor); break;
                case HandlePos::TopRight:
                case HandlePos::BottomLeft:
                    setMouseCursor(juce::MouseCursor::TopRightCornerResizeCursor); break;
                case HandlePos::Top:
                case HandlePos::Bottom:
                    setMouseCursor(juce::MouseCursor::UpDownResizeCursor); break;
                case HandlePos::Left:
                case HandlePos::Right:
                    setMouseCursor(juce::MouseCursor::LeftRightResizeCursor); break;
                case HandlePos::None:
                    setMouseCursor(hit ? juce::MouseCursor::PointingHandCursor
                                       : juce::MouseCursor::NormalCursor);
                    break;
            }
        } else {
            setMouseCursor(hit ? juce::MouseCursor::PointingHandCursor
                               : juce::MouseCursor::NormalCursor);
        }
    }

    if (toolMode_ == ToolMode::EditShape && !editingShapeId_.empty()) {
        // Show resize cursors over edit handles
        auto hp = editHitTestHandle(e.position);
        switch (hp) {
            case HandlePos::TopLeft:
            case HandlePos::BottomRight:
                setMouseCursor(juce::MouseCursor::TopLeftCornerResizeCursor); break;
            case HandlePos::TopRight:
            case HandlePos::BottomLeft:
                setMouseCursor(juce::MouseCursor::TopRightCornerResizeCursor); break;
            case HandlePos::Top:
            case HandlePos::Bottom:
                setMouseCursor(juce::MouseCursor::UpDownResizeCursor); break;
            case HandlePos::Left:
            case HandlePos::Right:
                setMouseCursor(juce::MouseCursor::LeftRightResizeCursor); break;
            case HandlePos::None:
                setMouseCursor(juce::MouseCursor::CrosshairCursor); break;
        }
        repaint();
    }

    if (toolMode_ == ToolMode::Paint || toolMode_ == ToolMode::Erase
        || toolMode_ == ToolMode::DrawPixel)
        repaint();

    if (creatingPoly_) {
        polyRubberBand_ = juce::Point<float>(snapToGrid(cursorGrid_.x), snapToGrid(cursorGrid_.y));
        repaint();
    }
}

void GridCanvas::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    auto gridPos = screenToGrid(e.position);
    float oldZoom = zoom_;
    float factor = (wheel.deltaY > 0) ? 1.1f : (1.0f / 1.1f);
    zoom_ = juce::jlimit(Theme::MinZoom, Theme::MaxZoom, zoom_ * factor);
    if (zoom_ != oldZoom) {
        auto newScreen = gridToScreen(gridPos);
        panOffset_ += (e.position - newScreen);
    }
    repaint();
}

bool GridCanvas::keyPressed(const juce::KeyPress& key)
{
    // Enter → finish poly/pixel creation
    if (key == juce::KeyPress::returnKey) {
        if (creatingPoly_ && polyVertices_.size() >= 3) { finishPolygonCreation(); return true; }
        if (creatingPixelShape_ && !pixelCells_.empty()) { finishPixelCreation(); return true; }
    }
    // Symmetry toggles in edit mode
    if (!editingShapeId_.empty()) {
        if (key.getTextCharacter() == 'x' || key.getTextCharacter() == 'X') {
            editSymmetryH_ = !editSymmetryH_;
            repaint();
            return true;
        }
        if (key.getTextCharacter() == 'y' || key.getTextCharacter() == 'Y') {
            editSymmetryV_ = !editSymmetryV_;
            repaint();
            return true;
        }
    }

    // Escape → exit edit mode or cancel poly/pixel creation
    if (key == juce::KeyPress::escapeKey) {
        if (!editingShapeId_.empty()) { exitEditMode(true); return true; }
        if (creatingPoly_)        { cancelPolygonCreation(); repaint(); return true; }
        if (creatingPixelShape_)  { cancelPixelCreation(); repaint(); return true; }
    }

    // Ctrl+Z in edit-shape mode → undo last stroke (per-stroke, before global undo)
    if (key.getModifiers().isCommandDown() && key.getKeyCode() == 'Z'
        && !key.getModifiers().isShiftDown() && !editingShapeId_.empty()) {
        if (editSnapshots_.size() > 1) {
            editSnapshots_.pop_back();
            editCells_ = editSnapshots_.back();
            syncEditCellsToShape();
            repaint();
        }
        return true;
    }

    // Ctrl+Z in pixel mode → undo stroke (session-local, before global undo)
    if (key.getModifiers().isCommandDown() && key.getKeyCode() == 'Z'
        && !key.getModifiers().isShiftDown() && creatingPixelShape_) {
        undoPixelStroke();
        return true;
    }

    // Undo/Redo
    if (key.getModifiers().isCommandDown() && key.getKeyCode() == 'Z') {
        if (key.getModifiers().isShiftDown())
            undoMgr_.redo();
        else
            undoMgr_.undo();
        return true;
    }
    // Clipboard
    if (key.getModifiers().isCommandDown() && key.getKeyCode() == 'C') {
        for (auto* l : canvasListeners_) l->copyRequested();
        return true;
    }
    if (key.getModifiers().isCommandDown() && key.getKeyCode() == 'X') {
        for (auto* l : canvasListeners_) l->cutRequested();
        return true;
    }
    if (key.getModifiers().isCommandDown() && key.getKeyCode() == 'V') {
        for (auto* l : canvasListeners_) l->pasteRequested();
        return true;
    }

    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) {
        deleteSelected();
        return true;
    }
    if (key.getModifiers().isCommandDown() && key.getKeyCode() == 'D') {
        duplicateSelected();
        return true;
    }
    if (key.getModifiers().isCommandDown() && key.getKeyCode() == 'A') {
        std::vector<std::string> allIds;
        for (auto& s : layout_->shapes())
            allIds.push_back(s->id);
        selMgr_.selectAll(allIds);
        return true;
    }

    // Tool shortcuts
    auto switchTool = [this](ToolMode m) {
        setToolMode(m);
        for (auto* l : canvasListeners_) l->toolModeChanged(m);
    };
    if (key.getTextCharacter() == 'v' || key.getTextCharacter() == 'V') { switchTool(ToolMode::Select); return true; }
    if (key.getTextCharacter() == 'b' || key.getTextCharacter() == 'B') { switchTool(ToolMode::Paint); return true; }
    if (key.getTextCharacter() == 'e' || key.getTextCharacter() == 'E') { switchTool(ToolMode::Erase); return true; }
    if (key.getTextCharacter() == 'r' || key.getTextCharacter() == 'R') { switchTool(ToolMode::DrawRect); return true; }
    if (key.getTextCharacter() == 'c' || key.getTextCharacter() == 'C') { switchTool(ToolMode::DrawCircle); return true; }
    if (key.getTextCharacter() == 'h' || key.getTextCharacter() == 'H') { switchTool(ToolMode::DrawHex); return true; }
    if (key.getTextCharacter() == 'p' || key.getTextCharacter() == 'P') { switchTool(ToolMode::DrawPoly); return true; }
    if (key.getTextCharacter() == 'g' || key.getTextCharacter() == 'G') { switchTool(ToolMode::DrawPixel); return true; }

    // Arrow keys: nudge all selected shapes
    if (!selMgr_.isEmpty()) {
        float step = key.getModifiers().isShiftDown() ? 5.0f : 1.0f;
        float dx = 0, dy = 0;
        if (key == juce::KeyPress::leftKey)  dx = -step;
        if (key == juce::KeyPress::rightKey) dx = step;
        if (key == juce::KeyPress::upKey)    dy = -step;
        if (key == juce::KeyPress::downKey)  dy = step;

        if (dx != 0 || dy != 0) {
            if (selMgr_.count() == 1) {
                auto id = selMgr_.getSingleSelectedId();
                auto* s = layout_->getShape(id);
                if (s) {
                    undoMgr_.perform(std::make_unique<MoveShapeAction>(
                        *layout_, id, s->x + dx, s->y + dy));
                }
            } else {
                std::vector<MoveMultipleAction::ShapePos> moves;
                for (auto& id : selMgr_.getSelectedIds()) {
                    auto* s = layout_->getShape(id);
                    if (s) moves.push_back({id, s->x, s->y, s->x + dx, s->y + dy});
                }
                undoMgr_.perform(std::make_unique<MoveMultipleAction>(*layout_, std::move(moves)));
            }
            return true;
        }
    }
    return false;
}

void GridCanvas::resized()
{
    static bool firstResize = true;
    if (firstResize && getWidth() > 0) {
        firstResize = false;
        zoomToFit();
    }
}

void GridCanvas::layoutChanged()
{
    repaint();
}

// ============================================================
// Finger overlay
// ============================================================

void GridCanvas::setFingers(const std::map<uint64_t, FingerDot>& fingers)
{
    if (fingers != fingers_) {
        fingers_ = fingers;
        repaint();
    }
}

void GridCanvas::setWidgetStates(const std::map<std::string, WidgetState>& states)
{
    if (states != widgetStates_) {
        widgetStates_ = states;
        repaint();
    }
}

void GridCanvas::setHighlightedShapes(const std::set<std::string>& ids)
{
    if (ids != highlightedShapes_) {
        highlightedShapes_ = ids;
        repaint();
    }
}

void GridCanvas::drawFingerOverlay(juce::Graphics& g)
{
    if (fingers_.empty()) return;

    // Draw DAW-highlighted shapes first (pulsing glow)
    if (!highlightedShapes_.empty()) {
        float phase = (float)(juce::Time::getMillisecondCounter() % 1000) / 1000.0f;
        float pulse = 0.2f + 0.15f * std::sin(phase * 6.2832f);

        for (auto& shapeId : highlightedShapes_) {
            auto* s = layout_->getShape(shapeId);
            if (!s) continue;
            auto b = s->bbox();
            auto r = gridCellToScreen(b.xMin, b.yMin, b.xMax - b.xMin, b.yMax - b.yMin);
            g.setColour(juce::Colour(255, 200, 50).withAlpha(pulse));
            g.fillRect(r);
            g.setColour(juce::Colour(255, 200, 50).withAlpha(pulse + 0.2f));
            g.drawRect(r, 2.0f);
        }
    }

    int fingerNum = 0;
    for (auto& [fid, dot] : fingers_) {
        auto screenPos = gridToScreen({dot.x, dot.y});
        float radius = 11.0f + dot.z * 5.0f;

        juce::Colour fingerCol = perFingerColors_
            ? FingerPalette::juceColorForFinger(fingerNum)
            : Theme::Colors::TextBright;
        juce::Colour glowCol = perFingerColors_
            ? fingerCol.withAlpha(0.25f)
            : Theme::Colors::Accent.withAlpha(0.25f);
        juce::Colour ringCol = perFingerColors_
            ? fingerCol.brighter(0.3f)
            : Theme::Colors::Accent;

        g.setColour(glowCol);
        g.fillEllipse(screenPos.x - radius - 3, screenPos.y - radius - 3,
                      (radius + 3) * 2, (radius + 3) * 2);

        g.setColour(fingerCol);
        g.fillEllipse(screenPos.x - radius, screenPos.y - radius,
                      radius * 2, radius * 2);

        g.setColour(ringCol);
        g.drawEllipse(screenPos.x - radius, screenPos.y - radius,
                      radius * 2, radius * 2, 1.5f);

        // Finger number label
        float lum = fingerCol.getFloatRed() * 0.299f
                  + fingerCol.getFloatGreen() * 0.587f
                  + fingerCol.getFloatBlue() * 0.114f;
        g.setColour(lum > 0.5f ? juce::Colours::black : juce::Colours::white);
        g.setFont(juce::Font(radius * 1.1f, juce::Font::bold));
        g.drawText(juce::String(fingerNum + 1),
                   (int)(screenPos.x - radius), (int)(screenPos.y - radius),
                   (int)(radius * 2), (int)(radius * 2),
                   juce::Justification::centred, false);
        ++fingerNum;
    }
}

} // namespace erae
