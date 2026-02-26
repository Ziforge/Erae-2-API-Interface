#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Core/SelectionManager.h"
#include "Core/Clipboard.h"
#include "UI/GridCanvas.h"
#include "UI/ColorPicker7Bit.h"
#include "UI/EraeLookAndFeel.h"
#include "UI/SidebarTabBar.h"
#include "UI/MidiPanel.h"
#include "UI/EffectPanel.h"
#include "UI/Theme.h"
#include "Core/ShapeLibrary.h"
#include "Core/ShapeMorph.h"
#include "Model/VisualStyle.h"

namespace erae {

class EraeEditor : public juce::AudioProcessorEditor,
                   public ColorPicker7Bit::Listener,
                   public MidiPanel::Listener,
                   public EffectPanel::Listener,
                   public SidebarTabBar::Listener,
                   public GridCanvas::Listener,
                   public SelectionManager::Listener,
                   public juce::Timer {
public:
    explicit EraeEditor(EraeProcessor& processor);
    ~EraeEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // ColorPicker7Bit::Listener
    void colorChanged(Color7 newColor) override;

    // MidiPanel::Listener
    void behaviorChanged(const std::string& shapeId) override;
    void midiLearnRequested(const std::string& shapeId) override;
    void midiLearnCancelled() override;

    // EffectPanel::Listener
    void effectChanged(const std::string& shapeId) override;

    // SidebarTabBar::Listener
    void tabChanged(SidebarTabBar::Tab newTab) override;

    // GridCanvas::Listener
    void selectionChanged() override;
    void toolModeChanged(ToolMode mode) override;
    void copyRequested() override;
    void cutRequested() override;
    void pasteRequested() override;
    void designModeChanged(bool active) override;
    void designFinished(std::set<std::pair<int,int>> cells) override;

    // juce::Timer — refresh finger overlay + connection status
    void timerCallback() override;

private:
    EraeProcessor& processor_;
    EraeLookAndFeel lookAndFeel_;
    SelectionManager selectionManager_;
    GridCanvas canvas_;

    // Toolbar — tools
    juce::TextButton selectButton_    {"Select"};
    juce::TextButton paintButton_     {"Paint"};
    juce::TextButton eraseButton_     {"Erase"};
    juce::TextButton drawRectButton_  {"Rect"};
    juce::TextButton drawCircButton_  {"Circle"};
    juce::TextButton drawHexButton_   {"Hex"};
    juce::TextButton drawPolyButton_  {"Poly"};
    juce::TextButton drawPixelButton_ {"Pixel"};
    juce::TextButton pixelDoneButton_ {"Done"};

    // Design mode toolbar
    juce::TextButton designButton_       {"Design"};
    juce::TextButton designDoneButton_   {"Done"};
    juce::TextButton designCancelButton_ {"Cancel"};
    juce::ToggleButton designSymHToggle_ {"Sym H"};
    juce::ToggleButton designSymVToggle_ {"Sym V"};

    // Toolbar — canvas actions
    juce::ComboBox brushSizeSelector_;
    juce::TextButton undoButton_    {"Undo"};
    juce::TextButton redoButton_    {"Redo"};
    juce::TextButton deleteButton_  {"Del"};
    juce::TextButton dupeButton_    {"Dupe"};
    juce::TextButton zoomFitButton_ {"Fit"};

    // Settings tab — file section
    juce::ComboBox presetSelector_;
    juce::TextButton newButton_     {"New"};
    juce::TextButton saveButton_    {"Save"};
    juce::TextButton loadButton_    {"Load"};
    juce::Label fileLabel_          {"", "FILE"};

    // Settings tab — pages section
    juce::TextButton pagePrevButton_ {"<"};
    juce::Label pageLabel_           {"", "Page 1/1"};
    juce::TextButton pageNextButton_ {">"};
    juce::TextButton pageAddButton_  {"+"};
    juce::TextButton pageDelButton_  {"-"};
    juce::TextButton pageDupButton_  {"Dup"};
    juce::Label pagesLabel_          {"", "PAGES"};

    // Settings tab — OSC output section (global)
    juce::Label oscLabel_          {"", "OSC OUTPUT"};
    juce::ToggleButton oscToggle_  {"Enable"};
    juce::Label oscHostLabel_      {"", "Host"};
    juce::TextEditor oscHostEditor_;
    juce::Label oscPortLabel_      {"", "Port"};
    juce::Slider oscPortSlider_;

    // Settings tab — hardware section
    juce::TextButton connectButton_ {"Connect"};
    juce::ToggleButton fingerColorsToggle_ {"Colors"};
    juce::ToggleButton dawFeedbackToggle_  {"DAW FB"};
    juce::Label hardwareLabel_             {"", "HARDWARE"};

    // Clipboard
    Clipboard clipboard_;
    int shapeCounterRef_ = 0;

    // MIDI learn target shape
    std::string midiLearnShapeId_;

    // File chooser (must persist during async dialog)
    std::unique_ptr<juce::FileChooser> fileChooser_;

    // Sidebar — Tab bar
    SidebarTabBar tabBar_;

    // Sidebar — Shape tab
    ColorPicker7Bit colorPicker_;
    juce::Label colorLabel_;
    juce::Label visualLabel_    {"", "VISUAL"};
    juce::ComboBox visualBox_;
    juce::Label fillHorizLabel_ {"", "Fill Horiz"};
    juce::ToggleButton fillHorizToggle_;

    // Sidebar — Shape tab: alignment (2+ selected)
    juce::TextButton alignLeftBtn_    {"L"};
    juce::TextButton alignRightBtn_   {"R"};
    juce::TextButton alignTopBtn_     {"T"};
    juce::TextButton alignBottomBtn_  {"B"};
    juce::TextButton alignCHBtn_      {"CH"};
    juce::TextButton alignCVBtn_      {"CV"};
    juce::TextButton distHBtn_        {"DH"};
    juce::TextButton distVBtn_        {"DV"};
    juce::Label alignLabel_           {"", "ALIGN"};

    // Sidebar — Shape tab: morph (2 selected)
    juce::Label morphLabel_          {"", "MORPH"};
    juce::TextButton morphButton_    {"Create Morph"};
    juce::Slider morphSlider_;
    std::string morphIdA_, morphIdB_;

    // Sidebar — Shape tab: scrollable viewport
    juce::Viewport shapeViewport_;
    juce::Component shapeContent_;

    // Sidebar — Shape tab: MIDI (embedded, not its own tab)
    MidiPanel midiPanel_;

    // Sidebar — Effects tab
    EffectPanel effectPanel_;

    // Sidebar — Shape tab: CV output (per-shape, moved from OutputPanel)
    juce::Label cvLabel_        {"", "CV OUTPUT"};
    juce::Label cvEnableLabel_  {"", "CV Enabled"};
    juce::ToggleButton cvEnableToggle_;
    juce::Label cvChannelLabel_ {"", "CV Channel"};
    juce::Slider cvChannelSlider_;

    // Sidebar — Library tab
    ShapeLibrary library_;
    juce::ListBox libraryList_;
    juce::TextButton libSaveBtn_   {"Save"};
    juce::TextButton libPlaceBtn_  {"Place"};
    juce::TextButton libFlipHBtn_  {"Flip H"};
    juce::TextButton libFlipVBtn_  {"Flip V"};
    juce::TextButton libDeleteBtn_ {"Del"};
    juce::Label libLabel_          {"", "SHAPE LIBRARY"};

    // Sidebar — selection info (always visible at bottom)
    juce::Label selectionLabel_;

    // Status bar
    juce::Label statusLabel_;

    // Library list model
    struct LibraryListModel : public juce::ListBoxModel {
        ShapeLibrary* library = nullptr;
        int getNumRows() override { return library ? library->numEntries() : 0; }
        void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override
        {
            if (!library || row < 0 || row >= library->numEntries()) return;
            if (selected) {
                g.setColour(Theme::Colors::Accent.withAlpha(0.3f));
                g.fillRect(0, 0, w, h);
            }
            g.setColour(Theme::Colors::Text);
            g.setFont(juce::Font(Theme::FontBase));
            g.drawText(library->getEntry(row).name, 4, 0, w - 8, h,
                       juce::Justification::centredLeft, true);
        }
    };
    LibraryListModel libraryListModel_;

    void loadPreset(int index);
    void savePresetToFile();
    void loadPresetFromFile();
    void setTool(ToolMode mode);
    void updateToolButtons();
    void updateStatus();
    void updateSelectionInfo();
    void updateConnectButton();
    void updateUndoButtons();
    void drawToolbarSeparators(juce::Graphics& g);
    void showAlignmentButtons(bool show);
    void performAlignment(std::function<std::vector<AlignResult>(Layout&, const std::set<std::string>&)> fn, const std::string& name);
    void showTabContent(SidebarTabBar::Tab tab);
    void updateVisualControls();
    void showDesignToolbar(bool show);
    void loadCVFromShape(Shape* shape);
    void clearCV();
    void writeCVToShape();
    void layoutShapeTabContent(int contentWidth);
    int designShapeCounter_ = 0;
    Shape* cvCurrentShape_ = nullptr;
    bool cvLoading_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EraeEditor)
};

} // namespace erae
