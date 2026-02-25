#pragma once

#include <memory>
#include <vector>
#include <string>
#include <functional>

namespace erae {

// Command pattern base for undoable actions
class UndoableAction {
public:
    virtual ~UndoableAction() = default;
    virtual void perform() = 0;
    virtual void undo() = 0;
    virtual std::string getName() const = 0;

    // For drag coalescing: if this returns true, the new action replaces this one
    virtual bool canCoalesceWith(const UndoableAction&) const { return false; }
};

class UndoManager {
public:
    UndoManager() = default;

    // Perform an action and push it to the undo stack
    void perform(std::unique_ptr<UndoableAction> action)
    {
        action->perform();

        // Coalesce with top of undo stack if possible
        if (!undoStack_.empty() && undoStack_.back()->canCoalesceWith(*action)) {
            // Keep the original action's undo, but replace perform state
            // The new action has already been performed, so just swap it in
            undoStack_.back() = std::move(action);
        } else {
            undoStack_.push_back(std::move(action));
        }

        // Clear redo stack on new action
        redoStack_.clear();

        if (onStateChanged) onStateChanged();
    }

    bool canUndo() const { return !undoStack_.empty(); }
    bool canRedo() const { return !redoStack_.empty(); }

    std::string getUndoName() const
    {
        return undoStack_.empty() ? "" : undoStack_.back()->getName();
    }

    std::string getRedoName() const
    {
        return redoStack_.empty() ? "" : redoStack_.back()->getName();
    }

    void undo()
    {
        if (undoStack_.empty()) return;
        auto action = std::move(undoStack_.back());
        undoStack_.pop_back();
        action->undo();
        redoStack_.push_back(std::move(action));
        if (onStateChanged) onStateChanged();
    }

    void redo()
    {
        if (redoStack_.empty()) return;
        auto action = std::move(redoStack_.back());
        redoStack_.pop_back();
        action->perform();
        undoStack_.push_back(std::move(action));
        if (onStateChanged) onStateChanged();
    }

    void clear()
    {
        undoStack_.clear();
        redoStack_.clear();
        if (onStateChanged) onStateChanged();
    }

    std::function<void()> onStateChanged;

private:
    std::vector<std::unique_ptr<UndoableAction>> undoStack_;
    std::vector<std::unique_ptr<UndoableAction>> redoStack_;
};

} // namespace erae
