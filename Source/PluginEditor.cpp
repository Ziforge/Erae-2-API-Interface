#include "PluginEditor.h"
#include "Model/Preset.h"
#include "Core/LayoutActions.h"
#include "Core/AlignmentTools.h"
#include "Core/ShapeMorph.h"

namespace erae {

EraeEditor::EraeEditor(EraeProcessor& p)
    : AudioProcessorEditor(p),
      processor_(p),
      canvas_(p.getLayout(), p.getUndoManager(), selectionManager_),
      midiPanel_(p.getLayout())
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
    drawRectButton_.onClick  = [this] { setTool(ToolMode::DrawRect); };
    drawCircButton_.onClick  = [this] { setTool(ToolMode::DrawCircle); };
    drawHexButton_.onClick   = [this] { setTool(ToolMode::DrawHex); };
    drawPolyButton_.onClick  = [this] { setTool(ToolMode::DrawPoly); };
    drawPixelButton_.onClick = [this] { setTool(ToolMode::DrawPixel); };
    pixelDoneButton_.onClick = [this] {
        if (canvas_.isCreatingPoly())
            canvas_.finishPolygonCreation();
        else
            canvas_.finishPixelCreation();
    };

    selectButton_.setTooltip("Select tool (V)");
    paintButton_.setTooltip("Paint pixels (B)");
    eraseButton_.setTooltip("Erase pixels (E)");
    drawRectButton_.setTooltip("Draw rectangle (R)");
    drawCircButton_.setTooltip("Draw circle (C)");
    drawHexButton_.setTooltip("Draw hexagon (H)");
    drawPolyButton_.setTooltip("Draw polygon (P)");
    drawPixelButton_.setTooltip("Draw pixel shape (G)");
    pixelDoneButton_.setTooltip("Finalize pixel shape (Enter)");

    addAndMakeVisible(selectButton_);
    addAndMakeVisible(paintButton_);
    addAndMakeVisible(eraseButton_);
    addAndMakeVisible(drawRectButton_);
    addAndMakeVisible(drawCircButton_);
    addAndMakeVisible(drawHexButton_);
    addAndMakeVisible(drawPolyButton_);
    addAndMakeVisible(drawPixelButton_);
    pixelDoneButton_.setVisible(false);
    addAndMakeVisible(pixelDoneButton_);

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

    newButton_.onClick = [this] {
        auto& ml = processor_.getMultiLayout();
        ml.reset();
        canvas_.setLayout(ml.currentPage());
        selectionManager_.clear();
        processor_.getDawFeedback().updateFromLayout(ml.currentPage());
        processor_.getUndoManager().clear();
        updateStatus();
    };
    saveButton_.onClick = [this] { savePresetToFile(); };
    loadButton_.onClick = [this] { loadPresetFromFile(); };

    deleteButton_.setTooltip("Delete selected (Del)");
    dupeButton_.setTooltip("Duplicate selected (Ctrl+D)");
    clearButton_.setTooltip("Clear all shapes");
    zoomFitButton_.setTooltip("Zoom to fit");
    newButton_.setTooltip("New blank layout");
    saveButton_.setTooltip("Save preset to file");
    loadButton_.setTooltip("Load preset from file");

    addAndMakeVisible(deleteButton_);
    addAndMakeVisible(dupeButton_);
    addAndMakeVisible(clearButton_);
    addAndMakeVisible(zoomFitButton_);
    addAndMakeVisible(newButton_);
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
        if (!ml.canAddPage()) return;
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
        if (!ml.canAddPage()) return;
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

    // --- Canvas ---
    canvas_.addListener(this);
    addAndMakeVisible(canvas_);

    // --- Sidebar: Tab bar ---
    tabBar_.addListener(this);
    addAndMakeVisible(tabBar_);

    // --- Sidebar: Shape tab — color picker ---
    colorLabel_.setText("COLOR", juce::dontSendNotification);
    colorLabel_.setFont(juce::Font(Theme::FontSection, juce::Font::bold));
    colorLabel_.setColour(juce::Label::textColourId, Theme::Colors::TextDim);
    addAndMakeVisible(colorLabel_);

    colorPicker_.addListener(this);
    colorPicker_.setColor(Color7{0, 80, 127});
    addAndMakeVisible(colorPicker_);

    // --- Sidebar: Shape tab — visual style controls ---
    visualLabel_.setFont(juce::Font(Theme::FontSection, juce::Font::bold));
    visualLabel_.setColour(juce::Label::textColourId, Theme::Colors::TextDim);
    addAndMakeVisible(visualLabel_);

    visualBox_.addItem("Static",        1);
    visualBox_.addItem("Fill Bar",      2);
    visualBox_.addItem("Position Dot",  3);
    visualBox_.addItem("Radial Arc",    4);
    visualBox_.addItem("Pressure Glow", 5);
    visualBox_.onChange = [this] {
        auto singleId = selectionManager_.getSingleSelectedId();
        if (singleId.empty()) return;
        auto* s = processor_.getLayout().getShape(singleId);
        if (!s) return;
        int id = visualBox_.getSelectedId();
        VisualStyle vstyle;
        switch (id) {
            case 1: vstyle = VisualStyle::Static; break;
            case 2: vstyle = VisualStyle::FillBar; break;
            case 3: vstyle = VisualStyle::PositionDot; break;
            case 4: vstyle = VisualStyle::RadialArc; break;
            case 5: vstyle = VisualStyle::PressureGlow; break;
            default: return;
        }
        s->visualStyle = visualStyleToString(vstyle);
        updateVisualControls();
        // Write visual params
        auto* vobj = new juce::DynamicObject();
        if (vstyle == VisualStyle::FillBar)
            vobj->setProperty("fill_horizontal", fillHorizToggle_.getToggleState());
        s->visualParams = juce::var(vobj);
        processor_.getLayout().setBehavior(singleId, s->behavior, s->behaviorParams);
    };
    addAndMakeVisible(visualBox_);

    fillHorizLabel_.setFont(juce::Font(Theme::FontBase));
    fillHorizLabel_.setColour(juce::Label::textColourId, Theme::Colors::TextDim);
    addAndMakeVisible(fillHorizLabel_);

    fillHorizToggle_.onClick = [this] {
        auto singleId = selectionManager_.getSingleSelectedId();
        if (singleId.empty()) return;
        auto* s = processor_.getLayout().getShape(singleId);
        if (!s) return;
        auto* vobj = new juce::DynamicObject();
        vobj->setProperty("fill_horizontal", fillHorizToggle_.getToggleState());
        s->visualParams = juce::var(vobj);
        processor_.getLayout().setBehavior(singleId, s->behavior, s->behaviorParams);
    };
    addAndMakeVisible(fillHorizToggle_);

    // --- Sidebar: Shape tab — alignment buttons ---
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

    // --- Sidebar: Shape tab — morph controls ---
    morphLabel_.setFont(juce::Font(Theme::FontSection, juce::Font::bold));
    morphLabel_.setColour(juce::Label::textColourId, Theme::Colors::TextDim);
    addAndMakeVisible(morphLabel_);

    morphSlider_.setRange(0.0, 1.0, 0.01);
    morphSlider_.setValue(0.5, juce::dontSendNotification);
    morphSlider_.setSliderStyle(juce::Slider::LinearBar);
    morphSlider_.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 40, 20);
    morphSlider_.setColour(juce::Slider::trackColourId, Theme::Colors::Accent);
    morphSlider_.setColour(juce::Slider::textBoxTextColourId, Theme::Colors::Text);
    addAndMakeVisible(morphSlider_);

    morphButton_.setTooltip("Create a morphed shape between two selected shapes");
    morphButton_.onClick = [this] {
        auto& ids = selectionManager_.getSelectedIds();
        if (ids.size() != 2) return;
        auto it = ids.begin();
        auto* shapeA = processor_.getLayout().getShape(*it++);
        auto* shapeB = processor_.getLayout().getShape(*it);
        if (!shapeA || !shapeB) return;

        float t = (float)morphSlider_.getValue();
        auto newId = "morph_" + std::to_string(++shapeCounterRef_);
        auto morphed = ShapeMorph::morph(*shapeA, *shapeB, t, newId);
        if (morphed) {
            morphed->behavior = shapeA->behavior;
            morphed->behaviorParams = shapeA->behaviorParams;
            morphed->visualStyle = shapeA->visualStyle;
            morphed->visualParams = shapeA->visualParams;
            processor_.getUndoManager().perform(
                std::make_unique<AddShapeAction>(processor_.getLayout(), std::move(morphed)));
            selectionManager_.select(newId);
        }
    };
    addAndMakeVisible(morphButton_);

    morphLabel_.setVisible(false);
    morphSlider_.setVisible(false);
    morphButton_.setVisible(false);

    // --- Sidebar: MIDI tab ---
    midiPanel_.addListener(this);
    addAndMakeVisible(midiPanel_);

    // --- Sidebar: Output tab ---
    outputPanel_.addListener(this);
    outputPanel_.setOscState(processor_.getOscOutput().isEnabled(),
                             processor_.getOscOutput().getHost(),
                             processor_.getOscOutput().getPort());
    addAndMakeVisible(outputPanel_);

    // --- Sidebar: Library tab ---
    libLabel_.setFont(juce::Font(Theme::FontSection, juce::Font::bold));
    libLabel_.setColour(juce::Label::textColourId, Theme::Colors::TextDim);
    addAndMakeVisible(libLabel_);

    libraryListModel_.library = &library_;
    libraryList_.setModel(&libraryListModel_);
    libraryList_.setRowHeight(20);
    libraryList_.setColour(juce::ListBox::backgroundColourId, Theme::Colors::ButtonBg);
    libraryList_.setColour(juce::ListBox::outlineColourId, Theme::Colors::Separator);
    addAndMakeVisible(libraryList_);

    libSaveBtn_.setTooltip("Save selected canvas shape to library");
    libSaveBtn_.onClick = [this] {
        auto selId = selectionManager_.getSingleSelectedId();
        if (selId.empty()) return;
        auto* s = processor_.getLayout().getShape(selId);
        if (!s) return;
        auto name = s->typeString() + "_" + std::to_string(library_.numEntries() + 1);
        library_.addEntry(name, s);
        library_.save(ShapeLibrary::getDefaultLibraryFile());
        libraryList_.updateContent();
        libraryList_.repaint();
    };
    addAndMakeVisible(libSaveBtn_);

    libPlaceBtn_.setTooltip("Place selected library entry on canvas");
    libPlaceBtn_.onClick = [this] {
        int row = libraryList_.getSelectedRow();
        if (row < 0) return;
        auto id = library_.placeOnCanvas(row, processor_.getLayout(),
                                          processor_.getUndoManager(),
                                          10.0f, 10.0f, shapeCounterRef_);
        if (!id.empty())
            selectionManager_.select(id);
    };
    addAndMakeVisible(libPlaceBtn_);

    libFlipHBtn_.setTooltip("Flip selected shape horizontally");
    libFlipHBtn_.onClick = [this] {
        auto selId = selectionManager_.getSingleSelectedId();
        if (selId.empty()) return;
        auto* s = processor_.getLayout().getShape(selId);
        if (!s) return;
        ShapeLibrary::flipHorizontal(*s);
        processor_.getLayout().notifyListeners();
    };
    addAndMakeVisible(libFlipHBtn_);

    libFlipVBtn_.setTooltip("Flip selected shape vertically");
    libFlipVBtn_.onClick = [this] {
        auto selId = selectionManager_.getSingleSelectedId();
        if (selId.empty()) return;
        auto* s = processor_.getLayout().getShape(selId);
        if (!s) return;
        ShapeLibrary::flipVertical(*s);
        processor_.getLayout().notifyListeners();
    };
    addAndMakeVisible(libFlipVBtn_);

    libDeleteBtn_.setTooltip("Remove entry from library");
    libDeleteBtn_.onClick = [this] {
        int row = libraryList_.getSelectedRow();
        if (row < 0) return;
        library_.removeEntry(row);
        library_.save(ShapeLibrary::getDefaultLibraryFile());
        libraryList_.updateContent();
        libraryList_.repaint();
    };
    addAndMakeVisible(libDeleteBtn_);

    // Load library from disk
    library_.load(ShapeLibrary::getDefaultLibraryFile());
    libraryList_.updateContent();

    // --- Sidebar: Selection info (always visible) ---
    selectionLabel_.setFont(juce::Font(Theme::FontSmall));
    selectionLabel_.setColour(juce::Label::textColourId, Theme::Colors::TextDim);
    addAndMakeVisible(selectionLabel_);

    // --- Status bar ---
    statusLabel_.setFont(juce::Font(Theme::FontStatus));
    statusLabel_.setColour(juce::Label::textColourId, Theme::Colors::TextDim);
    addAndMakeVisible(statusLabel_);

    // Default to select mode + Shape tab
    setTool(ToolMode::Select);
    showTabContent(SidebarTabBar::Shape);
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
    midiPanel_.removeListener(this);
    outputPanel_.removeListener(this);
    tabBar_.removeListener(this);
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

    float x2 = (float)(pixelDoneButton_.isVisible()
                        ? pixelDoneButton_.getRight() + 5
                        : drawPixelButton_.getRight() + 5);
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
    toolbar.removeFromLeft(Theme::SpaceXS);
    drawPolyButton_.setBounds(toolbar.removeFromLeft(btnW));
    toolbar.removeFromLeft(Theme::SpaceXS);
    drawPixelButton_.setBounds(toolbar.removeFromLeft(btnW + 4));
    toolbar.removeFromLeft(Theme::SpaceXS);
    pixelDoneButton_.setBounds(toolbar.removeFromLeft(btnW));
    toolbar.removeFromLeft(Theme::SpaceLG);

    // Brush size
    brushSizeSelector_.setBounds(toolbar.removeFromLeft(56));
    toolbar.removeFromLeft(Theme::SpaceMD);

    // Presets + New/Save/Load
    presetSelector_.setBounds(toolbar.removeFromLeft(140));
    toolbar.removeFromLeft(Theme::SpaceSM);
    newButton_.setBounds(toolbar.removeFromLeft(btnW - 4));
    toolbar.removeFromLeft(Theme::SpaceXS);
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

    // Tab bar at top of sidebar
    tabBar_.setBounds(sidebar.removeFromTop(Theme::TabBarHeight));
    sidebar.removeFromTop(Theme::SpaceSM);

    // Selection info at bottom of sidebar
    auto selArea = sidebar.removeFromBottom(36);
    selectionLabel_.setBounds(selArea);

    // Tab content area
    auto tabContent = sidebar;

    // Layout active tab content
    auto activeTab = tabBar_.getActiveTab();

    // ===== Shape Tab =====
    if (activeTab == SidebarTabBar::Shape) {
        auto content = tabContent;

        colorLabel_.setBounds(content.removeFromTop(18));
        content.removeFromTop(Theme::SpaceMD);

        int pickerH = std::min(content.getHeight() / 2, 280);
        colorPicker_.setBounds(content.removeFromTop(pickerH));
        content.removeFromTop(Theme::SpaceLG);

        // Visual style section (only when single shape selected)
        if (visualLabel_.isVisible()) {
            visualLabel_.setBounds(content.removeFromTop(18));
            content.removeFromTop(3);
            visualBox_.setBounds(content.removeFromTop(26));
            content.removeFromTop(5);
            if (fillHorizToggle_.isVisible()) {
                auto row = content.removeFromTop(26);
                fillHorizLabel_.setBounds(row.removeFromLeft(74));
                fillHorizToggle_.setBounds(row.removeFromLeft(26));
                content.removeFromTop(3);
            }
            content.removeFromTop(Theme::SpaceSM);
        }

        // Alignment section (when 2+ selected)
        if (alignLabel_.isVisible()) {
            alignLabel_.setBounds(content.removeFromTop(18));
            content.removeFromTop(3);
            {
                auto alignRow1 = content.removeFromTop(24);
                int abw = (alignRow1.getWidth() - 3 * Theme::SpaceXS) / 4;
                alignLeftBtn_.setBounds(alignRow1.removeFromLeft(abw));
                alignRow1.removeFromLeft(Theme::SpaceXS);
                alignRightBtn_.setBounds(alignRow1.removeFromLeft(abw));
                alignRow1.removeFromLeft(Theme::SpaceXS);
                alignTopBtn_.setBounds(alignRow1.removeFromLeft(abw));
                alignRow1.removeFromLeft(Theme::SpaceXS);
                alignBottomBtn_.setBounds(alignRow1);
            }
            content.removeFromTop(3);
            {
                auto alignRow2 = content.removeFromTop(24);
                int abw = (alignRow2.getWidth() - 3 * Theme::SpaceXS) / 4;
                alignCHBtn_.setBounds(alignRow2.removeFromLeft(abw));
                alignRow2.removeFromLeft(Theme::SpaceXS);
                alignCVBtn_.setBounds(alignRow2.removeFromLeft(abw));
                alignRow2.removeFromLeft(Theme::SpaceXS);
                distHBtn_.setBounds(alignRow2.removeFromLeft(abw));
                alignRow2.removeFromLeft(Theme::SpaceXS);
                distVBtn_.setBounds(alignRow2);
            }
            content.removeFromTop(Theme::SpaceSM);
        }

        // Morph section (when 2 selected)
        if (morphLabel_.isVisible()) {
            morphLabel_.setBounds(content.removeFromTop(18));
            content.removeFromTop(3);
            morphSlider_.setBounds(content.removeFromTop(24));
            content.removeFromTop(3);
            morphButton_.setBounds(content.removeFromTop(24));
        }
    }

    // ===== MIDI Tab =====
    if (activeTab == SidebarTabBar::MIDI) {
        midiPanel_.setBounds(tabContent);
    }

    // ===== Output Tab =====
    if (activeTab == SidebarTabBar::Output) {
        outputPanel_.setBounds(tabContent);
    }

    // ===== Library Tab =====
    if (activeTab == SidebarTabBar::Library) {
        auto content = tabContent;

        libLabel_.setBounds(content.removeFromTop(18));
        content.removeFromTop(3);

        // Library list fills available space minus buttons
        int btnAreaH = 24 + 3 + 24;
        auto btnArea = content.removeFromBottom(btnAreaH);

        libraryList_.setBounds(content);

        {
            auto btnRow1 = btnArea.removeFromTop(24);
            int lbw = (btnRow1.getWidth() - 2 * Theme::SpaceXS) / 3;
            libSaveBtn_.setBounds(btnRow1.removeFromLeft(lbw));
            btnRow1.removeFromLeft(Theme::SpaceXS);
            libPlaceBtn_.setBounds(btnRow1.removeFromLeft(lbw));
            btnRow1.removeFromLeft(Theme::SpaceXS);
            libDeleteBtn_.setBounds(btnRow1);
        }
        btnArea.removeFromTop(3);
        {
            auto btnRow2 = btnArea.removeFromTop(24);
            int lbw = (btnRow2.getWidth() - Theme::SpaceXS) / 2;
            libFlipHBtn_.setBounds(btnRow2.removeFromLeft(lbw));
            btnRow2.removeFromLeft(Theme::SpaceXS);
            libFlipVBtn_.setBounds(btnRow2);
        }
    }

    // Canvas (with 2px inset for recessed effect)
    area.reduce(2, 2);
    canvas_.setBounds(area);
}

// ============================================================
// Tab switching
// ============================================================

void EraeEditor::tabChanged(SidebarTabBar::Tab newTab)
{
    showTabContent(newTab);
    resized();
}

void EraeEditor::showTabContent(SidebarTabBar::Tab tab)
{
    // Hide all tab content first
    // Shape tab components
    colorLabel_.setVisible(false);
    colorPicker_.setVisible(false);
    visualLabel_.setVisible(false);
    visualBox_.setVisible(false);
    fillHorizLabel_.setVisible(false);
    fillHorizToggle_.setVisible(false);
    showAlignmentButtons(false);
    morphLabel_.setVisible(false);
    morphSlider_.setVisible(false);
    morphButton_.setVisible(false);

    // MIDI tab
    midiPanel_.setVisible(false);

    // Output tab
    outputPanel_.setVisible(false);

    // Library tab
    libLabel_.setVisible(false);
    libraryList_.setVisible(false);
    libSaveBtn_.setVisible(false);
    libPlaceBtn_.setVisible(false);
    libFlipHBtn_.setVisible(false);
    libFlipVBtn_.setVisible(false);
    libDeleteBtn_.setVisible(false);

    // Show active tab content
    switch (tab) {
        case SidebarTabBar::Shape: {
            colorLabel_.setVisible(true);
            colorPicker_.setVisible(true);

            auto singleId = selectionManager_.getSingleSelectedId();
            bool multi = selectionManager_.count() > 1;

            // Visual controls only when single shape selected
            if (!singleId.empty()) {
                visualLabel_.setVisible(true);
                visualBox_.setVisible(true);
                updateVisualControls();
            }

            // Alignment when 2+ selected
            if (multi)
                showAlignmentButtons(true);

            // Morph when exactly 2 selected
            if (selectionManager_.count() == 2) {
                morphLabel_.setVisible(true);
                morphSlider_.setVisible(true);
                morphButton_.setVisible(true);
            }
            break;
        }
        case SidebarTabBar::MIDI:
            midiPanel_.setVisible(true);
            break;

        case SidebarTabBar::Output:
            outputPanel_.setVisible(true);
            break;

        case SidebarTabBar::Library:
            libLabel_.setVisible(true);
            libraryList_.setVisible(true);
            libSaveBtn_.setVisible(true);
            libPlaceBtn_.setVisible(true);
            libFlipHBtn_.setVisible(true);
            libFlipVBtn_.setVisible(true);
            libDeleteBtn_.setVisible(true);
            break;

        default:
            break;
    }
}

void EraeEditor::updateVisualControls()
{
    auto singleId = selectionManager_.getSingleSelectedId();
    if (singleId.empty()) return;
    auto* s = processor_.getLayout().getShape(singleId);
    if (!s) return;

    auto vstyle = visualStyleFromString(s->visualStyle);
    bool showFillHoriz = (vstyle == VisualStyle::FillBar);
    fillHorizLabel_.setVisible(showFillHoriz);
    fillHorizToggle_.setVisible(showFillHoriz);
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

    style(selectButton_,    mode == ToolMode::Select);
    style(paintButton_,     mode == ToolMode::Paint);
    style(eraseButton_,     mode == ToolMode::Erase);
    style(drawRectButton_,  mode == ToolMode::DrawRect);
    style(drawCircButton_,  mode == ToolMode::DrawCircle);
    style(drawHexButton_,   mode == ToolMode::DrawHex);
    style(drawPolyButton_,  mode == ToolMode::DrawPoly);
    style(drawPixelButton_, mode == ToolMode::DrawPixel);

    // Show "Done" button only in DrawPixel/DrawPoly mode
    pixelDoneButton_.setVisible(mode == ToolMode::DrawPixel || mode == ToolMode::DrawPoly);
    if (mode == ToolMode::DrawPoly)
        pixelDoneButton_.setButtonText("Done");
    else
        pixelDoneButton_.setButtonText("Done");
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
// MidiPanel::Listener — behavior/param changes
// ============================================================

void EraeEditor::behaviorChanged(const std::string& shapeId)
{
    auto* s = processor_.getLayout().getShape(shapeId);
    if (s) {
        processor_.getLayout().setBehavior(shapeId, s->behavior, s->behaviorParams);
        processor_.getDawFeedback().updateFromLayout(processor_.getLayout());
    }
}

void EraeEditor::midiLearnRequested(const std::string& shapeId)
{
    midiLearnShapeId_ = shapeId;
    processor_.startMidiLearn();
}

void EraeEditor::midiLearnCancelled()
{
    midiLearnShapeId_.clear();
    processor_.cancelMidiLearn();
}

// ============================================================
// OutputPanel::Listener — CV + OSC changes
// ============================================================

void EraeEditor::cvParamsChanged(const std::string& shapeId)
{
    auto* s = processor_.getLayout().getShape(shapeId);
    if (s) {
        processor_.getLayout().setBehavior(shapeId, s->behavior, s->behaviorParams);
    }
}

void EraeEditor::oscSettingsChanged(bool enabled, const std::string& host, int port)
{
    auto& osc = processor_.getOscOutput();
    if (enabled)
        osc.enable(host, port);
    else
        osc.disable();
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

    // Update all panel data
    if (!singleId.empty()) {
        auto* s = processor_.getLayout().getShape(singleId);
        if (s) {
            colorPicker_.setColor(s->color);
            midiPanel_.loadShape(s);
            outputPanel_.loadShape(s);

            // Visual style controls
            auto vstyle = visualStyleFromString(s->visualStyle);
            switch (vstyle) {
                case VisualStyle::Static:       visualBox_.setSelectedId(1, juce::dontSendNotification); break;
                case VisualStyle::FillBar:      visualBox_.setSelectedId(2, juce::dontSendNotification); break;
                case VisualStyle::PositionDot:  visualBox_.setSelectedId(3, juce::dontSendNotification); break;
                case VisualStyle::RadialArc:    visualBox_.setSelectedId(4, juce::dontSendNotification); break;
                case VisualStyle::PressureGlow: visualBox_.setSelectedId(5, juce::dontSendNotification); break;
            }
            if (auto* vobj = s->visualParams.getDynamicObject())
                if (vobj->hasProperty("fill_horizontal"))
                    fillHorizToggle_.setToggleState((bool)vobj->getProperty("fill_horizontal"), juce::dontSendNotification);
        }
    } else {
        midiPanel_.clearShape();
        outputPanel_.clearShape();
    }

    // Refresh visible tab content (don't auto-switch)
    showTabContent(tabBar_.getActiveTab());
    resized();
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
    } else if (s->type == ShapeType::Pixel) {
        auto* p = static_cast<PixelShape*>(s);
        info << "  Cells: " << juce::String((int)p->relCells.size());
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
        case ToolMode::DrawPoly:   modeName = "Draw Poly"; break;
        case ToolMode::DrawPixel:  modeName = "Draw Pixel"; break;
        case ToolMode::EditShape:  modeName = "Edit Shape"; break;
    }

    auto& conn = processor_.getConnection();
    juce::String connStr = conn.isConnected() ? "Connected" : "--";

    auto& ml = processor_.getMultiLayout();
    juce::String pageStr = "Page " + juce::String(ml.currentPageIndex() + 1)
                         + "/" + juce::String(ml.numPages());
    pageLabel_.setText(pageStr, juce::dontSendNotification);

    // Enforce 8-page limit on add/dup buttons
    bool canAdd = ml.canAddPage();
    pageAddButton_.setEnabled(canAdd);
    pageDupButton_.setEnabled(canAdd);
    pageDelButton_.setEnabled(ml.numPages() > 1);

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

    // MIDI learn: poll for result and apply to target shape
    if (!midiLearnShapeId_.empty() && processor_.hasMidiLearnResult()) {
        midiPanel_.applyMidiLearnResult(
            processor_.getMidiLearnNote(),
            processor_.getMidiLearnCC(),
            processor_.getMidiLearnChannel(),
            processor_.getMidiLearnIsCC());
        midiLearnShapeId_.clear();
    }

    updateConnectButton();
}

} // namespace erae
