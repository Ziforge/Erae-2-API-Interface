#include "BehaviorEngine.h"
#include "../Erae/EraeSysEx.h"

namespace erae {

BehaviorEngine::BehaviorEngine(EraeMidiOut& midi, MPEAllocator& mpe)
    : midi_(midi), mpe_(mpe) {}

int BehaviorEngine::zToVelocity(float z, CurveType curve)
{
    return juce::jlimit(1, 127, (int)(applyCurve(z, curve) * 127.0f));
}

int BehaviorEngine::zToPressure(float z, CurveType curve)
{
    return juce::jlimit(0, 127, (int)(applyCurve(z, curve) * 127.0f));
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

float BehaviorEngine::getParamFloat(const Shape& shape, const juce::String& key, float defaultVal) const
{
    if (auto* obj = shape.behaviorParams.getDynamicObject())
        if (obj->hasProperty(key))
            return (float)(double)obj->getProperty(key);
    return defaultVal;
}

std::string BehaviorEngine::getParamString(const Shape& shape, const juce::String& key, const std::string& defaultVal) const
{
    if (auto* obj = shape.behaviorParams.getDynamicObject())
        if (obj->hasProperty(key))
            return obj->getProperty(key).toString().toStdString();
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
    auto velCurve = curveFromString(getParamString(*shape, "velocity_curve", "linear"));
    bool latch = getParamBool(*shape, "latch", false);

    if (latch) {
        if (event.action == SysEx::ACTION_DOWN) {
            bool& latched = latchedShapes_[shape->id];
            if (latched) {
                midi_.noteOff(channel, note);
                if (oscOut_) oscOut_->noteOff(channel, note);
                latched = false;
            } else {
                int vel = getParam(*shape, "velocity", -1);
                if (vel < 0) vel = zToVelocity(fs.z, velCurve);
                midi_.noteOn(channel, note, vel);
                if (oscOut_) oscOut_->noteOn(channel, note, vel);
                latched = true;
            }
        }
    } else {
        if (event.action == SysEx::ACTION_DOWN) {
            int vel = getParam(*shape, "velocity", -1);
            if (vel < 0) vel = zToVelocity(fs.z, velCurve);
            midi_.noteOn(channel, note, vel);
            if (oscOut_) oscOut_->noteOn(channel, note, vel);
        } else if (event.action == SysEx::ACTION_UP) {
            midi_.noteOff(channel, note);
            if (oscOut_) oscOut_->noteOff(channel, note);
        }
    }
}

void BehaviorEngine::handleMomentary(const FingerEvent& event, Shape* shape, FingerState& fs)
{
    int note = getParam(*shape, "note", 60);
    int channel = getParam(*shape, "channel", 0);
    auto velCurve = curveFromString(getParamString(*shape, "velocity_curve", "linear"));
    auto pressCurve = curveFromString(getParamString(*shape, "pressure_curve", "linear"));

    if (event.action == SysEx::ACTION_DOWN) {
        midi_.noteOn(channel, note, zToVelocity(fs.z, velCurve));
    } else if (event.action == SysEx::ACTION_MOVE) {
        midi_.pressure(channel, zToPressure(fs.z, pressCurve));
    } else if (event.action == SysEx::ACTION_UP) {
        midi_.noteOff(channel, note);
    }
}

void BehaviorEngine::handleNotePad(const FingerEvent& event, Shape* shape, FingerState& fs)
{
    int note = getParam(*shape, "note", 60);
    int slideCC = getParam(*shape, "slide_cc", 74);
    auto velCurve = curveFromString(getParamString(*shape, "velocity_curve", "linear"));
    auto pressCurve = curveFromString(getParamString(*shape, "pressure_curve", "linear"));
    auto scaleType = scaleFromString(getParamString(*shape, "scale", "chromatic"));
    int rootNote = getParam(*shape, "root_note", 0);
    bool pitchQuantize = getParamBool(*shape, "pitch_quantize", false);
    float glideAmount = getParamFloat(*shape, "glide_amount", 0.0f);
    int pbRange = getParam(*shape, "pitchbend_range", 2);

    if (event.action == SysEx::ACTION_DOWN) {
        int ch = mpe_.allocate(fs.fingerId);
        midi_.pitchBend(ch, 8192);
        auto [nx, ny] = normalizeInShape(fs.x, fs.y, *shape);
        midi_.cc(ch, slideCC, (int)(ny * 127.0f));
        midi_.noteOn(ch, note, zToVelocity(fs.z, velCurve));
    } else if (event.action == SysEx::ACTION_MOVE) {
        int ch = mpe_.getChannel(fs.fingerId);
        if (ch < 0) return;

        // Pitch bend from X-slide
        auto b = shape->bbox();
        float shapeW = b.xMax - b.xMin;
        if (shapeW > 0) {
            float dxNorm = (fs.x - fs.startX) / shapeW;
            int pb = juce::jlimit(0, 16383, (int)(8192 + dxNorm * 8191.0f));

            // Apply scale quantization to pitch bend if enabled
            if (pitchQuantize && scaleType != ScaleType::Chromatic) {
                pb = quantizePitchBend(pb, note, rootNote, scaleType, pbRange, glideAmount);
            }

            midi_.pitchBend(ch, pb);
        }

        // Y-slide as CC
        auto [nx, ny] = normalizeInShape(fs.x, fs.y, *shape);
        midi_.cc(ch, slideCC, (int)(ny * 127.0f));
        // Pressure (Z)
        midi_.pressure(ch, zToPressure(fs.z, pressCurve));
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
    bool highres = getParamBool(*shape, "highres", false);
    int ccXMin = getParam(*shape, "cc_x_min", 0);
    int ccXMax = getParam(*shape, "cc_x_max", highres ? 16383 : 127);
    int ccYMin = getParam(*shape, "cc_y_min", 0);
    int ccYMax = getParam(*shape, "cc_y_max", highres ? 16383 : 127);

    if (event.action == SysEx::ACTION_DOWN || event.action == SysEx::ACTION_MOVE) {
        auto [nx, ny] = normalizeInShape(fs.x, fs.y, *shape);
        if (highres) {
            int valX = ccXMin + (int)(nx * (float)(ccXMax - ccXMin));
            int valY = ccYMin + (int)(ny * (float)(ccYMax - ccYMin));
            midi_.cc14bit(channel, ccX, juce::jlimit(0, 16383, valX));
            midi_.cc14bit(channel, ccY, juce::jlimit(0, 16383, valY));
        } else {
            int valX = ccXMin + (int)(nx * (float)(ccXMax - ccXMin));
            int valY = ccYMin + (int)(ny * (float)(ccYMax - ccYMin));
            midi_.cc(channel, ccX, juce::jlimit(0, 127, valX));
            midi_.cc(channel, ccY, juce::jlimit(0, 127, valY));
        }
    }
}

void BehaviorEngine::handleFader(const FingerEvent& event, Shape* shape, FingerState& fs)
{
    int ccNum = getParam(*shape, "cc", 1);
    int channel = getParam(*shape, "channel", 0);
    bool horizontal = getParamBool(*shape, "horizontal", false);
    bool highres = getParamBool(*shape, "highres", false);
    int ccMin = getParam(*shape, "cc_min", 0);
    int ccMax = getParam(*shape, "cc_max", highres ? 16383 : 127);

    if (event.action == SysEx::ACTION_DOWN || event.action == SysEx::ACTION_MOVE) {
        auto [nx, ny] = normalizeInShape(fs.x, fs.y, *shape);
        float value = horizontal ? nx : ny;
        if (highres) {
            int val = ccMin + (int)(value * (float)(ccMax - ccMin));
            midi_.cc14bit(channel, ccNum, juce::jlimit(0, 16383, val));
        } else {
            int val = ccMin + (int)(value * (float)(ccMax - ccMin));
            midi_.cc(channel, ccNum, juce::jlimit(0, 127, val));
        }
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

    // Clear all latched notes
    latchedShapes_.clear();
}

} // namespace erae
