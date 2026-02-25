#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace erae {
namespace Theme {

    // Erae II grid dimensions
    inline constexpr int GridW = 42;
    inline constexpr int GridH = 24;

    // Display
    inline constexpr float CellSize    = 20.0f;  // pixels per grid cell at 1x zoom
    inline constexpr float MinZoom     = 0.25f;
    inline constexpr float MaxZoom     = 4.0f;
    inline constexpr float DefaultZoom = 1.0f;

    // Canvas dimensions at 1x zoom
    inline constexpr float CanvasW = GridW * CellSize;  // 840
    inline constexpr float CanvasH = GridH * CellSize;  // 480

    // Grid line style
    inline constexpr float GridLineWidth     = 0.5f;
    inline constexpr float GridBorderWidth   = 1.5f;

    // UI layout
    inline constexpr int ToolbarHeight   = 40;
    inline constexpr int SidebarWidth    = 270;
    inline constexpr int StatusBarHeight = 24;
    inline constexpr int DefaultWindowW  = 1200;
    inline constexpr int DefaultWindowH  = 720;

    // 8-point spacing scale
    inline constexpr int SpaceXS  = 2;
    inline constexpr int SpaceSM  = 4;
    inline constexpr int SpaceMD  = 8;
    inline constexpr int SpaceLG  = 12;
    inline constexpr int SpaceXL  = 16;

    // Rounded corners
    inline constexpr float CornerRadius = 4.0f;
    inline constexpr float ButtonRadius = 3.0f;
    inline constexpr float PopupRadius  = 5.0f;

    // Typography
    inline constexpr float FontBase     = 11.0f;
    inline constexpr float FontSmall    = 10.0f;
    inline constexpr float FontSection  = 11.0f;  // uppercase, semibold
    inline constexpr float FontToolbar  = 11.5f;
    inline constexpr float FontStatus   = 10.5f;

    // ============================================================
    // Colors — refined dark theme (Ableton/Bitwig/Vital-inspired)
    // ============================================================
    namespace Colors {
        // Backgrounds — multi-level elevation
        inline const juce::Colour Background     {0xff18181b};
        inline const juce::Colour CanvasBg       {0xff0e0e11};
        inline const juce::Colour Toolbar        {0xff1e1e24};
        inline const juce::Colour Sidebar        {0xff1c1c20};
        inline const juce::Colour StatusBar      {0xff131316};
        inline const juce::Colour PopupBg        {0xff1e1e24};

        // Canvas inset (recessed effect — outer then inner)
        inline const juce::Colour CanvasInsetOuter {0xff0a0a0d};
        inline const juce::Colour CanvasInsetInner {0xff22222c};

        // Grid
        inline const juce::Colour GridLine       {0xff22222c};
        inline const juce::Colour GridMajor      {0xff2c2c38};
        inline const juce::Colour GridBorder     {0xff3a3a48};

        // Text
        inline const juce::Colour Text           {0xffd8d8dc};
        inline const juce::Colour TextDim        {0xff6e6e78};
        inline const juce::Colour TextBright     {0xfff0f0f0};
        inline const juce::Colour TextDisabled   {0xff4a4a54};

        // Accent — warm amber/orange
        inline const juce::Colour Accent         {0xffe8873a};
        inline const juce::Colour AccentDim      {0xff9e5a28};
        inline const juce::Colour AccentHover    {0xfff09848};
        inline const juce::Colour AccentGlow     {0x33e8873a};

        // Semantic
        inline const juce::Colour Error          {0xffe85050};
        inline const juce::Colour Success        {0xff50c878};

        // Selection
        inline const juce::Colour Selection      {0xffe8873a};
        inline const juce::Colour SelectionFill  {0x22e8873a};

        // Handles
        inline const juce::Colour HandleFill     {0xffffffff};
        inline const juce::Colour HandleBorder   {0xffe8873a};

        // Interactive elements
        inline const juce::Colour ButtonBg       {0xff28282e};
        inline const juce::Colour ButtonHover    {0xff32323c};
        inline const juce::Colour ButtonActive   {0xffe8873a};
        inline const juce::Colour SliderTrack    {0xff353540};
        inline const juce::Colour SliderThumb    {0xffe8873a};
        inline const juce::Colour FocusRing      {0x80e8873a};

        // Separators
        inline const juce::Colour Separator      {0xff252530};
        inline const juce::Colour SeparatorLight {0xff353540};
    }

} // namespace Theme
} // namespace erae
