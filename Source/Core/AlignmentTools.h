#pragma once

#include "../Model/Layout.h"
#include <vector>
#include <string>
#include <algorithm>
#include <limits>

namespace erae {

struct AlignResult {
    std::string id;
    float newX, newY;
};

namespace AlignmentTools {

inline std::vector<AlignResult> alignLeft(Layout& layout, const std::set<std::string>& ids)
{
    float minX = std::numeric_limits<float>::max();
    for (auto& id : ids) {
        auto* s = layout.getShape(id);
        if (s) minX = std::min(minX, s->bbox().xMin);
    }

    std::vector<AlignResult> results;
    for (auto& id : ids) {
        auto* s = layout.getShape(id);
        if (!s) continue;
        float offsetX = s->bbox().xMin - s->x;
        float newX = minX - offsetX;
        results.push_back({id, newX, s->y});
    }
    return results;
}

inline std::vector<AlignResult> alignRight(Layout& layout, const std::set<std::string>& ids)
{
    float maxX = std::numeric_limits<float>::lowest();
    for (auto& id : ids) {
        auto* s = layout.getShape(id);
        if (s) maxX = std::max(maxX, s->bbox().xMax);
    }

    std::vector<AlignResult> results;
    for (auto& id : ids) {
        auto* s = layout.getShape(id);
        if (!s) continue;
        float offsetRight = s->bbox().xMax - s->x;
        float newX = maxX - offsetRight;
        results.push_back({id, newX, s->y});
    }
    return results;
}

inline std::vector<AlignResult> alignTop(Layout& layout, const std::set<std::string>& ids)
{
    float minY = std::numeric_limits<float>::max();
    for (auto& id : ids) {
        auto* s = layout.getShape(id);
        if (s) minY = std::min(minY, s->bbox().yMin);
    }

    std::vector<AlignResult> results;
    for (auto& id : ids) {
        auto* s = layout.getShape(id);
        if (!s) continue;
        float offsetY = s->bbox().yMin - s->y;
        float newY = minY - offsetY;
        results.push_back({id, s->x, newY});
    }
    return results;
}

inline std::vector<AlignResult> alignBottom(Layout& layout, const std::set<std::string>& ids)
{
    float maxY = std::numeric_limits<float>::lowest();
    for (auto& id : ids) {
        auto* s = layout.getShape(id);
        if (s) maxY = std::max(maxY, s->bbox().yMax);
    }

    std::vector<AlignResult> results;
    for (auto& id : ids) {
        auto* s = layout.getShape(id);
        if (!s) continue;
        float offsetBottom = s->bbox().yMax - s->y;
        float newY = maxY - offsetBottom;
        results.push_back({id, s->x, newY});
    }
    return results;
}

inline std::vector<AlignResult> alignCenterH(Layout& layout, const std::set<std::string>& ids)
{
    float sum = 0;
    int count = 0;
    for (auto& id : ids) {
        auto* s = layout.getShape(id);
        if (s) {
            auto b = s->bbox();
            sum += (b.xMin + b.xMax) / 2.0f;
            count++;
        }
    }
    if (count == 0) return {};
    float avgCX = sum / count;

    std::vector<AlignResult> results;
    for (auto& id : ids) {
        auto* s = layout.getShape(id);
        if (!s) continue;
        auto b = s->bbox();
        float shapeCX = (b.xMin + b.xMax) / 2.0f;
        float newX = s->x + (avgCX - shapeCX);
        results.push_back({id, newX, s->y});
    }
    return results;
}

inline std::vector<AlignResult> alignCenterV(Layout& layout, const std::set<std::string>& ids)
{
    float sum = 0;
    int count = 0;
    for (auto& id : ids) {
        auto* s = layout.getShape(id);
        if (s) {
            auto b = s->bbox();
            sum += (b.yMin + b.yMax) / 2.0f;
            count++;
        }
    }
    if (count == 0) return {};
    float avgCY = sum / count;

    std::vector<AlignResult> results;
    for (auto& id : ids) {
        auto* s = layout.getShape(id);
        if (!s) continue;
        auto b = s->bbox();
        float shapeCY = (b.yMin + b.yMax) / 2.0f;
        float newY = s->y + (avgCY - shapeCY);
        results.push_back({id, s->x, newY});
    }
    return results;
}

inline std::vector<AlignResult> distributeH(Layout& layout, const std::set<std::string>& ids)
{
    if (ids.size() < 3) return {};

    struct ShapeInfo { std::string id; float cx; float x, y; };
    std::vector<ShapeInfo> infos;
    for (auto& id : ids) {
        auto* s = layout.getShape(id);
        if (!s) continue;
        auto b = s->bbox();
        infos.push_back({id, (b.xMin + b.xMax) / 2.0f, s->x, s->y});
    }
    std::sort(infos.begin(), infos.end(), [](auto& a, auto& b) { return a.cx < b.cx; });

    float leftCX = infos.front().cx;
    float rightCX = infos.back().cx;
    float step = (rightCX - leftCX) / ((float)infos.size() - 1);

    std::vector<AlignResult> results;
    for (int i = 0; i < (int)infos.size(); ++i) {
        float targetCX = leftCX + i * step;
        float dx = targetCX - infos[i].cx;
        results.push_back({infos[i].id, infos[i].x + dx, infos[i].y});
    }
    return results;
}

inline std::vector<AlignResult> distributeV(Layout& layout, const std::set<std::string>& ids)
{
    if (ids.size() < 3) return {};

    struct ShapeInfo { std::string id; float cy; float x, y; };
    std::vector<ShapeInfo> infos;
    for (auto& id : ids) {
        auto* s = layout.getShape(id);
        if (!s) continue;
        auto b = s->bbox();
        infos.push_back({id, (b.yMin + b.yMax) / 2.0f, s->x, s->y});
    }
    std::sort(infos.begin(), infos.end(), [](auto& a, auto& b) { return a.cy < b.cy; });

    float topCY = infos.front().cy;
    float botCY = infos.back().cy;
    float step = (botCY - topCY) / ((float)infos.size() - 1);

    std::vector<AlignResult> results;
    for (int i = 0; i < (int)infos.size(); ++i) {
        float targetCY = topCY + i * step;
        float dy = targetCY - infos[i].cy;
        results.push_back({infos[i].id, infos[i].x, infos[i].y + dy});
    }
    return results;
}

} // namespace AlignmentTools
} // namespace erae
