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
    toolMode_ = mode;
    creating_ = false;
    painting_ = false;

    switch (mode) {
        case ToolMode::Select:     setMouseCursor(juce::MouseCursor::NormalCursor); break;
        case ToolMode::Paint:
        case ToolMode::Erase:      setMouseCursor(juce::MouseCursor::CrosshairCursor); break;
        case ToolMode::DrawRect:
        case ToolMode::DrawCircle:
        case ToolMode::DrawHex:    setMouseCursor(juce::MouseCursor::CrosshairCursor); break;
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

    // Draw handles only for single selection
    if (ids.size() == 1) {
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

void GridCanvas::drawCursor(juce::Graphics& g)
{
    if (toolMode_ != ToolMode::Paint && toolMode_ != ToolMode::Erase) return;
    if (cursorGrid_.x < 0 || cursorGrid_.y < 0) return;

    int cx = (int)std::floor(cursorGrid_.x);
    int cy = (int)std::floor(cursorGrid_.y);
    int half = brushSize_ / 2;

    auto cursorCol = (toolMode_ == ToolMode::Paint)
                     ? paintColor_.toJuceColour().withAlpha(0.3f)
                     : Theme::Colors::Error.withAlpha(0.25f);

    for (int dy = -half; dy < brushSize_ - half; ++dy) {
        for (int dx = -half; dx < brushSize_ - half; ++dx) {
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
        }
    }
    // Right-click erase in paint mode
    else if (e.mods.isRightButtonDown() && toolMode_ == ToolMode::Paint) {
        painting_ = true;
        strokeCells_.clear();
        eraseAtScreen(e.position);
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

    repaint();
}

void GridCanvas::mouseUp(const juce::MouseEvent&)
{
    panning_ = false;
    painting_ = false;
    draggingShape_ = false;
    draggingHandle_ = HandlePos::None;
    strokeCells_.clear();
    dragOrigins_.clear();

    if (creating_)
        finishCreation();
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

    if (toolMode_ == ToolMode::Paint || toolMode_ == ToolMode::Erase)
        repaint();
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
