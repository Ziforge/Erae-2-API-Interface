#pragma once

#include "EraeMidiOut.h"
#include "MPEAllocator.h"
#include "VelocityCurve.h"
#include "ScaleQuantizer.h"
#include "OscOutput.h"
#include "CVOutput.h"
#include "../Model/Shape.h"
#include "../Model/Behavior.h"
#include "../Erae/FingerStream.h"
#include <map>

namespace erae {

class BehaviorEngine {
public:
    BehaviorEngine(EraeMidiOut& midi, MPEAllocator& mpe);
    void setOscOutput(OscOutput* osc) { oscOut_ = osc; }
    void setCVOutput(CVOutput* cv) { cvOut_ = cv; }

    void handle(const FingerEvent& event, Shape* shape);
    void allNotesOff();

private:
    struct FingerState {
        uint64_t fingerId;
        float startX, startY;
        float x, y, z;
        Shape* shape = nullptr;
    };

    void handleTrigger(const FingerEvent& event, Shape* shape, FingerState& fs);
    void handleMomentary(const FingerEvent& event, Shape* shape, FingerState& fs);
    void handleNotePad(const FingerEvent& event, Shape* shape, FingerState& fs);
    void handleXY(const FingerEvent& event, Shape* shape, FingerState& fs);
    void handleFader(const FingerEvent& event, Shape* shape, FingerState& fs);

    int zToVelocity(float z, CurveType curve);
    int zToPressure(float z, CurveType curve);
    static std::pair<float, float> normalizeInShape(float x, float y, const Shape& shape);

    int getParam(const Shape& shape, const juce::String& key, int defaultVal) const;
    bool getParamBool(const Shape& shape, const juce::String& key, bool defaultVal) const;
    float getParamFloat(const Shape& shape, const juce::String& key, float defaultVal) const;
    std::string getParamString(const Shape& shape, const juce::String& key, const std::string& defaultVal) const;

    EraeMidiOut& midi_;
    MPEAllocator& mpe_;
    OscOutput* oscOut_ = nullptr;
    CVOutput* cvOut_ = nullptr;
    std::map<uint64_t, FingerState> activeFingers_;

    // Latch state: shapeId -> true if currently latched on
    std::map<std::string, bool> latchedShapes_;
};

} // namespace erae
