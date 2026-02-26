#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

namespace erae {

class SidebarTabBar : public juce::Component {
public:
    enum Tab { Shape = 0, Library, Settings, NumTabs };

    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void tabChanged(Tab newTab) = 0;
    };

    SidebarTabBar();

    void paint(juce::Graphics& g) override;
    void resized() override;

    Tab getActiveTab() const { return activeTab_; }
    void setActiveTab(Tab tab);

    void addListener(Listener* l) { listeners_.push_back(l); }
    void removeListener(Listener* l) {
        listeners_.erase(std::remove(listeners_.begin(), listeners_.end(), l), listeners_.end());
    }

private:
    Tab activeTab_ = Tab::Shape;
    juce::TextButton buttons_[NumTabs];
    std::vector<Listener*> listeners_;

    void updateButtonColors();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SidebarTabBar)
};

} // namespace erae
