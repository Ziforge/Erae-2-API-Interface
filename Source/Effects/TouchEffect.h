#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <utility>
#include "../Model/Shape.h"

namespace erae {

// ============================================================
// Effect type enum
// ============================================================
enum class TouchEffectType {
    None, Trail, Ripple, Particles, Pulse, Breathe, Spin, Orbit, Boundary,
    String, Membrane, Fluid, SpringLattice, Pendulum, Collision,
    Tombolo, GravityWell, ElasticBand, Bow, WaveInterference
};

inline TouchEffectType effectFromString(const std::string& s)
{
    if (s == "trail")             return TouchEffectType::Trail;
    if (s == "ripple")            return TouchEffectType::Ripple;
    if (s == "particles")         return TouchEffectType::Particles;
    if (s == "pulse")             return TouchEffectType::Pulse;
    if (s == "breathe")           return TouchEffectType::Breathe;
    if (s == "spin")              return TouchEffectType::Spin;
    if (s == "orbit")             return TouchEffectType::Orbit;
    if (s == "boundary")          return TouchEffectType::Boundary;
    if (s == "string")            return TouchEffectType::String;
    if (s == "membrane")          return TouchEffectType::Membrane;
    if (s == "fluid")             return TouchEffectType::Fluid;
    if (s == "spring_lattice")    return TouchEffectType::SpringLattice;
    if (s == "pendulum")          return TouchEffectType::Pendulum;
    if (s == "collision")         return TouchEffectType::Collision;
    if (s == "tombolo")           return TouchEffectType::Tombolo;
    if (s == "gravity_well")      return TouchEffectType::GravityWell;
    if (s == "elastic_band")      return TouchEffectType::ElasticBand;
    if (s == "bow")               return TouchEffectType::Bow;
    if (s == "wave_interference") return TouchEffectType::WaveInterference;
    return TouchEffectType::None;
}

inline std::string effectToString(TouchEffectType t)
{
    switch (t) {
        case TouchEffectType::Trail:            return "trail";
        case TouchEffectType::Ripple:           return "ripple";
        case TouchEffectType::Particles:        return "particles";
        case TouchEffectType::Pulse:            return "pulse";
        case TouchEffectType::Breathe:          return "breathe";
        case TouchEffectType::Spin:             return "spin";
        case TouchEffectType::Orbit:            return "orbit";
        case TouchEffectType::Boundary:         return "boundary";
        case TouchEffectType::String:           return "string";
        case TouchEffectType::Membrane:         return "membrane";
        case TouchEffectType::Fluid:            return "fluid";
        case TouchEffectType::SpringLattice:    return "spring_lattice";
        case TouchEffectType::Pendulum:         return "pendulum";
        case TouchEffectType::Collision:        return "collision";
        case TouchEffectType::Tombolo:          return "tombolo";
        case TouchEffectType::GravityWell:      return "gravity_well";
        case TouchEffectType::ElasticBand:      return "elastic_band";
        case TouchEffectType::Bow:              return "bow";
        case TouchEffectType::WaveInterference: return "wave_interference";
        default:                                return "none";
    }
}

// ============================================================
// Modulation target enum
// ============================================================
enum class ModTarget { None, MidiCC, PitchBend, Pressure, CV, OSC, MPE };

inline ModTarget modTargetFromString(const std::string& s)
{
    if (s == "midi_cc")    return ModTarget::MidiCC;
    if (s == "pitch_bend") return ModTarget::PitchBend;
    if (s == "pressure")   return ModTarget::Pressure;
    if (s == "cv")         return ModTarget::CV;
    if (s == "osc")        return ModTarget::OSC;
    if (s == "mpe")        return ModTarget::MPE;
    return ModTarget::None;
}

inline std::string modTargetToString(ModTarget t)
{
    switch (t) {
        case ModTarget::MidiCC:    return "midi_cc";
        case ModTarget::PitchBend: return "pitch_bend";
        case ModTarget::Pressure:  return "pressure";
        case ModTarget::CV:        return "cv";
        case ModTarget::OSC:       return "osc";
        case ModTarget::MPE:       return "mpe";
        default:                   return "none";
    }
}

// ============================================================
// Effect parameters (parsed from Shape::behaviorParams["effect"])
// ============================================================
struct EffectParams {
    TouchEffectType type = TouchEffectType::None;
    float speed     = 1.0f;   // 0.1 - 5.0
    float intensity = 0.8f;   // 0.0 - 1.0
    float decay     = 0.5f;   // 0.1 - 2.0
    bool motionReactive = false;
    bool useShapeColor  = true;
    Color7 effectColor  {0, 80, 127};  // used when !useShapeColor
    ModTarget modTarget = ModTarget::None;
    int modCC      = 74;   // 0-127
    int modChannel = 0;    // 0-15
    int modCVCh    = 0;    // 0-31
    int mpeChannel = 1;    // 1-15 (MPE member channel)
};

// ============================================================
// Per-effect-type state structs
// ============================================================
struct TrailPoint {
    float x, y;
    float age;       // 0 = fresh, advances toward 1
    float velocity;  // capture motion speed at creation
};

struct RippleState {
    float cx, cy;      // center
    float radius;      // expanding
    float age;         // 0 = fresh
    float initialZ;    // pressure at tap
};

struct ParticleState {
    float x, y;
    float vx, vy;
    float age;
    float lifetime;   // 0.5 - 1.5s
    float brightness;
};

// Spin: orbiting pixel positions around shape center
struct SpinDot {
    float angle;      // current angle in radians
    float radius;     // orbit radius in grid units
    float brightness;
};

// Orbit: pivot point + orbiting elements (multi-finger)
struct OrbitState {
    float pivotX, pivotY;     // first finger = pivot
    float controlX, controlY; // second finger = radius/speed control
    float orbitRadius;         // distance between fingers
    bool hasPivot = false;
    bool hasControl = false;
    uint64_t pivotFingerId = 0;
    uint64_t controlFingerId = 0;
};

// Boundary: convex hull from multiple finger positions
struct BoundaryFinger {
    uint64_t fingerId;
    float x, y;
};

// ============================================================
// GridField: shared 2D float grid for grid-based physics models
// ============================================================
struct GridField {
    std::vector<float> data;
    int width = 0, height = 0;

    void init(int w, int h, float val = 0.0f) {
        width = w; height = h;
        data.assign((size_t)(w * h), val);
    }
    float get(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height) return 0.0f;
        return data[(size_t)(y * width + x)];
    }
    void set(int x, int y, float v) {
        if (x >= 0 && x < width && y >= 0 && y < height)
            data[(size_t)(y * width + x)] = v;
    }
    void add(int x, int y, float v) {
        if (x >= 0 && x < width && y >= 0 && y < height)
            data[(size_t)(y * width + x)] += v;
    }
    bool valid() const { return width > 0 && height > 0 && !data.empty(); }
    void clear() { std::fill(data.begin(), data.end(), 0.0f); }
};

// ============================================================
// Physical model state structs (11 new types)
// ============================================================

struct StringState {
    float ax = 0, ay = 0, bx = 0, by = 0;
    std::vector<float> displacement;
    std::vector<float> stringVel;
    bool hasA = false, hasB = false;
    uint64_t fingerA = 0, fingerB = 0;
};

struct MembraneState {
    GridField displacement, velocity;
};

struct FluidState {
    GridField vx, vy, density;
    GridField vx0, vy0, d0;
};

struct SpringState {
    GridField displacement, velocity;
};

struct PendulumState {
    float pivotX = 0, pivotY = 0;
    float theta1 = 0, omega1 = 0, length1 = 5.0f;
    float theta2 = 0, omega2 = 0, length2 = 4.0f;
    bool isDouble = false, dragging = false;
    uint64_t pivotFingerId = 0, bobFingerId = 0;
    std::vector<std::pair<float,float>> bobTrail;
};

struct CollisionBall {
    float x, y, vx, vy, radius, brightness;
};

struct CollisionState {
    std::vector<CollisionBall> balls;
    int recentCollisions = 0;
};

struct TomboloState {
    GridField height;
};

struct GravityParticle {
    float x, y, vx, vy, brightness;
};

struct GravityState {
    std::vector<GravityParticle> particles;
};

struct BandPoint {
    float x, y, vx, vy;
    bool anchored;
};

struct ElasticState {
    std::vector<BandPoint> points;
    std::vector<std::pair<uint64_t, int>> anchors;
};

struct BowState {
    float bowX = 0, bowY = 0, bowVelX = 0, bowVelY = 0, bowPressure = 0;
    float displacement = 0, stringVel = 0;
    bool sticking = false, bowing = false;
    float frictionForce = 0;
    uint64_t bowFingerId = 0;
    std::vector<float> waveform;
};

struct WaveSource {
    float x, y, frequency, phase;
    uint64_t fingerId;
};

struct WaveInterfState {
    std::vector<WaveSource> sources;
    GridField field;
};

// ============================================================
// Per-shape effect state (persists across frames)
// ============================================================
struct ShapeEffectState {
    std::vector<TrailPoint> trail;
    std::vector<RippleState> ripples;
    std::vector<ParticleState> particles;
    float phase = 0.0f;       // for Pulse/Breathe/Spin oscillation
    float prevX = -1.0f;
    float prevY = -1.0f;
    float velocity = 0.0f;    // pixels/sec
    float direction = 0.0f;   // radians
    float modValue = 0.0f;    // 0.0 - 1.0 output
    float modX = 0.5f;       // 0.0 - 1.0 MPE X (pitch bend)
    float modY = 0.5f;       // 0.0 - 1.0 MPE Y (slide CC74)
    float modZ = 0.0f;       // 0.0 - 1.0 MPE Z (pressure)
    bool touched = false;     // finger currently down

    // Spin state
    std::vector<SpinDot> spinDots;
    float spinAngle = 0.0f;   // accumulated rotation angle

    // Orbit state (multi-finger)
    OrbitState orbit;
    std::vector<SpinDot> orbitDots;

    // Boundary state (multi-finger convex hull)
    std::vector<BoundaryFinger> boundaryFingers;
    std::vector<std::pair<float,float>> convexHull; // computed hull vertices

    // Grid-field origin (shape bbox min corner, for shape-relative grids)
    float gridOriginX = 0.0f;
    float gridOriginY = 0.0f;

    // Physical model states (11 new types)
    StringState stringState;
    MembraneState membraneState;
    FluidState fluidState;
    SpringState springState;
    PendulumState pendulumState;
    CollisionState collisionState;
    TomboloState tomboloState;
    GravityState gravityState;
    ElasticState elasticState;
    BowState bowState;
    WaveInterfState waveInterfState;

    bool isEmpty() const
    {
        return trail.empty() && ripples.empty() && particles.empty()
               && spinDots.empty() && orbitDots.empty()
               && boundaryFingers.empty() && convexHull.empty()
               && !stringState.hasA && !stringState.hasB
               && !membraneState.displacement.valid()
               && !fluidState.density.valid()
               && !springState.displacement.valid()
               && !pendulumState.pivotFingerId
               && collisionState.balls.empty()
               && !tomboloState.height.valid()
               && gravityState.particles.empty()
               && elasticState.points.empty()
               && !bowState.bowing
               && waveInterfState.sources.empty()
               && phase == 0.0f && !touched;
    }
};

} // namespace erae
