#pragma once

#include "../Model/Layout.h"
#include "../Model/Shape.h"
#include "UndoManager.h"
#include "LayoutActions.h"
#include "SelectionManager.h"
#include <vector>
#include <memory>

namespace erae {

class Clipboard {
public:
    Clipboard() = default;

    void copy(Layout& layout, const std::set<std::string>& ids)
    {
        buffer_.clear();
        for (auto& id : ids) {
            auto* s = layout.getShape(id);
            if (s) buffer_.push_back(s->clone());
        }
    }

    void cut(Layout& layout, UndoManager& undoMgr, SelectionManager& selMgr)
    {
        auto& ids = selMgr.getSelectedIds();
        copy(layout, ids);
        // Remove via undo
        if (ids.size() == 1) {
            undoMgr.perform(std::make_unique<RemoveShapeAction>(layout, *ids.begin()));
        } else if (ids.size() > 1) {
            undoMgr.perform(std::make_unique<RemoveMultipleAction>(layout, ids));
        }
        selMgr.clear();
    }

    void paste(Layout& layout, UndoManager& undoMgr, SelectionManager& selMgr, int& shapeCounter)
    {
        if (buffer_.empty()) return;

        selMgr.clear();
        for (auto& s : buffer_) {
            auto dup = s->clone();
            dup->id = "shape_" + std::to_string(++shapeCounter);
            dup->x += 1.0f; // offset so paste is visible
            dup->y += 1.0f;
            auto newId = dup->id;
            undoMgr.perform(std::make_unique<AddShapeAction>(layout, std::move(dup)));
            selMgr.addToSelection(newId);
        }
    }

    bool hasContent() const { return !buffer_.empty(); }

private:
    std::vector<std::unique_ptr<Shape>> buffer_;
};

} // namespace erae
