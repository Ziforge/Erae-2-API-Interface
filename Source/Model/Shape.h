#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <cmath>
#include <vector>
#include <memory>
#include <string>
#include "VisualStyle.h"

namespace erae {

enum class ShapeType { Rect, Circle, Hex, Polygon, Pixel };

// 7-bit RGB color for Erae II (0-127 per channel)
struct Color7 {
    int r = 0, g = 0, b = 0;

    constexpr Color7() = default;
    constexpr Color7(int r_, int g_, int b_) : r(r_), g(g_), b(b_) {}

    bool operator==(const Color7& o) const { return r == o.r && g == o.g && b == o.b; }
    bool operator!=(const Color7& o) const { return !(*this == o); }

    juce::Colour toJuceColour() const
    {
        return juce::Colour((juce::uint8)(r * 2), (juce::uint8)(g * 2), (juce::uint8)(b * 2));
    }
};

struct BBox {
    float xMin, yMin, xMax, yMax;
};

// ============================================================
// Shape base — all coordinates in grid units (Erae: 42×24)
// Origin is top-left. x: 0-41, y: 0-23.
// ============================================================
struct Shape {
    std::string id;
    ShapeType type;
    float x = 0, y = 0;        // reference point (top-left for rect, center for circle/hex)
    Color7 color    {0, 0, 127};
    Color7 colorActive {127, 127, 127};
    std::string behavior = "trigger";
    juce::var behaviorParams;   // JSON-like var for flexibility
    int zOrder = 0;
    std::string visualStyle = "static";
    juce::var visualParams;

    virtual ~Shape() = default;

    virtual BBox bbox() const = 0;
    virtual bool contains(float px, float py) const = 0;
    virtual std::vector<std::pair<int,int>> gridPixels() const = 0;
    virtual std::unique_ptr<Shape> clone() const = 0;

    virtual juce::var toVar() const
    {
        auto obj = new juce::DynamicObject();
        obj->setProperty("id", juce::String(id));
        obj->setProperty("type", juce::String(typeString()));
        obj->setProperty("x", x);
        obj->setProperty("y", y);
        juce::Array<juce::var> col;
        col.add(color.r); col.add(color.g); col.add(color.b);
        obj->setProperty("color", col);
        juce::Array<juce::var> colA;
        colA.add(colorActive.r); colA.add(colorActive.g); colA.add(colorActive.b);
        obj->setProperty("color_active", colA);
        obj->setProperty("behavior", juce::String(behavior));
        obj->setProperty("behavior_params", behaviorParams);
        obj->setProperty("z_order", zOrder);
        obj->setProperty("visual_style", juce::String(visualStyle));
        obj->setProperty("visual_params", visualParams);
        return juce::var(obj);
    }

    std::string typeString() const
    {
        switch (type) {
            case ShapeType::Rect:    return "rect";
            case ShapeType::Circle:  return "circle";
            case ShapeType::Hex:     return "hex";
            case ShapeType::Polygon: return "polygon";
            case ShapeType::Pixel:   return "pixel";
        }
        return "rect";
    }

protected:
    Shape(std::string id_, ShapeType t, float x_, float y_)
        : id(std::move(id_)), type(t), x(x_), y(y_) {}
    Shape(const Shape&) = default;
};

// ============================================================
// RectShape
// ============================================================
struct RectShape : Shape {
    float width = 1.0f, height = 1.0f;

    RectShape(std::string id_, float x_, float y_, float w, float h)
        : Shape(std::move(id_), ShapeType::Rect, x_, y_), width(w), height(h) {}

    BBox bbox() const override { return {x, y, x + width, y + height}; }

    bool contains(float px, float py) const override
    {
        return px >= x && px < x + width && py >= y && py < y + height;
    }

    std::vector<std::pair<int,int>> gridPixels() const override
    {
        std::vector<std::pair<int,int>> px;
        int x0 = (int)x, y0 = (int)y;
        int x1 = (int)(x + width), y1 = (int)(y + height);
        for (int gy = y0; gy < y1; ++gy)
            for (int gx = x0; gx < x1; ++gx)
                px.push_back({gx, gy});
        return px;
    }

    std::unique_ptr<Shape> clone() const override { return std::make_unique<RectShape>(*this); }

    juce::var toVar() const override
    {
        auto v = Shape::toVar();
        if (auto* obj = v.getDynamicObject()) {
            obj->setProperty("width", width);
            obj->setProperty("height", height);
        }
        return v;
    }
};

// ============================================================
// CircleShape
// ============================================================
struct CircleShape : Shape {
    float radius = 1.0f;

    CircleShape(std::string id_, float cx, float cy, float r)
        : Shape(std::move(id_), ShapeType::Circle, cx, cy), radius(r) {}

    BBox bbox() const override
    {
        return {x - radius, y - radius, x + radius, y + radius};
    }

    bool contains(float px, float py) const override
    {
        float dx = px - x, dy = py - y;
        return dx*dx + dy*dy <= radius*radius;
    }

    std::vector<std::pair<int,int>> gridPixels() const override
    {
        std::vector<std::pair<int,int>> px;
        int x0 = (int)(x - radius);
        int y0 = (int)(y - radius);
        int x1 = (int)(x + radius) + 1;
        int y1 = (int)(y + radius) + 1;
        float r2 = radius * radius;
        for (int gy = y0; gy < y1; ++gy)
            for (int gx = x0; gx < x1; ++gx) {
                float dx = gx + 0.5f - x;
                float dy = gy + 0.5f - y;
                if (dx*dx + dy*dy <= r2)
                    px.push_back({gx, gy});
            }
        return px;
    }

    std::unique_ptr<Shape> clone() const override { return std::make_unique<CircleShape>(*this); }

    juce::var toVar() const override
    {
        auto v = Shape::toVar();
        if (auto* obj = v.getDynamicObject())
            obj->setProperty("radius", radius);
        return v;
    }
};

// ============================================================
// HexShape — flat-top regular hexagon
// ============================================================
struct HexShape : Shape {
    float radius = 1.0f;

    HexShape(std::string id_, float cx, float cy, float r)
        : Shape(std::move(id_), ShapeType::Hex, cx, cy), radius(r) {}

    BBox bbox() const override
    {
        float h = radius * 0.866f; // sqrt(3)/2
        return {x - radius, y - h, x + radius, y + h};
    }

    std::vector<std::pair<float,float>> vertices() const
    {
        std::vector<std::pair<float,float>> v;
        for (int i = 0; i < 6; ++i) {
            float angle = (float)(i * 60) * (3.14159265f / 180.0f);
            v.push_back({x + radius * std::cos(angle), y + radius * std::sin(angle)});
        }
        return v;
    }

    bool contains(float px, float py) const override
    {
        return pointInPolygon(px, py, vertices());
    }

    std::vector<std::pair<int,int>> gridPixels() const override
    {
        auto verts = vertices();
        auto b = bbox();
        int x0 = (int)b.xMin, y0 = (int)b.yMin;
        int x1 = (int)b.xMax + 1, y1 = (int)b.yMax + 1;
        std::vector<std::pair<int,int>> px;
        for (int gy = y0; gy < y1; ++gy)
            for (int gx = x0; gx < x1; ++gx)
                if (pointInPolygon(gx + 0.5f, gy + 0.5f, verts))
                    px.push_back({gx, gy});
        return px;
    }

    std::unique_ptr<Shape> clone() const override { return std::make_unique<HexShape>(*this); }

    juce::var toVar() const override
    {
        auto v = Shape::toVar();
        if (auto* obj = v.getDynamicObject())
            obj->setProperty("radius", radius);
        return v;
    }

    static bool pointInPolygon(float px, float py,
                               const std::vector<std::pair<float,float>>& verts)
    {
        int n = (int)verts.size();
        bool inside = false;
        for (int i = 0, j = n - 1; i < n; j = i++) {
            float xi = verts[i].first, yi = verts[i].second;
            float xj = verts[j].first, yj = verts[j].second;
            if (((yi > py) != (yj > py)) &&
                (px < (xj - xi) * (py - yi) / (yj - yi) + xi))
                inside = !inside;
        }
        return inside;
    }
};

// ============================================================
// PolygonShape — arbitrary polygon with relative vertices
// ============================================================
struct PolygonShape : Shape {
    std::vector<std::pair<float,float>> relVertices; // relative to (x,y)

    PolygonShape(std::string id_, float x_, float y_,
                 std::vector<std::pair<float,float>> verts)
        : Shape(std::move(id_), ShapeType::Polygon, x_, y_), relVertices(std::move(verts)) {}

    std::vector<std::pair<float,float>> absVertices() const
    {
        std::vector<std::pair<float,float>> av;
        for (auto& [vx, vy] : relVertices)
            av.push_back({x + vx, y + vy});
        return av;
    }

    BBox bbox() const override
    {
        auto av = absVertices();
        if (av.empty()) return {x, y, x, y};
        float xMin = av[0].first, yMin = av[0].second;
        float xMax = xMin, yMax = yMin;
        for (auto& [px, py] : av) {
            xMin = std::min(xMin, px); yMin = std::min(yMin, py);
            xMax = std::max(xMax, px); yMax = std::max(yMax, py);
        }
        return {xMin, yMin, xMax, yMax};
    }

    bool contains(float px, float py) const override
    {
        return HexShape::pointInPolygon(px, py, absVertices());
    }

    std::vector<std::pair<int,int>> gridPixels() const override
    {
        auto av = absVertices();
        auto b = bbox();
        int x0 = (int)b.xMin, y0 = (int)b.yMin;
        int x1 = (int)b.xMax + 1, y1 = (int)b.yMax + 1;
        std::vector<std::pair<int,int>> px;
        for (int gy = y0; gy < y1; ++gy)
            for (int gx = x0; gx < x1; ++gx)
                if (HexShape::pointInPolygon(gx + 0.5f, gy + 0.5f, av))
                    px.push_back({gx, gy});
        return px;
    }

    juce::var toVar() const override
    {
        auto v = Shape::toVar();
        if (auto* obj = v.getDynamicObject()) {
            juce::Array<juce::var> verts;
            for (auto& [vx, vy] : relVertices) {
                juce::Array<juce::var> pt;
                pt.add(vx); pt.add(vy);
                verts.add(juce::var(pt));
            }
            obj->setProperty("vertices", verts);
        }
        return v;
    }

    std::unique_ptr<Shape> clone() const override { return std::make_unique<PolygonShape>(*this); }
};

// ============================================================
// PixelShape — freeform shape from painted grid cells
// ============================================================
struct PixelShape : Shape {
    std::vector<std::pair<int,int>> relCells; // relative to (x,y) origin

    PixelShape(std::string id_, float x_, float y_,
               std::vector<std::pair<int,int>> cells)
        : Shape(std::move(id_), ShapeType::Pixel, x_, y_), relCells(std::move(cells)) {}

    BBox bbox() const override
    {
        if (relCells.empty()) return {x, y, x + 1, y + 1};
        int minX = relCells[0].first, minY = relCells[0].second;
        int maxX = minX, maxY = minY;
        for (auto& [cx, cy] : relCells) {
            minX = std::min(minX, cx); minY = std::min(minY, cy);
            maxX = std::max(maxX, cx); maxY = std::max(maxY, cy);
        }
        return {x + minX, y + minY, x + maxX + 1.0f, y + maxY + 1.0f};
    }

    bool contains(float px, float py) const override
    {
        int cx = (int)std::floor(px - x);
        int cy = (int)std::floor(py - y);
        for (auto& [rx, ry] : relCells)
            if (rx == cx && ry == cy) return true;
        return false;
    }

    std::vector<std::pair<int,int>> gridPixels() const override
    {
        std::vector<std::pair<int,int>> px;
        int ox = (int)x, oy = (int)y;
        for (auto& [cx, cy] : relCells)
            px.push_back({ox + cx, oy + cy});
        return px;
    }

    juce::var toVar() const override
    {
        auto v = Shape::toVar();
        if (auto* obj = v.getDynamicObject()) {
            juce::Array<juce::var> cells;
            for (auto& [cx, cy] : relCells) {
                juce::Array<juce::var> pt;
                pt.add(cx); pt.add(cy);
                cells.add(juce::var(pt));
            }
            obj->setProperty("cells", cells);
        }
        return v;
    }

    std::unique_ptr<Shape> clone() const override { return std::make_unique<PixelShape>(*this); }
};

} // namespace erae
