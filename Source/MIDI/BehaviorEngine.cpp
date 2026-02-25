#include "BehaviorEngine.h"
#include "../Erae/EraeSysEx.h"

namespace erae {

BehaviorEngine::BehaviorEngine(EraeMidiOut& midi, MPEAllocator& mpe)
    : midi_(midi), mpe_(mpe) {}

int BehaviorEngine::zToVelocity(float z)
{
    return juce::jlimit(1, 127, (int)(z * 127.0f));
}

int BehaviorEngine::zToPressure(float z)
{
    return juce::jlimit(0, 127, (int)(z * 127.0f));
}

std::pair<float, float> BehaviorEngine::normalizeInShape(float fx, float fy, const Shape& shape)
{
    auto b = shape.bbox();
    float w = b.xMax - b.xMin;
    float h = b.yMax - b.yMin;
    float nx = (w > 0) ? (fx - b.xMin) / w : 0.5f;
    float ny = (h > 0) ? (fy - b.yMin) / h : 0.5f;
    return {juce::jlimit(0.0f, 1.0f, nx), juce::jlimit(0.0f, 1.0f, ny)};
}

int BehaviorEngine::getParam(const Shape& shape, const juce::String& key, int defaultVal) const
{
    if (auto* obj = shape.behaviorParams.getDynamicObject())
        if (obj->hasProperty(key))
            return (int)obj->getProperty(key);
    return defaultVal;
}

bool BehaviorEngine::getParamBool(const Shape& shape, const juce::String& key, bool defaultVal) const
{
    if (auto* obj = shape.behaviorParams.getDynamicObject())
        if (obj->hasProperty(key))
            return (bool)obj->getProperty(key);
    return defaultVal;
}

void BehaviorEngine::handle(const FingerEvent& event, Shape* shape)
{
    if (!shape) return;

    auto btype = behaviorFromString(shape->behavior);

    if (event.action == SysEx::ACTION_DOWN) {
        FingerState fs;
        fs.fingerId = event.fingerId;
        fs.startX = event.x;
        fs.startY = event.y;
        fs.x = event.x;
        fs.y = event.y;
        fs.z = event.z;
        fs.shape = shape;
        activeFingers_[event.fingerId] = fs;
    }

    auto it = activeFingers_.find(event.fingerId);
    if (it == activeFingers_.end()) return;

    auto& fs = it->second;
    fs.x = event.x;
    fs.y = event.y;
    fs.z = event.z;

    switch (btype) {
        case BehaviorType::Trigger:      handleTrigger(event, shape, fs); break;
        case BehaviorType::Momentary:    handleMomentary(event, shape, fs); break;
        case BehaviorType::NotePad:      handleNotePad(event, shape, fs); break;
        case BehaviorType::XYController: handleXY(event, shape, fs); break;
        case BehaviorType::Fader:        handleFader(event, shape, fs); break;
    }

    if (event.action == SysEx::ACTION_UP)
        activeFingers_.erase(event.fingerId);
}

void BehaviorEngine::handleTrigger(const FingerEvent& event, Shape* shape, FingerState& fs)
{
    int note = getParam(*shape, "note", 60);
    int channel = getParam(*shape, "channel", 0);

    if (event.action == SysEx::ACTION_DOWN) {
        int vel = getParam(*shape, "velocity", -1);
        if (vel < 0) vel = zToVelocity(fs.z);
        midi_.noteOn(channel, note, vel);
    } else if (event.action == SysEx::ACTION_UP) {
        midi_.noteOff(channel, note);
    }
}

void BehaviorEngine::handleMomentary(const FingerEvent& event, Shape* shape, FingerState& fs)
{
    int note = getParam(*shape, "note", 60);
    int channel = getParam(*shape, "channel", 0);

    if (event.action == SysEx::ACTION_DOWN) {
        midi_.noteOn(channel, note, zToVelocity(fs.z));
    } else if (event.action == SysEx::ACTION_MOVE) {
        midi_.pressure(channel, zToPressure(fs.z));
    } else if (event.action == SysEx::ACTION_UP) {
        midi_.noteOff(channel, note);
    }
}

void BehaviorEngine::handleNotePad(const FingerEvent& event, Shape* shape, FingerState& fs)
{
    int note = getParam(*shape, "note", 60);

    if (event.action == SysEx::ACTION_DOWN) {
        int ch = mpe_.allocate(fs.fingerId);
        midi_.pitchBend(ch, 8192);
        midi_.noteOn(ch, note, zToVelocity(fs.z));
    } else if (event.action == SysEx::ACTION_MOVE) {
        int ch = mpe_.getChannel(fs.fingerId);
        if (ch < 0) return;
        auto b = shape->bbox();
        float shapeW = b.xMax - b.xMin;
        if (shapeW > 0) {
            float dxNorm = (fs.x - fs.startX) / shapeW;
            int pb = juce::jlimit(0, 16383, (int)(8192 + dxNorm * 8191.0f));
            midi_.pitchBend(ch, pb);
        }
        midi_.pressure(ch, zToPressure(fs.z));
    } else if (event.action == SysEx::ACTION_UP) {
        int ch = mpe_.getChannel(fs.fingerId);
        if (ch >= 0) {
            midi_.noteOff(ch, note);
            mpe_.release(fs.fingerId);
        }
    }
}

void BehaviorEngine::handleXY(const FingerEvent& event, Shape* shape, FingerState& fs)
{
    int ccX = getParam(*shape, "cc_x", 1);
    int ccY = getParam(*shape, "cc_y", 2);
    int channel = getParam(*shape, "channel", 0);

    if (event.action == SysEx::ACTION_DOWN || event.action == SysEx::ACTION_MOVE) {
        auto [nx, ny] = normalizeInShape(fs.x, fs.y, *shape);
        midi_.cc(channel, ccX, (int)(nx * 127.0f));
        midi_.cc(channel, ccY, (int)(ny * 127.0f));
    }
}

void BehaviorEngine::handleFader(const FingerEvent& event, Shape* shape, FingerState& fs)
{
    int ccNum = getParam(*shape, "cc", 1);
    int channel = getParam(*shape, "channel", 0);
    bool horizontal = getParamBool(*shape, "horizontal", false);

    if (event.action == SysEx::ACTION_DOWN || event.action == SysEx::ACTION_MOVE) {
        auto [nx, ny] = normalizeInShape(fs.x, fs.y, *shape);
        float value = horizontal ? nx : ny;
        midi_.cc(channel, ccNum, (int)(value * 127.0f));
    }
}

void BehaviorEngine::allNotesOff()
{
    for (auto& [fid, fs] : activeFingers_) {
        if (fs.shape) {
            auto btype = behaviorFromString(fs.shape->behavior);
            int note = getParam(*fs.shape, "note", 60);
            if (btype == BehaviorType::NotePad) {
                int ch = mpe_.getChannel(fid);
                if (ch >= 0) {
                    midi_.noteOff(ch, note);
                    mpe_.release(fid);
                }
            } else if (btype == BehaviorType::Trigger || btype == BehaviorType::Momentary) {
                int channel = getParam(*fs.shape, "channel", 0);
                midi_.noteOff(channel, note);
            }
        }
    }
    activeFingers_.clear();
    mpe_.releaseAll();
}

} // namespace erae
