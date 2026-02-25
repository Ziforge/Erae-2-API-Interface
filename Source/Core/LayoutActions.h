#pragma once

#include "UndoManager.h"
#include "AlignmentTools.h"
#include "../Model/Layout.h"
#include "../Model/Shape.h"
#include <juce_core/juce_core.h>
#include <memory>
#include <vector>

namespace erae {

// ============================================================
// AddShape
// ============================================================
class AddShapeAction : public UndoableAction {
public:
    AddShapeAction(Layout& layout, std::unique_ptr<Shape> shape)
        : layout_(layout), shape_(std::move(shape)), id_(shape_ ? shape_->id : "") {}

    void perform() override
    {
        if (shape_)
            layout_.addShape(std::move(shape_));
    }

    void undo() override
    {
        // Extract the shape back before removing
        shape_ = layout_.extractShape(id_);
    }

    std::string getName() const override { return "Add Shape"; }

private:
    Layout& layout_;
    std::unique_ptr<Shape> shape_;
    std::string id_;
};

// ============================================================
// RemoveShape
// ============================================================
class RemoveShapeAction : public UndoableAction {
public:
    RemoveShapeAction(Layout& layout, const std::string& id)
        : layout_(layout), id_(id) {}

    void perform() override
    {
        removed_ = layout_.extractShape(id_);
    }

    void undo() override
    {
        if (removed_)
            layout_.addShape(std::move(removed_));
    }

    std::string getName() const override { return "Delete Shape"; }

private:
    Layout& layout_;
    std::string id_;
    std::unique_ptr<Shape> removed_;
};

// ============================================================
// RemoveMultiple
// ============================================================
class RemoveMultipleAction : public UndoableAction {
public:
    RemoveMultipleAction(Layout& layout, const std::set<std::string>& ids)
        : layout_(layout), ids_(ids) {}

    void perform() override
    {
        removed_.clear();
        for (auto& id : ids_) {
            auto s = layout_.extractShape(id);
            if (s) removed_.push_back(std::move(s));
        }
    }

    void undo() override
    {
        for (auto& s : removed_)
            layout_.addShape(std::move(s));
        removed_.clear();
    }

    std::string getName() const override { return "Delete Shapes"; }

private:
    Layout& layout_;
    std::set<std::string> ids_;
    std::vector<std::unique_ptr<Shape>> removed_;
};

// ============================================================
// MoveShape (single) — supports drag coalescing
// ============================================================
class MoveShapeAction : public UndoableAction {
public:
    MoveShapeAction(Layout& layout, const std::string& id, float newX, float newY, int dragId = 0)
        : layout_(layout), id_(id), newX_(newX), newY_(newY), dragId_(dragId)
    {
        auto* s = layout.getShape(id);
        if (s) { oldX_ = s->x; oldY_ = s->y; }
    }

    void perform() override { layout_.moveShape(id_, newX_, newY_); }
    void undo() override { layout_.moveShape(id_, oldX_, oldY_); }
    std::string getName() const override { return "Move Shape"; }

    bool canCoalesceWith(const UndoableAction& other) const override
    {
        if (dragId_ == 0) return false;
        auto* o = dynamic_cast<const MoveShapeAction*>(&other);
        return o && o->dragId_ == dragId_ && o->id_ == id_;
    }

private:
    Layout& layout_;
    std::string id_;
    float newX_, newY_, oldX_ = 0, oldY_ = 0;
    int dragId_;
};

// ============================================================
// MoveMultiple — move multiple shapes by same delta, supports coalescing
// ============================================================
class MoveMultipleAction : public UndoableAction {
public:
    struct ShapePos { std::string id; float oldX, oldY, newX, newY; };

    MoveMultipleAction(Layout& layout, std::vector<ShapePos> moves, int dragId = 0)
        : layout_(layout), moves_(std::move(moves)), dragId_(dragId) {}

    void perform() override
    {
        for (auto& m : moves_)
            layout_.moveShape(m.id, m.newX, m.newY);
    }

    void undo() override
    {
        for (auto& m : moves_)
            layout_.moveShape(m.id, m.oldX, m.oldY);
    }

    std::string getName() const override { return "Move Shapes"; }

    bool canCoalesceWith(const UndoableAction& other) const override
    {
        if (dragId_ == 0) return false;
        auto* o = dynamic_cast<const MoveMultipleAction*>(&other);
        if (!o || o->dragId_ != dragId_ || o->moves_.size() != moves_.size()) return false;
        for (size_t i = 0; i < moves_.size(); ++i)
            if (moves_[i].id != o->moves_[i].id) return false;
        return true;
    }

private:
    Layout& layout_;
    std::vector<ShapePos> moves_;
    int dragId_;
};

// ============================================================
// ResizeRect
// ============================================================
class ResizeRectAction : public UndoableAction {
public:
    ResizeRectAction(Layout& layout, const std::string& id, float x, float y, float w, float h, int dragId = 0)
        : layout_(layout), id_(id), newX_(x), newY_(y), newW_(w), newH_(h), dragId_(dragId)
    {
        auto* s = layout.getShape(id);
        if (s && s->type == ShapeType::Rect) {
            auto* r = static_cast<RectShape*>(s);
            oldX_ = r->x; oldY_ = r->y; oldW_ = r->width; oldH_ = r->height;
        }
    }

    void perform() override { layout_.resizeRect(id_, newX_, newY_, newW_, newH_); }
    void undo() override { layout_.resizeRect(id_, oldX_, oldY_, oldW_, oldH_); }
    std::string getName() const override { return "Resize"; }

    bool canCoalesceWith(const UndoableAction& other) const override
    {
        if (dragId_ == 0) return false;
        auto* o = dynamic_cast<const ResizeRectAction*>(&other);
        return o && o->dragId_ == dragId_ && o->id_ == id_;
    }

private:
    Layout& layout_;
    std::string id_;
    float newX_, newY_, newW_, newH_;
    float oldX_ = 0, oldY_ = 0, oldW_ = 1, oldH_ = 1;
    int dragId_;
};

// ============================================================
// ResizeCircle
// ============================================================
class ResizeCircleAction : public UndoableAction {
public:
    ResizeCircleAction(Layout& layout, const std::string& id, float cx, float cy, float r, int dragId = 0)
        : layout_(layout), id_(id), newCX_(cx), newCY_(cy), newR_(r), dragId_(dragId)
    {
        auto* s = layout.getShape(id);
        if (s && s->type == ShapeType::Circle) {
            auto* c = static_cast<CircleShape*>(s);
            oldCX_ = c->x; oldCY_ = c->y; oldR_ = c->radius;
        }
    }

    void perform() override { layout_.resizeCircle(id_, newCX_, newCY_, newR_); }
    void undo() override { layout_.resizeCircle(id_, oldCX_, oldCY_, oldR_); }
    std::string getName() const override { return "Resize"; }

    bool canCoalesceWith(const UndoableAction& other) const override
    {
        if (dragId_ == 0) return false;
        auto* o = dynamic_cast<const ResizeCircleAction*>(&other);
        return o && o->dragId_ == dragId_ && o->id_ == id_;
    }

private:
    Layout& layout_;
    std::string id_;
    float newCX_, newCY_, newR_;
    float oldCX_ = 0, oldCY_ = 0, oldR_ = 1;
    int dragId_;
};

// ============================================================
// ResizeHex
// ============================================================
class ResizeHexAction : public UndoableAction {
public:
    ResizeHexAction(Layout& layout, const std::string& id, float cx, float cy, float r, int dragId = 0)
        : layout_(layout), id_(id), newCX_(cx), newCY_(cy), newR_(r), dragId_(dragId)
    {
        auto* s = layout.getShape(id);
        if (s && s->type == ShapeType::Hex) {
            auto* h = static_cast<HexShape*>(s);
            oldCX_ = h->x; oldCY_ = h->y; oldR_ = h->radius;
        }
    }

    void perform() override { layout_.resizeHex(id_, newCX_, newCY_, newR_); }
    void undo() override { layout_.resizeHex(id_, oldCX_, oldCY_, oldR_); }
    std::string getName() const override { return "Resize"; }

    bool canCoalesceWith(const UndoableAction& other) const override
    {
        if (dragId_ == 0) return false;
        auto* o = dynamic_cast<const ResizeHexAction*>(&other);
        return o && o->dragId_ == dragId_ && o->id_ == id_;
    }

private:
    Layout& layout_;
    std::string id_;
    float newCX_, newCY_, newR_;
    float oldCX_ = 0, oldCY_ = 0, oldR_ = 1;
    int dragId_;
};

// ============================================================
// SetColor
// ============================================================
class SetColorAction : public UndoableAction {
public:
    SetColorAction(Layout& layout, const std::string& id, Color7 newCol, Color7 newColActive)
        : layout_(layout), id_(id), newCol_(newCol), newColActive_(newColActive)
    {
        auto* s = layout.getShape(id);
        if (s) { oldCol_ = s->color; oldColActive_ = s->colorActive; }
    }

    void perform() override { layout_.setShapeColor(id_, newCol_, newColActive_); }
    void undo() override { layout_.setShapeColor(id_, oldCol_, oldColActive_); }
    std::string getName() const override { return "Change Color"; }

private:
    Layout& layout_;
    std::string id_;
    Color7 newCol_, newColActive_, oldCol_, oldColActive_;
};

// ============================================================
// SetBehavior
// ============================================================
class SetBehaviorAction : public UndoableAction {
public:
    SetBehaviorAction(Layout& layout, const std::string& id,
                      const std::string& newBeh, juce::var newParams)
        : layout_(layout), id_(id), newBeh_(newBeh), newParams_(newParams)
    {
        auto* s = layout.getShape(id);
        if (s) { oldBeh_ = s->behavior; oldParams_ = s->behaviorParams; }
    }

    void perform() override { layout_.setBehavior(id_, newBeh_, newParams_); }
    void undo() override { layout_.setBehavior(id_, oldBeh_, oldParams_); }
    std::string getName() const override { return "Change Behavior"; }

private:
    Layout& layout_;
    std::string id_;
    std::string newBeh_, oldBeh_;
    juce::var newParams_, oldParams_;
};

// ============================================================
// SetShapes (preset load) — replaces all shapes
// ============================================================
class SetShapesAction : public UndoableAction {
public:
    SetShapesAction(Layout& layout, std::vector<std::unique_ptr<Shape>> newShapes)
        : layout_(layout)
    {
        newShapes_ = std::move(newShapes);
    }

    void perform() override
    {
        // Save current state
        oldShapes_.clear();
        for (auto& s : layout_.shapes())
            oldShapes_.push_back(s->clone());
        layout_.setShapes(std::move(newShapes_));
        newShapes_.clear(); // moved out
    }

    void undo() override
    {
        // Save current (the "new" state) for redo
        newShapes_.clear();
        for (auto& s : layout_.shapes())
            newShapes_.push_back(s->clone());
        layout_.setShapes(std::move(oldShapes_));
        oldShapes_.clear();
    }

    std::string getName() const override { return "Load Preset"; }

private:
    Layout& layout_;
    std::vector<std::unique_ptr<Shape>> newShapes_;
    std::vector<std::unique_ptr<Shape>> oldShapes_;
};

// ============================================================
// AlignAction — applies alignment moves
// ============================================================
class AlignAction : public UndoableAction {
public:
    AlignAction(Layout& layout, std::vector<AlignResult> moves, const std::string& name)
        : layout_(layout), name_(name)
    {
        for (auto& m : moves) {
            auto* s = layout.getShape(m.id);
            if (!s) continue;
            moves_.push_back({m.id, s->x, s->y, m.newX, m.newY});
        }
    }

    void perform() override
    {
        for (auto& m : moves_)
            layout_.moveShape(m.id, m.newX, m.newY);
    }

    void undo() override
    {
        for (auto& m : moves_)
            layout_.moveShape(m.id, m.oldX, m.oldY);
    }

    std::string getName() const override { return name_; }

private:
    struct Move { std::string id; float oldX, oldY, newX, newY; };
    Layout& layout_;
    std::vector<Move> moves_;
    std::string name_;
};

} // namespace erae
