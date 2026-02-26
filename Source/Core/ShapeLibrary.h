#pragma once

#include "../Model/Shape.h"
#include "../Model/Layout.h"
#include "../Model/Preset.h"
#include "UndoManager.h"
#include "LayoutActions.h"
#include <juce_core/juce_core.h>
#include <vector>
#include <string>
#include <memory>

namespace erae {

// ============================================================
// ShapeLibrary — persistent library of reusable shapes
// ============================================================
class ShapeLibrary {
public:
    struct LibraryEntry {
        std::string name;
        std::unique_ptr<Shape> shape;
        std::string description;  // short effect description (built-ins only)
    };

    ShapeLibrary() { populateBuiltins(); }

    int numEntries() const { return (int)entries_.size(); }
    int builtinCount() const { return builtinCount_; }
    bool isBuiltin(int index) const { return index >= 0 && index < builtinCount_; }

    const LibraryEntry& getEntry(int index) const { return entries_[(size_t)index]; }

    void addEntry(const std::string& name, const Shape* shape)
    {
        entries_.push_back({name, shape->clone()});
    }

    void removeEntry(int index)
    {
        if (index < builtinCount_) return;  // protect built-ins
        if (index >= 0 && index < (int)entries_.size())
            entries_.erase(entries_.begin() + index);
    }

    void populateBuiltins()
    {
        auto templates = Preset::effectTemplates();
        builtinCount_ = (int)templates.size();
        // Insert at front in order
        for (int i = 0; i < (int)templates.size(); ++i)
            entries_.insert(entries_.begin() + i,
                            LibraryEntry{std::move(templates[(size_t)i].name),
                                         std::move(templates[(size_t)i].shape),
                                         std::move(templates[(size_t)i].description)});
    }

    // Clone a library entry and place it on the canvas at (x, y)
    std::string placeOnCanvas(int index, Layout& layout, UndoManager& undoMgr,
                              float x, float y, int& shapeCounter)
    {
        if (index < 0 || index >= (int)entries_.size()) return {};
        auto clone = entries_[(size_t)index].shape->clone();
        clone->id = "shape_" + std::to_string(++shapeCounter);
        clone->x = x;
        clone->y = y;
        std::string id = clone->id;
        undoMgr.perform(std::make_unique<AddShapeAction>(layout, std::move(clone)));
        return id;
    }

    // Flip a shape horizontally around its center
    static void flipHorizontal(Shape& shape)
    {
        auto bb = shape.bbox();
        float centerX = (bb.xMin + bb.xMax) / 2.0f;

        if (shape.type == ShapeType::Polygon) {
            auto& poly = static_cast<PolygonShape&>(shape);
            for (auto& [vx, vy] : poly.relVertices) {
                float absX = shape.x + vx;
                float newAbsX = 2.0f * centerX - absX;
                vx = newAbsX - shape.x;
            }
        }
        else if (shape.type == ShapeType::Pixel) {
            auto& pix = static_cast<PixelShape&>(shape);
            if (pix.relCells.empty()) return;
            // Find min/max of relative cells
            int minRX = pix.relCells[0].first, maxRX = minRX;
            for (auto& [cx, cy] : pix.relCells) {
                minRX = std::min(minRX, cx);
                maxRX = std::max(maxRX, cx);
            }
            for (auto& [cx, cy] : pix.relCells)
                cx = maxRX - (cx - minRX);
        }
        // Rect, Circle, Hex — symmetric, no internal change needed
    }

    // Flip a shape vertically around its center
    static void flipVertical(Shape& shape)
    {
        auto bb = shape.bbox();
        float centerY = (bb.yMin + bb.yMax) / 2.0f;

        if (shape.type == ShapeType::Polygon) {
            auto& poly = static_cast<PolygonShape&>(shape);
            for (auto& [vx, vy] : poly.relVertices) {
                float absY = shape.y + vy;
                float newAbsY = 2.0f * centerY - absY;
                vy = newAbsY - shape.y;
            }
        }
        else if (shape.type == ShapeType::Pixel) {
            auto& pix = static_cast<PixelShape&>(shape);
            if (pix.relCells.empty()) return;
            int minRY = pix.relCells[0].second, maxRY = minRY;
            for (auto& [cx, cy] : pix.relCells) {
                minRY = std::min(minRY, cy);
                maxRY = std::max(maxRY, cy);
            }
            for (auto& [cx, cy] : pix.relCells)
                cy = maxRY - (cy - minRY);
        }
        // Rect, Circle, Hex — symmetric, no internal change needed
    }

    // Persistence — only saves user entries (skips built-ins)
    bool save(const juce::File& file) const
    {
        juce::Array<juce::var> arr;
        for (int i = builtinCount_; i < (int)entries_.size(); ++i) {
            auto obj = new juce::DynamicObject();
            obj->setProperty("name", juce::String(entries_[(size_t)i].name));
            obj->setProperty("shape", entries_[(size_t)i].shape->toVar());
            arr.add(juce::var(obj));
        }
        auto root = new juce::DynamicObject();
        root->setProperty("library", arr);
        return file.replaceWithText(juce::JSON::toString(juce::var(root)));
    }

    bool load(const juce::File& file)
    {
        entries_.clear();
        populateBuiltins();  // built-ins always at front

        if (!file.existsAsFile()) return false;
        auto parsed = juce::JSON::parse(file.loadFileAsString());
        if (!parsed.isObject()) return false;

        auto* libArr = parsed.getProperty("library", {}).getArray();
        if (!libArr) return false;

        for (auto& item : *libArr) {
            if (!item.isObject()) continue;
            auto name = item.getProperty("name", "").toString().toStdString();
            auto shapeVar = item.getProperty("shape", {});
            auto shape = parseShape(shapeVar);
            if (shape)
                entries_.push_back({name, std::move(shape)});
        }
        return true;
    }

    static juce::File getDefaultLibraryFile()
    {
        auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("EraeShapeEditor");
        dir.createDirectory();
        return dir.getChildFile("library.json");
    }

private:
    std::vector<LibraryEntry> entries_;
    int builtinCount_ = 0;

    // Parse a single shape from a var (reuses Preset.cpp logic inline)
    static std::unique_ptr<Shape> parseShape(const juce::var& item)
    {
        if (!item.isObject()) return nullptr;

        auto id   = item.getProperty("id", "").toString().toStdString();
        auto type = item.getProperty("type", "rect").toString().toStdString();
        float x   = (float)item.getProperty("x", 0.0);
        float y   = (float)item.getProperty("y", 0.0);

        std::unique_ptr<Shape> shape;

        if (type == "rect") {
            float w = (float)item.getProperty("width", 1.0);
            float h = (float)item.getProperty("height", 1.0);
            shape = std::make_unique<RectShape>(id, x, y, w, h);
        }
        else if (type == "circle") {
            float r = (float)item.getProperty("radius", 1.0);
            shape = std::make_unique<CircleShape>(id, x, y, r);
        }
        else if (type == "hex") {
            float r = (float)item.getProperty("radius", 1.0);
            shape = std::make_unique<HexShape>(id, x, y, r);
        }
        else if (type == "polygon") {
            std::vector<std::pair<float,float>> verts;
            if (auto* varr = item.getProperty("vertices", {}).getArray()) {
                for (auto& pt : *varr) {
                    if (auto* ptArr = pt.getArray(); ptArr && ptArr->size() >= 2)
                        verts.push_back({(float)(*ptArr)[0], (float)(*ptArr)[1]});
                }
            }
            shape = std::make_unique<PolygonShape>(id, x, y, std::move(verts));
        }
        else if (type == "pixel") {
            std::vector<std::pair<int,int>> cells;
            if (auto* carr = item.getProperty("cells", {}).getArray()) {
                for (auto& pt : *carr) {
                    if (auto* ptArr = pt.getArray(); ptArr && ptArr->size() >= 2)
                        cells.push_back({(int)(*ptArr)[0], (int)(*ptArr)[1]});
                }
            }
            shape = std::make_unique<PixelShape>(id, x, y, std::move(cells));
        }
        else {
            return nullptr;
        }

        // Restore common properties
        if (auto* colArr = item.getProperty("color", {}).getArray(); colArr && colArr->size() >= 3)
            shape->color = {(int)(*colArr)[0], (int)(*colArr)[1], (int)(*colArr)[2]};
        if (auto* colArr = item.getProperty("color_active", {}).getArray(); colArr && colArr->size() >= 3)
            shape->colorActive = {(int)(*colArr)[0], (int)(*colArr)[1], (int)(*colArr)[2]};
        shape->behavior = item.getProperty("behavior", "trigger").toString().toStdString();
        shape->behaviorParams = item.getProperty("behavior_params", {});
        shape->zOrder = (int)item.getProperty("z_order", 0);
        shape->visualStyle = item.getProperty("visual_style", "static").toString().toStdString();
        shape->visualParams = item.getProperty("visual_params", {});

        return shape;
    }
};

} // namespace erae
