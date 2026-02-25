#pragma once

#include "EraeMidiOut.h"
#include "MPEAllocator.h"
#include "../Model/Shape.h"
#include "../Model/Behavior.h"
#include "../Erae/FingerStream.h"
#include <map>

namespace erae {

class BehaviorEngine {
public:
    BehaviorEngine(EraeMidiOut& midi, MPEAllocator& mpe);

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

    static int zToVelocity(float z);
    static int zToPressure(float z);
    static std::pair<float, float> normalizeInShape(float x, float y, const Shape& shape);

    int getParam(const Shape& shape, const juce::String& key, int defaultVal) const;
    bool getParamBool(const Shape& shape, const juce::String& key, bool defaultVal) const;

    EraeMidiOut& midi_;
    MPEAllocator& mpe_;
    std::map<uint64_t, FingerState> activeFingers_;
};

} // namespace erae
