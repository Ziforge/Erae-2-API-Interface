#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Core/SelectionManager.h"
#include "Core/Clipboard.h"
#include "UI/GridCanvas.h"
#include "UI/ColorPicker7Bit.h"
#include "UI/EraeLookAndFeel.h"
#include "UI/PropertyPanel.h"
#include "UI/Theme.h"

namespace erae {

class EraeEditor : public juce::AudioProcessorEditor,
                   public ColorPicker7Bit::Listener,
                   public PropertyPanel::Listener,
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

    // PropertyPanel::Listener
    void behaviorChanged(const std::string& shapeId) override;

    // GridCanvas::Listener
    void selectionChanged() override;
    void toolModeChanged(ToolMode mode) override;
    void copyRequested() override;
    void cutRequested() override;
    void pasteRequested() override;

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

    // Toolbar — actions
    juce::ComboBox presetSelector_;
    juce::ComboBox brushSizeSelector_;
    juce::TextButton undoButton_    {"Undo"};
    juce::TextButton redoButton_    {"Redo"};
    juce::TextButton clearButton_   {"Clear"};
    juce::TextButton zoomFitButton_ {"Fit"};
    juce::TextButton deleteButton_  {"Del"};
    juce::TextButton dupeButton_    {"Dupe"};
    juce::TextButton saveButton_    {"Save"};
    juce::TextButton loadButton_    {"Load"};

    // Toolbar — Page navigation
    juce::TextButton pagePrevButton_ {"<"};
    juce::Label pageLabel_           {"", "Page 1/1"};
    juce::TextButton pageNextButton_ {">"};
    juce::TextButton pageAddButton_  {"+"};
    juce::TextButton pageDelButton_  {"-"};
    juce::TextButton pageDupButton_  {"Dup"};

    // Toolbar — Erae connection
    juce::TextButton connectButton_ {"Connect"};

    // Toolbar — Phase 5 toggles
    juce::ToggleButton fingerColorsToggle_ {"Colors"};
    juce::ToggleButton dawFeedbackToggle_  {"DAW FB"};

    // OSC settings (sidebar)
    juce::Label oscLabel_          {"", "OSC OUTPUT"};
    juce::ToggleButton oscToggle_  {"Enable"};
    juce::Label oscHostLabel_      {"", "Host"};
    juce::TextEditor oscHostEditor_;
    juce::Label oscPortLabel_      {"", "Port"};
    juce::Slider oscPortSlider_;

    // Clipboard
    Clipboard clipboard_;
    int shapeCounterRef_ = 0; // shared counter for clipboard paste

    // File chooser (must persist during async dialog)
    std::unique_ptr<juce::FileChooser> fileChooser_;

    // Sidebar
    ColorPicker7Bit colorPicker_;
    juce::Label colorLabel_;
    PropertyPanel propertyPanel_;
    juce::Label selectionLabel_;

    // Alignment buttons (shown when 2+ selected)
    juce::TextButton alignLeftBtn_    {"L"};
    juce::TextButton alignRightBtn_   {"R"};
    juce::TextButton alignTopBtn_     {"T"};
    juce::TextButton alignBottomBtn_  {"B"};
    juce::TextButton alignCHBtn_      {"CH"};
    juce::TextButton alignCVBtn_      {"CV"};
    juce::TextButton distHBtn_        {"DH"};
    juce::TextButton distVBtn_        {"DV"};
    juce::Label alignLabel_           {"", "ALIGN"};

    // Status bar
    juce::Label statusLabel_;

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EraeEditor)
};

} // namespace erae
