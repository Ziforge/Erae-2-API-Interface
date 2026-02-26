#pragma once

#include "../Model/Layout.h"
#include "../Rendering/WidgetRenderer.h"
#include "EraeConnection.h"
#include <juce_events/juce_events.h>
#include <map>

namespace erae {

class EraeProcessor;

class EraeRenderer : public Layout::Listener,
                     public juce::Timer {
public:
    EraeRenderer(Layout& layout, EraeConnection& connection);
    ~EraeRenderer() override;

    void setProcessor(EraeProcessor* p) { processor_ = p; }

    void setLayout(Layout& newLayout);
    void requestFullRedraw();

    // Layout::Listener
    void layoutChanged() override;

    // juce::Timer
    void timerCallback() override;

private:
    void render();
    void renderShape(const Shape& shape, const WidgetState& state);

    Layout* layout_;
    EraeConnection& connection_;
    EraeProcessor* processor_ = nullptr;
    bool dirty_ = false;
    bool forceFullFrame_ = true;  // first frame is always full
    std::map<std::string, WidgetState> lastWidgetStates_;

    static constexpr int FBW = 42, FBH = 24;
    uint8_t prevFb_[FBH][FBW][3] {};  // previous frame for diff
};

} // namespace erae
