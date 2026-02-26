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
// buchla_thunder — faithful recreation on 42x24 grid
//
// From the Sensel/Buchla Thunder overlay reference:
//   y  0-2:   4 trigger buttons (top strip)
//   y  2-12:  5+5 feathers (parallelogram strips) in V-chevron
//   y 12-19:  2 bird's tail pieces (large V-shaped pads)
//   y 19-24:  4 hexagonal palm pads
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

    // --- 4 trigger buttons across the top (y=0 to y=2) ---
    {
        // 4 buttons × 9 wide + 3 gaps × 2 = 42
        int trigW = 9;
        int xPositions[] = {0, 11, 22, 33};
        int trigNotes[] = {60, 61, 62, 63};
        for (int i = 0; i < 4; ++i) {
            float x = (float)xPositions[i];
            auto s = makePoly(
                "trig_" + std::to_string(i),
                {{x, 0}, {x + trigW, 0}, {x + trigW, 2}, {x, 2}},
                teal((float)(i * 8), 0.6f, 0.4f),
                teal((float)(i * 8), 0.6f, 1.0f),
                "trigger", noteParams(trigNotes[i]));
            s->visualStyle = "pressure_glow";
            shapes.push_back(std::move(s));
        }
    }

    // --- 5+5 feathers: V-chevron (y=2 to y=12, 10 rows) ---
    // Lean = 2 cells over 10 rows (~11°). Inner feathers at center,
    // outer feathers at edges. Packed tight (no gaps between feathers).
    // The V opens 4 cells at the bottom (x=19 to x=23).
    {
        float fTop = 2, fBot = 12;

        // Left feathers (inner→outer): top leans right of bottom
        struct FeatherDef { std::string id; float tL, tR, bL, bR; int note; float hue; };
        FeatherDef leftF[] = {
            {"feath_L1", 17, 21, 15, 19, 48, 20},   // inner  (W=4)
            {"feath_L2", 13, 17, 11, 15, 49, 15},   //        (W=4)
            {"feath_L3",  9, 13,  7, 11, 50, 10},   //        (W=4)
            {"feath_L4",  5,  9,  3,  7, 51,  5},   //        (W=4)
            {"feath_L5",  2,  5,  0,  3, 52,  0},   // outer  (W=3)
        };
        for (auto& f : leftF) {
            auto s = makePoly(f.id,
                {{f.tL, fTop}, {f.tR, fTop}, {f.bR, fBot}, {f.bL, fBot}},
                teal(f.hue, 0.8f, 0.5f), teal(f.hue, 0.8f, 1.0f),
                "note_pad", noteParams(f.note));
            s->visualStyle = "pressure_glow";
            shapes.push_back(std::move(s));
        }

        // Right feathers (mirror): top leans left of bottom
        FeatherDef rightF[] = {
            {"feath_R1", 21, 25, 23, 27, 55, 20},   // inner  (W=4)
            {"feath_R2", 25, 29, 27, 31, 56, 15},   //        (W=4)
            {"feath_R3", 29, 33, 31, 35, 57, 10},   //        (W=4)
            {"feath_R4", 33, 37, 35, 39, 58,  5},   //        (W=4)
            {"feath_R5", 37, 40, 39, 42, 59,  0},   // outer  (W=3)
        };
        for (auto& f : rightF) {
            auto s = makePoly(f.id,
                {{f.tL, fTop}, {f.tR, fTop}, {f.bR, fBot}, {f.bL, fBot}},
                teal(f.hue, 0.8f, 0.5f), teal(f.hue, 0.8f, 1.0f),
                "note_pad", noteParams(f.note));
            s->visualStyle = "pressure_glow";
            shapes.push_back(std::move(s));
        }
    }

    // --- Bird's tail: 2 large V-shaped pads (y=12 to y=19, 7 rows) ---
    // These continue the V downward — large parallelograms that meet
    // at the center top and spread apart. Lean = 7 cells over 7 rows.
    {
        float tTop = 12, tBot = 19;
        // Left tail: top(12,21) → bot(5,14), W=9, lean=7
        auto tailL = makePoly("tail_L",
            {{12, tTop}, {21, tTop}, {14, tBot}, {5, tBot}},
            teal(-5, 0.75f, 0.45f), teal(-5, 0.75f, 1.0f),
            "note_pad", noteParams(36));
        tailL->visualStyle = "pressure_glow";
        shapes.push_back(std::move(tailL));

        // Right tail: top(21,30) → bot(28,37), W=9, lean=7
        auto tailR = makePoly("tail_R",
            {{21, tTop}, {30, tTop}, {37, tBot}, {28, tBot}},
            teal(5, 0.75f, 0.45f), teal(5, 0.75f, 1.0f),
            "note_pad", noteParams(43));
        tailR->visualStyle = "pressure_glow";
        shapes.push_back(std::move(tailR));
    }

    // --- 4 hexagonal palm pads (center y=21.5, radius 2.5) ---
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

        // Outer pads: XY controllers for expressive palm control
        shapes.push_back(makeHex("hex_L", 5.0f,
            teal(-15, 0.85f, 0.55f), teal(-15, 0.85f, 1.0f),
            "xy_controller", xyParams(74, 71)));
        shapes.push_back(makeHex("hex_R", 37.0f,
            teal(25, 0.85f, 0.55f), teal(25, 0.85f, 1.0f),
            "xy_controller", xyParams(1, 2)));
        // Inner pads: note pads for bass notes
        shapes.push_back(makeHex("hex_CL", 17.0f,
            teal(5, 0.8f, 0.5f), teal(5, 0.8f, 1.0f),
            "note_pad", noteParams(24)));
        shapes.push_back(makeHex("hex_CR", 25.0f,
            teal(10, 0.8f, 0.5f), teal(10, 0.8f, 1.0f),
            "note_pad", noteParams(28)));
    }

    return shapes;
}

// ============================================================
// auto_harp — Omnichord-style: chord buttons + strum strings
//
//   y  0-1:  Major chord buttons (12 keys, channel 0)
//   y  2-3:  Minor chord buttons (12 keys, channel 1)
//   y  4-5:  7th chord buttons   (12 keys, channel 2)
//   y  6-23: 42 chromatic strum strings C3-F6 (channel 3)
//
// Natural notes (C,D,E,F,G,A,B) get 4-cell-wide buttons,
// sharps (C#,D#,F#,G#,A#) get 3-cell-wide buttons. Total = 42.
// ============================================================
std::vector<std::unique_ptr<Shape>> autoHarp(int gridW, int gridH)
{
    std::vector<std::unique_ptr<Shape>> shapes;

    // Note names for IDs
    static const char* noteNames[] = {"C","Cs","D","Ds","E","F","Fs","G","Gs","A","As","B"};
    // Natural notes are wider (4 cells), sharps are narrower (3 cells)
    // C=4 C#=3 D=4 D#=3 E=4 F=3 F#=4 G=3 G#=4 A=3 A#=4 B=3 = 42
    static const int buttonWidths[] = {4,3,4,3,4,3,4,3,4,3,4,3};

    // Chord button colors: warm palette by root, shifted per type
    // Major = saturated, Minor = desaturated/cool, 7th = warm/orange tint
    struct ChordRow {
        const char* suffix;
        int channel;
        float yTop;
        float saturation;
        float hueShift;
    };
    ChordRow rows[] = {
        {"maj", 0, 0.0f, 0.85f, 0.0f},    // Major: vivid
        {"min", 1, 2.0f, 0.65f, -20.0f},   // Minor: cooler
        {"7",   2, 4.0f, 0.80f, 15.0f},    // 7th: warmer
    };

    float btnH = 2.0f;

    for (auto& row : rows) {
        float xPos = 0;
        for (int i = 0; i < 12; ++i) {
            int rootNote = 48 + i; // C3-B3 as chord roots
            float w = (float)buttonWidths[i];
            float hue = (float)(i * 30) + row.hueShift;
            if (hue < 0) hue += 360.0f;

            std::string id = std::string("chord_") + noteNames[i] + "_" + row.suffix;
            auto s = makeRect(id, xPos, row.yTop, w, btnH,
                hsvToRgb7(hue, row.saturation, 0.45f),
                hsvToRgb7(hue, row.saturation, 1.0f),
                "trigger", noteParams(rootNote, row.channel));
            s->visualStyle = "pressure_glow";
            shapes.push_back(std::move(s));
            xPos += w;
        }
    }

    // Strum strings: 42 strings from C3 (48) to F6 (89), each 1 cell wide
    float strumTop = 6.0f;
    float strumH = (float)gridH - strumTop; // 18 cells
    int baseNote = 48; // C3

    for (int i = 0; i < gridW; ++i) {
        int note = baseNote + i; // C3=48 ... F6=89
        if (note > 127) break;
        int pc = note % 12; // pitch class
        float hue = (float)(pc * 30);
        bool isNatural = (pc == 0 || pc == 2 || pc == 4 || pc == 5 ||
                          pc == 7 || pc == 9 || pc == 11);

        std::string id = "strum_" + std::to_string(note);
        auto s = makeRect(id, (float)i, strumTop, 1.0f, strumH,
            hsvToRgb7(hue, isNatural ? 0.75f : 0.90f, isNatural ? 0.50f : 0.35f),
            hsvToRgb7(hue, 0.85f, 1.0f),
            "trigger", noteParams(note, 3));
        s->visualStyle = "pressure_glow";
        shapes.push_back(std::move(s));
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
        {"Auto Harp",      [] { return autoHarp(); }},
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
        else if (type == "pixel") {
            std::vector<std::pair<int,int>> cells;
            if (auto* carr = item.getProperty("cells", {}).getArray()) {
                for (auto& pt : *carr) {
                    if (auto* ptArr = pt.getArray(); ptArr && ptArr->size() >= 2)
                        cells.push_back({(int)(*ptArr)[0], (int)(*ptArr)[1]});
                }
            }
            shape = std::make_unique<PixelShape>(id, x, y, std::move(cells));
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
