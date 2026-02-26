#include "PluginEditor.h"
#include "Model/Preset.h"
#include "Core/LayoutActions.h"
#include "Core/AlignmentTools.h"

namespace erae {

EraeEditor::EraeEditor(EraeProcessor& p)
    : AudioProcessorEditor(p),
      processor_(p),
      canvas_(p.getLayout(), p.getUndoManager(), selectionManager_),
      propertyPanel_(p.getLayout())
{
    setLookAndFeel(&lookAndFeel_);

    // Listen for selection changes
    selectionManager_.addListener(this);

    // Undo state change callback
    processor_.getUndoManager().onStateChanged = [this] {
        juce::MessageManager::callAsync([this] { updateUndoButtons(); });
    };

    // --- Toolbar: tool buttons ---
    selectButton_.onClick   = [this] { setTool(ToolMode::Select); };
    paintButton_.onClick    = [this] { setTool(ToolMode::Paint); };
    eraseButton_.onClick    = [this] { setTool(ToolMode::Erase); };
    drawRectButton_.onClick = [this] { setTool(ToolMode::DrawRect); };
    drawCircButton_.onClick = [this] { setTool(ToolMode::DrawCircle); };
    drawHexButton_.onClick  = [this] { setTool(ToolMode::DrawHex); };

    selectButton_.setTooltip("Select tool (V)");
    paintButton_.setTooltip("Paint pixels (B)");
    eraseButton_.setTooltip("Erase pixels (E)");
    drawRectButton_.setTooltip("Draw rectangle (R)");
    drawCircButton_.setTooltip("Draw circle (C)");
    drawHexButton_.setTooltip("Draw hexagon (H)");

    addAndMakeVisible(selectButton_);
    addAndMakeVisible(paintButton_);
    addAndMakeVisible(eraseButton_);
    addAndMakeVisible(drawRectButton_);
    addAndMakeVisible(drawCircButton_);
    addAndMakeVisible(drawHexButton_);

    // --- Toolbar: brush size ---
    brushSizeSelector_.addItem("1px", 1);
    brushSizeSelector_.addItem("2px", 2);
    brushSizeSelector_.addItem("3px", 3);
    brushSizeSelector_.addItem("5px", 5);
    brushSizeSelector_.setSelectedId(1);
    brushSizeSelector_.onChange = [this] {
        canvas_.setBrushSize(brushSizeSelector_.getSelectedId());
    };
    addAndMakeVisible(brushSizeSelector_);

    // --- Toolbar: presets ---
    presetSelector_.setTextWhenNothingSelected("Presets...");
    auto& gens = Preset::getGenerators();
    for (int i = 0; i < (int)gens.size(); ++i)
        presetSelector_.addItem(gens[i].name, i + 1);
    presetSelector_.onChange = [this] {
        int idx = presetSelector_.getSelectedId() - 1;
        if (idx >= 0) loadPreset(idx);
    };
    addAndMakeVisible(presetSelector_);

    // --- Toolbar: undo/redo ---
    undoButton_.onClick = [this] { processor_.getUndoManager().undo(); };
    redoButton_.onClick = [this] { processor_.getUndoManager().redo(); };
    undoButton_.setTooltip("Undo (Ctrl+Z)");
    redoButton_.setTooltip("Redo (Ctrl+Shift+Z)");
    addAndMakeVisible(undoButton_);
    addAndMakeVisible(redoButton_);

    // --- Toolbar: actions ---
    deleteButton_.onClick = [this] { canvas_.deleteSelected(); };
    dupeButton_.onClick   = [this] { canvas_.duplicateSelected(); };
    clearButton_.onClick  = [this] {
        auto& um = processor_.getUndoManager();
        std::vector<std::unique_ptr<Shape>> empty;
        um.perform(std::make_unique<SetShapesAction>(processor_.getLayout(), std::move(empty)));
        selectionManager_.clear();
        updateStatus();
    };
    zoomFitButton_.onClick = [this] { canvas_.zoomToFit(); };

    saveButton_.onClick = [this] { savePresetToFile(); };
    loadButton_.onClick = [this] { loadPresetFromFile(); };

    deleteButton_.setTooltip("Delete selected (Del)");
    dupeButton_.setTooltip("Duplicate selected (Ctrl+D)");
    clearButton_.setTooltip("Clear all shapes");
    zoomFitButton_.setTooltip("Zoom to fit");
    saveButton_.setTooltip("Save preset to file");
    loadButton_.setTooltip("Load preset from file");

    addAndMakeVisible(deleteButton_);
    addAndMakeVisible(dupeButton_);
    addAndMakeVisible(clearButton_);
    addAndMakeVisible(zoomFitButton_);
    addAndMakeVisible(saveButton_);
    addAndMakeVisible(loadButton_);

    // --- Toolbar: Page navigation ---
    pagePrevButton_.onClick = [this] {
        auto& ml = processor_.getMultiLayout();
        if (ml.currentPageIndex() > 0) {
            ml.switchToPage(ml.currentPageIndex() - 1);
            canvas_.setLayout(ml.currentPage());
            selectionManager_.clear();
            processor_.getDawFeedback().updateFromLayout(ml.currentPage());
            updateStatus();
        }
    };
    pageNextButton_.onClick = [this] {
        auto& ml = processor_.getMultiLayout();
        if (ml.currentPageIndex() < ml.numPages() - 1) {
            ml.switchToPage(ml.currentPageIndex() + 1);
            canvas_.setLayout(ml.currentPage());
            selectionManager_.clear();
            processor_.getDawFeedback().updateFromLayout(ml.currentPage());
            updateStatus();
        }
    };
    pageAddButton_.onClick = [this] {
        auto& ml = processor_.getMultiLayout();
        ml.addPage();
        canvas_.setLayout(ml.currentPage());
        selectionManager_.clear();
        processor_.getDawFeedback().updateFromLayout(ml.currentPage());
        updateStatus();
    };
    pageDelButton_.onClick = [this] {
        auto& ml = processor_.getMultiLayout();
        if (ml.numPages() > 1) {
            ml.removePage(ml.currentPageIndex());
            canvas_.setLayout(ml.currentPage());
            selectionManager_.clear();
            processor_.getDawFeedback().updateFromLayout(ml.currentPage());
            updateStatus();
        }
    };
    pageDupButton_.onClick = [this] {
        auto& ml = processor_.getMultiLayout();
        ml.duplicatePage(ml.currentPageIndex());
        canvas_.setLayout(ml.currentPage());
        selectionManager_.clear();
        processor_.getDawFeedback().updateFromLayout(ml.currentPage());
        updateStatus();
    };
    pageDelButton_.setTooltip("Delete current page");
    pageDupButton_.setTooltip("Duplicate current page");
    pageLabel_.setFont(juce::Font(Theme::FontSmall));
    pageLabel_.setColour(juce::Label::textColourId, Theme::Colors::Text);
    pageLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(pagePrevButton_);
    addAndMakeVisible(pageLabel_);
    addAndMakeVisible(pageNextButton_);
    addAndMakeVisible(pageAddButton_);
    addAndMakeVisible(pageDelButton_);
    addAndMakeVisible(pageDupButton_);

    // --- Toolbar: Erae connection ---
    connectButton_.onClick = [this] {
        auto& conn = processor_.getConnection();
        if (conn.isConnected()) {
            conn.disconnect();
        } else {
            if (conn.connect())
                conn.enableAPI();
        }
        updateConnectButton();
        updateStatus();
    };
    addAndMakeVisible(connectButton_);
    updateConnectButton();

    // --- Toolbar: Phase 5 toggles ---
    fingerColorsToggle_.setToggleState(processor_.getPerFingerColors(), juce::dontSendNotification);
    fingerColorsToggle_.onClick = [this] {
        bool en = fingerColorsToggle_.getToggleState();
        processor_.setPerFingerColors(en);
        canvas_.setPerFingerColors(en);
    };
    fingerColorsToggle_.setTooltip("Per-finger LED colors");
    addAndMakeVisible(fingerColorsToggle_);

    dawFeedbackToggle_.setToggleState(processor_.getDawFeedback().isEnabled(), juce::dontSendNotification);
    dawFeedbackToggle_.onClick = [this] {
        bool en = dawFeedbackToggle_.getToggleState();
        processor_.getDawFeedback().setEnabled(en);
        if (en)
            processor_.getDawFeedback().updateFromLayout(processor_.getLayout());
    };
    dawFeedbackToggle_.setTooltip("DAW MIDI feedback highlights");
    addAndMakeVisible(dawFeedbackToggle_);

    // --- Sidebar: OSC output settings ---
    oscLabel_.setFont(juce::Font(Theme::FontSection, juce::Font::bold));
    oscLabel_.setColour(juce::Label::textColourId, Theme::Colors::TextDim);
    addAndMakeVisible(oscLabel_);

    oscToggle_.setToggleState(processor_.getOscOutput().isEnabled(), juce::dontSendNotification);
    oscToggle_.onClick = [this] {
        auto& osc = processor_.getOscOutput();
        if (oscToggle_.getToggleState())
            osc.enable(oscHostEditor_.getText().toStdString(), (int)oscPortSlider_.getValue());
        else
            osc.disable();
    };
    addAndMakeVisible(oscToggle_);

    oscHostLabel_.setFont(juce::Font(Theme::FontBase));
    oscHostLabel_.setColour(juce::Label::textColourId, Theme::Colors::TextDim);
    addAndMakeVisible(oscHostLabel_);

    oscHostEditor_.setText(processor_.getOscOutput().getHost());
    oscHostEditor_.setFont(juce::Font(Theme::FontBase));
    oscHostEditor_.onReturnKey = [this] {
        if (oscToggle_.getToggleState())
            processor_.getOscOutput().enable(oscHostEditor_.getText().toStdString(), (int)oscPortSlider_.getValue());
    };
    addAndMakeVisible(oscHostEditor_);

    oscPortLabel_.setFont(juce::Font(Theme::FontBase));
    oscPortLabel_.setColour(juce::Label::textColourId, Theme::Colors::TextDim);
    addAndMakeVisible(oscPortLabel_);

    oscPortSlider_.setRange(1024, 65535, 1);
    oscPortSlider_.setValue(processor_.getOscOutput().getPort(), juce::dontSendNotification);
    oscPortSlider_.setSliderStyle(juce::Slider::LinearBar);
    oscPortSlider_.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 50, 20);
    oscPortSlider_.setColour(juce::Slider::trackColourId, Theme::Colors::Accent);
    oscPortSlider_.setColour(juce::Slider::textBoxTextColourId, Theme::Colors::Text);
    oscPortSlider_.onValueChange = [this] {
        if (oscToggle_.getToggleState())
            processor_.getOscOutput().enable(oscHostEditor_.getText().toStdString(), (int)oscPortSlider_.getValue());
    };
    addAndMakeVisible(oscPortSlider_);

    // --- Canvas ---
    canvas_.addListener(this);
    addAndMakeVisible(canvas_);

    // --- Sidebar: section header ---
    colorLabel_.setText("COLOR", juce::dontSendNotification);
    colorLabel_.setFont(juce::Font(Theme::FontSection, juce::Font::bold));
    colorLabel_.setColour(juce::Label::textColourId, Theme::Colors::TextDim);
    addAndMakeVisible(colorLabel_);

    colorPicker_.addListener(this);
    colorPicker_.setColor(Color7{0, 80, 127});
    addAndMakeVisible(colorPicker_);

    // --- Sidebar: property panel ---
    propertyPanel_.addListener(this);
    propertyPanel_.setVisible(false);
    addAndMakeVisible(propertyPanel_);

    // Selection info
    selectionLabel_.setFont(juce::Font(Theme::FontSmall));
    selectionLabel_.setColour(juce::Label::textColourId, Theme::Colors::TextDim);
    addAndMakeVisible(selectionLabel_);

    // --- Sidebar: alignment buttons ---
    alignLabel_.setFont(juce::Font(Theme::FontSection, juce::Font::bold));
    alignLabel_.setColour(juce::Label::textColourId, Theme::Colors::TextDim);
    addAndMakeVisible(alignLabel_);

    auto setupAlignBtn = [this](juce::TextButton& btn, const juce::String& tip) {
        btn.setTooltip(tip);
        addAndMakeVisible(btn);
    };
    setupAlignBtn(alignLeftBtn_, "Align Left");
    setupAlignBtn(alignRightBtn_, "Align Right");
    setupAlignBtn(alignTopBtn_, "Align Top");
    setupAlignBtn(alignBottomBtn_, "Align Bottom");
    setupAlignBtn(alignCHBtn_, "Align Center H");
    setupAlignBtn(alignCVBtn_, "Align Center V");
    setupAlignBtn(distHBtn_, "Distribute H");
    setupAlignBtn(distVBtn_, "Distribute V");

    alignLeftBtn_.onClick   = [this] { performAlignment(AlignmentTools::alignLeft, "Align Left"); };
    alignRightBtn_.onClick  = [this] { performAlignment(AlignmentTools::alignRight, "Align Right"); };
    alignTopBtn_.onClick    = [this] { performAlignment(AlignmentTools::alignTop, "Align Top"); };
    alignBottomBtn_.onClick = [this] { performAlignment(AlignmentTools::alignBottom, "Align Bottom"); };
    alignCHBtn_.onClick     = [this] { performAlignment(AlignmentTools::alignCenterH, "Align Center H"); };
    alignCVBtn_.onClick     = [this] { performAlignment(AlignmentTools::alignCenterV, "Align Center V"); };
    distHBtn_.onClick       = [this] { performAlignment(AlignmentTools::distributeH, "Distribute H"); };
    distVBtn_.onClick       = [this] { performAlignment(AlignmentTools::distributeV, "Distribute V"); };

    showAlignmentButtons(false);

    // --- Status bar ---
    statusLabel_.setFont(juce::Font(Theme::FontStatus));
    statusLabel_.setColour(juce::Label::textColourId, Theme::Colors::TextDim);
    addAndMakeVisible(statusLabel_);

    // Default to select mode
    setTool(ToolMode::Select);
    updateStatus();
    updateUndoButtons();

    // Timer for finger overlay refresh + connection status
    startTimer(50); // 20 fps

    setSize(Theme::DefaultWindowW, Theme::DefaultWindowH);
    setResizable(true, true);
}

EraeEditor::~EraeEditor()
{
    processor_.getUndoManager().onStateChanged = nullptr;
    stopTimer();
    canvas_.removeListener(this);
    colorPicker_.removeListener(this);
    propertyPanel_.removeListener(this);
    selectionManager_.removeListener(this);
    setLookAndFeel(nullptr);
}

void EraeEditor::paint(juce::Graphics& g)
{
    g.fillAll(Theme::Colors::Background);

    // Toolbar background
    g.setColour(Theme::Colors::Toolbar);
    g.fillRect(0, 0, getWidth(), Theme::ToolbarHeight);

    // Toolbar bottom separator
    g.setColour(Theme::Colors::Separator);
    g.fillRect(0, Theme::ToolbarHeight, getWidth(), 1);

    // Sidebar background
    int sepX = getWidth() - Theme::SidebarWidth;
    int contentTop = Theme::ToolbarHeight + 1;
    int contentBottom = getHeight() - Theme::StatusBarHeight;
    g.setColour(Theme::Colors::Sidebar);
    g.fillRect(sepX, contentTop, Theme::SidebarWidth, contentBottom - contentTop);

    // Sidebar left separator
    g.setColour(Theme::Colors::Separator);
    g.fillRect(sepX, contentTop, 1, contentBottom - contentTop);

    // Canvas inset effect (recessed look)
    auto canvasArea = juce::Rectangle<int>(0, contentTop, sepX, contentBottom - contentTop);
    g.setColour(Theme::Colors::CanvasInsetOuter);
    g.drawRect(canvasArea, 1);
    g.setColour(Theme::Colors::CanvasInsetInner);
    g.drawRect(canvasArea.reduced(1), 1);

    // Status bar background
    g.setColour(Theme::Colors::StatusBar);
    g.fillRect(0, contentBottom, getWidth(), Theme::StatusBarHeight);

    // Status bar top separator
    g.setColour(Theme::Colors::Separator);
    g.fillRect(0, contentBottom, getWidth(), 1);

    // Toolbar group separators
    drawToolbarSeparators(g);
}

void EraeEditor::drawToolbarSeparators(juce::Graphics& g)
{
    g.setColour(Theme::Colors::Separator);
    float sepTop = 10.0f;
    float sepBottom = (float)Theme::ToolbarHeight - 10.0f;

    float x1 = (float)(eraseButton_.getRight() + 5);
    g.drawLine(x1, sepTop, x1, sepBottom, 1.0f);

    float x2 = (float)(drawHexButton_.getRight() + 5);
    g.drawLine(x2, sepTop, x2, sepBottom, 1.0f);

    float x3 = (float)(presetSelector_.getRight() + 6);
    g.drawLine(x3, sepTop, x3, sepBottom, 1.0f);
}

void EraeEditor::resized()
{
    auto area = getLocalBounds();

    // Toolbar
    auto toolbar = area.removeFromTop(Theme::ToolbarHeight);
    toolbar.reduce(Theme::SpaceMD, 6);

    int btnH = toolbar.getHeight();
    int btnW = btnH + 16;

    // Tool group
    selectButton_.setBounds(toolbar.removeFromLeft(btnW + 4));
    toolbar.removeFromLeft(Theme::SpaceXS);
    paintButton_.setBounds(toolbar.removeFromLeft(btnW));
    toolbar.removeFromLeft(Theme::SpaceXS);
    eraseButton_.setBounds(toolbar.removeFromLeft(btnW));
    toolbar.removeFromLeft(Theme::SpaceLG);

    // Shape creation group
    drawRectButton_.setBounds(toolbar.removeFromLeft(btnW - 6));
    toolbar.removeFromLeft(Theme::SpaceXS);
    drawCircButton_.setBounds(toolbar.removeFromLeft(btnW + 6));
    toolbar.removeFromLeft(Theme::SpaceXS);
    drawHexButton_.setBounds(toolbar.removeFromLeft(btnW - 8));
    toolbar.removeFromLeft(Theme::SpaceLG);

    // Brush size
    brushSizeSelector_.setBounds(toolbar.removeFromLeft(56));
    toolbar.removeFromLeft(Theme::SpaceMD);

    // Presets + Save/Load
    presetSelector_.setBounds(toolbar.removeFromLeft(140));
    toolbar.removeFromLeft(Theme::SpaceSM);
    saveButton_.setBounds(toolbar.removeFromLeft(btnW + 4));
    toolbar.removeFromLeft(Theme::SpaceXS);
    loadButton_.setBounds(toolbar.removeFromLeft(btnW + 4));

    // Action buttons (right-aligned)
    connectButton_.setBounds(toolbar.removeFromRight(78));
    toolbar.removeFromRight(Theme::SpaceXS);
    dawFeedbackToggle_.setBounds(toolbar.removeFromRight(68));
    toolbar.removeFromRight(Theme::SpaceXS);
    fingerColorsToggle_.setBounds(toolbar.removeFromRight(68));
    toolbar.removeFromRight(Theme::SpaceSM);
    pageDupButton_.setBounds(toolbar.removeFromRight(32));
    toolbar.removeFromRight(Theme::SpaceXS);
    pageDelButton_.setBounds(toolbar.removeFromRight(24));
    toolbar.removeFromRight(Theme::SpaceXS);
    pageAddButton_.setBounds(toolbar.removeFromRight(24));
    pageNextButton_.setBounds(toolbar.removeFromRight(24));
    pageLabel_.setBounds(toolbar.removeFromRight(60));
    pagePrevButton_.setBounds(toolbar.removeFromRight(24));
    toolbar.removeFromRight(Theme::SpaceSM);
    toolbar.removeFromRight(Theme::SpaceMD);
    zoomFitButton_.setBounds(toolbar.removeFromRight(btnW - 8));
    toolbar.removeFromRight(Theme::SpaceSM);
    clearButton_.setBounds(toolbar.removeFromRight(btnW - 2));
    toolbar.removeFromRight(Theme::SpaceMD);
    dupeButton_.setBounds(toolbar.removeFromRight(btnW));
    toolbar.removeFromRight(Theme::SpaceSM);
    deleteButton_.setBounds(toolbar.removeFromRight(btnW - 8));
    toolbar.removeFromRight(Theme::SpaceMD);
    redoButton_.setBounds(toolbar.removeFromRight(btnW));
    toolbar.removeFromRight(Theme::SpaceXS);
    undoButton_.setBounds(toolbar.removeFromRight(btnW));

    // Status bar
    auto statusBar = area.removeFromBottom(Theme::StatusBarHeight);
    statusBar.reduce(Theme::SpaceLG, Theme::SpaceSM);
    statusLabel_.setBounds(statusBar);

    // Sidebar
    auto sidebar = area.removeFromRight(Theme::SidebarWidth);
    sidebar.reduce(Theme::SpaceLG, Theme::SpaceLG);

    colorLabel_.setBounds(sidebar.removeFromTop(18));
    sidebar.removeFromTop(Theme::SpaceMD);

    // Reserve space for selection info at the bottom
    auto selArea = sidebar.removeFromBottom(36);
    selectionLabel_.setBounds(selArea);

    // Color picker gets up to 280px, leaving room for property panel
    int pickerH = std::min(sidebar.getHeight() / 2, 280);
    colorPicker_.setBounds(sidebar.removeFromTop(pickerH));
    sidebar.removeFromTop(Theme::SpaceLG);

    // Alignment section (shows when 2+ selected)
    alignLabel_.setBounds(sidebar.removeFromTop(18));
    sidebar.removeFromTop(3);
    {
        auto alignRow1 = sidebar.removeFromTop(24);
        int abw = (alignRow1.getWidth() - 3 * Theme::SpaceXS) / 4;
        alignLeftBtn_.setBounds(alignRow1.removeFromLeft(abw));
        alignRow1.removeFromLeft(Theme::SpaceXS);
        alignRightBtn_.setBounds(alignRow1.removeFromLeft(abw));
        alignRow1.removeFromLeft(Theme::SpaceXS);
        alignTopBtn_.setBounds(alignRow1.removeFromLeft(abw));
        alignRow1.removeFromLeft(Theme::SpaceXS);
        alignBottomBtn_.setBounds(alignRow1);
    }
    sidebar.removeFromTop(3);
    {
        auto alignRow2 = sidebar.removeFromTop(24);
        int abw = (alignRow2.getWidth() - 3 * Theme::SpaceXS) / 4;
        alignCHBtn_.setBounds(alignRow2.removeFromLeft(abw));
        alignRow2.removeFromLeft(Theme::SpaceXS);
        alignCVBtn_.setBounds(alignRow2.removeFromLeft(abw));
        alignRow2.removeFromLeft(Theme::SpaceXS);
        distHBtn_.setBounds(alignRow2.removeFromLeft(abw));
        alignRow2.removeFromLeft(Theme::SpaceXS);
        distVBtn_.setBounds(alignRow2);
    }
    sidebar.removeFromTop(Theme::SpaceLG);

    // OSC settings section (at bottom of sidebar)
    auto oscArea = sidebar.removeFromBottom(110);
    oscLabel_.setBounds(oscArea.removeFromTop(18));
    oscArea.removeFromTop(3);
    oscToggle_.setBounds(oscArea.removeFromTop(22));
    oscArea.removeFromTop(3);
    {
        auto row = oscArea.removeFromTop(22);
        oscHostLabel_.setBounds(row.removeFromLeft(34));
        oscHostEditor_.setBounds(row);
        oscArea.removeFromTop(3);
    }
    {
        auto row = oscArea.removeFromTop(22);
        oscPortLabel_.setBounds(row.removeFromLeft(34));
        oscPortSlider_.setBounds(row);
    }

    // Property panel gets the remaining space
    propertyPanel_.setBounds(sidebar);

    // Canvas (with 2px inset for recessed effect)
    area.reduce(2, 2);
    canvas_.setBounds(area);
}

// ============================================================
// Tool switching
// ============================================================

void EraeEditor::setTool(ToolMode mode)
{
    canvas_.setToolMode(mode);
    updateToolButtons();
    updateStatus();
}

void EraeEditor::updateToolButtons()
{
    auto mode = canvas_.getToolMode();
    auto activeCol = Theme::Colors::Accent;
    auto normalCol = Theme::Colors::ButtonBg;

    auto style = [&](juce::TextButton& btn, bool active) {
        btn.setColour(juce::TextButton::buttonColourId, active ? activeCol : normalCol);
        btn.setColour(juce::TextButton::textColourOffId,
                      active ? Theme::Colors::TextBright : Theme::Colors::Text);
    };

    style(selectButton_,   mode == ToolMode::Select);
    style(paintButton_,    mode == ToolMode::Paint);
    style(eraseButton_,    mode == ToolMode::Erase);
    style(drawRectButton_, mode == ToolMode::DrawRect);
    style(drawCircButton_, mode == ToolMode::DrawCircle);
    style(drawHexButton_,  mode == ToolMode::DrawHex);
}

// ============================================================
// Color picker -> paint color + selected shape color
// ============================================================

void EraeEditor::colorChanged(Color7 newColor)
{
    canvas_.setPaintColor(newColor);

    auto& ids = selectionManager_.getSelectedIds();
    auto& um = processor_.getUndoManager();
    for (auto& id : ids) {
        um.perform(std::make_unique<SetColorAction>(
            processor_.getLayout(), id, newColor, brighten(newColor)));
    }
}

// ============================================================
// Property panel -> behavior/param changes
// ============================================================

void EraeEditor::behaviorChanged(const std::string& shapeId)
{
    auto* s = processor_.getLayout().getShape(shapeId);
    if (s) {
        processor_.getLayout().setBehavior(shapeId, s->behavior, s->behaviorParams);
        // Rebuild DawFeedback lookup when note/channel assignments change
        processor_.getDawFeedback().updateFromLayout(processor_.getLayout());
    }
}

// ============================================================
// Canvas selection callback
// ============================================================

void EraeEditor::toolModeChanged(ToolMode)
{
    updateToolButtons();
    updateStatus();
}

void EraeEditor::selectionChanged()
{
    updateSelectionInfo();

    auto singleId = selectionManager_.getSingleSelectedId();
    bool multi = selectionManager_.count() > 1;

    if (!singleId.empty()) {
        auto* s = processor_.getLayout().getShape(singleId);
        if (s) {
            colorPicker_.setColor(s->color);
            propertyPanel_.loadShape(s);
        }
        showAlignmentButtons(false);
    } else if (multi) {
        propertyPanel_.clearShape();
        showAlignmentButtons(true);
    } else {
        propertyPanel_.clearShape();
        showAlignmentButtons(false);
    }
}

void EraeEditor::copyRequested()
{
    clipboard_.copy(processor_.getLayout(), selectionManager_.getSelectedIds());
}

void EraeEditor::cutRequested()
{
    clipboard_.cut(processor_.getLayout(), processor_.getUndoManager(), selectionManager_);
}

void EraeEditor::pasteRequested()
{
    clipboard_.paste(processor_.getLayout(), processor_.getUndoManager(),
                     selectionManager_, shapeCounterRef_);
}

void EraeEditor::updateSelectionInfo()
{
    int count = selectionManager_.count();
    if (count == 0) {
        selectionLabel_.setText("No selection", juce::dontSendNotification);
        return;
    }

    if (count > 1) {
        selectionLabel_.setText(juce::String(count) + " shapes selected", juce::dontSendNotification);
        return;
    }

    auto selId = selectionManager_.getSingleSelectedId();
    auto* s = processor_.getLayout().getShape(selId);
    if (!s) {
        selectionLabel_.setText("No selection", juce::dontSendNotification);
        return;
    }

    juce::String info;
    info << s->typeString() << " \"" << s->id << "\"\n";
    info << "Pos: " << juce::String(s->x, 1) << ", " << juce::String(s->y, 1);

    if (s->type == ShapeType::Rect) {
        auto* r = static_cast<RectShape*>(s);
        info << "  Size: " << juce::String(r->width, 1) << "x" << juce::String(r->height, 1);
    } else if (s->type == ShapeType::Circle) {
        auto* c = static_cast<CircleShape*>(s);
        info << "  R: " << juce::String(c->radius, 1);
    } else if (s->type == ShapeType::Hex) {
        auto* h = static_cast<HexShape*>(s);
        info << "  R: " << juce::String(h->radius, 1);
    }

    selectionLabel_.setText(info, juce::dontSendNotification);
}

// ============================================================
// Alignment
// ============================================================

void EraeEditor::showAlignmentButtons(bool show)
{
    alignLabel_.setVisible(show);
    alignLeftBtn_.setVisible(show);
    alignRightBtn_.setVisible(show);
    alignTopBtn_.setVisible(show);
    alignBottomBtn_.setVisible(show);
    alignCHBtn_.setVisible(show);
    alignCVBtn_.setVisible(show);
    distHBtn_.setVisible(show);
    distVBtn_.setVisible(show);
}

void EraeEditor::performAlignment(
    std::function<std::vector<AlignResult>(Layout&, const std::set<std::string>&)> fn,
    const std::string& name)
{
    auto& ids = selectionManager_.getSelectedIds();
    if (ids.size() < 2) return;

    auto results = fn(processor_.getLayout(), ids);
    if (!results.empty()) {
        processor_.getUndoManager().perform(
            std::make_unique<AlignAction>(processor_.getLayout(), std::move(results), name));
    }
}

// ============================================================
// Undo/Redo buttons
// ============================================================

void EraeEditor::updateUndoButtons()
{
    auto& um = processor_.getUndoManager();
    undoButton_.setEnabled(um.canUndo());
    redoButton_.setEnabled(um.canRedo());
}

// ============================================================
// Presets
// ============================================================

void EraeEditor::loadPreset(int index)
{
    auto& gens = Preset::getGenerators();
    if (index >= 0 && index < (int)gens.size()) {
        auto shapes = gens[index].fn();
        processor_.getUndoManager().perform(
            std::make_unique<SetShapesAction>(processor_.getLayout(), std::move(shapes)));
        selectionManager_.clear();
        processor_.getDawFeedback().updateFromLayout(processor_.getLayout());
        updateStatus();
    }
}

void EraeEditor::savePresetToFile()
{
    fileChooser_ = std::make_unique<juce::FileChooser>(
        "Save Preset", juce::File::getSpecialLocation(juce::File::userHomeDirectory),
        "*.json");

    fileChooser_->launchAsync(juce::FileBrowserComponent::saveMode |
                               juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc) {
            auto file = fc.getResult();
            if (file == juce::File{}) return;

            if (!file.hasFileExtension("json"))
                file = file.withFileExtension("json");

            Preset::saveToFile(file, processor_.getLayout().shapes());
        });
}

void EraeEditor::loadPresetFromFile()
{
    fileChooser_ = std::make_unique<juce::FileChooser>(
        "Load Preset", juce::File::getSpecialLocation(juce::File::userHomeDirectory),
        "*.json");

    fileChooser_->launchAsync(juce::FileBrowserComponent::openMode |
                               juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc) {
            auto file = fc.getResult();
            if (file == juce::File{}) return;

            auto shapes = Preset::loadFromFile(file);
            if (!shapes.empty()) {
                processor_.getUndoManager().perform(
                    std::make_unique<SetShapesAction>(processor_.getLayout(), std::move(shapes)));
                selectionManager_.clear();
                updateStatus();
            }
        });
}

void EraeEditor::updateStatus()
{
    auto& layout = processor_.getLayout();
    auto mode = canvas_.getToolMode();
    const char* modeName = "?";
    switch (mode) {
        case ToolMode::Select:     modeName = "Select"; break;
        case ToolMode::Paint:      modeName = "Paint"; break;
        case ToolMode::Erase:      modeName = "Erase"; break;
        case ToolMode::DrawRect:   modeName = "Draw Rect"; break;
        case ToolMode::DrawCircle: modeName = "Draw Circle"; break;
        case ToolMode::DrawHex:    modeName = "Draw Hex"; break;
    }

    auto& conn = processor_.getConnection();
    juce::String connStr = conn.isConnected() ? "Connected" : "--";

    auto& ml = processor_.getMultiLayout();
    juce::String pageStr = "Page " + juce::String(ml.currentPageIndex() + 1)
                         + "/" + juce::String(ml.numPages());
    pageLabel_.setText(pageStr, juce::dontSendNotification);

    statusLabel_.setText(
        juce::String(layout.numShapes()) + " shapes  |  " +
        modeName + "  |  " + pageStr + "  |  " + connStr +
        "  |  Zoom " + juce::String((int)(canvas_.getZoom() * 100)) + "%",
        juce::dontSendNotification);
}

void EraeEditor::updateConnectButton()
{
    bool connected = processor_.getConnection().isConnected();
    connectButton_.setButtonText(connected ? "Connected" : "Connect");
    connectButton_.setColour(juce::TextButton::buttonColourId,
                             connected ? Theme::Colors::Success.darker(0.4f) : Theme::Colors::ButtonBg);
    connectButton_.setColour(juce::TextButton::textColourOffId,
                             connected ? Theme::Colors::Success : Theme::Colors::Text);
}

// ============================================================
// Timer — refresh finger overlay + connection status
// ============================================================

void EraeEditor::timerCallback()
{
    auto rawFingers = processor_.getActiveFingers();
    std::map<uint64_t, GridCanvas::FingerDot> dots;
    for (auto& [id, fi] : rawFingers)
        dots[id] = {fi.x, fi.y, fi.z};
    canvas_.setFingers(dots);

    canvas_.setWidgetStates(processor_.getShapeWidgetStates());

    // DAW feedback highlights
    auto& daw = processor_.getDawFeedback();
    if (daw.isEnabled())
        canvas_.setHighlightedShapes(daw.getHighlightedShapes());
    else
        canvas_.setHighlightedShapes({});

    updateConnectButton();
}

} // namespace erae
