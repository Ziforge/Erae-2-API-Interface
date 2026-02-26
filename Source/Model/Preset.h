#pragma once

#include "Shape.h"
#include "Color.h"
#include <vector>
#include <memory>
#include <functional>

namespace erae {

// ============================================================
// Preset — JSON load/save + 6 built-in layout generators
// Ported from presets.py
// ============================================================
namespace Preset {

    // Built-in generators — each returns a vector of shapes
    std::vector<std::unique_ptr<Shape>> drumPads(int rows = 4, int cols = 4, int baseNote = 36,
                                                  int gridW = 42, int gridH = 24);
    std::vector<std::unique_ptr<Shape>> piano(int octaves = 3, int startNote = 48,
                                               int gridW = 42, int gridH = 24);
    std::vector<std::unique_ptr<Shape>> wickiHayden(int rows = 6, int cols = 10, int baseNote = 48,
                                                     int gridW = 42, int gridH = 24);
    std::vector<std::unique_ptr<Shape>> faderBank(int numFaders = 8, int ccStart = 1,
                                                   int gridW = 42, int gridH = 24);
    std::vector<std::unique_ptr<Shape>> xyPad(int gridW = 42, int gridH = 24);
    std::vector<std::unique_ptr<Shape>> buchlaThunder(int gridW = 42, int gridH = 24);
    std::vector<std::unique_ptr<Shape>> autoHarp(int gridW = 42, int gridH = 24);

    // Generator registry
    using GeneratorFn = std::function<std::vector<std::unique_ptr<Shape>>()>;

    struct GeneratorEntry {
        std::string name;
        GeneratorFn fn;
    };

    const std::vector<GeneratorEntry>& getGenerators();

    // JSON serialization
    juce::String toJSON(const std::vector<std::unique_ptr<Shape>>& shapes);
    std::vector<std::unique_ptr<Shape>> fromJSON(const juce::String& json);

    // File I/O
    bool saveToFile(const juce::File& file, const std::vector<std::unique_ptr<Shape>>& shapes);
    std::vector<std::unique_ptr<Shape>> loadFromFile(const juce::File& file);

} // namespace Preset
} // namespace erae
