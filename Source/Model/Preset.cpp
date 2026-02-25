#include "Preset.h"
#include "Color.h"
#include <cmath>

namespace erae {
namespace Preset {

// ============================================================
// Helper: make a RectShape with common params
// ============================================================
static std::unique_ptr<RectShape> makeRect(const std::string& id, float x, float y,
                                            float w, float h, Color7 col, Color7 colActive,
                                            const std::string& behavior, juce::var params, int z = 0)
{
    auto s = std::make_unique<RectShape>(id, x, y, w, h);
    s->color = col;
    s->colorActive = colActive;
    s->behavior = behavior;
    s->behaviorParams = params;
    s->zOrder = z;
    return s;
}

static juce::var noteParams(int note, int channel = 0)
{
    auto obj = new juce::DynamicObject();
    obj->setProperty("note", note);
    obj->setProperty("channel", channel);
    return juce::var(obj);
}

static juce::var ccParams(int cc, int channel = 0, bool highres = true)
{
    auto obj = new juce::DynamicObject();
    obj->setProperty("cc", cc);
    obj->setProperty("channel", channel);
    obj->setProperty("highres", highres);
    return juce::var(obj);
}

static juce::var xyParams(int ccX, int ccY, int channel = 0, bool highres = true)
{
    auto obj = new juce::DynamicObject();
    obj->setProperty("cc_x", ccX);
    obj->setProperty("cc_y", ccY);
    obj->setProperty("channel", channel);
    obj->setProperty("highres", highres);
    return juce::var(obj);
}

// ============================================================
// drum_pads — 4×4 MPC-style grid with chromatic coloring
// ============================================================
std::vector<std::unique_ptr<Shape>> drumPads(int rows, int cols, int baseNote,
                                              int gridW, int gridH)
{
    // Integer-aligned pad layout: distribute cells evenly with 1-cell gap
    int gap = 1;
    int usableW = gridW - (cols + 1) * gap;
    int usableH = gridH - (rows + 1) * gap;
    int padW = usableW / cols;
    int padH = usableH / rows;

    std::vector<std::unique_ptr<Shape>> shapes;

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            int note = baseNote + r * cols + c;
            int x = gap + c * (padW + gap);
            int y = gap + r * (padH + gap);
            auto s = makeRect(
                "drum_" + std::to_string(note),
                (float)x, (float)y, (float)padW, (float)padH,
                hsvToRgb7((float)((note % 12) * 30), 0.85f, 0.6f),
                hsvToRgb7((float)((note % 12) * 30), 0.85f, 1.0f),
                "trigger", noteParams(note, 9));
            s->visualStyle = "pressure_glow";
            shapes.push_back(std::move(s));
        }
    }
    return shapes;
}

// ============================================================
// piano — 3-octave keyboard
// ============================================================
std::vector<std::unique_ptr<Shape>> piano(int octaves, int startNote,
                                           int gridW, int gridH)
{
    const int whiteInOctave[] = {0, 2, 4, 5, 7, 9, 11};
    const int blackInOctave[] = {1, 3, 6, 8, 10};
    // Black key positions relative to white key index (in white-key units)
    const float blackPositions[] = {0.5f, 1.5f, 3.5f, 4.5f, 5.5f};

    int nWhites = octaves * 7;  // 21 white keys
    int whiteW = gridW / nWhites;  // 42/21 = 2 cells per white key
    std::vector<std::unique_ptr<Shape>> shapes;

    // White keys (lower z-order)
    for (int oct = 0; oct < octaves; ++oct) {
        for (int i = 0; i < 7; ++i) {
            int note = startNote + oct * 12 + whiteInOctave[i];
            int idx = oct * 7 + i;
            int x = idx * whiteW;
            auto w = makeRect(
                "piano_w_" + std::to_string(note),
                (float)x, 0, (float)whiteW, (float)gridH,
                Color7{38, 38, 38}, Color7{102, 102, 102},
                "note_pad", noteParams(note), 0);
            w->visualStyle = "pressure_glow";
            shapes.push_back(std::move(w));
        }
    }

    // Black keys (higher z-order)
    int blackH = (int)(gridH * 0.55f);  // ~13 cells
    int blackW = std::max(1, whiteW - 1);  // 1 cell wide
    for (int oct = 0; oct < octaves; ++oct) {
        for (int i = 0; i < 5; ++i) {
            int note = startNote + oct * 12 + blackInOctave[i];
            float pos = blackPositions[i];
            int x = (int)std::round((oct * 7 + pos) * whiteW + (whiteW - blackW) / 2.0f);
            int y = gridH - blackH;
            auto bk = makeRect(
                "piano_b_" + std::to_string(note),
                (float)x, (float)y, (float)blackW, (float)blackH,
                Color7{24, 24, 24}, Color7{0, 102, 102},
                "note_pad", noteParams(note), 1);
            bk->visualStyle = "pressure_glow";
            shapes.push_back(std::move(bk));
        }
    }

    return shapes;
}

// ============================================================
// wicki_hayden — isomorphic hex layout
// ============================================================
std::vector<std::unique_ptr<Shape>> wickiHayden(int rows, int cols, int baseNote,
                                                 int gridW, int gridH)
{
    // Use integer-friendly hex sizing: each hex ~3 cells wide
    int hexCellW = gridW / cols;
    float hexR = hexCellW / 2.0f;
    float rowH = (float)gridH / rows;

    std::vector<std::unique_ptr<Shape>> shapes;
    for (int r = 0; r < rows; ++r) {
        float xOff = (r % 2) ? hexCellW / 2.0f : 0;
        for (int c = 0; c < cols; ++c) {
            int note = baseNote + r * 7 + c * 2;
            if (note > 127) continue;
            float cx = std::round(xOff + c * hexCellW + hexR);
            float cy = std::round(r * rowH + rowH / 2.0f);
            auto hex = std::make_unique<HexShape>(
                "wh_" + std::to_string(r) + "_" + std::to_string(c),
                cx, cy, hexR * 0.9f);
            hex->color = hsvToRgb7((float)((note % 12) * 30), 0.85f, 0.6f);
            hex->colorActive = hsvToRgb7((float)((note % 12) * 30), 0.85f, 1.0f);
            hex->behavior = "note_pad";
            hex->behaviorParams = noteParams(note);
            hex->visualStyle = "pressure_glow";
            shapes.push_back(std::move(hex));
        }
    }
    return shapes;
}

// ============================================================
// fader_bank — 8 vertical faders
// ============================================================
std::vector<std::unique_ptr<Shape>> faderBank(int numFaders, int ccStart,
                                               int gridW, int gridH)
{
    int faderW = gridW / numFaders;  // 42/8 = 5 cells per fader (40 used, 2 spare)
    int gap = 1;
    int totalUsed = numFaders * faderW;
    int leftPad = (gridW - totalUsed) / 2;  // center the bank

    std::vector<std::unique_ptr<Shape>> shapes;

    for (int i = 0; i < numFaders; ++i) {
        int cc = ccStart + i;
        float hue = i * (360.0f / numFaders);
        int x = leftPad + i * faderW;
        auto s = makeRect(
            "fader_" + std::to_string(i),
            (float)(x + (gap > 0 ? 1 : 0)), 0,
            (float)(faderW - gap), (float)gridH,
            hsvToRgb7(hue, 0.7f, 0.6f),
            hsvToRgb7(hue, 0.7f, 1.0f),
            "fader", ccParams(cc));
        s->visualStyle = "fill_bar";
        shapes.push_back(std::move(s));
    }
    return shapes;
}

// ============================================================
// xy_pad — single large pad
// ============================================================
std::vector<std::unique_ptr<Shape>> xyPad(int gridW, int gridH)
{
    std::vector<std::unique_ptr<Shape>> shapes;
    auto s = makeRect(
        "xy_pad", 0, 0, (float)gridW, (float)gridH,
        hsvToRgb7(180, 0.85f, 0.6f),
        hsvToRgb7(180, 0.85f, 1.0f),
        "xy_controller", xyParams(1, 2));
    s->visualStyle = "position_dot";
    shapes.push_back(std::move(s));
    return shapes;
}

// ============================================================
// buchla_thunder — complex multi-section layout
// ============================================================
std::vector<std::unique_ptr<Shape>> buchlaThunder(int gridW, int gridH)
{
    std::vector<std::unique_ptr<Shape>> shapes;

    // Teal/cyan color palette inspired by Sensel Thunder overlay
    auto teal = [](float hShift, float sat, float val) {
        return hsvToRgb7(170.0f + hShift, sat, val);
    };

    // Helper: create a polygon from absolute vertex coords
    auto makePoly = [&](const std::string& id,
                        std::vector<std::pair<float,float>> absVerts,
                        Color7 col, Color7 colAct,
                        const std::string& beh, juce::var params) {
        float rx = absVerts[0].first, ry = absVerts[0].second;
        for (auto& [vx, vy] : absVerts) {
            rx = std::min(rx, vx); ry = std::min(ry, vy);
        }
        std::vector<std::pair<float,float>> rel;
        for (auto& [vx, vy] : absVerts)
            rel.push_back({vx - rx, vy - ry});
        auto s = std::make_unique<PolygonShape>(id, rx, ry, std::move(rel));
        s->color = col;
        s->colorActive = colAct;
        s->behavior = beh;
        s->behaviorParams = params;
        return s;
    };

    // =====================================================
    // Buchla Thunder on 42x24 grid
    //   Row 0-1:   7 trigger pads (unique colors)
    //   Row 2-8:   6 wide parallelograms (3L+3R) in V-chevron
    //   Row 10-18: 4 wider parallelograms (2L+2R) in V-chevron
    //   Row 19-23: 4 hexagonal pads
    // =====================================================

    // --- Top triggers: 7 pads, 2 rows tall, unique colors ---
    {
        int trigW = 5, trigGap = 1;
        int trigTotal = 7 * trigW + 6 * trigGap;  // 41
        int trigPad = (gridW - trigTotal) / 2;
        float trigHues[] = {0, 30, 60, 120, 180, 240, 300};
        for (int i = 0; i < 7; ++i) {
            float x = (float)(trigPad + i * (trigW + trigGap));
            auto s = makePoly(
                "trig_" + std::to_string(i),
                {{x, 0}, {x + trigW, 0}, {x + trigW, 2}, {x, 2}},
                hsvToRgb7(trigHues[i], 0.7f, 0.45f),
                hsvToRgb7(trigHues[i], 0.7f, 1.0f),
                "trigger", noteParams(60 + i));
            s->visualStyle = "pressure_glow";
            shapes.push_back(std::move(s));
        }
    }

    // --- Row 1 wings: 3 left + 3 right, V-chevron (y=2-9) ---
    {
        float r1t = 2, r1b = 9;
        // Outer shapes: width 6, lean 3. Inner shapes: width 4, lean 3.
        // Inner shapes meet at center (x=21) at top, V opens at bottom.
        struct WingDef { std::string id; float tL, tR, bL, bR; int note; float hue; };
        WingDef row1[] = {
            {"wing_L0",  3, 9,   0, 6,  48, 0},     // outer left
            {"wing_L1", 10, 16,  7, 13, 49, 10},    // middle left
            {"wing_L2", 17, 21, 14, 18, 50, 20},    // inner left
            {"wing_R2", 21, 25, 24, 28, 55, 20},    // inner right
            {"wing_R1", 26, 32, 29, 35, 56, 10},    // middle right
            {"wing_R0", 33, 39, 36, 42, 57, 0},     // outer right
        };
        for (auto& w : row1) {
            auto s = makePoly(w.id,
                {{w.tL, r1t}, {w.tR, r1t}, {w.bR, r1b}, {w.bL, r1b}},
                teal(w.hue, 0.8f, 0.5f), teal(w.hue, 0.8f, 1.0f),
                "note_pad", noteParams(w.note));
            s->visualStyle = "pressure_glow";
            shapes.push_back(std::move(s));
        }
    }

    // --- Row 2 wings: 2 left + 2 right, wider V-chevron (y=10-19) ---
    {
        float r2t = 10, r2b = 19;
        // Width 8, lean 5. Inner shapes meet at center (x=21) at top.
        struct WingDef { std::string id; float tL, tR, bL, bR; int note; float hue; };
        WingDef row2[] = {
            {"wing2_L0",  5, 13,  0, 8,  36, -5},   // outer left
            {"wing2_L1", 14, 21,  9, 16, 38, 5},    // inner left
            {"wing2_R1", 21, 28, 26, 33, 43, 5},    // inner right
            {"wing2_R0", 29, 37, 34, 42, 45, -5},   // outer right
        };
        for (auto& w : row2) {
            auto s = makePoly(w.id,
                {{w.tL, r2t}, {w.tR, r2t}, {w.bR, r2b}, {w.bL, r2b}},
                teal(w.hue, 0.75f, 0.45f), teal(w.hue, 0.75f, 1.0f),
                "note_pad", noteParams(w.note));
            s->visualStyle = "pressure_glow";
            shapes.push_back(std::move(s));
        }
    }

    // --- Bottom: 4 hexagonal pads (y~19-24, radius 2.5) ---
    {
        float hexY = 21.5f;
        float hexR = 2.5f;

        auto makeHex = [&](const std::string& id, float cx,
                           Color7 col, Color7 colAct,
                           const std::string& beh, juce::var params) {
            auto h = std::make_unique<HexShape>(id, cx, hexY, hexR);
            h->color = col;
            h->colorActive = colAct;
            h->behavior = beh;
            h->behaviorParams = params;
            h->visualStyle = "pressure_glow";
            return h;
        };

        shapes.push_back(makeHex("hex_L", 4.0f,
            teal(-15, 0.85f, 0.55f), teal(-15, 0.85f, 1.0f),
            "xy_controller", xyParams(74, 71)));
        shapes.push_back(makeHex("hex_R", 38.0f,
            teal(25, 0.85f, 0.55f), teal(25, 0.85f, 1.0f),
            "xy_controller", xyParams(1, 2)));
        shapes.push_back(makeHex("hex_CL", 16.0f,
            teal(5, 0.8f, 0.5f), teal(5, 0.8f, 1.0f),
            "note_pad", noteParams(24)));
        shapes.push_back(makeHex("hex_CR", 26.0f,
            teal(10, 0.8f, 0.5f), teal(10, 0.8f, 1.0f),
            "note_pad", noteParams(28)));
    }

    return shapes;
}

// ============================================================
// Generator registry
// ============================================================
const std::vector<GeneratorEntry>& getGenerators()
{
    static const std::vector<GeneratorEntry> generators = {
        {"Drum Pads",      [] { return drumPads(); }},
        {"Piano",          [] { return piano(); }},
        {"Wicki-Hayden",   [] { return wickiHayden(); }},
        {"Fader Bank",     [] { return faderBank(); }},
        {"XY Pad",         [] { return xyPad(); }},
        {"Buchla Thunder", [] { return buchlaThunder(); }},
    };
    return generators;
}

// ============================================================
// JSON serialization — compatible with Python erae_shapes format
// ============================================================
static Color7 parseColor(const juce::var& v)
{
    if (auto* arr = v.getArray()) {
        if (arr->size() >= 3)
            return {(int)(*arr)[0], (int)(*arr)[1], (int)(*arr)[2]};
    }
    return Palette::Blue;
}

juce::String toJSON(const std::vector<std::unique_ptr<Shape>>& shapes)
{
    juce::Array<juce::var> arr;
    for (auto& s : shapes)
        arr.add(s->toVar());

    auto root = new juce::DynamicObject();
    root->setProperty("shapes", arr);
    return juce::JSON::toString(juce::var(root));
}

std::vector<std::unique_ptr<Shape>> fromJSON(const juce::String& json)
{
    std::vector<std::unique_ptr<Shape>> shapes;

    auto parsed = juce::JSON::parse(json);
    if (!parsed.isObject()) return shapes;

    auto* shapesArr = parsed.getProperty("shapes", {}).getArray();
    if (!shapesArr) return shapes;

    for (auto& item : *shapesArr) {
        if (!item.isObject()) continue;

        auto id   = item.getProperty("id", "").toString().toStdString();
        auto type = item.getProperty("type", "rect").toString().toStdString();
        float x   = (float)item.getProperty("x", 0.0);
        float y   = (float)item.getProperty("y", 0.0);
        auto col      = parseColor(item.getProperty("color", {}));
        auto colAct   = parseColor(item.getProperty("color_active", {}));
        auto behavior = item.getProperty("behavior", "trigger").toString().toStdString();
        auto params   = item.getProperty("behavior_params", {});
        int zOrder    = (int)item.getProperty("z_order", 0);

        std::unique_ptr<Shape> shape;

        if (type == "rect") {
            float w = (float)item.getProperty("width", 1.0);
            float h = (float)item.getProperty("height", 1.0);
            shape = std::make_unique<RectShape>(id, x, y, w, h);
        }
        else if (type == "circle") {
            float r = (float)item.getProperty("radius", 1.0);
            shape = std::make_unique<CircleShape>(id, x, y, r);
        }
        else if (type == "hex") {
            float r = (float)item.getProperty("radius", 1.0);
            shape = std::make_unique<HexShape>(id, x, y, r);
        }
        else if (type == "polygon") {
            std::vector<std::pair<float,float>> verts;
            if (auto* varr = item.getProperty("vertices", {}).getArray()) {
                for (auto& pt : *varr) {
                    if (auto* ptArr = pt.getArray(); ptArr && ptArr->size() >= 2)
                        verts.push_back({(float)(*ptArr)[0], (float)(*ptArr)[1]});
                }
            }
            shape = std::make_unique<PolygonShape>(id, x, y, std::move(verts));
        }
        else {
            continue;
        }

        shape->color = col;
        shape->colorActive = colAct;
        shape->behavior = behavior;
        shape->behaviorParams = params;
        shape->zOrder = zOrder;
        shape->visualStyle = item.getProperty("visual_style", "static").toString().toStdString();
        shape->visualParams = item.getProperty("visual_params", {});
        shapes.push_back(std::move(shape));
    }

    return shapes;
}

bool saveToFile(const juce::File& file, const std::vector<std::unique_ptr<Shape>>& shapes)
{
    auto json = toJSON(shapes);
    return file.replaceWithText(json);
}

std::vector<std::unique_ptr<Shape>> loadFromFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return {};
    return fromJSON(file.loadFileAsString());
}

} // namespace Preset
} // namespace erae
