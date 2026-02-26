#include "Layout.h"

namespace erae {

void Layout::addShape(std::unique_ptr<Shape> shape)
{
    shapes_.push_back(std::move(shape));
    sortByZOrder();
    notifyListeners();
}

void Layout::removeShape(const std::string& id)
{
    shapes_.erase(
        std::remove_if(shapes_.begin(), shapes_.end(),
                       [&](const auto& s) { return s->id == id; }),
        shapes_.end());
    notifyListeners();
}

std::unique_ptr<Shape> Layout::extractShape(const std::string& id)
{
    std::unique_ptr<Shape> result;
    for (auto it = shapes_.begin(); it != shapes_.end(); ++it) {
        if ((*it)->id == id) {
            result = std::move(*it);
            shapes_.erase(it);
            notifyListeners();
            break;
        }
    }
    return result;
}

void Layout::replaceShape(const std::string& id, std::unique_ptr<Shape> newShape)
{
    for (auto& s : shapes_) {
        if (s->id == id) {
            s = std::move(newShape);
            sortByZOrder();
            notifyListeners();
            return;
        }
    }
}

void Layout::moveShape(const std::string& id, float newX, float newY)
{
    if (auto* s = getShape(id)) {
        s->x = newX;
        s->y = newY;
        notifyListeners();
    }
}

void Layout::resizeRect(const std::string& id, float newX, float newY, float newW, float newH)
{
    if (auto* s = getShape(id)) {
        if (s->type == ShapeType::Rect) {
            auto* r = static_cast<RectShape*>(s);
            r->x = newX; r->y = newY;
            r->width = std::max(0.5f, newW);
            r->height = std::max(0.5f, newH);
            notifyListeners();
        }
    }
}

void Layout::resizeCircle(const std::string& id, float newCX, float newCY, float newR)
{
    if (auto* s = getShape(id)) {
        if (s->type == ShapeType::Circle) {
            auto* c = static_cast<CircleShape*>(s);
            c->x = newCX; c->y = newCY;
            c->radius = std::max(0.25f, newR);
            notifyListeners();
        }
    }
}

void Layout::resizeHex(const std::string& id, float newCX, float newCY, float newR)
{
    if (auto* s = getShape(id)) {
        if (s->type == ShapeType::Hex) {
            auto* h = static_cast<HexShape*>(s);
            h->x = newCX; h->y = newCY;
            h->radius = std::max(0.25f, newR);
            notifyListeners();
        }
    }
}

void Layout::setShapeColor(const std::string& id, Color7 col, Color7 colActive)
{
    if (auto* s = getShape(id)) {
        s->color = col;
        s->colorActive = colActive;
        notifyListeners();
    }
}

void Layout::setBehavior(const std::string& id, const std::string& behavior, juce::var params)
{
    if (auto* s = getShape(id)) {
        s->behavior = behavior;
        s->behaviorParams = params;
        notifyListeners();
    }
}

Shape* Layout::hitTest(float x, float y) const
{
    // Iterate in reverse z-order (highest on top)
    for (auto it = shapes_.rbegin(); it != shapes_.rend(); ++it) {
        auto& s = *it;
        auto b = s->bbox();
        // Quick AABB rejection
        if (x < b.xMin || x > b.xMax || y < b.yMin || y > b.yMax)
            continue;
        if (s->contains(x, y))
            return s.get();
    }
    return nullptr;
}

Shape* Layout::getShape(const std::string& id) const
{
    for (auto& s : shapes_)
        if (s->id == id)
            return s.get();
    return nullptr;
}

void Layout::clear()
{
    shapes_.clear();
    notifyListeners();
}

void Layout::setShapes(std::vector<std::unique_ptr<Shape>> newShapes)
{
    shapes_ = std::move(newShapes);
    sortByZOrder();
    notifyListeners();
}

juce::var Layout::toVar() const
{
    juce::Array<juce::var> arr;
    for (auto& s : shapes_)
        arr.add(s->toVar());
    auto obj = new juce::DynamicObject();
    obj->setProperty("shapes", arr);
    return juce::var(obj);
}

int Layout::nextAvailableNote(int startFrom) const
{
    std::set<int> usedNotes;
    for (auto& s : shapes_) {
        auto btype = behaviorFromString(s->behavior);
        if (btype == BehaviorType::Trigger || btype == BehaviorType::Momentary || btype == BehaviorType::NotePad) {
            if (auto* obj = s->behaviorParams.getDynamicObject())
                if (obj->hasProperty("note"))
                    usedNotes.insert((int)obj->getProperty("note"));
        }
    }
    // Search from startFrom upward, wrapping around 0-127
    for (int i = 0; i < 128; ++i) {
        int candidate = (startFrom + i) % 128;
        if (usedNotes.find(candidate) == usedNotes.end())
            return candidate;
    }
    return startFrom; // all 128 used, just return startFrom
}

int Layout::nextAvailableCC(int startFrom) const
{
    std::set<int> usedCCs;
    for (auto& s : shapes_) {
        auto btype = behaviorFromString(s->behavior);
        if (btype == BehaviorType::Fader) {
            if (auto* obj = s->behaviorParams.getDynamicObject())
                if (obj->hasProperty("cc"))
                    usedCCs.insert((int)obj->getProperty("cc"));
        }
        if (btype == BehaviorType::XYController) {
            if (auto* obj = s->behaviorParams.getDynamicObject()) {
                if (obj->hasProperty("cc_x"))
                    usedCCs.insert((int)obj->getProperty("cc_x"));
                if (obj->hasProperty("cc_y"))
                    usedCCs.insert((int)obj->getProperty("cc_y"));
            }
        }
    }
    for (int i = 0; i < 127; ++i) {
        int candidate = ((startFrom - 1 + i) % 127) + 1; // range 1-127
        if (usedCCs.find(candidate) == usedCCs.end())
            return candidate;
    }
    return startFrom;
}

void Layout::sortByZOrder()
{
    std::stable_sort(shapes_.begin(), shapes_.end(),
                     [](const auto& a, const auto& b) { return a->zOrder < b->zOrder; });
}

void Layout::notifyListeners()
{
    for (auto* l : listeners_)
        l->layoutChanged();
}

} // namespace erae
