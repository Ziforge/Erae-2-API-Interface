// Automated tests for built-in effect templates & shape library
// Compile: cmake --build build --target EraeTests
// Run:     ./build/EraeTests_artefacts/Release/Erae\ Tests

#include <juce_core/juce_core.h>
#include "../Source/Model/Shape.h"
#include "../Source/Model/Preset.h"
#include "../Source/Core/ShapeLibrary.h"
#include "../Source/Effects/TouchEffect.h"
// Note: TouchEffectEngine.cpp is not linked into the test binary
// (heavy dependencies). We include the header for type definitions
// and test parseParams logic inline.
#include "../Source/Effects/TouchEffectEngine.h"

using namespace erae;

static int passed = 0, failed = 0;

#define CHECK(cond, msg) do { \
    if (cond) { ++passed; } \
    else { ++failed; fprintf(stderr, "FAIL: %s\n", msg); } \
} while(0)

#define CHECK_EQ(a, b, msg) do { \
    if ((a) == (b)) { ++passed; } \
    else { ++failed; fprintf(stderr, "FAIL: %s (got %d, expected %d)\n", msg, (int)(a), (int)(b)); } \
} while(0)

#define CHECK_NEAR(a, b, tol, msg) do { \
    if (std::abs((a) - (b)) <= (tol)) { ++passed; } \
    else { ++failed; fprintf(stderr, "FAIL: %s (got %f, expected %f, tol %f)\n", msg, (double)(a), (double)(b), (double)(tol)); } \
} while(0)

// ============================================================
// Test 1: effectTemplates() produces 19 valid entries
// ============================================================
static void testEffectTemplates()
{
    fprintf(stdout, "--- Test: effectTemplates() ---\n");
    auto templates = Preset::effectTemplates();

    CHECK_EQ((int)templates.size(), 19, "effectTemplates returns 19 entries");

    // Expected: name, effect type string, shape type
    struct Expected {
        const char* name;
        const char* effectType;
        ShapeType shapeType;
    };
    static const Expected expected[] = {
        {"Trail",             "trail",             ShapeType::Rect},
        {"Ripple",            "ripple",            ShapeType::Circle},
        {"Particles",         "particles",         ShapeType::Rect},
        {"Pulse",             "pulse",             ShapeType::Circle},
        {"Breathe",           "breathe",           ShapeType::Circle},
        {"Spin",              "spin",              ShapeType::Circle},
        {"Orbit",             "orbit",             ShapeType::Circle},
        {"Boundary",          "boundary",          ShapeType::Rect},
        {"String",            "string",            ShapeType::Rect},
        {"Membrane",          "membrane",          ShapeType::Circle},
        {"Fluid",             "fluid",             ShapeType::Rect},
        {"Spring Lattice",    "spring_lattice",    ShapeType::Rect},
        {"Pendulum",          "pendulum",          ShapeType::Rect},
        {"Collision",         "collision",         ShapeType::Rect},
        {"Tombolo",           "tombolo",           ShapeType::Hex},
        {"Gravity Well",      "gravity_well",      ShapeType::Circle},
        {"Elastic Band",      "elastic_band",      ShapeType::Rect},
        {"Bow",               "bow",               ShapeType::Rect},
        {"Wave Interference", "wave_interference", ShapeType::Circle},
    };

    for (int i = 0; i < 19 && i < (int)templates.size(); ++i) {
        auto& t = templates[(size_t)i];
        auto nameMatch = t.name == expected[i].name;
        CHECK(nameMatch, (std::string("name[") + std::to_string(i) + "]=" + t.name
                          + " expected " + expected[i].name).c_str());

        CHECK(t.shape != nullptr,
              (std::string("shape[") + std::to_string(i) + "] not null").c_str());
        if (!t.shape) continue;

        // Shape type
        CHECK_EQ((int)t.shape->type, (int)expected[i].shapeType,
                 (std::string("shapeType[") + std::to_string(i) + "] " + t.name).c_str());

        // Non-zero color
        CHECK(t.shape->color.r > 0 || t.shape->color.g > 0 || t.shape->color.b > 0,
              (std::string("color non-zero[") + std::to_string(i) + "]").c_str());

        // Behavior
        CHECK(t.shape->behavior == "note_pad",
              (std::string("behavior[") + std::to_string(i) + "]=" + t.shape->behavior).c_str());

        // Visual style
        CHECK(t.shape->visualStyle == "pressure_glow",
              (std::string("visualStyle[") + std::to_string(i) + "]=" + t.shape->visualStyle).c_str());

        // Effect params present
        auto effectVar = t.shape->behaviorParams.getProperty("effect", {});
        CHECK(effectVar.isObject(),
              (std::string("effect params present[") + std::to_string(i) + "]").c_str());

        if (effectVar.isObject()) {
            auto typeStr = effectVar.getProperty("type", "").toString().toStdString();
            CHECK(typeStr == expected[i].effectType,
                  (std::string("effectType[") + std::to_string(i) + "]=" + typeStr
                   + " expected " + expected[i].effectType).c_str());

            // Parse via effectFromString
            auto parsedType = effectFromString(typeStr);
            CHECK(parsedType != TouchEffectType::None,
                  (std::string("effectFromString[") + std::to_string(i) + "]").c_str());

            // Mod target should be MPE
            auto modStr = effectVar.getProperty("mod_target", "").toString().toStdString();
            CHECK(modStr == "mpe",
                  (std::string("modTarget[") + std::to_string(i) + "]=" + modStr).c_str());
        }

        // Description non-empty
        CHECK(!t.description.empty(),
              (std::string("description non-empty[") + std::to_string(i) + "]").c_str());

        // Bbox has positive area
        auto bb = t.shape->bbox();
        float area = (bb.xMax - bb.xMin) * (bb.yMax - bb.yMin);
        CHECK(area > 0.0f,
              (std::string("bbox area > 0[") + std::to_string(i) + "]").c_str());
    }
}

// ============================================================
// Test 2: Shape clone preserves type
// ============================================================
static void testShapeClone()
{
    fprintf(stdout, "--- Test: Shape::clone() type preservation ---\n");

    RectShape r("r1", 0, 0, 5, 3);
    r.behavior = "note_pad";
    r.visualStyle = "pressure_glow";
    auto rc = r.clone();
    CHECK_EQ((int)rc->type, (int)ShapeType::Rect, "RectShape clone type");
    CHECK(rc->behavior == "note_pad", "RectShape clone behavior");

    CircleShape c("c1", 5, 5, 3);
    c.color = {100, 50, 20};
    auto cc = c.clone();
    CHECK_EQ((int)cc->type, (int)ShapeType::Circle, "CircleShape clone type");
    CHECK_EQ(cc->color.r, 100, "CircleShape clone color.r");

    HexShape h("h1", 10, 10, 4);
    auto hc = h.clone();
    CHECK_EQ((int)hc->type, (int)ShapeType::Hex, "HexShape clone type");

    PolygonShape p("p1", 0, 0, {{0,0},{3,0},{3,3}});
    auto pc = p.clone();
    CHECK_EQ((int)pc->type, (int)ShapeType::Polygon, "PolygonShape clone type");

    PixelShape px("px1", 0, 0, {{0,0},{1,0},{0,1}});
    auto pxc = px.clone();
    CHECK_EQ((int)pxc->type, (int)ShapeType::Pixel, "PixelShape clone type");
}

// ============================================================
// Test 3: ShapeLibrary built-in protection
// ============================================================
static void testShapeLibrary()
{
    fprintf(stdout, "--- Test: ShapeLibrary ---\n");

    ShapeLibrary lib;
    CHECK_EQ(lib.builtinCount(), 19, "builtinCount == 19");
    CHECK_EQ(lib.numEntries(), 19, "initial numEntries == 19");

    // isBuiltin
    CHECK(lib.isBuiltin(0), "entry 0 is builtin");
    CHECK(lib.isBuiltin(18), "entry 18 is builtin");
    CHECK(!lib.isBuiltin(19), "entry 19 not builtin");
    CHECK(!lib.isBuiltin(-1), "entry -1 not builtin");

    // Built-in shape types match templates
    auto templates = Preset::effectTemplates();
    for (int i = 0; i < 19; ++i) {
        CHECK_EQ((int)lib.getEntry(i).shape->type, (int)templates[(size_t)i].shape->type,
                 (std::string("lib entry type[") + std::to_string(i) + "]").c_str());
    }

    // Delete built-in: no-op
    lib.removeEntry(0);
    CHECK_EQ(lib.numEntries(), 19, "delete builtin is no-op");

    // Add user entry
    RectShape userShape("user1", 0, 0, 4, 4);
    lib.addEntry("My Shape", &userShape);
    CHECK_EQ(lib.numEntries(), 20, "user entry added");
    CHECK(!lib.isBuiltin(19), "user entry not builtin");
    CHECK(lib.getEntry(19).name == "My Shape", "user entry name");

    // Delete user entry
    lib.removeEntry(19);
    CHECK_EQ(lib.numEntries(), 19, "user entry deleted");
}

// ============================================================
// Test 4: ShapeLibrary save/load round-trip
// ============================================================
static void testLibrarySaveLoad()
{
    fprintf(stdout, "--- Test: ShapeLibrary save/load ---\n");

    auto tmpFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("erae_test_library.json");

    // Create library with user entries
    {
        ShapeLibrary lib;
        RectShape r1("r1", 5, 5, 10, 8);
        r1.color = {100, 50, 25};
        r1.behavior = "note_pad";
        lib.addEntry("TestRect", &r1);

        CircleShape c1("c1", 10, 10, 5);
        c1.color = {25, 100, 50};
        lib.addEntry("TestCircle", &c1);

        CHECK(lib.save(tmpFile), "save succeeds");
        CHECK_EQ(lib.numEntries(), 21, "19 builtins + 2 user = 21");
    }

    // Load into fresh library
    {
        ShapeLibrary lib2;
        CHECK(lib2.load(tmpFile), "load succeeds");
        CHECK_EQ(lib2.builtinCount(), 19, "loaded builtinCount == 19");
        CHECK_EQ(lib2.numEntries(), 21, "loaded 19 builtins + 2 user = 21");

        // User entries preserved after built-ins
        CHECK(lib2.getEntry(19).name == "TestRect", "loaded user entry 0 name");
        CHECK(lib2.getEntry(20).name == "TestCircle", "loaded user entry 1 name");

        // User entry types preserved
        CHECK_EQ((int)lib2.getEntry(19).shape->type, (int)ShapeType::Rect, "loaded rect type");
        CHECK_EQ((int)lib2.getEntry(20).shape->type, (int)ShapeType::Circle, "loaded circle type");

        // Color preserved
        CHECK_EQ(lib2.getEntry(19).shape->color.r, 100, "loaded rect color.r");
    }

    tmpFile.deleteFile();
}

// ============================================================
// Test 5: placeOnCanvas creates correct types
// ============================================================
static void testPlaceOnCanvas()
{
    fprintf(stdout, "--- Test: placeOnCanvas ---\n");

    ShapeLibrary lib;
    Layout layout;
    UndoManager undoMgr;
    int counter = 0;

    // Place each built-in and verify type + effect params
    struct Expected {
        ShapeType type;
        const char* effectType;
    };
    static const Expected expected[] = {
        {ShapeType::Rect,   "trail"},
        {ShapeType::Circle, "ripple"},
        {ShapeType::Rect,   "particles"},
        {ShapeType::Circle, "pulse"},
        {ShapeType::Circle, "breathe"},
        {ShapeType::Circle, "spin"},
        {ShapeType::Circle, "orbit"},
        {ShapeType::Rect,   "boundary"},
        {ShapeType::Rect,   "string"},
        {ShapeType::Circle, "membrane"},
        {ShapeType::Rect,   "fluid"},
        {ShapeType::Rect,   "spring_lattice"},
        {ShapeType::Rect,   "pendulum"},
        {ShapeType::Rect,   "collision"},
        {ShapeType::Hex,    "tombolo"},
        {ShapeType::Circle, "gravity_well"},
        {ShapeType::Rect,   "elastic_band"},
        {ShapeType::Rect,   "bow"},
        {ShapeType::Circle, "wave_interference"},
    };

    for (int i = 0; i < 19; ++i) {
        std::string id = lib.placeOnCanvas(i, layout, undoMgr, 10.0f, 10.0f, counter);
        CHECK(!id.empty(),
              (std::string("placed[") + std::to_string(i) + "] has id").c_str());

        auto* shape = layout.getShape(id);
        CHECK(shape != nullptr,
              (std::string("placed[") + std::to_string(i) + "] on layout").c_str());
        if (!shape) continue;

        CHECK_EQ((int)shape->type, (int)expected[i].type,
                 (std::string("placed type[") + std::to_string(i) + "]").c_str());

        // Verify effect params survive placement
        auto effectVar = shape->behaviorParams.getProperty("effect", {});
        if (effectVar.isObject()) {
            auto typeStr = effectVar.getProperty("type", "").toString().toStdString();
            CHECK(typeStr == expected[i].effectType,
                  (std::string("placed effect[") + std::to_string(i) + "]=" + typeStr).c_str());
        } else {
            CHECK(false,
                  (std::string("placed effect params missing[") + std::to_string(i) + "]").c_str());
        }
    }

    CHECK_EQ(layout.numShapes(), 19, "19 shapes on layout after placement");
}

// ============================================================
// Test 6: Shape bbox dimensions match expectations
// ============================================================
static void testShapeDimensions()
{
    fprintf(stdout, "--- Test: Shape dimensions ---\n");

    auto templates = Preset::effectTemplates();

    // Expected dimensions (w, h from bbox)
    struct Dim { float w, h; };
    static const Dim expected[] = {
        {10, 8},   // Trail
        {10, 10},  // Ripple r=5 → bbox 10x10
        {10, 8},   // Particles
        {6, 6},    // Pulse r=3
        {6, 6},    // Breathe r=3
        {8, 8},    // Spin r=4
        {12, 12},  // Orbit r=6
        {12, 10},  // Boundary
        {18, 4},   // String
        {12, 12},  // Membrane r=6
        {14, 10},  // Fluid
        {10, 10},  // Spring Lattice
        {6, 12},   // Pendulum
        {12, 10},  // Collision
        {14, 14},  // Tombolo hex r=7 → bbox ~14x(14*0.866)
        {10, 10},  // Gravity Well r=5
        {16, 4},   // Elastic Band
        {14, 4},   // Bow
        {12, 12},  // Wave Interference r=6
    };

    for (int i = 0; i < 19 && i < (int)templates.size(); ++i) {
        auto bb = templates[(size_t)i].shape->bbox();
        float w = bb.xMax - bb.xMin;
        float h = bb.yMax - bb.yMin;

        // For hex, height is radius*sqrt(3) not diameter, allow tolerance
        float tol = 1.0f;  // 1 grid unit tolerance (hex rounding)
        CHECK(std::abs(w - expected[i].w) < tol,
              (std::string("width[") + std::to_string(i) + "]="
               + std::to_string(w) + " expected " + std::to_string(expected[i].w)).c_str());

        // Skip exact height check for hex (sqrt(3) factor)
        if (templates[(size_t)i].shape->type != ShapeType::Hex) {
            CHECK(std::abs(h - expected[i].h) < tol,
                  (std::string("height[") + std::to_string(i) + "]="
                   + std::to_string(h) + " expected " + std::to_string(expected[i].h)).c_str());
        } else {
            CHECK(h > 0.0f,
                  (std::string("hex height > 0[") + std::to_string(i) + "]").c_str());
        }
    }
}

// ============================================================
// Test 7: JSON serialization round-trip for each shape type
// ============================================================
static void testJsonRoundTrip()
{
    fprintf(stdout, "--- Test: JSON round-trip ---\n");

    auto templates = Preset::effectTemplates();

    // Collect shapes into a vector for serialization
    std::vector<std::unique_ptr<Shape>> shapes;
    for (auto& t : templates)
        shapes.push_back(t.shape->clone());

    // Serialize
    auto json = Preset::toJSON(shapes);
    CHECK(json.length() > 0, "toJSON produces non-empty string");

    // Deserialize
    auto loaded = Preset::fromJSON(json);
    CHECK_EQ((int)loaded.size(), 19, "fromJSON restores 19 shapes");

    for (int i = 0; i < 19 && i < (int)loaded.size(); ++i) {
        CHECK_EQ((int)loaded[(size_t)i]->type, (int)shapes[(size_t)i]->type,
                 (std::string("json round-trip type[") + std::to_string(i) + "]").c_str());
        CHECK_EQ(loaded[(size_t)i]->color.r, shapes[(size_t)i]->color.r,
                 (std::string("json round-trip color.r[") + std::to_string(i) + "]").c_str());
        CHECK(loaded[(size_t)i]->behavior == shapes[(size_t)i]->behavior,
              (std::string("json round-trip behavior[") + std::to_string(i) + "]").c_str());

        // Effect type preserved
        auto origEffect = shapes[(size_t)i]->behaviorParams.getProperty("effect", {});
        auto loadEffect = loaded[(size_t)i]->behaviorParams.getProperty("effect", {});
        if (origEffect.isObject() && loadEffect.isObject()) {
            auto origType = origEffect.getProperty("type", "").toString().toStdString();
            auto loadType = loadEffect.getProperty("type", "").toString().toStdString();
            CHECK(origType == loadType,
                  (std::string("json effect type[") + std::to_string(i) + "]").c_str());
        }
    }
}

// ============================================================
// Test 8: Unique colors across all 19 templates
// ============================================================
static void testUniqueColors()
{
    fprintf(stdout, "--- Test: Unique colors ---\n");

    auto templates = Preset::effectTemplates();
    std::set<std::tuple<int,int,int>> colors;
    for (auto& t : templates) {
        auto key = std::make_tuple(t.shape->color.r, t.shape->color.g, t.shape->color.b);
        colors.insert(key);
    }
    CHECK_EQ((int)colors.size(), 19, "all 19 templates have unique colors");
}

// ============================================================
// Test 9: Grid-field sizing — shape-relative grids
// ============================================================
static void testGridFieldSizing()
{
    fprintf(stdout, "--- Test: Grid-field sizing ---\n");

    // GridField init with various sizes (not always 42x24)
    {
        GridField gf;
        CHECK(!gf.valid(), "GridField starts invalid");
        gf.init(10, 8, 0.0f);
        CHECK(gf.valid(), "GridField valid after init");
        CHECK_EQ(gf.width, 10, "GridField width");
        CHECK_EQ(gf.height, 8, "GridField height");
    }

    // OOB access returns 0
    {
        GridField gf;
        gf.init(5, 5, 0.0f);
        gf.set(2, 2, 7.0f);
        CHECK(gf.get(2, 2) == 7.0f, "in-bounds get works");
        CHECK(gf.get(-1, 0) == 0.0f, "OOB x<0 returns 0");
        CHECK(gf.get(5, 0) == 0.0f, "OOB x>=w returns 0");
        CHECK(gf.get(0, -1) == 0.0f, "OOB y<0 returns 0");
        CHECK(gf.get(0, 5) == 0.0f, "OOB y>=h returns 0");
        CHECK(gf.get(100, 100) == 0.0f, "OOB large returns 0");
    }

    // OOB set/add are no-ops
    {
        GridField gf;
        gf.init(3, 3, 0.0f);
        gf.set(-1, 0, 99.0f);
        gf.add(3, 0, 99.0f);
        // Verify no crash and all values still 0
        for (int y = 0; y < 3; ++y)
            for (int x = 0; x < 3; ++x)
                CHECK(gf.get(x, y) == 0.0f,
                      (std::string("OOB set/add no corruption at ") +
                       std::to_string(x) + "," + std::to_string(y)).c_str());
    }

    // BBox → grid dims for rect shape
    {
        RectShape r("r1", 5, 3, 10, 8);
        auto bb = r.bbox();
        int w = std::max(1, (int)std::ceil(bb.xMax - bb.xMin));
        int h = std::max(1, (int)std::ceil(bb.yMax - bb.yMin));
        CHECK_EQ(w, 10, "rect grid width from bbox");
        CHECK_EQ(h, 8, "rect grid height from bbox");
    }

    // BBox → grid dims for circle shape
    {
        CircleShape c("c1", 10, 10, 6);
        auto bb = c.bbox();
        int w = std::max(1, (int)std::ceil(bb.xMax - bb.xMin));
        int h = std::max(1, (int)std::ceil(bb.yMax - bb.yMin));
        CHECK_EQ(w, 12, "circle grid width from bbox");
        CHECK_EQ(h, 12, "circle grid height from bbox");
    }

    // BBox → grid dims for hex shape
    {
        HexShape hex("h1", 10, 10, 7);
        auto bb = hex.bbox();
        int w = std::max(1, (int)std::ceil(bb.xMax - bb.xMin));
        int h = std::max(1, (int)std::ceil(bb.yMax - bb.yMin));
        CHECK(w == 14, "hex grid width from bbox");
        CHECK(h > 0 && h < 20, "hex grid height reasonable");
    }

    // Touch coordinate translation round-trip
    {
        // Shape at (5, 3) with bbox xMin=5, yMin=3
        RectShape r("r1", 5, 3, 8, 6);
        auto bb = r.bbox();
        float originX = bb.xMin;  // 5
        float originY = bb.yMin;  // 3

        // Absolute touch at (9, 5) → local (4, 2)
        float touchX = 9.0f, touchY = 5.0f;
        int localX = (int)std::round(touchX - originX);
        int localY = (int)std::round(touchY - originY);
        CHECK_EQ(localX, 4, "touch coord translation X");
        CHECK_EQ(localY, 2, "touch coord translation Y");

        // Back to absolute for rendering: local + origin
        int absX = localX + (int)std::round(originX);
        int absY = localY + (int)std::round(originY);
        CHECK_EQ(absX, 9, "render coord round-trip X");
        CHECK_EQ(absY, 5, "render coord round-trip Y");
    }

    // Small shapes create small grids (not 42x24)
    {
        RectShape small("s1", 20, 10, 3, 2);
        auto bb = small.bbox();
        int w = std::max(1, (int)std::ceil(bb.xMax - bb.xMin));
        int h = std::max(1, (int)std::ceil(bb.yMax - bb.yMin));
        CHECK_EQ(w, 3, "small rect grid width");
        CHECK_EQ(h, 2, "small rect grid height");
        CHECK(w < 42 && h < 24, "small grid is smaller than full surface");
    }

    // 1x1 shape creates valid 1x1 grid
    {
        RectShape tiny("t1", 0, 0, 1, 1);
        auto bb = tiny.bbox();
        int w = std::max(1, (int)std::ceil(bb.xMax - bb.xMin));
        int h = std::max(1, (int)std::ceil(bb.yMax - bb.yMin));
        CHECK_EQ(w, 1, "1x1 rect grid width");
        CHECK_EQ(h, 1, "1x1 rect grid height");
        GridField gf;
        gf.init(w, h, 0.0f);
        CHECK(gf.valid(), "1x1 grid is valid");
        gf.set(0, 0, 5.0f);
        CHECK(gf.get(0, 0) == 5.0f, "1x1 grid set/get");
    }
}

// ============================================================
// Test 10: Effect type string round-trips
// ============================================================
static void testEffectTypeStrings()
{
    fprintf(stdout, "--- Test: Effect type string round-trips ---\n");

    // All 19 types round-trip through effectToString → effectFromString
    static const TouchEffectType allTypes[] = {
        TouchEffectType::Trail, TouchEffectType::Ripple, TouchEffectType::Particles,
        TouchEffectType::Pulse, TouchEffectType::Breathe, TouchEffectType::Spin,
        TouchEffectType::Orbit, TouchEffectType::Boundary, TouchEffectType::String,
        TouchEffectType::Membrane, TouchEffectType::Fluid, TouchEffectType::SpringLattice,
        TouchEffectType::Pendulum, TouchEffectType::Collision, TouchEffectType::Tombolo,
        TouchEffectType::GravityWell, TouchEffectType::ElasticBand, TouchEffectType::Bow,
        TouchEffectType::WaveInterference
    };

    for (auto t : allTypes) {
        auto str = effectToString(t);
        auto back = effectFromString(str);
        CHECK(back == t,
              (std::string("effectType round-trip: ") + str).c_str());
        CHECK(!str.empty() && str != "none",
              (std::string("effectType string non-empty: ") + str).c_str());
    }

    // None round-trips
    CHECK(effectFromString("none") == TouchEffectType::None, "effectFromString(none) = None");
    CHECK(effectFromString("") == TouchEffectType::None, "effectFromString('') = None");
    CHECK(effectFromString("garbage") == TouchEffectType::None, "effectFromString(garbage) = None");
    CHECK(effectToString(TouchEffectType::None) == "none", "effectToString(None) = 'none'");

    // ModTarget round-trips
    static const ModTarget allMods[] = {
        ModTarget::MidiCC, ModTarget::PitchBend, ModTarget::Pressure,
        ModTarget::CV, ModTarget::OSC, ModTarget::MPE
    };
    for (auto m : allMods) {
        auto str = modTargetToString(m);
        auto back = modTargetFromString(str);
        CHECK(back == m,
              (std::string("modTarget round-trip: ") + str).c_str());
    }
    CHECK(modTargetFromString("") == ModTarget::None, "modTargetFromString('') = None");
    CHECK(modTargetFromString("xyz") == ModTarget::None, "modTargetFromString(xyz) = None");
}

// ============================================================
// Test 11: Effect params from behaviorParams for all 19 templates
// ============================================================
static void testParseParams()
{
    fprintf(stdout, "--- Test: Effect params parsing for all templates ---\n");

    // Replicate parseParams logic inline (TouchEffectEngine.cpp not linked)
    auto parseEffect = [](const Shape& shape) -> EffectParams {
        EffectParams p;
        auto* obj = shape.behaviorParams.getDynamicObject();
        if (!obj || !obj->hasProperty("effect")) return p;
        auto effectVar = obj->getProperty("effect");
        auto* eff = effectVar.getDynamicObject();
        if (!eff) return p;
        auto getStr = [&](const char* key, const char* def) -> std::string {
            return eff->hasProperty(key) ? eff->getProperty(key).toString().toStdString() : def;
        };
        auto getFloat = [&](const char* key, float def) -> float {
            return eff->hasProperty(key) ? (float)(double)eff->getProperty(key) : def;
        };
        auto getInt = [&](const char* key, int def) -> int {
            return eff->hasProperty(key) ? (int)eff->getProperty(key) : def;
        };
        p.type      = effectFromString(getStr("type", "none"));
        p.speed     = getFloat("speed", 1.0f);
        p.intensity = getFloat("intensity", 0.8f);
        p.decay     = getFloat("decay", 0.5f);
        p.modTarget = modTargetFromString(getStr("mod_target", "none"));
        p.modCC     = getInt("mod_cc", 74);
        return p;
    };

    auto templates = Preset::effectTemplates();

    static const char* expectedTypes[] = {
        "trail", "ripple", "particles", "pulse", "breathe", "spin", "orbit",
        "boundary", "string", "membrane", "fluid", "spring_lattice", "pendulum",
        "collision", "tombolo", "gravity_well", "elastic_band", "bow", "wave_interference"
    };

    for (int i = 0; i < 19 && i < (int)templates.size(); ++i) {
        auto p = parseEffect(*templates[(size_t)i].shape);

        // Type matches expected
        auto expectedType = effectFromString(expectedTypes[i]);
        CHECK(p.type == expectedType,
              (std::string("parseParams type[") + std::to_string(i) + "] " + expectedTypes[i]).c_str());

        // All templates use MPE modulation
        CHECK(p.modTarget == ModTarget::MPE,
              (std::string("parseParams modTarget[") + std::to_string(i) + "] = MPE").c_str());

        // Speed, intensity, decay within valid ranges
        CHECK(p.speed >= 0.1f && p.speed <= 5.0f,
              (std::string("parseParams speed in range[") + std::to_string(i) + "]").c_str());
        CHECK(p.intensity >= 0.0f && p.intensity <= 1.0f,
              (std::string("parseParams intensity in range[") + std::to_string(i) + "]").c_str());
        CHECK(p.decay >= 0.1f && p.decay <= 2.0f,
              (std::string("parseParams decay in range[") + std::to_string(i) + "]").c_str());

        // modCC in MIDI range
        CHECK(p.modCC >= 0 && p.modCC <= 127,
              (std::string("parseParams modCC valid[") + std::to_string(i) + "]").c_str());
    }

    // Shape with no effect returns None
    {
        RectShape plain("plain", 0, 0, 5, 5);
        auto p = parseEffect(plain);
        CHECK(p.type == TouchEffectType::None, "parseParams no effect = None");
        CHECK(p.modTarget == ModTarget::None, "parseParams no effect modTarget = None");
    }

    // Empty behaviorParams returns None
    {
        CircleShape c("c", 5, 5, 3);
        c.behaviorParams = juce::var();
        auto p = parseEffect(c);
        CHECK(p.type == TouchEffectType::None, "parseParams empty params = None");
    }
}

// ============================================================
// Test 12: ShapeLibrary rename
// ============================================================
static void testLibraryRename()
{
    fprintf(stdout, "--- Test: ShapeLibrary rename ---\n");

    ShapeLibrary lib;
    RectShape r("r1", 0, 0, 5, 5);
    lib.addEntry("Original", &r);
    int userIdx = lib.numEntries() - 1;

    // Rename user entry
    lib.renameEntry(userIdx, "Renamed");
    CHECK(lib.getEntry(userIdx).name == "Renamed", "rename user entry works");

    // Rename built-in is no-op
    std::string origName = lib.getEntry(0).name;
    lib.renameEntry(0, "Hacked");
    CHECK(lib.getEntry(0).name == origName, "rename builtin is no-op");

    // Rename with negative index is no-op (no crash)
    lib.renameEntry(-1, "Bad");
    CHECK(lib.getEntry(userIdx).name == "Renamed", "rename -1 no crash");

    // Rename with out-of-bounds index is no-op
    lib.renameEntry(999, "Bad");
    CHECK(lib.getEntry(userIdx).name == "Renamed", "rename OOB no crash");

    // Rename persists through save/load
    auto tmpFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("erae_test_rename.json");
    lib.save(tmpFile);
    ShapeLibrary lib2;
    lib2.load(tmpFile);
    CHECK(lib2.getEntry(lib2.numEntries() - 1).name == "Renamed",
          "rename persists after save/load");
    tmpFile.deleteFile();
}

// ============================================================
// Test 13: normToBBox logic
// ============================================================
static void testNormToBBox()
{
    fprintf(stdout, "--- Test: normToBBox logic ---\n");

    // Replicate the normToBBox helper inline for testing
    auto normToBBox = [](float absCoord, float bbMin, float bbMax) -> float {
        float range = bbMax - bbMin;
        return range > 0.0f ? std::clamp((absCoord - bbMin) / range, 0.0f, 1.0f) : 0.5f;
    };

    // Center of bbox → 0.5
    CHECK_NEAR(normToBBox(5.0f, 0.0f, 10.0f), 0.5f, 0.001f, "center = 0.5");

    // Min edge → 0.0
    CHECK_NEAR(normToBBox(0.0f, 0.0f, 10.0f), 0.0f, 0.001f, "min edge = 0.0");

    // Max edge → 1.0
    CHECK_NEAR(normToBBox(10.0f, 0.0f, 10.0f), 1.0f, 0.001f, "max edge = 1.0");

    // Below min → clamped to 0.0
    CHECK_NEAR(normToBBox(-5.0f, 0.0f, 10.0f), 0.0f, 0.001f, "below min = 0.0");

    // Above max → clamped to 1.0
    CHECK_NEAR(normToBBox(15.0f, 0.0f, 10.0f), 1.0f, 0.001f, "above max = 1.0");

    // Quarter point
    CHECK_NEAR(normToBBox(2.5f, 0.0f, 10.0f), 0.25f, 0.001f, "quarter = 0.25");

    // Offset bbox (not starting at 0)
    CHECK_NEAR(normToBBox(15.0f, 10.0f, 20.0f), 0.5f, 0.001f, "offset center = 0.5");
    CHECK_NEAR(normToBBox(10.0f, 10.0f, 20.0f), 0.0f, 0.001f, "offset min = 0.0");
    CHECK_NEAR(normToBBox(20.0f, 10.0f, 20.0f), 1.0f, 0.001f, "offset max = 1.0");

    // Zero-size bbox → 0.5 fallback
    CHECK_NEAR(normToBBox(5.0f, 5.0f, 5.0f), 0.5f, 0.001f, "zero range = 0.5");

    // Small bbox (1 unit)
    CHECK_NEAR(normToBBox(0.5f, 0.0f, 1.0f), 0.5f, 0.001f, "1-unit center = 0.5");

    // Large offset
    CHECK_NEAR(normToBBox(30.0f, 20.0f, 40.0f), 0.5f, 0.001f, "large offset center = 0.5");
}

// ============================================================
// Test 14: ShapeEffectState defaults
// ============================================================
static void testEffectStateDefaults()
{
    fprintf(stdout, "--- Test: ShapeEffectState defaults ---\n");

    ShapeEffectState st;

    CHECK_NEAR(st.modValue, 0.0f, 0.001f, "default modValue = 0");
    CHECK_NEAR(st.modX, 0.5f, 0.001f, "default modX = 0.5");
    CHECK_NEAR(st.modY, 0.5f, 0.001f, "default modY = 0.5");
    CHECK_NEAR(st.modZ, 0.0f, 0.001f, "default modZ = 0");
    CHECK(!st.touched, "default touched = false");
    CHECK_NEAR(st.phase, 0.0f, 0.001f, "default phase = 0");
    CHECK_NEAR(st.velocity, 0.0f, 0.001f, "default velocity = 0");
    CHECK_NEAR(st.gridOriginX, 0.0f, 0.001f, "default gridOriginX = 0");
    CHECK_NEAR(st.gridOriginY, 0.0f, 0.001f, "default gridOriginY = 0");
    CHECK(st.trail.empty(), "default trail empty");
    CHECK(st.ripples.empty(), "default ripples empty");
    CHECK(st.particles.empty(), "default particles empty");
    CHECK(st.spinDots.empty(), "default spinDots empty");
    CHECK(st.orbitDots.empty(), "default orbitDots empty");
    CHECK(st.boundaryFingers.empty(), "default boundaryFingers empty");
    CHECK(st.convexHull.empty(), "default convexHull empty");
}

// ============================================================
// Test 15: EffectParams defaults
// ============================================================
static void testEffectParamsDefaults()
{
    fprintf(stdout, "--- Test: EffectParams defaults ---\n");

    EffectParams p;

    CHECK(p.type == TouchEffectType::None, "default type = None");
    CHECK_NEAR(p.speed, 1.0f, 0.001f, "default speed = 1.0");
    CHECK_NEAR(p.intensity, 0.8f, 0.001f, "default intensity = 0.8");
    CHECK_NEAR(p.decay, 0.5f, 0.001f, "default decay = 0.5");
    CHECK(!p.motionReactive, "default motionReactive = false");
    CHECK(p.useShapeColor, "default useShapeColor = true");
    CHECK(p.modTarget == ModTarget::None, "default modTarget = None");
    CHECK_EQ(p.modCC, 74, "default modCC = 74");
    CHECK_EQ(p.modChannel, 0, "default modChannel = 0");
    CHECK_EQ(p.modCVCh, 0, "default modCVCh = 0");
    CHECK_EQ(p.mpeChannel, 1, "default mpeChannel = 1");
}

// ============================================================
// Test 16: GridField operations
// ============================================================
static void testGridFieldOps()
{
    fprintf(stdout, "--- Test: GridField operations ---\n");

    // Basic set/get
    {
        GridField gf;
        gf.init(10, 8, 0.0f);
        gf.set(5, 3, 42.0f);
        CHECK_NEAR(gf.get(5, 3), 42.0f, 0.001f, "set/get basic");
    }

    // add() accumulates
    {
        GridField gf;
        gf.init(4, 4, 0.0f);
        gf.set(1, 1, 10.0f);
        gf.add(1, 1, 5.0f);
        CHECK_NEAR(gf.get(1, 1), 15.0f, 0.001f, "add accumulates");
        gf.add(1, 1, -3.0f);
        CHECK_NEAR(gf.get(1, 1), 12.0f, 0.001f, "add negative");
    }

    // clear() zeros all
    {
        GridField gf;
        gf.init(5, 5, 0.0f);
        gf.set(2, 2, 99.0f);
        gf.set(4, 4, 77.0f);
        gf.clear();
        CHECK_NEAR(gf.get(2, 2), 0.0f, 0.001f, "clear zeros (2,2)");
        CHECK_NEAR(gf.get(4, 4), 0.0f, 0.001f, "clear zeros (4,4)");
        CHECK(gf.valid(), "still valid after clear");
    }

    // init with non-zero value
    {
        GridField gf;
        gf.init(3, 3, 5.0f);
        CHECK_NEAR(gf.get(0, 0), 5.0f, 0.001f, "init fill (0,0)");
        CHECK_NEAR(gf.get(2, 2), 5.0f, 0.001f, "init fill (2,2)");
    }

    // Large grid
    {
        GridField gf;
        gf.init(42, 24, 0.0f);
        CHECK(gf.valid(), "42x24 grid valid");
        CHECK_EQ(gf.width, 42, "42x24 width");
        CHECK_EQ(gf.height, 24, "42x24 height");
        gf.set(41, 23, 1.0f);
        CHECK_NEAR(gf.get(41, 23), 1.0f, 0.001f, "42x24 corner set/get");
        CHECK_NEAR(gf.get(42, 24), 0.0f, 0.001f, "42x24 OOB returns 0");
    }

    // Copy constructor / assignment
    {
        GridField gf;
        gf.init(5, 5, 0.0f);
        gf.set(2, 3, 88.0f);
        GridField gf2 = gf;
        CHECK(gf2.valid(), "copy is valid");
        CHECK_EQ(gf2.width, 5, "copy width");
        CHECK_NEAR(gf2.get(2, 3), 88.0f, 0.001f, "copy preserves data");

        // Modify original doesn't affect copy
        gf.set(2, 3, 0.0f);
        CHECK_NEAR(gf2.get(2, 3), 88.0f, 0.001f, "copy is independent");
    }
}

// ============================================================
// Test 17: initGridForShape logic (replicated — method is private)
// ============================================================
static void testInitGridForShape()
{
    fprintf(stdout, "--- Test: initGridForShape logic ---\n");

    // Helper replicating the private initGridForShape logic
    auto initGrid = [](GridField& gf, const BBox& bb) {
        if (!gf.valid()) {
            int w = std::max(1, (int)std::ceil(bb.xMax - bb.xMin));
            int h = std::max(1, (int)std::ceil(bb.yMax - bb.yMin));
            gf.init(w, h, 0.0f);
        }
    };

    // Rect shape
    {
        RectShape r("r1", 5, 3, 10, 8);
        auto bb = r.bbox();
        GridField gf;
        initGrid(gf, bb);
        CHECK(gf.valid(), "initGrid rect valid");
        CHECK_EQ(gf.width, 10, "initGrid rect width");
        CHECK_EQ(gf.height, 8, "initGrid rect height");
    }

    // Circle shape
    {
        CircleShape c("c1", 10, 10, 5);
        auto bb = c.bbox();
        GridField gf;
        initGrid(gf, bb);
        CHECK(gf.valid(), "initGrid circle valid");
        CHECK_EQ(gf.width, 10, "initGrid circle width");
        CHECK_EQ(gf.height, 10, "initGrid circle height");
    }

    // Small shape (3x2)
    {
        RectShape small("s1", 20, 10, 3, 2);
        auto bb = small.bbox();
        GridField gf;
        initGrid(gf, bb);
        CHECK(gf.valid(), "initGrid small valid");
        CHECK_EQ(gf.width, 3, "initGrid small width");
        CHECK_EQ(gf.height, 2, "initGrid small height");
    }

    // 1x1 shape
    {
        RectShape tiny("t1", 0, 0, 1, 1);
        auto bb = tiny.bbox();
        GridField gf;
        initGrid(gf, bb);
        CHECK(gf.valid(), "initGrid 1x1 valid");
        CHECK_EQ(gf.width, 1, "initGrid 1x1 width");
        CHECK_EQ(gf.height, 1, "initGrid 1x1 height");
    }

    // Does not re-init if already valid
    {
        RectShape r("r2", 0, 0, 10, 10);
        auto bb = r.bbox();
        GridField gf;
        gf.init(5, 5, 0.0f);  // pre-init with different size
        initGrid(gf, bb);
        CHECK_EQ(gf.width, 5, "initGrid no re-init width");
        CHECK_EQ(gf.height, 5, "initGrid no re-init height");
    }

    // Hex shape
    {
        HexShape h("h1", 10, 10, 6);
        auto bb = h.bbox();
        GridField gf;
        initGrid(gf, bb);
        CHECK(gf.valid(), "initGrid hex valid");
        CHECK(gf.width > 0, "initGrid hex width > 0");
        CHECK(gf.height > 0, "initGrid hex height > 0");
        CHECK(gf.width <= 14, "initGrid hex width reasonable");
        CHECK(gf.height <= 14, "initGrid hex height reasonable");
    }
}

// ============================================================
// Test 18: Library duplicate entry
// ============================================================
static void testLibraryDuplicate()
{
    fprintf(stdout, "--- Test: ShapeLibrary duplicate ---\n");

    ShapeLibrary lib;
    int origCount = lib.numEntries();

    // Duplicate a built-in
    auto& entry = lib.getEntry(0);  // Trail
    lib.addEntry(entry.name + " (copy)", entry.shape.get());
    CHECK_EQ(lib.numEntries(), origCount + 1, "duplicate increases count");
    CHECK(lib.getEntry(origCount).name == entry.name + " (copy)", "duplicate name correct");
    CHECK_EQ((int)lib.getEntry(origCount).shape->type, (int)entry.shape->type,
             "duplicate shape type matches");
    CHECK(!lib.isBuiltin(origCount), "duplicate is not builtin");

    // Duplicate preserves color
    CHECK_EQ(lib.getEntry(origCount).shape->color.r, entry.shape->color.r, "duplicate color.r");
    CHECK_EQ(lib.getEntry(origCount).shape->color.g, entry.shape->color.g, "duplicate color.g");
    CHECK_EQ(lib.getEntry(origCount).shape->color.b, entry.shape->color.b, "duplicate color.b");

    // Duplicate preserves behavior
    CHECK(lib.getEntry(origCount).shape->behavior == entry.shape->behavior,
          "duplicate behavior matches");

    // Duplicated shape is independent (modify original doesn't affect copy)
    auto origBB = entry.shape->bbox();
    auto dupBB = lib.getEntry(origCount).shape->bbox();
    CHECK_NEAR(origBB.xMax - origBB.xMin, dupBB.xMax - dupBB.xMin, 0.01f,
               "duplicate bbox width matches");
}

// ============================================================
// Test 19: Shape flip operations
// ============================================================
static void testShapeFlip()
{
    fprintf(stdout, "--- Test: Shape flip operations ---\n");

    // Flip H on rect — no-op (symmetric)
    {
        RectShape r("r1", 5, 5, 10, 8);
        auto bb1 = r.bbox();
        ShapeLibrary::flipHorizontal(r);
        auto bb2 = r.bbox();
        CHECK_NEAR(bb2.xMax - bb2.xMin, bb1.xMax - bb1.xMin, 0.01f, "flipH rect width unchanged");
        CHECK_NEAR(bb2.yMax - bb2.yMin, bb1.yMax - bb1.yMin, 0.01f, "flipH rect height unchanged");
    }

    // Flip V on rect — no-op (symmetric)
    {
        RectShape r("r2", 5, 5, 10, 8);
        auto bb1 = r.bbox();
        ShapeLibrary::flipVertical(r);
        auto bb2 = r.bbox();
        CHECK_NEAR(bb2.xMax - bb2.xMin, bb1.xMax - bb1.xMin, 0.01f, "flipV rect width unchanged");
        CHECK_NEAR(bb2.yMax - bb2.yMin, bb1.yMax - bb1.yMin, 0.01f, "flipV rect height unchanged");
    }

    // Flip H on polygon — changes vertices
    {
        PolygonShape p("p1", 0, 0, {{0,0},{4,0},{4,2},{0,2}});
        auto bb1 = p.bbox();
        ShapeLibrary::flipHorizontal(p);
        auto bb2 = p.bbox();
        // BBox dimensions should stay the same
        CHECK_NEAR(bb2.xMax - bb2.xMin, bb1.xMax - bb1.xMin, 0.01f, "flipH poly width unchanged");
        CHECK_NEAR(bb2.yMax - bb2.yMin, bb1.yMax - bb1.yMin, 0.01f, "flipH poly height unchanged");
    }

    // Flip V on polygon
    {
        PolygonShape p("p2", 0, 0, {{0,0},{4,0},{2,3}});
        auto bb1 = p.bbox();
        ShapeLibrary::flipVertical(p);
        auto bb2 = p.bbox();
        CHECK_NEAR(bb2.xMax - bb2.xMin, bb1.xMax - bb1.xMin, 0.01f, "flipV poly width unchanged");
        CHECK_NEAR(bb2.yMax - bb2.yMin, bb1.yMax - bb1.yMin, 0.01f, "flipV poly height unchanged");
    }

    // Flip H on pixel shape
    {
        PixelShape px("px1", 0, 0, {{0,0},{1,0},{2,0},{0,1}});
        ShapeLibrary::flipHorizontal(px);
        auto bb = px.bbox();
        CHECK(bb.xMax - bb.xMin > 0, "flipH pixel has width");
    }

    // Flip V on pixel shape
    {
        PixelShape px("px2", 0, 0, {{0,0},{1,0},{0,1},{0,2}});
        ShapeLibrary::flipVertical(px);
        auto bb = px.bbox();
        CHECK(bb.yMax - bb.yMin > 0, "flipV pixel has height");
    }
}

// ============================================================
// Test 20: ShapeEffectState grid origin usage
// ============================================================
static void testGridOriginUsage()
{
    fprintf(stdout, "--- Test: Grid origin usage ---\n");

    // Verify grid origin stored from bbox translates correctly
    auto templates = Preset::effectTemplates();
    for (int i = 0; i < (int)templates.size(); ++i) {
        auto bb = templates[(size_t)i].shape->bbox();
        ShapeEffectState st;
        st.gridOriginX = bb.xMin;
        st.gridOriginY = bb.yMin;

        // Grid origin should equal bbox min
        CHECK_NEAR(st.gridOriginX, bb.xMin, 0.01f,
                   (std::string("gridOriginX[") + std::to_string(i) + "]").c_str());
        CHECK_NEAR(st.gridOriginY, bb.yMin, 0.01f,
                   (std::string("gridOriginY[") + std::to_string(i) + "]").c_str());

        // Absolute pixel coord = local + origin; should be within surface
        int gridW = std::max(1, (int)std::ceil(bb.xMax - bb.xMin));
        int gridH = std::max(1, (int)std::ceil(bb.yMax - bb.yMin));
        int absMaxX = (gridW - 1) + (int)std::round(st.gridOriginX);
        int absMaxY = (gridH - 1) + (int)std::round(st.gridOriginY);
        CHECK(absMaxX < 42,
              (std::string("absMaxX < 42[") + std::to_string(i) + "]").c_str());
        CHECK(absMaxY < 24,
              (std::string("absMaxY < 24[") + std::to_string(i) + "]").c_str());
    }
}

// ============================================================
// Test 21: BBox consistency across shape types
// ============================================================
static void testBBoxConsistency()
{
    fprintf(stdout, "--- Test: BBox consistency ---\n");

    // Rect bbox matches constructor
    {
        RectShape r("r1", 3, 5, 10, 8);
        auto bb = r.bbox();
        CHECK_NEAR(bb.xMin, 3.0f, 0.01f, "rect xMin");
        CHECK_NEAR(bb.yMin, 5.0f, 0.01f, "rect yMin");
        CHECK_NEAR(bb.xMax, 13.0f, 0.01f, "rect xMax");
        CHECK_NEAR(bb.yMax, 13.0f, 0.01f, "rect yMax");
    }

    // Circle bbox is symmetric
    {
        CircleShape c("c1", 10, 10, 5);
        auto bb = c.bbox();
        float cx = (bb.xMin + bb.xMax) / 2.0f;
        float cy = (bb.yMin + bb.yMax) / 2.0f;
        CHECK_NEAR(cx, 10.0f, 0.1f, "circle center X");
        CHECK_NEAR(cy, 10.0f, 0.1f, "circle center Y");
        float w = bb.xMax - bb.xMin;
        float h = bb.yMax - bb.yMin;
        CHECK_NEAR(w, h, 0.1f, "circle bbox is square");
    }

    // Hex bbox center
    {
        HexShape h("h1", 15, 12, 4);
        auto bb = h.bbox();
        float cx = (bb.xMin + bb.xMax) / 2.0f;
        float cy = (bb.yMin + bb.yMax) / 2.0f;
        CHECK_NEAR(cx, 15.0f, 0.5f, "hex center X");
        CHECK_NEAR(cy, 12.0f, 0.5f, "hex center Y");
    }

    // All template shapes have positive-area bboxes
    {
        auto templates = Preset::effectTemplates();
        for (int i = 0; i < (int)templates.size(); ++i) {
            auto bb = templates[(size_t)i].shape->bbox();
            float w = bb.xMax - bb.xMin;
            float h = bb.yMax - bb.yMin;
            CHECK(w > 0.0f && h > 0.0f,
                  (std::string("template bbox positive area[") + std::to_string(i) + "]").c_str());
            CHECK(bb.xMin >= 0.0f && bb.yMin >= 0.0f,
                  (std::string("template bbox non-negative origin[") + std::to_string(i) + "]").c_str());
        }
    }
}

// ============================================================
// Test 22: Coordinate translation round-trip for all shape types
// ============================================================
static void testCoordTranslation()
{
    fprintf(stdout, "--- Test: Coordinate translation round-trip ---\n");

    // For each template shape, verify touch→local→absolute round-trip
    auto templates = Preset::effectTemplates();
    for (int i = 0; i < (int)templates.size(); ++i) {
        auto bb = templates[(size_t)i].shape->bbox();
        float originX = bb.xMin;
        float originY = bb.yMin;

        // Touch at center of shape
        float touchX = (bb.xMin + bb.xMax) / 2.0f;
        float touchY = (bb.yMin + bb.yMax) / 2.0f;

        // To local
        int localX = (int)std::round(touchX - originX);
        int localY = (int)std::round(touchY - originY);

        // Back to absolute
        float absX = (float)localX + originX;
        float absY = (float)localY + originY;

        CHECK(std::abs(absX - touchX) < 1.0f,
              (std::string("coord round-trip X[") + std::to_string(i) + "]").c_str());
        CHECK(std::abs(absY - touchY) < 1.0f,
              (std::string("coord round-trip Y[") + std::to_string(i) + "]").c_str());

        // Local coords should be non-negative and within grid
        int gridW = std::max(1, (int)std::ceil(bb.xMax - bb.xMin));
        int gridH = std::max(1, (int)std::ceil(bb.yMax - bb.yMin));
        CHECK(localX >= 0 && localX < gridW,
              (std::string("local X in grid[") + std::to_string(i) + "]").c_str());
        CHECK(localY >= 0 && localY < gridH,
              (std::string("local Y in grid[") + std::to_string(i) + "]").c_str());
    }
}

// ============================================================
// main
// ============================================================
int main()
{
    // Initialize JUCE (minimal — needed for var, JSON, File operations)
    juce::ScopedJuceInitialiser_GUI init;

    fprintf(stdout, "=== Erae Effect Template Tests ===\n\n");

    testEffectTemplates();
    testShapeClone();
    testShapeLibrary();
    testLibrarySaveLoad();
    testPlaceOnCanvas();
    testShapeDimensions();
    testJsonRoundTrip();
    testUniqueColors();
    testGridFieldSizing();
    testEffectTypeStrings();
    testParseParams();
    testLibraryRename();
    testNormToBBox();
    testEffectStateDefaults();
    testEffectParamsDefaults();
    testGridFieldOps();
    testInitGridForShape();
    testLibraryDuplicate();
    testShapeFlip();
    testGridOriginUsage();
    testBBoxConsistency();
    testCoordTranslation();

    fprintf(stdout, "\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
