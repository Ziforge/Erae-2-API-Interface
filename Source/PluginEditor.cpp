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

    // --- Toolbar: Design button ---
    designButton_.setTooltip("Design a new shape for the library");
    designButton_.onClick = [this] { canvas_.enterDesignMode(); };
    addAndMakeVisible(designButton_);

    // --- Toolbar: Design mode Done/Cancel/Symmetry (hidden by default) ---
    designDoneButton_.setTooltip("Finish design and save to library (Enter)");
    designDoneButton_.onClick = [this] { canvas_.exitDesignMode(true); };
    designDoneButton_.setVisible(false);
    addAndMakeVisible(designDoneButton_);

    designCancelButton_.setTooltip("Cancel design (ESC)");
    designCancelButton_.onClick = [this] { canvas_.exitDesignMode(false); };
    designCancelButton_.setVisible(false);
    addAndMakeVisible(designCancelButton_);

    designSymHToggle_.setTooltip("Horizontal symmetry (S)");
    designSymHToggle_.onClick = [this] {
        canvas_.setDesignSymmetryH(designSymHToggle_.getToggleState());
    };
    designSymHToggle_.setVisible(false);
    addAndMakeVisible(designSymHToggle_);

    designSymVToggle_.setTooltip("Vertical symmetry (D)");
    designSymVToggle_.onClick = [this] {
        canvas_.setDesignSymmetryV(designSymVToggle_.getToggleState());
    };
    designSymVToggle_.setVisible(false);
    addAndMakeVisible(designSymVToggle_);

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

    // --- Toolbar: undo/redo ---
    undoButton_.onClick = [this] { processor_.getUndoManager().undo(); };
    redoButton_.onClick = [this] { processor_.getUndoManager().redo(); };
    undoButton_.setTooltip("Undo (Ctrl+Z)");
    redoButton_.setTooltip("Redo (Ctrl+Shift+Z)");
    addAndMakeVisible(undoButton_);
    addAndMakeVisible(redoButton_);

    // --- Toolbar: delete/dupe/fit ---
    deleteButton_.onClick = [this] { canvas_.deleteSelected(); };
    dupeButton_.onClick   = [this] { canvas_.duplicateSelected(); };
    zoomFitButton_.onClick = [this] { canvas_.zoomToFit(); };

    deleteButton_.setTooltip("Delete selected (Del)");
    dupeButton_.setTooltip("Duplicate selected (Ctrl+D)");
    zoomFitButton_.setTooltip("Zoom to fit");

    addAndMakeVisible(deleteButton_);
    addAndMakeVisible(dupeButton_);
    addAndMakeVisible(zoomFitButton_);

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
    colorPicker_.addListener(this);
    colorPicker_.setColor(Color7{0, 80, 127});

    // --- Sidebar: Shape tab — visual style controls ---
    visualLabel_.setFont(juce::Font(Theme::FontSection, juce::Font::bold));
    visualLabel_.setColour(juce::Label::textColourId, Theme::Colors::TextDim);

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
        auto* vobj = new juce::DynamicObject();
        if (vstyle == VisualStyle::FillBar)
            vobj->setProperty("fill_horizontal", fillHorizToggle_.getToggleState());
        s->visualParams = juce::var(vobj);
        processor_.getLayout().setBehavior(singleId, s->behavior, s->behaviorParams);
    };

    fillHorizLabel_.setFont(juce::Font(Theme::FontBase));
    fillHorizLabel_.setColour(juce::Label::textColourId, Theme::Colors::TextDim);

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

    // --- Sidebar: Shape tab — alignment buttons ---
    alignLabel_.setFont(juce::Font(Theme::FontSection, juce::Font::bold));
    alignLabel_.setColour(juce::Label::textColourId, Theme::Colors::TextDim);

    auto setupAlignBtn = [this](juce::TextButton& btn, const juce::String& tip) {
        btn.setTooltip(tip);
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

    morphSlider_.setRange(0.0, 1.0, 0.01);
    morphSlider_.setValue(0.5, juce::dontSendNotification);
    morphSlider_.setSliderStyle(juce::Slider::LinearBar);
    morphSlider_.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 40, 20);
    morphSlider_.setColour(juce::Slider::trackColourId, Theme::Colors::Accent);
    morphSlider_.setColour(juce::Slider::textBoxTextColourId, Theme::Colors::Text);

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

    morphLabel_.setVisible(false);
    morphSlider_.setVisible(false);
    morphButton_.setVisible(false);

    // --- Sidebar: Shape tab — MIDI panel (embedded in viewport) ---
    midiPanel_.addListener(this);

    // --- Sidebar: Effects tab — EffectPanel ---
    effectPanel_.addListener(this);
    addAndMakeVisible(effectPanel_);
    effectPanel_.setVisible(false);

    // --- Sidebar: Shape tab — CV controls (per-shape) ---
    cvLabel_.setFont(juce::Font(Theme::FontSection, juce::Font::bold));
    cvLabel_.setColour(juce::Label::textColourId, Theme::Colors::TextDim);
    cvEnableLabel_.setFont(juce::Font(Theme::FontBase));
    cvEnableLabel_.setColour(juce::Label::textColourId, Theme::Colors::TextDim);
    cvEnableToggle_.onClick = [this] {
        if (cvLoading_ || !cvCurrentShape_) return;
        writeCVToShape();
        auto* s = cvCurrentShape_;
        processor_.getLayout().setBehavior(s->id, s->behavior, s->behaviorParams);
        // Re-layout to show/hide channel slider
        resized();
    };
    cvChannelLabel_.setFont(juce::Font(Theme::FontBase));
    cvChannelLabel_.setColour(juce::Label::textColourId, Theme::Colors::TextDim);
    cvChannelSlider_.setRange(0, 31, 1.0);
    cvChannelSlider_.setValue(0, juce::dontSendNotification);
    cvChannelSlider_.setSliderStyle(juce::Slider::LinearBar);
    cvChannelSlider_.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 40, 20);
    cvChannelSlider_.setColour(juce::Slider::trackColourId, Theme::Colors::Accent);
    cvChannelSlider_.setColour(juce::Slider::textBoxTextColourId, Theme::Colors::Text);
    cvChannelSlider_.onValueChange = [this] {
        if (cvLoading_ || !cvCurrentShape_) return;
        writeCVToShape();
        auto* s = cvCurrentShape_;
        processor_.getLayout().setBehavior(s->id, s->behavior, s->behaviorParams);
    };

    // Add shape tab components to shapeContent_ (child of viewport)
    shapeContent_.addAndMakeVisible(colorLabel_);
    shapeContent_.addAndMakeVisible(colorPicker_);
    shapeContent_.addAndMakeVisible(visualLabel_);
    shapeContent_.addAndMakeVisible(visualBox_);
    shapeContent_.addAndMakeVisible(fillHorizLabel_);
    shapeContent_.addAndMakeVisible(fillHorizToggle_);
    shapeContent_.addAndMakeVisible(midiPanel_);
    shapeContent_.addAndMakeVisible(cvLabel_);
    shapeContent_.addAndMakeVisible(cvEnableLabel_);
    shapeContent_.addAndMakeVisible(cvEnableToggle_);
    shapeContent_.addAndMakeVisible(cvChannelLabel_);
    shapeContent_.addAndMakeVisible(cvChannelSlider_);
    shapeContent_.addAndMakeVisible(alignLabel_);
    shapeContent_.addAndMakeVisible(alignLeftBtn_);
    shapeContent_.addAndMakeVisible(alignRightBtn_);
    shapeContent_.addAndMakeVisible(alignTopBtn_);
    shapeContent_.addAndMakeVisible(alignBottomBtn_);
    shapeContent_.addAndMakeVisible(alignCHBtn_);
    shapeContent_.addAndMakeVisible(alignCVBtn_);
    shapeContent_.addAndMakeVisible(distHBtn_);
    shapeContent_.addAndMakeVisible(distVBtn_);
    shapeContent_.addAndMakeVisible(morphLabel_);
    shapeContent_.addAndMakeVisible(morphSlider_);
    shapeContent_.addAndMakeVisible(morphButton_);

    shapeViewport_.setViewedComponent(&shapeContent_, false);
    shapeViewport_.setScrollBarsShown(true, false);
    addAndMakeVisible(shapeViewport_);

    // --- Settings tab: file section ---
    fileLabel_.setFont(juce::Font(Theme::FontSection, juce::Font::bold));
    fileLabel_.setColour(juce::Label::textColourId, Theme::Colors::TextDim);
    addAndMakeVisible(fileLabel_);

    presetSelector_.setTextWhenNothingSelected("Presets...");
    auto& gens = Preset::getGenerators();
    for (int i = 0; i < (int)gens.size(); ++i)
        presetSelector_.addItem(gens[i].name, i + 1);
    presetSelector_.onChange = [this] {
        int idx = presetSelector_.getSelectedId() - 1;
        if (idx >= 0) loadPreset(idx);
    };
    addAndMakeVisible(presetSelector_);

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

    newButton_.setTooltip("New blank layout");
    saveButton_.setTooltip("Save preset to file");
    loadButton_.setTooltip("Load preset from file");

    addAndMakeVisible(newButton_);
    addAndMakeVisible(saveButton_);
    addAndMakeVisible(loadButton_);

    // --- Settings tab: pages section ---
    pagesLabel_.setFont(juce::Font(Theme::FontSection, juce::Font::bold));
    pagesLabel_.setColour(juce::Label::textColourId, Theme::Colors::TextDim);
    addAndMakeVisible(pagesLabel_);

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

    // --- Settings tab: OSC output section ---
    oscLabel_.setFont(juce::Font(Theme::FontSection, juce::Font::bold));
    oscLabel_.setColour(juce::Label::textColourId, Theme::Colors::TextDim);
    addAndMakeVisible(oscLabel_);

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

    oscHostEditor_.setFont(juce::Font(Theme::FontBase));
    oscHostEditor_.onReturnKey = [this] {
        auto& osc = processor_.getOscOutput();
        if (oscToggle_.getToggleState())
            osc.enable(oscHostEditor_.getText().toStdString(), (int)oscPortSlider_.getValue());
    };
    addAndMakeVisible(oscHostEditor_);

    oscPortLabel_.setFont(juce::Font(Theme::FontBase));
    oscPortLabel_.setColour(juce::Label::textColourId, Theme::Colors::TextDim);
    addAndMakeVisible(oscPortLabel_);

    oscPortSlider_.setRange(1024, 65535, 1);
    oscPortSlider_.setValue(9000, juce::dontSendNotification);
    oscPortSlider_.setSliderStyle(juce::Slider::LinearBar);
    oscPortSlider_.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 50, 20);
    oscPortSlider_.setColour(juce::Slider::trackColourId, Theme::Colors::Accent);
    oscPortSlider_.setColour(juce::Slider::textBoxTextColourId, Theme::Colors::Text);
    oscPortSlider_.onValueChange = [this] {
        auto& osc = processor_.getOscOutput();
        if (oscToggle_.getToggleState())
            osc.enable(oscHostEditor_.getText().toStdString(), (int)oscPortSlider_.getValue());
    };
    addAndMakeVisible(oscPortSlider_);

    // Init OSC state from processor
    {
        auto& osc = processor_.getOscOutput();
        oscToggle_.setToggleState(osc.isEnabled(), juce::dontSendNotification);
        oscHostEditor_.setText(juce::String(osc.getHost()));
        oscPortSlider_.setValue(osc.getPort(), juce::dontSendNotification);
    }

    // --- Settings tab: hardware section ---
    hardwareLabel_.setFont(juce::Font(Theme::FontSection, juce::Font::bold));
    hardwareLabel_.setColour(juce::Label::textColourId, Theme::Colors::TextDim);
    addAndMakeVisible(hardwareLabel_);

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
    effectPanel_.removeListener(this);
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

    // After Select/Paint/Erase
    float x1 = (float)(eraseButton_.getRight() + 5);
    g.drawLine(x1, sepTop, x1, sepBottom, 1.0f);

    // After shape creation tools (Done/Design)
    float x2;
    if (canvas_.isDesigning()) {
        x2 = (float)(designSymVToggle_.getRight() + 5);
    } else if (pixelDoneButton_.isVisible()) {
        x2 = (float)(designButton_.getRight() + 5);
    } else {
        x2 = (float)(designButton_.getRight() + 5);
    }
    g.drawLine(x2, sepTop, x2, sepBottom, 1.0f);

    // After brush size
    float x3 = (float)(brushSizeSelector_.getRight() + 5);
    g.drawLine(x3, sepTop, x3, sepBottom, 1.0f);

    // After undo/redo
    float x4 = (float)(redoButton_.getRight() + 5);
    g.drawLine(x4, sepTop, x4, sepBottom, 1.0f);

    // After del/dupe
    float x5 = (float)(dupeButton_.getRight() + 5);
    g.drawLine(x5, sepTop, x5, sepBottom, 1.0f);
}

void EraeEditor::resized()
{
    auto area = getLocalBounds();

    // ===== Toolbar =====
    auto toolbar = area.removeFromTop(Theme::ToolbarHeight);
    toolbar.reduce(Theme::SpaceMD, 6);

    int btnH = toolbar.getHeight();
    int btnW = btnH + 16;

    // Tool group: Select Paint Erase
    selectButton_.setBounds(toolbar.removeFromLeft(btnW + 4));
    toolbar.removeFromLeft(Theme::SpaceXS);
    paintButton_.setBounds(toolbar.removeFromLeft(btnW));
    toolbar.removeFromLeft(Theme::SpaceXS);
    eraseButton_.setBounds(toolbar.removeFromLeft(btnW));
    toolbar.removeFromLeft(Theme::SpaceLG);

    // Shape creation group: Rect Circle Hex Poly Pixel Done Design
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
    toolbar.removeFromLeft(Theme::SpaceXS);
    designButton_.setBounds(toolbar.removeFromLeft(btnW + 12));
    toolbar.removeFromLeft(Theme::SpaceXS);

    // Design mode Done/Cancel/Symmetry (overlaid when active)
    if (canvas_.isDesigning()) {
        designDoneButton_.setBounds(toolbar.removeFromLeft(btnW));
        toolbar.removeFromLeft(Theme::SpaceXS);
        designCancelButton_.setBounds(toolbar.removeFromLeft(btnW + 8));
        toolbar.removeFromLeft(Theme::SpaceSM);
        designSymHToggle_.setBounds(toolbar.removeFromLeft(56));
        toolbar.removeFromLeft(Theme::SpaceXS);
        designSymVToggle_.setBounds(toolbar.removeFromLeft(56));
        toolbar.removeFromLeft(Theme::SpaceLG);
    } else {
        toolbar.removeFromLeft(Theme::SpaceLG);
    }

    // Brush size
    brushSizeSelector_.setBounds(toolbar.removeFromLeft(56));
    toolbar.removeFromLeft(Theme::SpaceMD);

    // Undo/Redo
    undoButton_.setBounds(toolbar.removeFromLeft(btnW));
    toolbar.removeFromLeft(Theme::SpaceXS);
    redoButton_.setBounds(toolbar.removeFromLeft(btnW));
    toolbar.removeFromLeft(Theme::SpaceMD);

    // Del/Dupe
    deleteButton_.setBounds(toolbar.removeFromLeft(btnW - 8));
    toolbar.removeFromLeft(Theme::SpaceSM);
    dupeButton_.setBounds(toolbar.removeFromLeft(btnW));
    toolbar.removeFromLeft(Theme::SpaceMD);

    // Fit (right-aligned)
    zoomFitButton_.setBounds(toolbar.removeFromRight(btnW - 8));

    // ===== Status bar =====
    auto statusBar = area.removeFromBottom(Theme::StatusBarHeight);
    statusBar.reduce(Theme::SpaceLG, Theme::SpaceSM);
    statusLabel_.setBounds(statusBar);

    // ===== Sidebar =====
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

    auto activeTab = tabBar_.getActiveTab();

    // ===== Shape Tab (scrollable) =====
    if (activeTab == SidebarTabBar::Shape) {
        shapeViewport_.setBounds(tabContent);
        layoutShapeTabContent(tabContent.getWidth() - 8); // minus scrollbar width
    }

    // ===== Library Tab =====
    if (activeTab == SidebarTabBar::Library) {
        auto content = tabContent;

        libLabel_.setBounds(content.removeFromTop(18));
        content.removeFromTop(3);

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

    // ===== Settings Tab =====
    if (activeTab == SidebarTabBar::Settings) {
        auto content = tabContent;
        int rowH = 26;

        // FILE section
        fileLabel_.setBounds(content.removeFromTop(18));
        content.removeFromTop(3);
        presetSelector_.setBounds(content.removeFromTop(rowH));
        content.removeFromTop(3);
        {
            auto row = content.removeFromTop(rowH);
            int bw = (row.getWidth() - 2 * Theme::SpaceXS) / 3;
            newButton_.setBounds(row.removeFromLeft(bw));
            row.removeFromLeft(Theme::SpaceXS);
            saveButton_.setBounds(row.removeFromLeft(bw));
            row.removeFromLeft(Theme::SpaceXS);
            loadButton_.setBounds(row);
        }
        content.removeFromTop(Theme::SpaceLG);

        // PAGES section
        pagesLabel_.setBounds(content.removeFromTop(18));
        content.removeFromTop(3);
        {
            auto row = content.removeFromTop(rowH);
            pagePrevButton_.setBounds(row.removeFromLeft(24));
            pageLabel_.setBounds(row.removeFromLeft(60));
            pageNextButton_.setBounds(row.removeFromLeft(24));
            row.removeFromLeft(Theme::SpaceSM);
            pageAddButton_.setBounds(row.removeFromLeft(24));
            row.removeFromLeft(Theme::SpaceXS);
            pageDelButton_.setBounds(row.removeFromLeft(24));
            row.removeFromLeft(Theme::SpaceXS);
            pageDupButton_.setBounds(row.removeFromLeft(32));
        }
        content.removeFromTop(Theme::SpaceLG);

        // OSC OUTPUT section
        oscLabel_.setBounds(content.removeFromTop(18));
        content.removeFromTop(3);
        oscToggle_.setBounds(content.removeFromTop(22));
        content.removeFromTop(3);
        {
            auto row = content.removeFromTop(22);
            oscHostLabel_.setBounds(row.removeFromLeft(34));
            oscHostEditor_.setBounds(row);
            content.removeFromTop(3);
        }
        {
            auto row = content.removeFromTop(22);
            oscPortLabel_.setBounds(row.removeFromLeft(34));
            oscPortSlider_.setBounds(row);
        }
        content.removeFromTop(Theme::SpaceLG);

        // HARDWARE section
        hardwareLabel_.setBounds(content.removeFromTop(18));
        content.removeFromTop(3);
        connectButton_.setBounds(content.removeFromTop(rowH));
        content.removeFromTop(3);
        {
            auto row = content.removeFromTop(rowH);
            fingerColorsToggle_.setBounds(row.removeFromLeft(row.getWidth() / 2));
            dawFeedbackToggle_.setBounds(row);
        }
    }

    // ===== Effects Tab =====
    if (activeTab == SidebarTabBar::Effects) {
        effectPanel_.setBounds(tabContent);
    }

    // ===== Canvas =====
    area.reduce(2, 2);
    canvas_.setBounds(area);
}

// ============================================================
// Shape tab content layout (inside scrollable viewport)
// ============================================================

void EraeEditor::layoutShapeTabContent(int contentWidth)
{
    int y = 0;
    int rowH = 26;
    int labelW = 74;
    int w = contentWidth;

    auto singleId = selectionManager_.getSingleSelectedId();
    bool hasSingle = !singleId.empty();
    bool multi = selectionManager_.count() > 1;

    // COLOR section
    colorLabel_.setBounds(0, y, w, 18);
    y += 18 + Theme::SpaceMD;

    int pickerH = 160;
    colorPicker_.setBounds(0, y, w, pickerH);
    y += pickerH + Theme::SpaceLG;

    // VISUAL section (only when single shape selected)
    if (hasSingle && visualLabel_.isVisible()) {
        visualLabel_.setBounds(0, y, w, 18);
        y += 18 + 3;
        visualBox_.setBounds(0, y, w, rowH);
        y += rowH + 5;
        if (fillHorizToggle_.isVisible()) {
            fillHorizLabel_.setBounds(0, y, labelW, rowH);
            fillHorizToggle_.setBounds(labelW, y, rowH, rowH);
            y += rowH + 3;
        }
        y += Theme::SpaceSM;
    }

    // MIDI section (embedded MidiPanel)
    if (hasSingle) {
        // Calculate needed height for MidiPanel based on visible controls
        int midiH = 500; // generous default; MidiPanel uses internal visibility
        midiPanel_.setBounds(0, y, w, midiH);
        midiPanel_.resized(); // let it layout internally

        // Measure actual used height by finding lowest visible child
        int maxBottom = 0;
        for (auto* child : midiPanel_.getChildren()) {
            if (child->isVisible())
                maxBottom = std::max(maxBottom, child->getBottom());
        }
        if (maxBottom > 0) {
            midiH = maxBottom + 6;
            midiPanel_.setBounds(0, y, w, midiH);
        }
        y += midiH + Theme::SpaceSM;
    }

    // CV OUTPUT section (per-shape)
    if (hasSingle) {
        cvLabel_.setBounds(0, y, w, 18);
        y += 18 + 3;
        cvEnableLabel_.setBounds(0, y, labelW, rowH);
        cvEnableToggle_.setBounds(labelW, y, rowH, rowH);
        y += rowH + 3;
        if (cvEnableToggle_.getToggleState()) {
            cvChannelLabel_.setBounds(0, y, labelW, rowH);
            cvChannelSlider_.setBounds(labelW, y, w - labelW, rowH);
            y += rowH + 3;
        }
        y += Theme::SpaceSM;
    }

    // ALIGN section (when 2+ selected)
    if (multi && alignLabel_.isVisible()) {
        alignLabel_.setBounds(0, y, w, 18);
        y += 18 + 3;
        {
            int abw = (w - 3 * Theme::SpaceXS) / 4;
            int x = 0;
            alignLeftBtn_.setBounds(x, y, abw, 24); x += abw + Theme::SpaceXS;
            alignRightBtn_.setBounds(x, y, abw, 24); x += abw + Theme::SpaceXS;
            alignTopBtn_.setBounds(x, y, abw, 24); x += abw + Theme::SpaceXS;
            alignBottomBtn_.setBounds(x, y, w - x, 24);
        }
        y += 24 + 3;
        {
            int abw = (w - 3 * Theme::SpaceXS) / 4;
            int x = 0;
            alignCHBtn_.setBounds(x, y, abw, 24); x += abw + Theme::SpaceXS;
            alignCVBtn_.setBounds(x, y, abw, 24); x += abw + Theme::SpaceXS;
            distHBtn_.setBounds(x, y, abw, 24); x += abw + Theme::SpaceXS;
            distVBtn_.setBounds(x, y, w - x, 24);
        }
        y += 24 + Theme::SpaceSM;
    }

    // MORPH section (when exactly 2 selected)
    if (morphLabel_.isVisible()) {
        morphLabel_.setBounds(0, y, w, 18);
        y += 18 + 3;
        morphSlider_.setBounds(0, y, w, 24);
        y += 24 + 3;
        morphButton_.setBounds(0, y, w, 24);
        y += 24;
    }

    y += Theme::SpaceLG; // bottom padding

    shapeContent_.setSize(contentWidth, y);
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

    // Shape tab (viewport)
    shapeViewport_.setVisible(false);
    colorLabel_.setVisible(false);
    colorPicker_.setVisible(false);
    visualLabel_.setVisible(false);
    visualBox_.setVisible(false);
    fillHorizLabel_.setVisible(false);
    fillHorizToggle_.setVisible(false);
    midiPanel_.setVisible(false);
    cvLabel_.setVisible(false);
    cvEnableLabel_.setVisible(false);
    cvEnableToggle_.setVisible(false);
    cvChannelLabel_.setVisible(false);
    cvChannelSlider_.setVisible(false);
    showAlignmentButtons(false);
    morphLabel_.setVisible(false);
    morphSlider_.setVisible(false);
    morphButton_.setVisible(false);

    // Library tab
    libLabel_.setVisible(false);
    libraryList_.setVisible(false);
    libSaveBtn_.setVisible(false);
    libPlaceBtn_.setVisible(false);
    libFlipHBtn_.setVisible(false);
    libFlipVBtn_.setVisible(false);
    libDeleteBtn_.setVisible(false);

    // Effects tab
    effectPanel_.setVisible(false);

    // Settings tab
    fileLabel_.setVisible(false);
    presetSelector_.setVisible(false);
    newButton_.setVisible(false);
    saveButton_.setVisible(false);
    loadButton_.setVisible(false);
    pagesLabel_.setVisible(false);
    pagePrevButton_.setVisible(false);
    pageLabel_.setVisible(false);
    pageNextButton_.setVisible(false);
    pageAddButton_.setVisible(false);
    pageDelButton_.setVisible(false);
    pageDupButton_.setVisible(false);
    oscLabel_.setVisible(false);
    oscToggle_.setVisible(false);
    oscHostLabel_.setVisible(false);
    oscHostEditor_.setVisible(false);
    oscPortLabel_.setVisible(false);
    oscPortSlider_.setVisible(false);
    hardwareLabel_.setVisible(false);
    connectButton_.setVisible(false);
    fingerColorsToggle_.setVisible(false);
    dawFeedbackToggle_.setVisible(false);

    // Show active tab content
    switch (tab) {
        case SidebarTabBar::Shape: {
            shapeViewport_.setVisible(true);
            colorLabel_.setVisible(true);
            colorPicker_.setVisible(true);

            auto singleId = selectionManager_.getSingleSelectedId();
            bool hasSingle = !singleId.empty();
            bool multi = selectionManager_.count() > 1;

            // Visual controls only when single shape selected
            if (hasSingle) {
                visualLabel_.setVisible(true);
                visualBox_.setVisible(true);
                updateVisualControls();
                midiPanel_.setVisible(true);
                cvLabel_.setVisible(true);
                cvEnableLabel_.setVisible(true);
                cvEnableToggle_.setVisible(true);
                bool showCVCh = cvEnableToggle_.getToggleState();
                cvChannelLabel_.setVisible(showCVCh);
                cvChannelSlider_.setVisible(showCVCh);
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

        case SidebarTabBar::Library:
            libLabel_.setVisible(true);
            libraryList_.setVisible(true);
            libSaveBtn_.setVisible(true);
            libPlaceBtn_.setVisible(true);
            libFlipHBtn_.setVisible(true);
            libFlipVBtn_.setVisible(true);
            libDeleteBtn_.setVisible(true);
            break;

        case SidebarTabBar::Settings:
            fileLabel_.setVisible(true);
            presetSelector_.setVisible(true);
            newButton_.setVisible(true);
            saveButton_.setVisible(true);
            loadButton_.setVisible(true);
            pagesLabel_.setVisible(true);
            pagePrevButton_.setVisible(true);
            pageLabel_.setVisible(true);
            pageNextButton_.setVisible(true);
            pageAddButton_.setVisible(true);
            pageDelButton_.setVisible(true);
            pageDupButton_.setVisible(true);
            oscLabel_.setVisible(true);
            oscToggle_.setVisible(true);
            oscHostLabel_.setVisible(true);
            oscHostEditor_.setVisible(true);
            oscPortLabel_.setVisible(true);
            oscPortSlider_.setVisible(true);
            hardwareLabel_.setVisible(true);
            connectButton_.setVisible(true);
            fingerColorsToggle_.setVisible(true);
            dawFeedbackToggle_.setVisible(true);
            break;

        case SidebarTabBar::Effects:
            effectPanel_.setVisible(true);
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
// CV helpers (replaces OutputPanel)
// ============================================================

void EraeEditor::loadCVFromShape(Shape* shape)
{
    cvCurrentShape_ = shape;
    if (!shape) return;

    cvLoading_ = true;

    auto getPBool = [&](const juce::String& key, bool def) -> bool {
        if (auto* obj = shape->behaviorParams.getDynamicObject())
            if (obj->hasProperty(key))
                return (bool)obj->getProperty(key);
        return def;
    };
    auto getP = [&](const juce::String& key, int def) -> int {
        if (auto* obj = shape->behaviorParams.getDynamicObject())
            if (obj->hasProperty(key))
                return (int)obj->getProperty(key);
        return def;
    };

    cvEnableToggle_.setToggleState(getPBool("cv_enabled", false), juce::dontSendNotification);
    cvChannelSlider_.setValue(getP("cv_channel", 0), juce::dontSendNotification);

    cvLoading_ = false;
}

void EraeEditor::clearCV()
{
    cvCurrentShape_ = nullptr;
}

void EraeEditor::writeCVToShape()
{
    if (!cvCurrentShape_) return;

    auto* obj = cvCurrentShape_->behaviorParams.getDynamicObject();
    if (!obj) {
        obj = new juce::DynamicObject();
        cvCurrentShape_->behaviorParams = juce::var(obj);
    }
    obj->setProperty("cv_enabled", cvEnableToggle_.getToggleState());
    obj->setProperty("cv_channel", (int)cvChannelSlider_.getValue());
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

    // Show "Done" button only in DrawPixel/DrawPoly mode (not in design mode)
    if (!canvas_.isDesigning())
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

void EraeEditor::effectChanged(const std::string& shapeId)
{
    auto* s = processor_.getLayout().getShape(shapeId);
    if (s)
        processor_.getLayout().setBehavior(shapeId, s->behavior, s->behaviorParams);
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
            loadCVFromShape(s);

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

            effectPanel_.loadShape(s);
        }
    } else {
        midiPanel_.clearShape();
        effectPanel_.clearShape();
        clearCV();
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

// ============================================================
// Design mode callbacks
// ============================================================

void EraeEditor::designModeChanged(bool active)
{
    showDesignToolbar(active);
    if (active) {
        designSymHToggle_.setToggleState(canvas_.getDesignSymmetryH(), juce::dontSendNotification);
        designSymVToggle_.setToggleState(canvas_.getDesignSymmetryV(), juce::dontSendNotification);
    }
    updateToolButtons();
    updateStatus();
    resized();
}

void EraeEditor::designFinished(std::set<std::pair<int,int>> cells)
{
    if (cells.empty()) return;

    auto it = cells.begin();
    int minX = it->first, minY = it->second;
    for (auto& [cx, cy] : cells) {
        minX = std::min(minX, cx);
        minY = std::min(minY, cy);
    }

    std::vector<std::pair<int,int>> relCells;
    for (auto& [cx, cy] : cells)
        relCells.push_back({cx - minX, cy - minY});

    auto name = "custom_" + std::to_string(++designShapeCounter_);
    auto shape = std::make_unique<PixelShape>(name, (float)minX, (float)minY, std::move(relCells));
    shape->color = canvas_.getPaintColor();
    shape->colorActive = brighten(canvas_.getPaintColor());

    library_.addEntry(name, shape.get());
    library_.save(ShapeLibrary::getDefaultLibraryFile());
    libraryList_.updateContent();
    libraryList_.repaint();

    tabBar_.setActiveTab(SidebarTabBar::Library);
    showTabContent(SidebarTabBar::Library);
    resized();
}

void EraeEditor::showDesignToolbar(bool show)
{
    selectButton_.setVisible(!show);
    designButton_.setVisible(!show);
    pixelDoneButton_.setVisible(!show && (canvas_.getToolMode() == ToolMode::DrawPixel
                                          || canvas_.getToolMode() == ToolMode::DrawPoly));

    designDoneButton_.setVisible(show);
    designCancelButton_.setVisible(show);
    designSymHToggle_.setVisible(show);
    designSymVToggle_.setVisible(show);

    deleteButton_.setVisible(!show);
    dupeButton_.setVisible(!show);
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

    if (canvas_.isDesigning()) {
        modeName = "Design Shape";
    } else {
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
    }

    auto& conn = processor_.getConnection();
    juce::String connStr = conn.isConnected() ? "Connected" : "--";

    auto& ml = processor_.getMultiLayout();
    juce::String pageStr = "Page " + juce::String(ml.currentPageIndex() + 1)
                         + "/" + juce::String(ml.numPages());
    pageLabel_.setText(pageStr, juce::dontSendNotification);

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

    // Pass effect states to canvas for visual overlay
    {
        auto& effectStates = processor_.getEffectEngine().getEffectStates();
        if (!effectStates.empty()) {
            std::map<std::string, EffectParams> effectParams;
            for (auto& [sid, _] : effectStates) {
                auto* s = processor_.getLayout().getShape(sid);
                if (s)
                    effectParams[sid] = TouchEffectEngine::parseParams(*s);
            }
            canvas_.setEffectStates(effectStates, effectParams);
        } else {
            canvas_.setEffectStates({}, {});
        }
    }

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
