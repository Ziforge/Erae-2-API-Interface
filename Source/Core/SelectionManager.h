#pragma once

#include <set>
#include <string>
#include <vector>
#include <algorithm>

namespace erae {

class SelectionManager {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void selectionChanged() = 0;
    };

    SelectionManager() = default;

    void select(const std::string& id)
    {
        selectedIds_.clear();
        selectedIds_.insert(id);
        notify();
    }

    void addToSelection(const std::string& id)
    {
        selectedIds_.insert(id);
        notify();
    }

    void toggleSelection(const std::string& id)
    {
        if (selectedIds_.count(id))
            selectedIds_.erase(id);
        else
            selectedIds_.insert(id);
        notify();
    }

    void removeFromSelection(const std::string& id)
    {
        if (selectedIds_.erase(id))
            notify();
    }

    void selectAll(const std::vector<std::string>& allIds)
    {
        selectedIds_.clear();
        for (auto& id : allIds)
            selectedIds_.insert(id);
        notify();
    }

    void clear()
    {
        if (!selectedIds_.empty()) {
            selectedIds_.clear();
            notify();
        }
    }

    bool isSelected(const std::string& id) const { return selectedIds_.count(id) > 0; }
    bool isEmpty() const { return selectedIds_.empty(); }
    int count() const { return (int)selectedIds_.size(); }

    const std::set<std::string>& getSelectedIds() const { return selectedIds_; }

    // Returns the single selected ID, or empty if 0 or 2+ selected
    std::string getSingleSelectedId() const
    {
        if (selectedIds_.size() == 1)
            return *selectedIds_.begin();
        return {};
    }

    void addListener(Listener* l) { listeners_.push_back(l); }
    void removeListener(Listener* l)
    {
        listeners_.erase(std::remove(listeners_.begin(), listeners_.end(), l), listeners_.end());
    }

private:
    void notify()
    {
        for (auto* l : listeners_)
            l->selectionChanged();
    }

    std::set<std::string> selectedIds_;
    std::vector<Listener*> listeners_;
};

} // namespace erae
