# Erae 2 API Interface

A JUCE-based visual layout editor and MIDI controller for the **Erae Touch II** playing surface. Design custom touch layouts with shapes, colors, and behaviors, then send them directly to the Erae hardware over USB.

## What It Does

- **Visual Layout Editor** - Design touch-sensitive zones on a 42x24 grid that maps 1:1 to the Erae Touch II LED surface
- **Real-time Hardware Rendering** - Layouts are pushed to the Erae surface as RGB pixel data via SysEx, with animated visual feedback (pressure glow, fill bars, position dots, radial arcs)
- **MIDI Generation** - Each shape can be assigned a MIDI behavior (trigger, momentary, note pad with MPE, XY controller, fader) that generates MIDI from finger touch events
- **Finger Tracking** - Receives finger position data from the Erae II fingerstream API and dispatches touch events to the appropriate shapes
- **Preset Library** - Built-in presets (Drum Pads, Piano, Wicki-Hayden, Fader Bank, XY Pad, Buchla Thunder) plus save/load of custom layouts as JSON
- **Runs as Standalone or VST3** - Use it as a standalone app or load it in your DAW

## Shape Types

- **Rectangle** - Axis-aligned rectangular zones
- **Circle** - Circular touch areas
- **Hexagon** - Regular hexagonal pads (great for isomorphic layouts)
- **Polygon** - Arbitrary convex/concave polygons defined by vertices (used for parallelogram wings in the Buchla Thunder preset)

## Visual Styles

Each shape can have an independent visual style that animates on the hardware surface:

- **Static** - Solid color fill (no animation)
- **Pressure Glow** - Color intensity tracks finger pressure
- **Fill Bar** - Vertical/horizontal fill level follows finger position
- **Position Dot** - Bright dot tracks finger position within shape
- **Radial Arc** - Arc sweeps based on finger angle from center

## MIDI Behaviors

- **Trigger** - Note on/off on touch down/up (fixed velocity or pressure-mapped)
- **Momentary** - Note on while held, off on release
- **Note Pad (MPE)** - Per-finger channel allocation with pitch bend and pressure
- **XY Controller** - Sends two CC values based on finger X/Y position
- **Fader** - Single CC value mapped to finger position along one axis

## Building

Requires CMake 3.22+ and a C++17 compiler. JUCE is fetched automatically via CMake's FetchContent.

```bash
mkdir build && cd build
cmake ..
cmake --build . -j$(nproc)
```

The standalone app will be at `build/EraeShapeEditor_artefacts/Debug/Standalone/Erae Shape Editor`.

## Erae Touch II Connection

The app auto-detects the Erae Touch II via its USB MIDI ports:
- **Output** (API commands): Sent to the "Lab" MIDI port
- **Input** (finger events): Received from the "Main" MIDI port

The Erae II SysEx API is used for:
- Enabling/disabling the API mode
- Drawing pixels and images to the LED surface
- Receiving finger position/pressure data
- Querying zone boundaries

## File Format

Layouts are saved as JSON files compatible with the Python `erae_shapes` format. Each shape stores its geometry, colors (7-bit RGB, 0-127), behavior type, behavior parameters, and visual style.

## License

MIT
