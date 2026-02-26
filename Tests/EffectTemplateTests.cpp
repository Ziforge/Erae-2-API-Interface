// Automated tests for built-in effect templates & shape library
// Compile: cmake --build build --target EraeTests
// Run:     ./build/EraeTests_artefacts/Release/Erae\ Tests

#include <juce_core/juce_core.h>
#include "../Source/Model/Shape.h"
#include "../Source/Model/Preset.h"
#include "../Source/Core/ShapeLibrary.h"
#include "../Source/Effects/TouchEffect.h"

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

    fprintf(stdout, "\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
