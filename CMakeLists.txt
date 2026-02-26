cmake_minimum_required(VERSION 3.22)

project(EraeShapeEditor VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE via FetchContent (same pattern as Gateless Gate VST)
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.9
)
FetchContent_MakeAvailable(JUCE)

# Plugin target — Standalone + VST3
juce_add_plugin(EraeShapeEditor
    COMPANY_NAME "EraeTools"
    BUNDLE_ID "com.eraetools.shapeeditor"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE ErTl
    PLUGIN_CODE ErSE
    FORMATS VST3 Standalone
    PRODUCT_NAME "Erae Shape Editor"
)

target_sources(EraeShapeEditor
    PRIVATE
        Source/PluginProcessor.h
        Source/PluginProcessor.cpp
        Source/PluginEditor.h
        Source/PluginEditor.cpp
        # Core (undo/redo, selection, alignment)
        Source/Core/UndoManager.h
        Source/Core/SelectionManager.h
        Source/Core/AlignmentTools.h
        Source/Core/LayoutActions.h
        Source/Core/Clipboard.h
        # Model
        Source/Model/Shape.h
        Source/Model/Layout.h
        Source/Model/Layout.cpp
        Source/Model/Behavior.h
        Source/Model/Preset.h
        Source/Model/Preset.cpp
        Source/Model/Color.h
        Source/Model/VisualStyle.h
        Source/Model/MultiPageLayout.h
        # Rendering
        Source/Rendering/WidgetRenderer.h
        Source/Rendering/WidgetRenderer.cpp
        # UI
        Source/UI/Theme.h
        Source/UI/GridCanvas.h
        Source/UI/GridCanvas.cpp
        Source/UI/EraeLookAndFeel.h
        Source/UI/EraeLookAndFeel.cpp
        Source/UI/ColorPicker7Bit.h
        Source/UI/ColorPicker7Bit.cpp
        Source/UI/PropertyPanel.h
        Source/UI/PropertyPanel.cpp
        # Erae hardware
        Source/Erae/EraeSysEx.h
        Source/Erae/FingerStream.h
        Source/Erae/EraeConnection.h
        Source/Erae/EraeConnection.cpp
        Source/Erae/EraeRenderer.h
        Source/Erae/EraeRenderer.cpp
        # MIDI output + behaviors
        Source/MIDI/EraeMidiOut.h
        Source/MIDI/EraeMidiOut.cpp
        Source/MIDI/MPEAllocator.h
        Source/MIDI/MPEAllocator.cpp
        Source/MIDI/BehaviorEngine.h
        Source/MIDI/BehaviorEngine.cpp
        Source/MIDI/VelocityCurve.h
        Source/MIDI/ScaleQuantizer.h
        Source/MIDI/OscOutput.h
        Source/MIDI/DawFeedback.h
        Source/MIDI/CVOutput.h
        # Rendering (Phase 5)
        Source/Rendering/FingerPalette.h
)

target_compile_definitions(EraeShapeEditor
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_compile_options(EraeShapeEditor PRIVATE
    -Wall
    -Wno-sign-conversion
    -Wno-overloaded-virtual
    -Wno-unused-parameter
)

target_link_libraries(EraeShapeEditor
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
)
