#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
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
    void selectionChanged(const std::string& shapeId) override;
    void toolModeChanged(ToolMode mode) override;

    // juce::Timer — refresh finger overlay + connection status
    void timerCallback() override;

private:
    EraeProcessor& processor_;
    EraeLookAndFeel lookAndFeel_;
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
    juce::TextButton clearButton_   {"Clear"};
    juce::TextButton zoomFitButton_ {"Fit"};
    juce::TextButton deleteButton_  {"Del"};
    juce::TextButton dupeButton_    {"Dupe"};
    juce::TextButton saveButton_    {"Save"};
    juce::TextButton loadButton_    {"Load"};

    // Toolbar — Erae connection
    juce::TextButton connectButton_ {"Connect"};

    // File chooser (must persist during async dialog)
    std::unique_ptr<juce::FileChooser> fileChooser_;

    // Sidebar
    ColorPicker7Bit colorPicker_;
    juce::Label colorLabel_;
    PropertyPanel propertyPanel_;
    juce::Label selectionLabel_;

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
    void drawToolbarSeparators(juce::Graphics& g);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EraeEditor)
};

} // namespace erae
