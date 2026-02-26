#include "TouchEffectEngine.h"
#include "../Erae/EraeSysEx.h"
#include <cmath>
#include <algorithm>
#include <random>

namespace erae {

// ============================================================
// Parse effect params from shape's behaviorParams["effect"]
// ============================================================
EffectParams TouchEffectEngine::parseParams(const Shape& shape)
{
    EffectParams p;
    auto* obj = shape.behaviorParams.getDynamicObject();
    if (!obj || !obj->hasProperty("effect"))
        return p;

    auto effectVar = obj->getProperty("effect");
    auto* eff = effectVar.getDynamicObject();
    if (!eff) return p;

    auto getStr = [&](const char* key, const char* def) -> std::string {
        if (eff->hasProperty(key))
            return eff->getProperty(key).toString().toStdString();
        return def;
    };
    auto getFloat = [&](const char* key, float def) -> float {
        if (eff->hasProperty(key))
            return (float)(double)eff->getProperty(key);
        return def;
    };
    auto getInt = [&](const char* key, int def) -> int {
        if (eff->hasProperty(key))
            return (int)eff->getProperty(key);
        return def;
    };
    auto getBool = [&](const char* key, bool def) -> bool {
        if (eff->hasProperty(key))
            return (bool)eff->getProperty(key);
        return def;
    };

    p.type           = effectFromString(getStr("type", "none"));
    p.speed          = getFloat("speed", 1.0f);
    p.intensity      = getFloat("intensity", 0.8f);
    p.decay          = getFloat("decay", 0.5f);
    p.motionReactive = getBool("motion_reactive", false);
    p.useShapeColor  = getBool("use_shape_color", true);
    p.modTarget      = modTargetFromString(getStr("mod_target", "none"));
    p.modCC          = getInt("mod_cc", 74);
    p.modChannel     = getInt("mod_channel", 0);
    p.modCVCh        = getInt("mod_cv_ch", 0);
    p.mpeChannel     = getInt("mpe_channel", 1);

    return p;
}

// ============================================================
// Handle finger events — update per-shape effect state
// ============================================================
void TouchEffectEngine::handleFinger(const FingerEvent& event, Shape* shape)
{
    if (!shape) {
        // Finger lifted off all shapes — mark as released
        if (event.action == SysEx::ACTION_UP) {
            auto it = activeFingers_.find(event.fingerId);
            if (it != activeFingers_.end()) {
                auto& st = states_[it->second.shapeId];
                st.touched = false;
                activeFingers_.erase(it);
            }
        }
        return;
    }

    auto p = parseParams(*shape);
    if (p.type == TouchEffectType::None)
        return;

    paramsCache_[shape->id] = p;
    auto& st = states_[shape->id];

    if (event.action == SysEx::ACTION_DOWN) {
        st.touched = true;
        st.prevX = event.x;
        st.prevY = event.y;
        st.velocity = 0.0f;
        activeFingers_[event.fingerId] = {shape->id, event.x, event.y, event.z};

        // Ripple: spawn on touch
        if (p.type == TouchEffectType::Ripple) {
            RippleState rip;
            rip.cx = event.x;
            rip.cy = event.y;
            rip.radius = 0.0f;
            rip.age = 0.0f;
            rip.initialZ = event.z;
            st.ripples.push_back(rip);
        }

        // Spin: initialize spin dots on first touch
        if (p.type == TouchEffectType::Spin && st.spinDots.empty()) {
            int numDots = 6;
            for (int i = 0; i < numDots; ++i) {
                SpinDot sd;
                sd.angle = (float)i / (float)numDots * 6.2832f;
                sd.radius = 2.0f; // grid units from center
                sd.brightness = p.intensity;
                st.spinDots.push_back(sd);
            }
        }

        // Orbit: first finger = pivot, second = control
        if (p.type == TouchEffectType::Orbit) {
            if (!st.orbit.hasPivot) {
                st.orbit.pivotX = event.x;
                st.orbit.pivotY = event.y;
                st.orbit.hasPivot = true;
                st.orbit.pivotFingerId = event.fingerId;
                // Initialize orbit dots
                if (st.orbitDots.empty()) {
                    for (int i = 0; i < 8; ++i) {
                        SpinDot sd;
                        sd.angle = (float)i / 8.0f * 6.2832f;
                        sd.radius = 3.0f;
                        sd.brightness = p.intensity;
                        st.orbitDots.push_back(sd);
                    }
                }
            } else if (!st.orbit.hasControl) {
                st.orbit.controlX = event.x;
                st.orbit.controlY = event.y;
                st.orbit.hasControl = true;
                st.orbit.controlFingerId = event.fingerId;
            }
        }

        // Boundary: track all fingers
        if (p.type == TouchEffectType::Boundary) {
            st.boundaryFingers.push_back({event.fingerId, event.x, event.y});
        }

        // String: first finger = endpoint A, second = endpoint B, third = pluck
        if (p.type == TouchEffectType::String) {
            auto& ss = st.stringState;
            if (!ss.hasA) {
                ss.ax = event.x; ss.ay = event.y;
                ss.hasA = true; ss.fingerA = event.fingerId;
            } else if (!ss.hasB) {
                ss.bx = event.x; ss.by = event.y;
                ss.hasB = true; ss.fingerB = event.fingerId;
                int N = 32;
                ss.displacement.assign(N, 0.0f);
                ss.stringVel.assign(N, 0.0f);
            } else {
                // Third finger = pluck
                float dx = ss.bx - ss.ax, dy = ss.by - ss.ay;
                float len = std::sqrt(dx*dx + dy*dy);
                if (len > 0.1f) {
                    float t = ((event.x - ss.ax)*dx + (event.y - ss.ay)*dy) / (len*len);
                    t = std::max(0.0f, std::min(1.0f, t));
                    int idx = (int)(t * (ss.displacement.size()-1));
                    float pluckAmt = event.z * 3.0f;
                    int N = (int)ss.displacement.size();
                    for (int i = 1; i < N-1; ++i) {
                        float dist = std::abs(i - idx);
                        ss.displacement[i] += pluckAmt * std::max(0.0f, 1.0f - dist/6.0f);
                    }
                }
            }
        }

        // Membrane: strike impulse at touch position
        if (p.type == TouchEffectType::Membrane) {
            auto& ms = st.membraneState;
            if (!ms.displacement.valid()) {
                initGridForShape(ms.displacement);
                initGridForShape(ms.velocity);
            }
            int gx = (int)std::round(event.x), gy = (int)std::round(event.y);
            float force = event.z * 5.0f;
            ms.velocity.add(gx, gy, force);
            ms.velocity.add(gx-1, gy, force*0.5f);
            ms.velocity.add(gx+1, gy, force*0.5f);
            ms.velocity.add(gx, gy-1, force*0.5f);
            ms.velocity.add(gx, gy+1, force*0.5f);
        }

        // Fluid: touch deposits dye
        if (p.type == TouchEffectType::Fluid) {
            auto& fs = st.fluidState;
            if (!fs.density.valid()) {
                initGridForShape(fs.vx); initGridForShape(fs.vy);
                initGridForShape(fs.density);
                initGridForShape(fs.vx0); initGridForShape(fs.vy0);
                initGridForShape(fs.d0);
            }
            int gx = (int)std::round(event.x), gy = (int)std::round(event.y);
            fs.density.add(gx, gy, 2.0f);
        }

        // SpringLattice: touch displaces grid
        if (p.type == TouchEffectType::SpringLattice) {
            auto& sp = st.springState;
            if (!sp.displacement.valid()) {
                initGridForShape(sp.displacement);
                initGridForShape(sp.velocity);
            }
            int gx = (int)std::round(event.x), gy = (int)std::round(event.y);
            sp.displacement.add(gx, gy, event.z * 3.0f);
        }

        // Pendulum: first finger = pivot, second = double pendulum
        if (p.type == TouchEffectType::Pendulum) {
            auto& ps = st.pendulumState;
            if (!ps.pivotFingerId) {
                ps.pivotX = event.x; ps.pivotY = event.y;
                ps.pivotFingerId = event.fingerId;
                ps.theta1 = 1.5f; ps.omega1 = 0.0f;
                ps.isDouble = false;
                ps.bobTrail.clear();
            } else if (!ps.bobFingerId) {
                ps.bobFingerId = event.fingerId;
                ps.isDouble = true;
                ps.theta2 = 1.0f; ps.omega2 = 0.0f;
            }
        }

        // Collision: spawn 2-3 balls
        if (p.type == TouchEffectType::Collision) {
            auto& cs = st.collisionState;
            static std::mt19937 crng(123);
            static std::uniform_real_distribution<float> cvel(-4.0f, 4.0f);
            int numSpawn = 2 + (crng() % 2);
            for (int i = 0; i < numSpawn && cs.balls.size() < 30; ++i) {
                CollisionBall b;
                b.x = event.x; b.y = event.y;
                b.vx = cvel(crng); b.vy = cvel(crng);
                b.radius = 0.5f; b.brightness = p.intensity;
                cs.balls.push_back(b);
            }
        }

        // Tombolo: deposit material
        if (p.type == TouchEffectType::Tombolo) {
            auto& ts = st.tomboloState;
            if (!ts.height.valid()) initGridForShape(ts.height);
            int gx = (int)std::round(event.x), gy = (int)std::round(event.y);
            ts.height.add(gx, gy, 4.0f);
        }

        // GravityWell: spawn ring of particles
        if (p.type == TouchEffectType::GravityWell) {
            auto& gs = st.gravityState;
            for (int i = 0; i < 12 && gs.particles.size() < 80; ++i) {
                float angle = (float)i / 12.0f * 6.2832f;
                GravityParticle gp;
                gp.x = event.x + 3.0f * std::cos(angle);
                gp.y = event.y + 3.0f * std::sin(angle);
                gp.vx = -std::sin(angle) * 2.0f;
                gp.vy = std::cos(angle) * 2.0f;
                gp.brightness = p.intensity;
                gs.particles.push_back(gp);
            }
        }

        // ElasticBand: grab nearest point
        if (p.type == TouchEffectType::ElasticBand) {
            auto& es = st.elasticState;
            if (es.points.empty()) {
                int N = 20;
                es.points.resize(N);
                for (int i = 0; i < N; ++i) {
                    es.points[i].x = 5.0f + (float)i * (32.0f / (N-1));
                    es.points[i].y = 12.0f;
                    es.points[i].vx = 0; es.points[i].vy = 0;
                    es.points[i].anchored = false;
                }
            }
            float bestDist = 999.0f; int bestIdx = -1;
            for (int i = 0; i < (int)es.points.size(); ++i) {
                float dx = es.points[i].x - event.x, dy = es.points[i].y - event.y;
                float d = std::sqrt(dx*dx + dy*dy);
                if (d < bestDist) { bestDist = d; bestIdx = i; }
            }
            if (bestIdx >= 0 && bestDist < 5.0f) {
                es.points[bestIdx].anchored = true;
                es.points[bestIdx].x = event.x;
                es.points[bestIdx].y = event.y;
                es.anchors.push_back({event.fingerId, bestIdx});
            }
        }

        // Bow: start bowing
        if (p.type == TouchEffectType::Bow) {
            auto& bs = st.bowState;
            bs.bowX = event.x; bs.bowY = event.y;
            bs.bowPressure = event.z;
            bs.bowing = true; bs.sticking = true;
            bs.bowFingerId = event.fingerId;
            if (bs.waveform.empty()) bs.waveform.resize(20, 0.0f);
        }

        // WaveInterference: add source
        if (p.type == TouchEffectType::WaveInterference) {
            auto& ws = st.waveInterfState;
            if (!ws.field.valid()) initGridForShape(ws.field);
            WaveSource src;
            src.x = event.x; src.y = event.y;
            src.frequency = 1.5f; src.phase = 0.0f;
            src.fingerId = event.fingerId;
            ws.sources.push_back(src);
        }
    }
    else if (event.action == SysEx::ACTION_MOVE) {
        activeFingers_[event.fingerId] = {shape->id, event.x, event.y, event.z};

        // Motion tracking
        if (st.prevX >= 0.0f) {
            float dx = event.x - st.prevX;
            float dy = event.y - st.prevY;
            float dist = std::sqrt(dx * dx + dy * dy);
            // dt is ~0.05 at 20fps, but finger events may come faster
            st.velocity = dist * 20.0f; // approximate pixels/sec
            if (dist > 0.01f)
                st.direction = std::atan2(dy, dx);
        }
        st.prevX = event.x;
        st.prevY = event.y;

        // Trail: add point on move
        if (p.type == TouchEffectType::Trail) {
            TrailPoint tp;
            tp.x = event.x;
            tp.y = event.y;
            tp.age = 0.0f;
            tp.velocity = st.velocity;
            st.trail.push_back(tp);
            // Limit trail length
            while (st.trail.size() > 20)
                st.trail.erase(st.trail.begin());
        }

        // Orbit: update pivot or control position
        if (p.type == TouchEffectType::Orbit) {
            if (event.fingerId == st.orbit.pivotFingerId) {
                st.orbit.pivotX = event.x;
                st.orbit.pivotY = event.y;
            } else if (event.fingerId == st.orbit.controlFingerId) {
                st.orbit.controlX = event.x;
                st.orbit.controlY = event.y;
            }
        }

        // Boundary: update finger position
        if (p.type == TouchEffectType::Boundary) {
            for (auto& bf : st.boundaryFingers) {
                if (bf.fingerId == event.fingerId) {
                    bf.x = event.x;
                    bf.y = event.y;
                    break;
                }
            }
        }

        // String: move endpoints
        if (p.type == TouchEffectType::String) {
            auto& ss = st.stringState;
            if (event.fingerId == ss.fingerA) { ss.ax = event.x; ss.ay = event.y; }
            if (event.fingerId == ss.fingerB) { ss.bx = event.x; ss.by = event.y; }
        }

        // Fluid: add velocity from finger motion + deposit dye
        if (p.type == TouchEffectType::Fluid) {
            auto& fs = st.fluidState;
            if (fs.density.valid()) {
                int gx = (int)std::round(event.x), gy = (int)std::round(event.y);
                float dx = event.x - st.prevX, dy = event.y - st.prevY;
                fs.vx.add(gx, gy, dx * 5.0f);
                fs.vy.add(gx, gy, dy * 5.0f);
                fs.density.add(gx, gy, 0.5f);
            }
        }

        // Pendulum: move pivot
        if (p.type == TouchEffectType::Pendulum) {
            auto& ps = st.pendulumState;
            if (event.fingerId == ps.pivotFingerId) {
                ps.pivotX = event.x; ps.pivotY = event.y;
            }
        }

        // Tombolo: erode (drag lowers, distributes)
        if (p.type == TouchEffectType::Tombolo) {
            auto& ts = st.tomboloState;
            if (ts.height.valid()) {
                int gx = (int)std::round(event.x), gy = (int)std::round(event.y);
                float h = ts.height.get(gx, gy);
                if (h > 0.5f) {
                    ts.height.add(gx, gy, -1.0f);
                    ts.height.add(gx-1, gy, 0.25f);
                    ts.height.add(gx+1, gy, 0.25f);
                    ts.height.add(gx, gy-1, 0.25f);
                    ts.height.add(gx, gy+1, 0.25f);
                }
            }
        }

        // ElasticBand: move anchored points
        if (p.type == TouchEffectType::ElasticBand) {
            auto& es = st.elasticState;
            for (auto& [fid, idx] : es.anchors) {
                if (fid == event.fingerId && idx < (int)es.points.size()) {
                    es.points[idx].x = event.x;
                    es.points[idx].y = event.y;
                }
            }
        }

        // Bow: update bow velocity and pressure
        if (p.type == TouchEffectType::Bow) {
            auto& bs = st.bowState;
            if (event.fingerId == bs.bowFingerId) {
                bs.bowVelX = (event.x - bs.bowX) * 20.0f;
                bs.bowVelY = (event.y - bs.bowY) * 20.0f;
                bs.bowX = event.x; bs.bowY = event.y;
                bs.bowPressure = event.z;
            }
        }

        // WaveInterference: move source (velocity → frequency)
        if (p.type == TouchEffectType::WaveInterference) {
            auto& ws = st.waveInterfState;
            for (auto& src : ws.sources) {
                if (src.fingerId == event.fingerId) {
                    float dx = event.x - src.x, dy = event.y - src.y;
                    float vel = std::sqrt(dx*dx + dy*dy);
                    src.x = event.x; src.y = event.y;
                    src.frequency = 0.5f + vel * 0.5f;
                    break;
                }
            }
        }
    }
    else if (event.action == SysEx::ACTION_UP) {
        activeFingers_.erase(event.fingerId);

        // Orbit: clear pivot or control
        if (p.type == TouchEffectType::Orbit) {
            if (event.fingerId == st.orbit.pivotFingerId) {
                st.orbit.hasPivot = false;
                st.orbit.pivotFingerId = 0;
                // If control finger remains, promote it to pivot
                if (st.orbit.hasControl) {
                    st.orbit.pivotX = st.orbit.controlX;
                    st.orbit.pivotY = st.orbit.controlY;
                    st.orbit.pivotFingerId = st.orbit.controlFingerId;
                    st.orbit.hasPivot = true;
                    st.orbit.hasControl = false;
                    st.orbit.controlFingerId = 0;
                }
            } else if (event.fingerId == st.orbit.controlFingerId) {
                st.orbit.hasControl = false;
                st.orbit.controlFingerId = 0;
            }
        }

        // Boundary: remove finger
        if (p.type == TouchEffectType::Boundary) {
            st.boundaryFingers.erase(
                std::remove_if(st.boundaryFingers.begin(), st.boundaryFingers.end(),
                               [&](const BoundaryFinger& bf) { return bf.fingerId == event.fingerId; }),
                st.boundaryFingers.end());
            if (st.boundaryFingers.empty())
                st.convexHull.clear();
        }

        // String: release endpoints
        if (p.type == TouchEffectType::String) {
            auto& ss = st.stringState;
            if (event.fingerId == ss.fingerA) {
                ss.hasA = false; ss.fingerA = 0;
            }
            if (event.fingerId == ss.fingerB) {
                ss.hasB = false; ss.fingerB = 0;
            }
        }

        // Pendulum: release pivot or bob
        if (p.type == TouchEffectType::Pendulum) {
            auto& ps = st.pendulumState;
            if (event.fingerId == ps.pivotFingerId) {
                ps.pivotFingerId = 0;
            }
            if (event.fingerId == ps.bobFingerId) {
                ps.bobFingerId = 0; ps.isDouble = false;
            }
        }

        // ElasticBand: release anchored points (snap!)
        if (p.type == TouchEffectType::ElasticBand) {
            auto& es = st.elasticState;
            for (auto it = es.anchors.begin(); it != es.anchors.end(); ) {
                if (it->first == event.fingerId) {
                    if (it->second < (int)es.points.size())
                        es.points[it->second].anchored = false;
                    it = es.anchors.erase(it);
                } else {
                    ++it;
                }
            }
        }

        // Bow: stop bowing
        if (p.type == TouchEffectType::Bow) {
            auto& bs = st.bowState;
            if (event.fingerId == bs.bowFingerId) {
                bs.bowing = false; bs.bowFingerId = 0;
            }
        }

        // WaveInterference: remove source
        if (p.type == TouchEffectType::WaveInterference) {
            auto& ws = st.waveInterfState;
            ws.sources.erase(
                std::remove_if(ws.sources.begin(), ws.sources.end(),
                    [&](const WaveSource& s) { return s.fingerId == event.fingerId; }),
                ws.sources.end());
        }

        // Check if any fingers remain on this shape
        bool anyFingerOnShape = false;
        for (auto& [fid, af] : activeFingers_) {
            if (af.shapeId == shape->id) {
                anyFingerOnShape = true;
                break;
            }
        }
        st.touched = anyFingerOnShape;
    }
}

// ============================================================
// Advance all active effects by dt seconds (called at 20fps)
// ============================================================
void TouchEffectEngine::advanceFrame(float dt)
{
    // Random engine for particle emission
    static std::mt19937 rng(42);
    static std::uniform_real_distribution<float> rnd01(0.0f, 1.0f);
    static std::uniform_real_distribution<float> rndPM(-1.0f, 1.0f);

    std::vector<std::string> toRemove;

    for (auto& [shapeId, st] : states_) {
        auto pit = paramsCache_.find(shapeId);
        if (pit == paramsCache_.end()) continue;
        auto& p = pit->second;

        float motionScale = 1.0f;
        if (p.motionReactive)
            motionScale = std::min(1.0f, st.velocity / 10.0f);

        switch (p.type) {
            case TouchEffectType::Trail:
                updateTrail(st, p, dt);
                break;
            case TouchEffectType::Ripple:
                updateRipple(st, p, dt);
                break;
            case TouchEffectType::Particles: {
                // Emit particles while touched
                if (st.touched) {
                    // Find the finger position for this shape
                    float fx = st.prevX, fy = st.prevY;
                    int numEmit = 2 + (int)(rnd01(rng) * 3.0f); // 2-4 particles
                    for (int i = 0; i < numEmit && st.particles.size() < 30; ++i) {
                        ParticleState ps;
                        ps.x = fx;
                        ps.y = fy;
                        ps.vx = rndPM(rng) * 3.0f * p.speed;
                        ps.vy = rndPM(rng) * 3.0f * p.speed - 1.0f; // slight upward bias
                        ps.age = 0.0f;
                        ps.lifetime = 0.5f + rnd01(rng) * 1.0f;
                        ps.brightness = p.intensity * motionScale;
                        st.particles.push_back(ps);
                    }
                }
                updateParticles(st, p, dt);
                break;
            }
            case TouchEffectType::Pulse:
                updatePulse(st, p, dt);
                break;
            case TouchEffectType::Breathe:
                updateBreathe(st, p, dt);
                break;
            case TouchEffectType::Spin:
                updateSpin(st, p, dt);
                break;
            case TouchEffectType::Orbit:
                updateOrbit(st, p, dt);
                break;
            case TouchEffectType::Boundary:
                updateBoundary(st, p, dt);
                break;
            case TouchEffectType::String:
                updateString(st, p, dt);
                break;
            case TouchEffectType::Membrane:
                updateMembrane(st, p, dt);
                break;
            case TouchEffectType::Fluid:
                updateFluid(st, p, dt);
                break;
            case TouchEffectType::SpringLattice:
                updateSpringLattice(st, p, dt);
                break;
            case TouchEffectType::Pendulum:
                updatePendulum(st, p, dt);
                break;
            case TouchEffectType::Collision:
                updateCollision(st, p, dt);
                break;
            case TouchEffectType::Tombolo:
                updateTombolo(st, p, dt);
                break;
            case TouchEffectType::GravityWell:
                updateGravityWell(st, p, dt);
                break;
            case TouchEffectType::ElasticBand:
                updateElasticBand(st, p, dt);
                break;
            case TouchEffectType::Bow:
                updateBow(st, p, dt);
                break;
            case TouchEffectType::WaveInterference:
                updateWaveInterference(st, p, dt);
                break;
            default:
                break;
        }

        // Send modulation
        if (p.modTarget != ModTarget::None && (st.modValue > 0.001f || p.modTarget == ModTarget::MPE))
            sendModulation(shapeId, p, st);

        // Clean up empty states
        if (!st.touched && st.isEmpty())
            toRemove.push_back(shapeId);
    }

    for (auto& id : toRemove) {
        states_.erase(id);
        paramsCache_.erase(id);
    }
}

// ============================================================
// Per-type update methods
// ============================================================

void TouchEffectEngine::updateTrail(ShapeEffectState& st, const EffectParams& p, float dt)
{
    for (auto& pt : st.trail)
        pt.age += dt * p.decay;

    // Remove expired points
    st.trail.erase(
        std::remove_if(st.trail.begin(), st.trail.end(),
                       [](const TrailPoint& t) { return t.age > 1.0f; }),
        st.trail.end());

    st.modValue = st.trail.empty() ? 0.0f : (float)st.trail.size() / 20.0f;

    // MPE XYZ: latest trail point position → X/Y, trail density → Z
    if (!st.trail.empty()) {
        auto& latest = st.trail.back();
        st.modX = std::max(0.0f, std::min(1.0f, latest.x / 42.0f));
        st.modY = std::max(0.0f, std::min(1.0f, latest.y / 24.0f));
        st.modZ = st.modValue;
    }
}

void TouchEffectEngine::updateRipple(ShapeEffectState& st, const EffectParams& p, float dt)
{
    float maxBrightness = 0.0f;
    for (auto& rip : st.ripples) {
        rip.radius += p.speed * 3.0f * dt;
        rip.age += dt * p.decay;
        float brightness = (1.0f - rip.age) * p.intensity;
        if (brightness > maxBrightness)
            maxBrightness = brightness;
    }

    // Remove expired ripples
    st.ripples.erase(
        std::remove_if(st.ripples.begin(), st.ripples.end(),
                       [](const RippleState& r) { return r.age > 1.0f; }),
        st.ripples.end());

    st.modValue = std::max(0.0f, maxBrightness);

    // MPE XYZ: brightest ripple center → X/Y, brightness → Z
    if (!st.ripples.empty()) {
        auto& rip = st.ripples.front(); // most recent active ripple
        st.modX = std::max(0.0f, std::min(1.0f, rip.cx / 42.0f));
        st.modY = std::max(0.0f, std::min(1.0f, rip.cy / 24.0f));
        st.modZ = st.modValue;
    }
}

void TouchEffectEngine::updateParticles(ShapeEffectState& st, const EffectParams& p, float dt)
{
    for (auto& ps : st.particles) {
        ps.x += ps.vx * dt;
        ps.y += ps.vy * dt;
        ps.vy += 2.0f * dt; // gravity
        ps.age += dt;
    }

    // Remove dead particles
    st.particles.erase(
        std::remove_if(st.particles.begin(), st.particles.end(),
                       [](const ParticleState& s) { return s.age >= s.lifetime; }),
        st.particles.end());

    st.modValue = st.particles.empty() ? 0.0f : (float)st.particles.size() / 30.0f;

    // MPE XYZ: particle centroid → X/Y, count → Z
    if (!st.particles.empty()) {
        float cx = 0.0f, cy = 0.0f;
        for (auto& ps : st.particles) { cx += ps.x; cy += ps.y; }
        cx /= (float)st.particles.size();
        cy /= (float)st.particles.size();
        st.modX = std::max(0.0f, std::min(1.0f, cx / 42.0f));
        st.modY = std::max(0.0f, std::min(1.0f, cy / 24.0f));
        st.modZ = st.modValue;
    }
}

void TouchEffectEngine::updatePulse(ShapeEffectState& st, const EffectParams& p, float dt)
{
    if (st.touched) {
        st.phase += dt * p.speed * 4.0f;
        st.modValue = 0.5f + 0.5f * std::sin(st.phase * 6.2832f);
        // MPE XYZ: finger position → X/Y, oscillation → Z
        if (st.prevX >= 0.0f) {
            st.modX = std::max(0.0f, std::min(1.0f, st.prevX / 42.0f));
            st.modY = std::max(0.0f, std::min(1.0f, st.prevY / 24.0f));
        }
        st.modZ = st.modValue;
    } else {
        // Fade out when released
        st.modValue *= (1.0f - dt * 3.0f);
        st.modZ = st.modValue;
        if (st.modValue < 0.01f) {
            st.modValue = 0.0f;
            st.phase = 0.0f;
        }
    }
}

void TouchEffectEngine::updateBreathe(ShapeEffectState& st, const EffectParams& p, float dt)
{
    if (st.touched) {
        st.phase += dt * p.speed;
        st.modValue = 0.5f + 0.5f * std::sin(st.phase * 6.2832f);
        // MPE XYZ: finger position → X/Y, oscillation → Z
        if (st.prevX >= 0.0f) {
            st.modX = std::max(0.0f, std::min(1.0f, st.prevX / 42.0f));
            st.modY = std::max(0.0f, std::min(1.0f, st.prevY / 24.0f));
        }
        st.modZ = st.modValue;
    } else {
        st.modValue *= (1.0f - dt * 2.0f);
        st.modZ = st.modValue;
        if (st.modValue < 0.01f) {
            st.modValue = 0.0f;
            st.phase = 0.0f;
        }
    }
}

void TouchEffectEngine::updateSpin(ShapeEffectState& st, const EffectParams& p, float dt)
{
    // Spin dots orbit around the finger position (or shape center when released)
    float spinSpeed = p.speed * 3.0f; // radians/sec
    // Direction from finger motion influences spin direction
    float dirSign = (st.direction >= 0.0f) ? 1.0f : -1.0f;
    if (st.velocity > 0.5f)
        dirSign = (std::cos(st.direction) > 0.0f) ? 1.0f : -1.0f;

    st.spinAngle += dirSign * spinSpeed * dt;

    for (auto& sd : st.spinDots) {
        sd.angle += dirSign * spinSpeed * dt;
        // Keep angle in range
        while (sd.angle > 6.2832f) sd.angle -= 6.2832f;
        while (sd.angle < 0.0f)    sd.angle += 6.2832f;
    }

    if (st.touched) {
        st.modValue = std::fmod(std::abs(st.spinAngle), 6.2832f) / 6.2832f;
        // MPE XYZ: primary spin dot absolute position → X/Y, phase → Z
        if (!st.spinDots.empty() && st.prevX >= 0.0f) {
            float dotX = st.prevX + std::cos(st.spinDots[0].angle) * st.spinDots[0].radius;
            float dotY = st.prevY + std::sin(st.spinDots[0].angle) * st.spinDots[0].radius;
            st.modX = std::max(0.0f, std::min(1.0f, dotX / 42.0f));
            st.modY = std::max(0.0f, std::min(1.0f, dotY / 24.0f));
        }
        st.modZ = st.modValue;
    } else {
        // Slow down and fade out
        for (auto& sd : st.spinDots)
            sd.brightness *= (1.0f - dt * p.decay);

        if (!st.spinDots.empty() && st.spinDots[0].brightness < 0.01f) {
            st.spinDots.clear();
            st.spinAngle = 0.0f;
        }
        st.modValue *= (1.0f - dt * 2.0f);
        if (st.modValue < 0.01f)
            st.modValue = 0.0f;
    }
}

void TouchEffectEngine::updateOrbit(ShapeEffectState& st, const EffectParams& p, float dt)
{
    if (!st.orbit.hasPivot) {
        // No pivot — fade out
        for (auto& od : st.orbitDots)
            od.brightness *= (1.0f - dt * p.decay * 2.0f);
        if (!st.orbitDots.empty() && st.orbitDots[0].brightness < 0.01f)
            st.orbitDots.clear();
        st.modValue *= (1.0f - dt * 2.0f);
        if (st.modValue < 0.01f) st.modValue = 0.0f;
        return;
    }

    // Compute orbit radius from control finger distance, or use default
    float orbitR = 3.0f;
    float orbitSpeed = p.speed * 2.0f;
    if (st.orbit.hasControl) {
        float dx = st.orbit.controlX - st.orbit.pivotX;
        float dy = st.orbit.controlY - st.orbit.pivotY;
        orbitR = std::sqrt(dx * dx + dy * dy);
        orbitR = std::max(1.0f, std::min(orbitR, 15.0f));
        // Speed scales inversely with radius (closer = faster)
        orbitSpeed = p.speed * 2.0f * (5.0f / std::max(1.0f, orbitR));
    }
    st.orbit.orbitRadius = orbitR;

    for (auto& od : st.orbitDots) {
        od.angle += orbitSpeed * dt;
        while (od.angle > 6.2832f) od.angle -= 6.2832f;
        od.radius = orbitR;
        od.brightness = p.intensity;
    }

    st.modValue = std::min(1.0f, orbitR / 15.0f);

    // MPE XYZ: orbiting dot centroid → X/Y, orbit radius → Z
    if (!st.orbitDots.empty()) {
        float cx = 0.0f, cy = 0.0f;
        for (auto& od : st.orbitDots) {
            cx += st.orbit.pivotX + std::cos(od.angle) * od.radius;
            cy += st.orbit.pivotY + std::sin(od.angle) * od.radius;
        }
        cx /= (float)st.orbitDots.size();
        cy /= (float)st.orbitDots.size();
        st.modX = std::max(0.0f, std::min(1.0f, cx / 42.0f));
        st.modY = std::max(0.0f, std::min(1.0f, cy / 24.0f));
    }
    st.modZ = st.modValue;
}

void TouchEffectEngine::updateBoundary(ShapeEffectState& st, const EffectParams& p, float /*dt*/)
{
    if (st.boundaryFingers.size() < 2) {
        st.convexHull.clear();
        st.modValue = 0.0f;
        return;
    }

    // Compute convex hull
    st.convexHull = computeConvexHull(st.boundaryFingers);

    // Mod value = enclosed area normalized (max ~42*24 = 1008 grid cells)
    if (st.convexHull.size() >= 3) {
        // Shoelace formula for area
        float area = 0.0f;
        int n = (int)st.convexHull.size();
        for (int i = 0; i < n; ++i) {
            int j = (i + 1) % n;
            area += st.convexHull[i].first * st.convexHull[j].second;
            area -= st.convexHull[j].first * st.convexHull[i].second;
        }
        area = std::abs(area) * 0.5f;
        // Normalize: 100 sq grid units = 1.0 (reasonable interactive range)
        st.modValue = std::min(1.0f, area / 100.0f) * p.intensity;

        // MPE XYZ: hull centroid → X/Y, area → Z
        float cx = 0.0f, cy = 0.0f;
        for (auto& pt : st.convexHull) { cx += pt.first; cy += pt.second; }
        cx /= (float)st.convexHull.size();
        cy /= (float)st.convexHull.size();
        st.modX = std::max(0.0f, std::min(1.0f, cx / 42.0f));
        st.modY = std::max(0.0f, std::min(1.0f, cy / 24.0f));
        st.modZ = st.modValue;
    } else {
        st.modValue = 0.0f;
    }
}

// Andrew's monotone chain convex hull algorithm
std::vector<std::pair<float,float>> TouchEffectEngine::computeConvexHull(
    const std::vector<BoundaryFinger>& fingers)
{
    int n = (int)fingers.size();
    if (n < 2) return {};

    // Sort by x, then y
    std::vector<std::pair<float,float>> pts;
    pts.reserve(n);
    for (auto& f : fingers)
        pts.push_back({f.x, f.y});

    std::sort(pts.begin(), pts.end());

    if (n == 2) return pts; // line segment, render as-is

    // Build lower hull
    std::vector<std::pair<float,float>> hull;
    for (auto& p : pts) {
        while (hull.size() >= 2) {
            auto& a = hull[hull.size() - 2];
            auto& b = hull[hull.size() - 1];
            float cross = (b.first - a.first) * (p.second - a.second)
                        - (b.second - a.second) * (p.first - a.first);
            if (cross <= 0.0f) hull.pop_back();
            else break;
        }
        hull.push_back(p);
    }

    // Build upper hull
    int lower_size = (int)hull.size() + 1;
    for (int i = n - 1; i >= 0; --i) {
        auto& p = pts[i];
        while ((int)hull.size() >= lower_size) {
            auto& a = hull[hull.size() - 2];
            auto& b = hull[hull.size() - 1];
            float cross = (b.first - a.first) * (p.second - a.second)
                        - (b.second - a.second) * (p.first - a.first);
            if (cross <= 0.0f) hull.pop_back();
            else break;
        }
        hull.push_back(p);
    }

    hull.pop_back(); // remove last point (duplicate of first)
    return hull;
}

// ============================================================
// Modulation output
// ============================================================
void TouchEffectEngine::sendModulation(const std::string& /*shapeId*/,
                                        const EffectParams& p,
                                        const ShapeEffectState& st)
{
    float value = st.modValue;
    int intVal = (int)(value * 127.0f);
    intVal = std::max(0, std::min(127, intVal));

    switch (p.modTarget) {
        case ModTarget::MidiCC:
            if (midiOut_)
                midiOut_->cc(p.modChannel, p.modCC, intVal);
            break;
        case ModTarget::PitchBend:
            if (midiOut_) {
                int pb = (int)(value * 16383.0f);
                midiOut_->pitchBend(p.modChannel, std::max(0, std::min(16383, pb)));
            }
            break;
        case ModTarget::Pressure:
            if (midiOut_)
                midiOut_->pressure(p.modChannel, intVal);
            break;
        case ModTarget::CV:
            if (cvOutput_)
                cvOutput_->set(p.modCVCh, value);
            break;
        case ModTarget::OSC:
            if (oscOutput_ && oscOutput_->isEnabled())
                oscOutput_->cc(p.modChannel, p.modCC, intVal);
            break;
        case ModTarget::MPE: {
            // MPE XYZ: X→PitchBend, Y→CC74(Slide), Z→Pressure
            int ch = std::max(1, std::min(15, p.mpeChannel));
            if (midiOut_) {
                // X → Pitch Bend (0.0=min, 0.5=center, 1.0=max)
                int pb = (int)(st.modX * 16383.0f);
                midiOut_->pitchBend(ch, std::max(0, std::min(16383, pb)));
                // Y → CC74 Slide
                int slide = (int)(st.modY * 127.0f);
                midiOut_->cc(ch, 74, std::max(0, std::min(127, slide)));
                // Z → Channel Pressure
                int pres = (int)(st.modZ * 127.0f);
                midiOut_->pressure(ch, std::max(0, std::min(127, pres)));
            }
            // CV: 3 consecutive channels for X/Y/Z
            if (cvOutput_) {
                int baseCh = p.modCVCh;
                cvOutput_->set(baseCh, st.modX);
                cvOutput_->set(baseCh + 1, st.modY);
                cvOutput_->set(baseCh + 2, st.modZ);
            }
            // OSC: send X/Y/Z as float triplet
            if (oscOutput_ && oscOutput_->isEnabled())
                oscOutput_->effectMPE(ch, st.modX, st.modY, st.modZ);
            break;
        }
        default:
            break;
    }
}

// ============================================================
// Grid initializer
// ============================================================
void TouchEffectEngine::initGridForShape(GridField& gf, int w, int h)
{
    if (!gf.valid())
        gf.init(w, h, 0.0f);
}

// ============================================================
// Physical model update methods (11 new types)
// ============================================================

void TouchEffectEngine::updateString(ShapeEffectState& st, const EffectParams& p, float dt)
{
    auto& ss = st.stringState;
    int N = (int)ss.displacement.size();
    if (N < 3) {
        st.modValue *= (1.0f - dt * 2.0f);
        return;
    }

    float c = p.speed * 40.0f;
    float damping = p.decay * 2.0f;
    int steps = 4;
    float subDt = dt / steps;
    for (int s = 0; s < steps; ++s) {
        ss.displacement[0] = 0.0f; ss.displacement[N-1] = 0.0f;
        for (int i = 1; i < N-1; ++i) {
            float laplacian = ss.displacement[i-1] - 2.0f*ss.displacement[i] + ss.displacement[i+1];
            ss.stringVel[i] += c * c * laplacian * subDt;
            ss.stringVel[i] *= (1.0f - damping * subDt);
        }
        for (int i = 1; i < N-1; ++i)
            ss.displacement[i] += ss.stringVel[i] * subDt;
    }

    float energy = 0.0f, maxDisp = 0.0f;
    int midIdx = N / 2;
    for (int i = 0; i < N; ++i) {
        energy += ss.displacement[i] * ss.displacement[i] + ss.stringVel[i] * ss.stringVel[i];
        if (std::abs(ss.displacement[i]) > maxDisp) maxDisp = std::abs(ss.displacement[i]);
    }
    energy = std::sqrt(energy / N);

    st.modValue = std::min(1.0f, energy * 0.5f) * p.intensity;
    if (ss.hasA && ss.hasB) {
        float mx = (ss.ax + ss.bx) * 0.5f, my = (ss.ay + ss.by) * 0.5f;
        st.modX = std::max(0.0f, std::min(1.0f, mx / 42.0f));
        st.modY = std::max(0.0f, std::min(1.0f, (my + ss.displacement[midIdx]) / 24.0f));
    }
    st.modZ = std::min(1.0f, energy * 0.3f);
}

void TouchEffectEngine::updateMembrane(ShapeEffectState& st, const EffectParams& p, float dt)
{
    auto& ms = st.membraneState;
    if (!ms.displacement.valid()) return;

    float c = p.speed * 30.0f;
    float damping = p.decay * 3.0f;
    int w = ms.displacement.width, h = ms.displacement.height;

    for (int y = 1; y < h-1; ++y) {
        for (int x = 1; x < w-1; ++x) {
            float lap = ms.displacement.get(x-1,y) + ms.displacement.get(x+1,y)
                      + ms.displacement.get(x,y-1) + ms.displacement.get(x,y+1)
                      - 4.0f * ms.displacement.get(x,y);
            float v = ms.velocity.get(x,y) + c * c * lap * dt;
            v *= (1.0f - damping * dt);
            ms.velocity.set(x, y, v);
        }
    }
    float peakDisp = 0.0f; int peakX = 0, peakY = 0;
    for (int y = 1; y < h-1; ++y) {
        for (int x = 1; x < w-1; ++x) {
            float d = ms.displacement.get(x,y) + ms.velocity.get(x,y) * dt;
            ms.displacement.set(x, y, d);
            if (std::abs(d) > peakDisp) { peakDisp = std::abs(d); peakX = x; peakY = y; }
        }
    }

    st.modValue = std::min(1.0f, peakDisp * 0.5f) * p.intensity;
    st.modX = std::max(0.0f, std::min(1.0f, (float)peakX / 42.0f));
    st.modY = std::max(0.0f, std::min(1.0f, (float)peakY / 24.0f));
    st.modZ = st.modValue;
}

void TouchEffectEngine::updateFluid(ShapeEffectState& st, const EffectParams& p, float dt)
{
    auto& fs = st.fluidState;
    if (!fs.density.valid()) return;
    int w = fs.density.width, h = fs.density.height;

    // Diffuse velocity
    float visc = 0.001f * (1.0f / std::max(0.1f, p.speed));
    float a = dt * visc * w * h;
    for (int k = 0; k < 8; ++k) {
        for (int y = 1; y < h-1; ++y) {
            for (int x = 1; x < w-1; ++x) {
                fs.vx.set(x,y, (fs.vx0.get(x,y) + a*(fs.vx.get(x-1,y)+fs.vx.get(x+1,y)+fs.vx.get(x,y-1)+fs.vx.get(x,y+1))) / (1+4*a));
                fs.vy.set(x,y, (fs.vy0.get(x,y) + a*(fs.vy.get(x-1,y)+fs.vy.get(x+1,y)+fs.vy.get(x,y-1)+fs.vy.get(x,y+1))) / (1+4*a));
            }
        }
    }

    // Pressure projection (divergence-free)
    GridField div, pf;
    div.init(w, h); pf.init(w, h);
    for (int y = 1; y < h-1; ++y)
        for (int x = 1; x < w-1; ++x)
            div.set(x,y, -0.5f*(fs.vx.get(x+1,y)-fs.vx.get(x-1,y)+fs.vy.get(x,y+1)-fs.vy.get(x,y-1)));

    for (int k = 0; k < 15; ++k)
        for (int y = 1; y < h-1; ++y)
            for (int x = 1; x < w-1; ++x)
                pf.set(x,y, (div.get(x,y)+pf.get(x-1,y)+pf.get(x+1,y)+pf.get(x,y-1)+pf.get(x,y+1))*0.25f);

    for (int y = 1; y < h-1; ++y)
        for (int x = 1; x < w-1; ++x) {
            fs.vx.add(x,y, -0.5f*(pf.get(x+1,y)-pf.get(x-1,y)));
            fs.vy.add(x,y, -0.5f*(pf.get(x,y+1)-pf.get(x,y-1)));
        }

    // Advect density (semi-Lagrangian)
    fs.d0 = fs.density;
    for (int y = 1; y < h-1; ++y) {
        for (int x = 1; x < w-1; ++x) {
            float sx = (float)x - dt * p.speed * 20.0f * fs.vx.get(x,y);
            float sy = (float)y - dt * p.speed * 20.0f * fs.vy.get(x,y);
            sx = std::max(0.5f, std::min((float)(w-1)-0.5f, sx));
            sy = std::max(0.5f, std::min((float)(h-1)-0.5f, sy));
            int i0 = (int)sx, j0 = (int)sy;
            float s1 = sx - i0, t1 = sy - j0;
            float s0 = 1.0f - s1, t0 = 1.0f - t1;
            float val = s0*(t0*fs.d0.get(i0,j0) + t1*fs.d0.get(i0,j0+1))
                      + s1*(t0*fs.d0.get(i0+1,j0) + t1*fs.d0.get(i0+1,j0+1));
            fs.density.set(x, y, val * (1.0f - p.decay * 0.05f * dt));
        }
    }

    // Save current velocity as previous for next diffuse
    fs.vx0 = fs.vx; fs.vy0 = fs.vy;

    // Compute centroid and vorticity
    float cx = 0, cy = 0, totalD = 0, vort = 0;
    for (int y = 1; y < h-1; ++y)
        for (int x = 1; x < w-1; ++x) {
            float d = fs.density.get(x,y);
            cx += d * x; cy += d * y; totalD += d;
            float dvx = fs.vx.get(x,y+1) - fs.vx.get(x,y-1);
            float dvy = fs.vy.get(x+1,y) - fs.vy.get(x-1,y);
            vort += std::abs(dvy - dvx);
        }
    if (totalD > 0.01f) { cx /= totalD; cy /= totalD; }
    st.modValue = std::min(1.0f, totalD * 0.01f) * p.intensity;
    st.modX = std::max(0.0f, std::min(1.0f, cx / (float)w));
    st.modY = std::max(0.0f, std::min(1.0f, cy / (float)h));
    st.modZ = std::min(1.0f, vort * 0.001f);
}

void TouchEffectEngine::updateSpringLattice(ShapeEffectState& st, const EffectParams& p, float dt)
{
    auto& sp = st.springState;
    if (!sp.displacement.valid()) return;
    int w = sp.displacement.width, h = sp.displacement.height;

    float k = p.speed * 50.0f;
    float damping = p.decay * 4.0f;

    for (int y = 1; y < h-1; ++y) {
        for (int x = 1; x < w-1; ++x) {
            float force = k * (sp.displacement.get(x-1,y) + sp.displacement.get(x+1,y)
                            + sp.displacement.get(x,y-1) + sp.displacement.get(x,y+1)
                            - 4.0f * sp.displacement.get(x,y));
            float v = sp.velocity.get(x,y) + force * dt;
            v *= (1.0f - damping * dt);
            sp.velocity.set(x, y, v);
        }
    }
    float totalE = 0, cx = 0, cy = 0, totalW = 0;
    for (int y = 1; y < h-1; ++y) {
        for (int x = 1; x < w-1; ++x) {
            float d = sp.displacement.get(x,y) + sp.velocity.get(x,y) * dt;
            sp.displacement.set(x, y, d);
            float e = d * d;
            totalE += e; cx += e * x; cy += e * y; totalW += e;
        }
    }
    if (totalW > 0.01f) { cx /= totalW; cy /= totalW; }
    st.modValue = std::min(1.0f, std::sqrt(totalE / (w*h)) * 2.0f) * p.intensity;
    st.modX = std::max(0.0f, std::min(1.0f, cx / (float)w));
    st.modY = std::max(0.0f, std::min(1.0f, cy / (float)h));
    st.modZ = st.modValue;
}

void TouchEffectEngine::updatePendulum(ShapeEffectState& st, const EffectParams& p, float dt)
{
    auto& ps = st.pendulumState;
    float g = 9.81f * p.speed;
    float damping = p.decay * 0.5f;

    if (!ps.isDouble) {
        // Single pendulum: symplectic Euler
        float alpha = -(g / ps.length1) * std::sin(ps.theta1) - damping * ps.omega1;
        ps.omega1 += alpha * dt;
        ps.theta1 += ps.omega1 * dt;
    } else {
        // Double pendulum (simplified Lagrangian)
        float m1 = 1.0f, m2 = 1.0f;
        float L1 = ps.length1, L2 = ps.length2;
        float dTheta = ps.theta1 - ps.theta2;
        float den1 = (2*m1 + m2 - m2*std::cos(2*dTheta)) * L1;
        float alpha1 = (-g*(2*m1+m2)*std::sin(ps.theta1) - m2*g*std::sin(ps.theta1-2*ps.theta2)
                        - 2*std::sin(dTheta)*m2*(ps.omega2*ps.omega2*L2+ps.omega1*ps.omega1*L1*std::cos(dTheta))) / den1;
        float den2 = (2*m1 + m2 - m2*std::cos(2*dTheta)) * L2;
        float alpha2 = (2*std::sin(dTheta)*(ps.omega1*ps.omega1*L1*(m1+m2)+g*(m1+m2)*std::cos(ps.theta1)
                        +ps.omega2*ps.omega2*L2*m2*std::cos(dTheta))) / den2;
        ps.omega1 += (alpha1 - damping*ps.omega1) * dt;
        ps.omega2 += (alpha2 - damping*ps.omega2) * dt;
        ps.theta1 += ps.omega1 * dt;
        ps.theta2 += ps.omega2 * dt;
    }

    // Bob position for trail
    float bob1x = ps.pivotX + ps.length1 * std::sin(ps.theta1);
    float bob1y = ps.pivotY + ps.length1 * std::cos(ps.theta1);
    float finalX = bob1x, finalY = bob1y;
    if (ps.isDouble) {
        finalX = bob1x + ps.length2 * std::sin(ps.theta2);
        finalY = bob1y + ps.length2 * std::cos(ps.theta2);
    }
    ps.bobTrail.push_back({finalX, finalY});
    while (ps.bobTrail.size() > 40) ps.bobTrail.erase(ps.bobTrail.begin());

    st.modValue = std::min(1.0f, std::abs(ps.omega1) * 0.3f) * p.intensity;
    st.modX = std::max(0.0f, std::min(1.0f, finalX / 42.0f));
    st.modY = std::max(0.0f, std::min(1.0f, finalY / 24.0f));
    st.modZ = std::min(1.0f, std::abs(ps.omega1) * 0.2f);
}

void TouchEffectEngine::updateCollision(ShapeEffectState& st, const EffectParams& p, float dt)
{
    auto& cs = st.collisionState;
    cs.recentCollisions = 0;

    for (auto& b : cs.balls) {
        b.x += b.vx * dt * p.speed;
        b.y += b.vy * dt * p.speed;
        b.vy += 2.0f * dt; // gravity

        // Wall bounce
        if (b.x - b.radius < 0)   { b.x = b.radius; b.vx = std::abs(b.vx); cs.recentCollisions++; }
        if (b.x + b.radius > 41)  { b.x = 41 - b.radius; b.vx = -std::abs(b.vx); cs.recentCollisions++; }
        if (b.y - b.radius < 0)   { b.y = b.radius; b.vy = std::abs(b.vy); cs.recentCollisions++; }
        if (b.y + b.radius > 23)  { b.y = 23 - b.radius; b.vy = -std::abs(b.vy); cs.recentCollisions++; }

        b.brightness *= (1.0f - 0.1f * dt);
    }

    // Ball-ball elastic collision
    for (int i = 0; i < (int)cs.balls.size(); ++i) {
        for (int j = i+1; j < (int)cs.balls.size(); ++j) {
            auto& a = cs.balls[i]; auto& b = cs.balls[j];
            float dx = b.x - a.x, dy = b.y - a.y;
            float dist = std::sqrt(dx*dx + dy*dy);
            float minDist = a.radius + b.radius;
            if (dist < minDist && dist > 0.01f) {
                float nx = dx/dist, ny = dy/dist;
                float dvx = a.vx - b.vx, dvy = a.vy - b.vy;
                float dvn = dvx*nx + dvy*ny;
                if (dvn > 0) {
                    a.vx -= dvn * nx; a.vy -= dvn * ny;
                    b.vx += dvn * nx; b.vy += dvn * ny;
                    float overlap = minDist - dist;
                    a.x -= overlap*0.5f*nx; a.y -= overlap*0.5f*ny;
                    b.x += overlap*0.5f*nx; b.y += overlap*0.5f*ny;
                    cs.recentCollisions++;
                }
            }
        }
    }

    // Finger acts as deflector — handled implicitly via spawning

    // Remove dim balls
    cs.balls.erase(
        std::remove_if(cs.balls.begin(), cs.balls.end(),
            [](const CollisionBall& b) { return b.brightness < 0.05f; }),
        cs.balls.end());

    // Centroid
    float cx = 0, cy = 0;
    for (auto& b : cs.balls) { cx += b.x; cy += b.y; }
    if (!cs.balls.empty()) { cx /= cs.balls.size(); cy /= cs.balls.size(); }
    st.modValue = cs.balls.empty() ? 0.0f : std::min(1.0f, (float)cs.balls.size() / 15.0f) * p.intensity;
    st.modX = std::max(0.0f, std::min(1.0f, cx / 42.0f));
    st.modY = std::max(0.0f, std::min(1.0f, cy / 24.0f));
    st.modZ = std::min(1.0f, (float)cs.recentCollisions / 10.0f);
}

void TouchEffectEngine::updateTombolo(ShapeEffectState& st, const EffectParams& p, float dt)
{
    auto& ts = st.tomboloState;
    if (!ts.height.valid()) return;
    int w = ts.height.width, h = ts.height.height;

    float threshold = 1.0f / p.speed;
    float flowRate = p.intensity * dt * 2.0f;

    // Sandpile automaton: material flows to lower neighbors
    for (int y = 1; y < h-1; ++y) {
        for (int x = 1; x < w-1; ++x) {
            float hc = ts.height.get(x, y);
            int dx[] = {-1,1,0,0}, dy[] = {0,0,-1,1};
            for (int d = 0; d < 4; ++d) {
                float hn = ts.height.get(x+dx[d], y+dy[d]);
                if (hc - hn > threshold) {
                    float flow = std::min(flowRate, (hc - hn) * 0.25f);
                    ts.height.add(x, y, -flow);
                    ts.height.add(x+dx[d], y+dy[d], flow);
                }
            }
        }
    }

    // Decay
    for (auto& v : ts.height.data)
        v *= (1.0f - p.decay * 0.01f * dt);

    // Centroid
    float cx = 0, cy = 0, total = 0;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            float hv = ts.height.get(x, y);
            cx += hv * x; cy += hv * y; total += hv;
        }
    if (total > 0.01f) { cx /= total; cy /= total; }
    st.modValue = std::min(1.0f, total * 0.01f) * p.intensity;
    st.modX = std::max(0.0f, std::min(1.0f, cx / (float)w));
    st.modY = std::max(0.0f, std::min(1.0f, cy / (float)h));
    st.modZ = st.modValue;
}

void TouchEffectEngine::updateGravityWell(ShapeEffectState& st, const EffectParams& p, float dt)
{
    auto& gs = st.gravityState;
    float G = p.speed * 50.0f;
    float softening = 1.0f;

    // Fingers act as heavy masses
    for (auto& gp : gs.particles) {
        float fx = 0, fy = 0;
        for (auto& [fid, af] : activeFingers_) {
            float dx = af.x - gp.x, dy = af.y - gp.y;
            float r2 = dx*dx + dy*dy + softening;
            float force = G / r2;
            float r = std::sqrt(r2);
            fx += force * dx / r;
            fy += force * dy / r;
        }
        gp.vx += fx * dt;
        gp.vy += fy * dt;
        gp.x += gp.vx * dt;
        gp.y += gp.vy * dt;
        gp.brightness *= (1.0f - p.decay * 0.05f * dt);
    }

    // Remove out-of-bounds or dim particles
    gs.particles.erase(
        std::remove_if(gs.particles.begin(), gs.particles.end(),
            [](const GravityParticle& gp) {
                return gp.brightness < 0.02f || gp.x < -10 || gp.x > 52 || gp.y < -10 || gp.y > 34;
            }),
        gs.particles.end());

    // Centroid + orbital energy
    float cx = 0, cy = 0, energy = 0;
    for (auto& gp : gs.particles) {
        cx += gp.x; cy += gp.y;
        energy += gp.vx*gp.vx + gp.vy*gp.vy;
    }
    if (!gs.particles.empty()) { cx /= gs.particles.size(); cy /= gs.particles.size(); }
    st.modValue = gs.particles.empty() ? 0.0f : std::min(1.0f, (float)gs.particles.size() / 40.0f) * p.intensity;
    st.modX = std::max(0.0f, std::min(1.0f, cx / 42.0f));
    st.modY = std::max(0.0f, std::min(1.0f, cy / 24.0f));
    st.modZ = std::min(1.0f, energy * 0.01f / std::max(1.0f, (float)gs.particles.size()));
}

void TouchEffectEngine::updateElasticBand(ShapeEffectState& st, const EffectParams& p, float dt)
{
    auto& es = st.elasticState;
    int N = (int)es.points.size();
    if (N < 2) return;

    float k = p.speed * 80.0f;
    float restLen = 32.0f / (N - 1);
    float damping = p.decay * 5.0f;

    for (int i = 0; i < N; ++i) {
        if (es.points[i].anchored) continue;
        float fx = 0, fy = 0;
        // Spring to prev
        if (i > 0) {
            float dx = es.points[i-1].x - es.points[i].x;
            float dy = es.points[i-1].y - es.points[i].y;
            float dist = std::sqrt(dx*dx + dy*dy);
            if (dist > 0.01f) {
                float f = k * (dist - restLen) / dist;
                fx += f * dx; fy += f * dy;
            }
        }
        // Spring to next
        if (i < N-1) {
            float dx = es.points[i+1].x - es.points[i].x;
            float dy = es.points[i+1].y - es.points[i].y;
            float dist = std::sqrt(dx*dx + dy*dy);
            if (dist > 0.01f) {
                float f = k * (dist - restLen) / dist;
                fx += f * dx; fy += f * dy;
            }
        }
        es.points[i].vx += fx * dt;
        es.points[i].vy += fy * dt;
        es.points[i].vx *= (1.0f - damping * dt);
        es.points[i].vy *= (1.0f - damping * dt);
        es.points[i].x += es.points[i].vx * dt;
        es.points[i].y += es.points[i].vy * dt;
        // Clamp to grid
        es.points[i].x = std::max(0.0f, std::min(41.0f, es.points[i].x));
        es.points[i].y = std::max(0.0f, std::min(23.0f, es.points[i].y));
    }

    // Midpoint + tension
    int mid = N / 2;
    float tension = 0;
    for (int i = 0; i < N-1; ++i) {
        float dx = es.points[i+1].x - es.points[i].x;
        float dy = es.points[i+1].y - es.points[i].y;
        float dist = std::sqrt(dx*dx + dy*dy);
        tension += std::abs(dist - restLen);
    }
    st.modValue = std::min(1.0f, tension * 0.05f) * p.intensity;
    st.modX = std::max(0.0f, std::min(1.0f, es.points[mid].x / 42.0f));
    st.modY = std::max(0.0f, std::min(1.0f, es.points[mid].y / 24.0f));
    st.modZ = st.modValue;
}

void TouchEffectEngine::updateBow(ShapeEffectState& st, const EffectParams& p, float dt)
{
    auto& bs = st.bowState;
    if (!bs.bowing) {
        bs.displacement *= (1.0f - p.decay * 5.0f * dt);
        bs.stringVel *= (1.0f - p.decay * 5.0f * dt);
        st.modValue *= (1.0f - dt * 3.0f);
        if (st.modValue < 0.01f) st.modValue = 0.0f;
        return;
    }

    float bowSpeed = std::sqrt(bs.bowVelX * bs.bowVelX + bs.bowVelY * bs.bowVelY);
    float mu_s = 0.8f;  // static friction
    float mu_k = 0.3f;  // kinetic friction
    float stiffness = 200.0f * p.speed;
    float resonatorDamp = p.decay * 10.0f;

    float relVel = bowSpeed - bs.stringVel;

    if (bs.sticking) {
        bs.stringVel = bowSpeed;
        bs.frictionForce = stiffness * bs.displacement;
        if (std::abs(bs.frictionForce) > mu_s * bs.bowPressure * 50.0f) {
            bs.sticking = false;
        }
    } else {
        bs.frictionForce = mu_k * bs.bowPressure * 50.0f * (relVel > 0 ? 1.0f : -1.0f);
        bs.stringVel += (bs.frictionForce - stiffness * bs.displacement - resonatorDamp * bs.stringVel) * dt;
        if (std::abs(relVel) < 0.5f) {
            bs.sticking = true;
        }
    }

    bs.displacement += bs.stringVel * dt;

    // Store waveform
    bs.waveform.push_back(bs.displacement);
    while (bs.waveform.size() > 20) bs.waveform.erase(bs.waveform.begin());

    st.modValue = std::min(1.0f, bowSpeed * 0.1f) * p.intensity;
    st.modX = std::max(0.0f, std::min(1.0f, bs.bowX / 42.0f));
    st.modY = std::max(0.0f, std::min(1.0f, bs.bowY / 24.0f));
    st.modZ = std::min(1.0f, std::abs(bs.frictionForce) * 0.01f);
}

void TouchEffectEngine::updateWaveInterference(ShapeEffectState& st, const EffectParams& p, float dt)
{
    auto& ws = st.waveInterfState;
    if (!ws.field.valid()) return;
    int w = ws.field.width, h = ws.field.height;

    // Advance phase for all sources
    for (auto& src : ws.sources)
        src.phase += dt * p.speed * 6.0f;

    // Compute superposition
    ws.field.clear();
    float peakAmp = 0, peakX = 0, peakY = 0;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float val = 0;
            for (auto& src : ws.sources) {
                float dx = (float)x - src.x, dy = (float)y - src.y;
                float dist = std::sqrt(dx*dx + dy*dy);
                float amp = 1.0f / (1.0f + dist * 0.2f); // distance attenuation
                val += amp * std::sin(6.2832f * src.frequency * dist * 0.1f - src.phase);
            }
            ws.field.set(x, y, val);
            if (std::abs(val) > peakAmp) {
                peakAmp = std::abs(val); peakX = (float)x; peakY = (float)y;
            }
        }
    }

    st.modValue = ws.sources.empty() ? 0.0f : std::min(1.0f, peakAmp * 0.5f) * p.intensity;
    st.modX = std::max(0.0f, std::min(1.0f, peakX / (float)w));
    st.modY = std::max(0.0f, std::min(1.0f, peakY / (float)h));
    st.modZ = std::min(1.0f, peakAmp * 0.3f);
}

// ============================================================
// Clear
// ============================================================
void TouchEffectEngine::clear()
{
    states_.clear();
    activeFingers_.clear();
    paramsCache_.clear();
}

} // namespace erae
