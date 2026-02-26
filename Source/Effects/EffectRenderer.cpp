#include "EffectRenderer.h"
#include <cmath>
#include <algorithm>

namespace erae {
namespace EffectRenderer {

// Helper: get effect color for a shape
static Color7 getEffectColor(const EffectParams& p, const Shape* shape)
{
    if (p.useShapeColor && shape)
        return shape->color;
    return p.effectColor;
}

// ============================================================
// LED framebuffer rendering (42x24 grid space)
// ============================================================
std::vector<EffectPixel> renderEffects(
    const std::map<std::string, ShapeEffectState>& states,
    const std::map<std::string, EffectParams>& params,
    const std::map<std::string, const Shape*>& shapes)
{
    std::vector<EffectPixel> pixels;

    for (auto& [shapeId, st] : states) {
        auto pit = params.find(shapeId);
        if (pit == params.end()) continue;
        auto& p = pit->second;

        auto sit = shapes.find(shapeId);
        const Shape* shape = (sit != shapes.end()) ? sit->second : nullptr;
        Color7 baseColor = getEffectColor(p, shape);

        switch (p.type) {
            case TouchEffectType::Trail: {
                for (auto& pt : st.trail) {
                    int gx = (int)std::round(pt.x);
                    int gy = (int)std::round(pt.y);
                    if (gx >= 0 && gx < 42 && gy >= 0 && gy < 24) {
                        float alpha = (1.0f - pt.age) * p.intensity;
                        pixels.push_back({gx, gy, baseColor, alpha});
                    }
                }
                break;
            }
            case TouchEffectType::Ripple: {
                for (auto& rip : st.ripples) {
                    float brightness = (1.0f - rip.age * p.decay) * p.intensity;
                    if (brightness <= 0.0f) continue;
                    int r = (int)std::round(rip.radius);
                    if (r < 1) r = 1;
                    // Bresenham circle
                    int cx = (int)std::round(rip.cx);
                    int cy = (int)std::round(rip.cy);
                    int x = 0, y = r;
                    int d = 3 - 2 * r;
                    auto plot = [&](int px, int py) {
                        if (px >= 0 && px < 42 && py >= 0 && py < 24)
                            pixels.push_back({px, py, baseColor, brightness});
                    };
                    while (x <= y) {
                        plot(cx + x, cy + y); plot(cx - x, cy + y);
                        plot(cx + x, cy - y); plot(cx - x, cy - y);
                        plot(cx + y, cy + x); plot(cx - y, cy + x);
                        plot(cx + y, cy - x); plot(cx - y, cy - x);
                        if (d < 0)
                            d += 4 * x + 6;
                        else {
                            d += 4 * (x - y) + 10;
                            --y;
                        }
                        ++x;
                    }
                }
                break;
            }
            case TouchEffectType::Particles: {
                for (auto& ps : st.particles) {
                    int gx = (int)std::round(ps.x);
                    int gy = (int)std::round(ps.y);
                    if (gx >= 0 && gx < 42 && gy >= 0 && gy < 24) {
                        float alpha = (1.0f - ps.age / ps.lifetime) * ps.brightness;
                        pixels.push_back({gx, gy, baseColor, alpha});
                    }
                }
                break;
            }
            case TouchEffectType::Pulse:
            case TouchEffectType::Breathe: {
                // Brightness multiplier applied to shape's own pixels
                if (shape && st.modValue > 0.01f) {
                    auto shapePixels = shape->gridPixels();
                    float mult = st.modValue * p.intensity;
                    Color7 bright = brighten(baseColor, 0.5f + mult * 1.5f);
                    for (auto& [px, py] : shapePixels) {
                        if (px >= 0 && px < 42 && py >= 0 && py < 24)
                            pixels.push_back({px, py, bright, mult * 0.5f});
                    }
                }
                break;
            }
            case TouchEffectType::Spin: {
                // Spin dots orbit around finger position (or shape center)
                float cx = st.prevX, cy = st.prevY;
                if (cx < 0 && shape) {
                    auto bb = shape->bbox();
                    cx = (bb.xMin + bb.xMax) * 0.5f;
                    cy = (bb.yMin + bb.yMax) * 0.5f;
                }
                for (auto& sd : st.spinDots) {
                    float px = cx + sd.radius * std::cos(sd.angle);
                    float py = cy + sd.radius * std::sin(sd.angle);
                    int gx = (int)std::round(px);
                    int gy = (int)std::round(py);
                    if (gx >= 0 && gx < 42 && gy >= 0 && gy < 24)
                        pixels.push_back({gx, gy, baseColor, sd.brightness});
                }
                break;
            }
            case TouchEffectType::Orbit: {
                // Orbit dots around pivot point
                if (st.orbit.hasPivot) {
                    // Draw pivot cross
                    int px = (int)std::round(st.orbit.pivotX);
                    int py = (int)std::round(st.orbit.pivotY);
                    for (int d = -1; d <= 1; ++d) {
                        if (px+d >= 0 && px+d < 42 && py >= 0 && py < 24)
                            pixels.push_back({px+d, py, baseColor, p.intensity * 0.5f});
                        if (px >= 0 && px < 42 && py+d >= 0 && py+d < 24)
                            pixels.push_back({px, py+d, baseColor, p.intensity * 0.5f});
                    }
                    // Draw orbiting dots
                    for (auto& od : st.orbitDots) {
                        float ox = st.orbit.pivotX + od.radius * std::cos(od.angle);
                        float oy = st.orbit.pivotY + od.radius * std::sin(od.angle);
                        int gx = (int)std::round(ox);
                        int gy = (int)std::round(oy);
                        if (gx >= 0 && gx < 42 && gy >= 0 && gy < 24)
                            pixels.push_back({gx, gy, baseColor, od.brightness});
                    }
                }
                break;
            }
            case TouchEffectType::Boundary: {
                // Draw convex hull edges + fill
                auto& hull = st.convexHull;
                if (hull.size() >= 2) {
                    // Draw edges using Bresenham lines
                    int n = (int)hull.size();
                    for (int i = 0; i < n; ++i) {
                        int j = (i + 1) % n;
                        int x0 = (int)std::round(hull[i].first);
                        int y0 = (int)std::round(hull[i].second);
                        int x1 = (int)std::round(hull[j].first);
                        int y1 = (int)std::round(hull[j].second);
                        // Bresenham line
                        int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
                        int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
                        int err = dx + dy;
                        int cx = x0, cy = y0;
                        for (;;) {
                            if (cx >= 0 && cx < 42 && cy >= 0 && cy < 24)
                                pixels.push_back({cx, cy, baseColor, p.intensity});
                            if (cx == x1 && cy == y1) break;
                            int e2 = 2 * err;
                            if (e2 >= dy) { err += dy; cx += sx; }
                            if (e2 <= dx) { err += dx; cy += sy; }
                        }
                    }
                    // Fill interior with dim color (scanline)
                    if (hull.size() >= 3) {
                        float minY = 24, maxY = 0;
                        for (auto& h : hull) { minY = std::min(minY, h.second); maxY = std::max(maxY, h.second); }
                        for (int sy = std::max(0, (int)minY); sy <= std::min(23, (int)maxY); ++sy) {
                            // Find x intersections
                            std::vector<float> xs;
                            for (int i = 0; i < n; ++i) {
                                int j = (i + 1) % n;
                                float y0f = hull[i].second, y1f = hull[j].second;
                                if ((y0f <= sy && y1f > sy) || (y1f <= sy && y0f > sy)) {
                                    float t = ((float)sy - y0f) / (y1f - y0f);
                                    xs.push_back(hull[i].first + t * (hull[j].first - hull[i].first));
                                }
                            }
                            std::sort(xs.begin(), xs.end());
                            for (int k = 0; k + 1 < (int)xs.size(); k += 2) {
                                int xStart = std::max(0, (int)std::ceil(xs[k]));
                                int xEnd = std::min(41, (int)std::floor(xs[k+1]));
                                for (int sx = xStart; sx <= xEnd; ++sx)
                                    pixels.push_back({sx, sy, baseColor, p.intensity * 0.3f});
                            }
                        }
                    }
                }
                // Draw finger positions as bright dots
                for (auto& bf : st.boundaryFingers) {
                    int gx = (int)std::round(bf.x);
                    int gy = (int)std::round(bf.y);
                    if (gx >= 0 && gx < 42 && gy >= 0 && gy < 24)
                        pixels.push_back({gx, gy, baseColor, p.intensity});
                }
                break;
            }
            case TouchEffectType::String: {
                auto& ss = st.stringState;
                if (!ss.hasA || !ss.hasB || ss.displacement.empty()) break;
                int N = (int)ss.displacement.size();
                float dx = ss.bx - ss.ax, dy = ss.by - ss.ay;
                float len = std::sqrt(dx*dx + dy*dy);
                if (len < 0.1f) break;
                float nx = -dy/len, ny = dx/len; // perpendicular
                for (int i = 0; i < N; ++i) {
                    float t = (float)i / (N - 1);
                    float px = ss.ax + t * dx + ss.displacement[i] * nx;
                    float py = ss.ay + t * dy + ss.displacement[i] * ny;
                    int gx = (int)std::round(px), gy = (int)std::round(py);
                    if (gx >= 0 && gx < 42 && gy >= 0 && gy < 24) {
                        float alpha = p.intensity * std::min(1.0f, 0.3f + std::abs(ss.displacement[i]) * 0.5f);
                        pixels.push_back({gx, gy, baseColor, alpha});
                    }
                }
                break;
            }
            case TouchEffectType::Membrane: {
                auto& ms = st.membraneState;
                if (!ms.displacement.valid()) break;
                for (int y = 0; y < ms.displacement.height; ++y)
                    for (int x = 0; x < ms.displacement.width; ++x) {
                        float val = std::abs(ms.displacement.get(x, y));
                        if (val > 0.01f) {
                            float alpha = std::min(1.0f, val * 0.5f) * p.intensity;
                            pixels.push_back({x, y, baseColor, alpha});
                        }
                    }
                break;
            }
            case TouchEffectType::Fluid: {
                auto& fs = st.fluidState;
                if (!fs.density.valid()) break;
                for (int y = 0; y < fs.density.height; ++y)
                    for (int x = 0; x < fs.density.width; ++x) {
                        float d = fs.density.get(x, y);
                        if (d > 0.01f) {
                            float alpha = std::min(1.0f, d) * p.intensity;
                            pixels.push_back({x, y, baseColor, alpha});
                        }
                    }
                break;
            }
            case TouchEffectType::SpringLattice: {
                auto& sp = st.springState;
                if (!sp.displacement.valid()) break;
                // Draw bright nodes at non-zero displacement
                for (int y = 0; y < sp.displacement.height; y += 2)
                    for (int x = 0; x < sp.displacement.width; x += 2) {
                        float val = std::abs(sp.displacement.get(x, y));
                        if (val > 0.01f) {
                            float alpha = std::min(1.0f, val * 0.8f + 0.2f) * p.intensity;
                            pixels.push_back({x, y, baseColor, alpha});
                        }
                    }
                break;
            }
            case TouchEffectType::Pendulum: {
                auto& ps = st.pendulumState;
                // Draw arm(s) and bob(s) using Bresenham
                float bob1x = ps.pivotX + ps.length1 * std::sin(ps.theta1);
                float bob1y = ps.pivotY + ps.length1 * std::cos(ps.theta1);
                auto drawLine = [&](int x0, int y0, int x1, int y1, float alpha) {
                    int ddx = std::abs(x1-x0), sx = x0<x1?1:-1;
                    int ddy = -std::abs(y1-y0), sy = y0<y1?1:-1;
                    int err = ddx+ddy, cx = x0, cy = y0;
                    for(;;) {
                        if (cx>=0&&cx<42&&cy>=0&&cy<24) pixels.push_back({cx,cy,baseColor,alpha*0.5f});
                        if (cx==x1&&cy==y1) break;
                        int e2=2*err;
                        if(e2>=ddy){err+=ddy;cx+=sx;}
                        if(e2<=ddx){err+=ddx;cy+=sy;}
                    }
                };
                drawLine((int)std::round(ps.pivotX),(int)std::round(ps.pivotY),
                         (int)std::round(bob1x),(int)std::round(bob1y), p.intensity);
                // Bob dot
                int bx = (int)std::round(bob1x), by = (int)std::round(bob1y);
                if (bx>=0&&bx<42&&by>=0&&by<24) pixels.push_back({bx,by,baseColor,p.intensity});
                if (ps.isDouble) {
                    float bob2x = bob1x + ps.length2*std::sin(ps.theta2);
                    float bob2y = bob1y + ps.length2*std::cos(ps.theta2);
                    drawLine((int)std::round(bob1x),(int)std::round(bob1y),
                             (int)std::round(bob2x),(int)std::round(bob2y), p.intensity);
                    int b2x=(int)std::round(bob2x), b2y=(int)std::round(bob2y);
                    if(b2x>=0&&b2x<42&&b2y>=0&&b2y<24) pixels.push_back({b2x,b2y,baseColor,p.intensity});
                }
                // Trail
                for (int i = 0; i < (int)ps.bobTrail.size(); ++i) {
                    float alpha = (float)i / ps.bobTrail.size() * p.intensity * 0.4f;
                    int tx=(int)std::round(ps.bobTrail[i].first), ty=(int)std::round(ps.bobTrail[i].second);
                    if(tx>=0&&tx<42&&ty>=0&&ty<24) pixels.push_back({tx,ty,baseColor,alpha});
                }
                break;
            }
            case TouchEffectType::Collision: {
                for (auto& b : st.collisionState.balls) {
                    int gx=(int)std::round(b.x), gy=(int)std::round(b.y);
                    if(gx>=0&&gx<42&&gy>=0&&gy<24)
                        pixels.push_back({gx,gy,baseColor,b.brightness});
                }
                break;
            }
            case TouchEffectType::Tombolo: {
                auto& ts = st.tomboloState;
                if (!ts.height.valid()) break;
                for (int y = 0; y < ts.height.height; ++y)
                    for (int x = 0; x < ts.height.width; ++x) {
                        float h = ts.height.get(x, y);
                        if (h > 0.05f) {
                            float alpha = std::min(1.0f, h * 0.25f) * p.intensity;
                            pixels.push_back({x, y, baseColor, alpha});
                        }
                    }
                break;
            }
            case TouchEffectType::GravityWell: {
                // Particles as bright pixels
                for (auto& gp : st.gravityState.particles) {
                    int gx=(int)std::round(gp.x), gy=(int)std::round(gp.y);
                    if(gx>=0&&gx<42&&gy>=0&&gy<24)
                        pixels.push_back({gx,gy,baseColor,gp.brightness});
                }
                break;
            }
            case TouchEffectType::ElasticBand: {
                auto& es = st.elasticState;
                // Bresenham lines between consecutive points
                for (int i = 0; i+1 < (int)es.points.size(); ++i) {
                    int x0=(int)std::round(es.points[i].x), y0=(int)std::round(es.points[i].y);
                    int x1=(int)std::round(es.points[i+1].x), y1=(int)std::round(es.points[i+1].y);
                    int ddx=std::abs(x1-x0), sx=x0<x1?1:-1;
                    int ddy=-std::abs(y1-y0), sy=y0<y1?1:-1;
                    int err=ddx+ddy, cx=x0, cy=y0;
                    for(;;) {
                        if(cx>=0&&cx<42&&cy>=0&&cy<24) pixels.push_back({cx,cy,baseColor,p.intensity*0.7f});
                        if(cx==x1&&cy==y1) break;
                        int e2=2*err;
                        if(e2>=ddy){err+=ddy;cx+=sx;}
                        if(e2<=ddx){err+=ddx;cy+=sy;}
                    }
                }
                // Bright dots at anchored points
                for (auto& pt : es.points) {
                    if (pt.anchored) {
                        int gx=(int)std::round(pt.x), gy=(int)std::round(pt.y);
                        if(gx>=0&&gx<42&&gy>=0&&gy<24) pixels.push_back({gx,gy,baseColor,p.intensity});
                    }
                }
                break;
            }
            case TouchEffectType::Bow: {
                auto& bs = st.bowState;
                // Bow position
                int bx=(int)std::round(bs.bowX), by=(int)std::round(bs.bowY);
                float bowBright = std::min(1.0f, 0.3f + std::abs(bs.displacement) * 2.0f) * p.intensity;
                if(bx>=0&&bx<42&&by>=0&&by<24) pixels.push_back({bx,by,baseColor,bowBright});
                // Waveform trail
                for (int i = 0; i < (int)bs.waveform.size(); ++i) {
                    int wx = bx - (int)bs.waveform.size() + i;
                    int wy = by + (int)std::round(bs.waveform[i] * 3.0f);
                    if(wx>=0&&wx<42&&wy>=0&&wy<24) pixels.push_back({wx,wy,baseColor,p.intensity*0.5f});
                }
                break;
            }
            case TouchEffectType::WaveInterference: {
                auto& ws = st.waveInterfState;
                if (!ws.field.valid()) break;
                for (int y = 0; y < ws.field.height; ++y)
                    for (int x = 0; x < ws.field.width; ++x) {
                        float val = (1.0f + ws.field.get(x, y)) * 0.5f; // normalize -1..1 to 0..1
                        if (val > 0.05f) {
                            float alpha = val * p.intensity;
                            pixels.push_back({x, y, baseColor, std::min(1.0f, alpha)});
                        }
                    }
                break;
            }
            default:
                break;
        }
    }

    return pixels;
}

// ============================================================
// Canvas drawing (screen-space, called from GridCanvas::paint)
// ============================================================
void drawEffects(
    juce::Graphics& g,
    const std::map<std::string, ShapeEffectState>& states,
    const std::map<std::string, EffectParams>& params,
    const std::map<std::string, const Shape*>& shapes,
    std::function<juce::Point<float>(juce::Point<float>)> gridToScreen,
    float cellPx)
{
    for (auto& [shapeId, st] : states) {
        auto pit = params.find(shapeId);
        if (pit == params.end()) continue;
        auto& p = pit->second;

        auto sit = shapes.find(shapeId);
        const Shape* shape = (sit != shapes.end()) ? sit->second : nullptr;
        Color7 baseColor = getEffectColor(p, shape);
        auto juceColor = baseColor.toJuceColour();

        switch (p.type) {
            case TouchEffectType::Trail: {
                for (auto& pt : st.trail) {
                    auto screen = gridToScreen({pt.x, pt.y});
                    float alpha = (1.0f - pt.age) * p.intensity;
                    float size = cellPx * (0.5f + 0.5f * (1.0f - pt.age));
                    g.setColour(juceColor.withAlpha(alpha));
                    g.fillEllipse(screen.x - size * 0.5f, screen.y - size * 0.5f, size, size);
                }
                break;
            }
            case TouchEffectType::Ripple: {
                for (auto& rip : st.ripples) {
                    float brightness = (1.0f - rip.age * p.decay) * p.intensity;
                    if (brightness <= 0.0f) continue;
                    auto center = gridToScreen({rip.cx, rip.cy});
                    float screenRadius = rip.radius * cellPx;
                    g.setColour(juceColor.withAlpha(brightness));
                    g.drawEllipse(center.x - screenRadius, center.y - screenRadius,
                                  screenRadius * 2.0f, screenRadius * 2.0f, 1.5f);
                }
                break;
            }
            case TouchEffectType::Particles: {
                for (auto& ps : st.particles) {
                    auto screen = gridToScreen({ps.x, ps.y});
                    float alpha = (1.0f - ps.age / ps.lifetime) * ps.brightness;
                    float size = cellPx * 0.6f;
                    g.setColour(juceColor.withAlpha(alpha));
                    g.fillEllipse(screen.x - size * 0.5f, screen.y - size * 0.5f, size, size);
                }
                break;
            }
            case TouchEffectType::Pulse:
            case TouchEffectType::Breathe: {
                if (shape && st.modValue > 0.01f) {
                    auto bb = shape->bbox();
                    auto topLeft = gridToScreen({bb.xMin, bb.yMin});
                    auto botRight = gridToScreen({bb.xMax, bb.yMax});
                    float w = botRight.x - topLeft.x;
                    float h = botRight.y - topLeft.y;
                    float alpha = st.modValue * p.intensity * 0.4f;
                    g.setColour(juceColor.withAlpha(alpha));
                    g.fillRect(topLeft.x, topLeft.y, w, h);
                }
                break;
            }
            case TouchEffectType::Spin: {
                float cx = st.prevX, cy = st.prevY;
                if (cx < 0 && shape) {
                    auto bb = shape->bbox();
                    cx = (bb.xMin + bb.xMax) * 0.5f;
                    cy = (bb.yMin + bb.yMax) * 0.5f;
                }
                auto center = gridToScreen({cx, cy});
                // Draw faint axis cross
                g.setColour(juceColor.withAlpha(0.15f));
                g.drawLine(center.x - cellPx * 3, center.y, center.x + cellPx * 3, center.y, 1.0f);
                g.drawLine(center.x, center.y - cellPx * 3, center.x, center.y + cellPx * 3, 1.0f);
                // Draw spinning dots with trails
                for (auto& sd : st.spinDots) {
                    float px = cx + sd.radius * std::cos(sd.angle);
                    float py = cy + sd.radius * std::sin(sd.angle);
                    auto screen = gridToScreen({px, py});
                    float size = cellPx * 0.7f;
                    g.setColour(juceColor.withAlpha(sd.brightness));
                    g.fillEllipse(screen.x - size * 0.5f, screen.y - size * 0.5f, size, size);
                    // Trail (previous position)
                    float prevAngle = sd.angle - 0.3f;
                    float tpx = cx + sd.radius * std::cos(prevAngle);
                    float tpy = cy + sd.radius * std::sin(prevAngle);
                    auto tscreen = gridToScreen({tpx, tpy});
                    g.setColour(juceColor.withAlpha(sd.brightness * 0.4f));
                    g.fillEllipse(tscreen.x - size * 0.3f, tscreen.y - size * 0.3f, size * 0.6f, size * 0.6f);
                }
                break;
            }
            case TouchEffectType::Orbit: {
                if (st.orbit.hasPivot) {
                    auto pivot = gridToScreen({st.orbit.pivotX, st.orbit.pivotY});
                    // Draw pivot marker (crosshair)
                    g.setColour(juceColor.withAlpha(p.intensity * 0.6f));
                    float arm = cellPx * 1.5f;
                    g.drawLine(pivot.x - arm, pivot.y, pivot.x + arm, pivot.y, 1.5f);
                    g.drawLine(pivot.x, pivot.y - arm, pivot.x, pivot.y + arm, 1.5f);
                    // Draw orbit circle outline
                    float screenR = st.orbit.orbitRadius * cellPx;
                    g.setColour(juceColor.withAlpha(0.2f));
                    g.drawEllipse(pivot.x - screenR, pivot.y - screenR,
                                  screenR * 2, screenR * 2, 1.0f);
                    // Draw orbiting dots
                    for (auto& od : st.orbitDots) {
                        float ox = st.orbit.pivotX + od.radius * std::cos(od.angle);
                        float oy = st.orbit.pivotY + od.radius * std::sin(od.angle);
                        auto screen = gridToScreen({ox, oy});
                        float size = cellPx * 0.7f;
                        g.setColour(juceColor.withAlpha(od.brightness));
                        g.fillEllipse(screen.x - size * 0.5f, screen.y - size * 0.5f, size, size);
                    }
                    // Draw line from pivot to control finger
                    if (st.orbit.hasControl) {
                        auto ctrl = gridToScreen({st.orbit.controlX, st.orbit.controlY});
                        g.setColour(juceColor.withAlpha(0.3f));
                        g.drawLine(pivot.x, pivot.y, ctrl.x, ctrl.y, 1.0f);
                    }
                }
                break;
            }
            case TouchEffectType::Boundary: {
                auto& hull = st.convexHull;
                if (hull.size() >= 2) {
                    // Draw hull edges
                    juce::Path path;
                    auto first = gridToScreen({hull[0].first, hull[0].second});
                    path.startNewSubPath(first.x, first.y);
                    for (int i = 1; i < (int)hull.size(); ++i) {
                        auto pt = gridToScreen({hull[i].first, hull[i].second});
                        path.lineTo(pt.x, pt.y);
                    }
                    path.closeSubPath();

                    // Fill
                    g.setColour(juceColor.withAlpha(p.intensity * 0.2f));
                    g.fillPath(path);
                    // Stroke
                    g.setColour(juceColor.withAlpha(p.intensity * 0.7f));
                    g.strokePath(path, juce::PathStrokeType(2.0f));
                }
                // Draw finger dots
                for (auto& bf : st.boundaryFingers) {
                    auto screen = gridToScreen({bf.x, bf.y});
                    float size = cellPx * 0.8f;
                    g.setColour(juceColor.withAlpha(p.intensity));
                    g.fillEllipse(screen.x - size * 0.5f, screen.y - size * 0.5f, size, size);
                    g.setColour(juceColor.brighter(0.3f).withAlpha(p.intensity));
                    g.drawEllipse(screen.x - size * 0.5f, screen.y - size * 0.5f, size, size, 1.0f);
                }
                break;
            }
            case TouchEffectType::String: {
                auto& ss = st.stringState;
                if (!ss.hasA || !ss.hasB || ss.displacement.empty()) break;
                int N = (int)ss.displacement.size();
                float dx = ss.bx - ss.ax, dy = ss.by - ss.ay;
                float len = std::sqrt(dx*dx + dy*dy);
                if (len < 0.1f) break;
                float nx = -dy/len, ny = dx/len;
                juce::Path path;
                for (int i = 0; i < N; ++i) {
                    float t = (float)i / (N-1);
                    float px = ss.ax + t*dx + ss.displacement[i]*nx;
                    float py = ss.ay + t*dy + ss.displacement[i]*ny;
                    auto screen = gridToScreen({px, py});
                    if (i == 0) path.startNewSubPath(screen.x, screen.y);
                    else path.lineTo(screen.x, screen.y);
                }
                g.setColour(juceColor.withAlpha(p.intensity));
                g.strokePath(path, juce::PathStrokeType(2.0f));
                // Endpoints
                auto sa = gridToScreen({ss.ax, ss.ay}), sb = gridToScreen({ss.bx, ss.by});
                g.fillEllipse(sa.x-3, sa.y-3, 6, 6);
                g.fillEllipse(sb.x-3, sb.y-3, 6, 6);
                break;
            }
            case TouchEffectType::Membrane: {
                auto& ms = st.membraneState;
                if (!ms.displacement.valid()) break;
                for (int y = 0; y < ms.displacement.height; ++y)
                    for (int x = 0; x < ms.displacement.width; ++x) {
                        float val = std::abs(ms.displacement.get(x,y));
                        if (val > 0.01f) {
                            auto screen = gridToScreen({(float)x, (float)y});
                            float alpha = std::min(1.0f, val*0.5f) * p.intensity;
                            g.setColour(juceColor.withAlpha(alpha));
                            g.fillRect(screen.x-cellPx*0.5f, screen.y-cellPx*0.5f, cellPx, cellPx);
                        }
                    }
                break;
            }
            case TouchEffectType::Fluid: {
                auto& fs = st.fluidState;
                if (!fs.density.valid()) break;
                for (int y = 0; y < fs.density.height; ++y)
                    for (int x = 0; x < fs.density.width; ++x) {
                        float d = fs.density.get(x,y);
                        if (d > 0.01f) {
                            auto screen = gridToScreen({(float)x, (float)y});
                            float alpha = std::min(1.0f, d) * p.intensity;
                            g.setColour(juceColor.withAlpha(alpha));
                            g.fillRect(screen.x-cellPx*0.5f, screen.y-cellPx*0.5f, cellPx, cellPx);
                        }
                    }
                break;
            }
            case TouchEffectType::SpringLattice: {
                auto& sp = st.springState;
                if (!sp.displacement.valid()) break;
                for (int y = 0; y < sp.displacement.height; y += 2)
                    for (int x = 0; x < sp.displacement.width; x += 2) {
                        float val = std::abs(sp.displacement.get(x,y));
                        if (val > 0.01f) {
                            auto screen = gridToScreen({(float)x, (float)y});
                            float size = cellPx * (0.3f + val * 0.5f);
                            g.setColour(juceColor.withAlpha(std::min(1.0f, val*0.8f+0.2f)*p.intensity));
                            g.fillEllipse(screen.x-size*0.5f, screen.y-size*0.5f, size, size);
                        }
                    }
                break;
            }
            case TouchEffectType::Pendulum: {
                auto& ps = st.pendulumState;
                float bob1x = ps.pivotX + ps.length1*std::sin(ps.theta1);
                float bob1y = ps.pivotY + ps.length1*std::cos(ps.theta1);
                auto pivotS = gridToScreen({ps.pivotX, ps.pivotY});
                auto bob1S = gridToScreen({bob1x, bob1y});
                g.setColour(juceColor.withAlpha(p.intensity*0.7f));
                g.drawLine(pivotS.x, pivotS.y, bob1S.x, bob1S.y, 2.0f);
                g.setColour(juceColor.withAlpha(p.intensity));
                g.fillEllipse(bob1S.x-cellPx*0.5f, bob1S.y-cellPx*0.5f, cellPx, cellPx);
                if (ps.isDouble) {
                    float bob2x = bob1x + ps.length2*std::sin(ps.theta2);
                    float bob2y = bob1y + ps.length2*std::cos(ps.theta2);
                    auto bob2S = gridToScreen({bob2x, bob2y});
                    g.setColour(juceColor.withAlpha(p.intensity*0.7f));
                    g.drawLine(bob1S.x, bob1S.y, bob2S.x, bob2S.y, 2.0f);
                    g.setColour(juceColor.withAlpha(p.intensity));
                    g.fillEllipse(bob2S.x-cellPx*0.5f, bob2S.y-cellPx*0.5f, cellPx, cellPx);
                }
                // Trail
                for (int i = 0; i < (int)ps.bobTrail.size(); ++i) {
                    float alpha = (float)i / ps.bobTrail.size() * p.intensity * 0.3f;
                    auto ts = gridToScreen({ps.bobTrail[i].first, ps.bobTrail[i].second});
                    g.setColour(juceColor.withAlpha(alpha));
                    g.fillEllipse(ts.x-2, ts.y-2, 4, 4);
                }
                // Pivot marker
                g.setColour(juceColor.withAlpha(p.intensity*0.5f));
                g.fillEllipse(pivotS.x-3, pivotS.y-3, 6, 6);
                break;
            }
            case TouchEffectType::Collision: {
                for (auto& b : st.collisionState.balls) {
                    auto screen = gridToScreen({b.x, b.y});
                    float size = cellPx * b.radius * 2.0f;
                    g.setColour(juceColor.withAlpha(b.brightness));
                    g.fillEllipse(screen.x-size*0.5f, screen.y-size*0.5f, size, size);
                }
                break;
            }
            case TouchEffectType::Tombolo: {
                auto& ts = st.tomboloState;
                if (!ts.height.valid()) break;
                for (int y = 0; y < ts.height.height; ++y)
                    for (int x = 0; x < ts.height.width; ++x) {
                        float h = ts.height.get(x,y);
                        if (h > 0.05f) {
                            auto screen = gridToScreen({(float)x, (float)y});
                            float alpha = std::min(1.0f, h*0.25f) * p.intensity;
                            g.setColour(juceColor.withAlpha(alpha));
                            g.fillRect(screen.x-cellPx*0.5f, screen.y-cellPx*0.5f, cellPx, cellPx);
                        }
                    }
                break;
            }
            case TouchEffectType::GravityWell: {
                for (auto& gp : st.gravityState.particles) {
                    auto screen = gridToScreen({gp.x, gp.y});
                    float size = cellPx * 0.5f;
                    g.setColour(juceColor.withAlpha(gp.brightness));
                    g.fillEllipse(screen.x-size*0.5f, screen.y-size*0.5f, size, size);
                }
                break;
            }
            case TouchEffectType::ElasticBand: {
                auto& es = st.elasticState;
                if (es.points.size() >= 2) {
                    juce::Path path;
                    auto first = gridToScreen({es.points[0].x, es.points[0].y});
                    path.startNewSubPath(first.x, first.y);
                    for (int i = 1; i < (int)es.points.size(); ++i) {
                        auto pt = gridToScreen({es.points[i].x, es.points[i].y});
                        path.lineTo(pt.x, pt.y);
                    }
                    g.setColour(juceColor.withAlpha(p.intensity*0.7f));
                    g.strokePath(path, juce::PathStrokeType(2.0f));
                }
                for (auto& pt : es.points) {
                    auto screen = gridToScreen({pt.x, pt.y});
                    float size = pt.anchored ? cellPx : cellPx * 0.4f;
                    float alpha = pt.anchored ? p.intensity : p.intensity * 0.5f;
                    g.setColour(juceColor.withAlpha(alpha));
                    g.fillEllipse(screen.x-size*0.5f, screen.y-size*0.5f, size, size);
                }
                break;
            }
            case TouchEffectType::Bow: {
                auto& bs = st.bowState;
                auto bowS = gridToScreen({bs.bowX, bs.bowY});
                float bowBright = std::min(1.0f, 0.3f+std::abs(bs.displacement)*2.0f)*p.intensity;
                g.setColour(juceColor.withAlpha(bowBright));
                g.fillEllipse(bowS.x-cellPx*0.5f, bowS.y-cellPx*0.5f, cellPx, cellPx);
                // Waveform
                if (bs.waveform.size() >= 2) {
                    juce::Path wPath;
                    for (int i = 0; i < (int)bs.waveform.size(); ++i) {
                        float wx = bs.bowX - (float)(bs.waveform.size()-i)*0.5f;
                        float wy = bs.bowY + bs.waveform[i]*3.0f;
                        auto ws = gridToScreen({wx, wy});
                        if (i == 0) wPath.startNewSubPath(ws.x, ws.y);
                        else wPath.lineTo(ws.x, ws.y);
                    }
                    g.setColour(juceColor.withAlpha(p.intensity*0.5f));
                    g.strokePath(wPath, juce::PathStrokeType(1.5f));
                }
                break;
            }
            case TouchEffectType::WaveInterference: {
                auto& ws = st.waveInterfState;
                if (!ws.field.valid()) break;
                for (int y = 0; y < ws.field.height; ++y)
                    for (int x = 0; x < ws.field.width; ++x) {
                        float val = (1.0f + ws.field.get(x,y)) * 0.5f;
                        if (val > 0.05f) {
                            auto screen = gridToScreen({(float)x, (float)y});
                            float alpha = std::min(1.0f, val) * p.intensity;
                            g.setColour(juceColor.withAlpha(alpha));
                            g.fillRect(screen.x-cellPx*0.5f, screen.y-cellPx*0.5f, cellPx, cellPx);
                        }
                    }
                // Draw source markers
                for (auto& src : ws.sources) {
                    auto screen = gridToScreen({src.x, src.y});
                    g.setColour(juceColor.withAlpha(p.intensity));
                    g.drawEllipse(screen.x-cellPx, screen.y-cellPx, cellPx*2, cellPx*2, 1.5f);
                }
                break;
            }
            default:
                break;
        }
    }
}

} // namespace EffectRenderer
} // namespace erae
