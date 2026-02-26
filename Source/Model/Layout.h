#pragma once

#include "Shape.h"
#include "Behavior.h"
#include <vector>
#include <memory>
#include <algorithm>
#include <set>

namespace erae {

// ============================================================
// Layout — shape collection with z-ordered hit testing
// Ported from layout.py. Single source of truth for all shapes.
// ============================================================
class Layout {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void layoutChanged() = 0;
    };

    Layout(int width = 42, int height = 24) : gridWidth(width), gridHeight(height) {}

    void addShape(std::unique_ptr<Shape> shape);
    void removeShape(const std::string& id);
    std::unique_ptr<Shape> extractShape(const std::string& id); // remove + return (for undo)
    void moveShape(const std::string& id, float newX, float newY);
    void resizeRect(const std::string& id, float newX, float newY, float newW, float newH);
    void resizeCircle(const std::string& id, float newCX, float newCY, float newR);
    void resizeHex(const std::string& id, float newCX, float newCY, float newR);
    void replaceShape(const std::string& id, std::unique_ptr<Shape> newShape);
    void setShapeColor(const std::string& id, Color7 col, Color7 colActive);
    void setBehavior(const std::string& id, const std::string& behavior, juce::var params);
    Shape* hitTest(float x, float y) const;
    Shape* getShape(const std::string& id) const;
    const std::vector<std::unique_ptr<Shape>>& shapes() const { return shapes_; }
    int numShapes() const { return (int)shapes_.size(); }
    void clear();

    // Replace all shapes (used by preset loading)
    void setShapes(std::vector<std::unique_ptr<Shape>> newShapes);

    // Listener management
    void addListener(Listener* l) { listeners_.push_back(l); }
    void removeListener(Listener* l)
    {
        listeners_.erase(std::remove(listeners_.begin(), listeners_.end(), l), listeners_.end());
    }

    // JSON serialization
    juce::var toVar() const;

    int getGridWidth() const { return gridWidth; }
    int getGridHeight() const { return gridHeight; }

    // Auto-assignment helpers — find next unused note/CC across all shapes
    int nextAvailableNote(int startFrom = 60) const;
    int nextAvailableCC(int startFrom = 1) const;

    void notifyListeners();

private:
    void sortByZOrder();

    std::vector<std::unique_ptr<Shape>> shapes_;
    std::vector<Listener*> listeners_;
    int gridWidth, gridHeight;
};

} // namespace erae
