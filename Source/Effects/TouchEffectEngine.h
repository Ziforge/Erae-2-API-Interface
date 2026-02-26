#pragma once

#include "TouchEffect.h"
#include "../Erae/FingerStream.h"
#include "../MIDI/EraeMidiOut.h"
#include "../MIDI/OscOutput.h"
#include "../MIDI/CVOutput.h"
#include <map>
#include <string>

namespace erae {

class TouchEffectEngine {
public:
    TouchEffectEngine() = default;

    void setMidiOut(EraeMidiOut* m)   { midiOut_ = m; }
    void setOscOutput(OscOutput* o)   { oscOutput_ = o; }
    void setCVOutput(CVOutput* c)     { cvOutput_ = c; }

    // Called from fingerEvent() — update effect state for the shape
    void handleFinger(const FingerEvent& event, Shape* shape);

    // Called at 20fps from renderer — advance all active effects
    void advanceFrame(float dt);

    // Read-only access to all effect states (for rendering)
    const std::map<std::string, ShapeEffectState>& getEffectStates() const { return states_; }

    // Parse EffectParams from a shape's behaviorParams
    static EffectParams parseParams(const Shape& shape);

    // Clear all state (e.g., on layout change)
    void clear();

private:
    void updateTrail(ShapeEffectState& st, const EffectParams& p, float dt);
    void updateRipple(ShapeEffectState& st, const EffectParams& p, float dt);
    void updateParticles(ShapeEffectState& st, const EffectParams& p, float dt);
    void updatePulse(ShapeEffectState& st, const EffectParams& p, float dt);
    void updateBreathe(ShapeEffectState& st, const EffectParams& p, float dt);
    void updateSpin(ShapeEffectState& st, const EffectParams& p, float dt);
    void updateOrbit(ShapeEffectState& st, const EffectParams& p, float dt);
    void updateBoundary(ShapeEffectState& st, const EffectParams& p, float dt);

    // Physical model update methods (11 new types)
    void updateString(ShapeEffectState& st, const EffectParams& p, float dt);
    void updateMembrane(ShapeEffectState& st, const EffectParams& p, float dt);
    void updateFluid(ShapeEffectState& st, const EffectParams& p, float dt);
    void updateSpringLattice(ShapeEffectState& st, const EffectParams& p, float dt);
    void updatePendulum(ShapeEffectState& st, const EffectParams& p, float dt);
    void updateCollision(ShapeEffectState& st, const EffectParams& p, float dt);
    void updateTombolo(ShapeEffectState& st, const EffectParams& p, float dt);
    void updateGravityWell(ShapeEffectState& st, const EffectParams& p, float dt);
    void updateElasticBand(ShapeEffectState& st, const EffectParams& p, float dt);
    void updateBow(ShapeEffectState& st, const EffectParams& p, float dt);
    void updateWaveInterference(ShapeEffectState& st, const EffectParams& p, float dt);

    static void initGridForShape(GridField& gf, int w = 42, int h = 24);

    static std::vector<std::pair<float,float>> computeConvexHull(
        const std::vector<BoundaryFinger>& fingers);

    void sendModulation(const std::string& shapeId, const EffectParams& p,
                        const ShapeEffectState& st);

    struct ActiveFinger {
        std::string shapeId;
        float x, y, z;
    };

    std::map<std::string, ShapeEffectState> states_;
    std::map<uint64_t, ActiveFinger> activeFingers_;

    EraeMidiOut* midiOut_  = nullptr;
    OscOutput* oscOutput_  = nullptr;
    CVOutput* cvOutput_    = nullptr;

    // Store params per shape so advanceFrame doesn't need Shape* pointers
    std::map<std::string, EffectParams> paramsCache_;
};

} // namespace erae
