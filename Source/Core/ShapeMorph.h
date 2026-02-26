#pragma once

#include "../Model/Shape.h"
#include <set>
#include <cmath>

namespace erae {

// Shape morphing — blends two shapes by interpolating their grid pixel sets
struct ShapeMorph {
    // Morph between two shapes at blend factor t (0.0 = shape A, 1.0 = shape B).
    // Returns a PixelShape with the morphed cells. Colors are also interpolated.
    static std::unique_ptr<PixelShape> morph(const Shape& a, const Shape& b,
                                              float t, const std::string& newId)
    {
        t = std::max(0.0f, std::min(1.0f, t));

        auto pixA = a.gridPixels();
        auto pixB = b.gridPixels();

        std::set<std::pair<int,int>> setA(pixA.begin(), pixA.end());
        std::set<std::pair<int,int>> setB(pixB.begin(), pixB.end());

        // Union of all cells
        std::set<std::pair<int,int>> allCells = setA;
        allCells.insert(setB.begin(), setB.end());

        // For each cell in the union, decide if it's included
        std::vector<std::pair<int,int>> resultCells;
        for (auto& [px, py] : allCells) {
            bool inA = setA.count({px, py}) > 0;
            bool inB = setB.count({px, py}) > 0;

            float score;
            if (inA && inB) {
                score = 1.0f; // always included
            } else if (inA) {
                score = 1.0f - t; // fades out as t→1
            } else {
                score = t; // fades in as t→1
            }

            if (score > 0.5f)
                resultCells.push_back({px, py});
            else if (std::abs(score - 0.5f) < 0.01f)
                resultCells.push_back({px, py}); // include at exact midpoint
        }

        if (resultCells.empty())
            return nullptr;

        // Compute origin (min x,y) and convert to relative cells
        int minX = resultCells[0].first, minY = resultCells[0].second;
        for (auto& [rx, ry] : resultCells) {
            minX = std::min(minX, rx);
            minY = std::min(minY, ry);
        }

        std::vector<std::pair<int,int>> relCells;
        for (auto& [rx, ry] : resultCells)
            relCells.push_back({rx - minX, ry - minY});

        auto result = std::make_unique<PixelShape>(newId, (float)minX, (float)minY, std::move(relCells));

        // Interpolate colors
        result->color = Color7{
            (int)std::round(a.color.r + t * (b.color.r - a.color.r)),
            (int)std::round(a.color.g + t * (b.color.g - a.color.g)),
            (int)std::round(a.color.b + t * (b.color.b - a.color.b))
        };
        result->colorActive = Color7{
            (int)std::round(a.colorActive.r + t * (b.colorActive.r - a.colorActive.r)),
            (int)std::round(a.colorActive.g + t * (b.colorActive.g - a.colorActive.g)),
            (int)std::round(a.colorActive.b + t * (b.colorActive.b - a.colorActive.b))
        };

        return result;
    }
};

} // namespace erae
