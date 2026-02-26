#include "SidebarTabBar.h"

namespace erae {

SidebarTabBar::SidebarTabBar()
{
    const char* names[] = {"Shape", "MIDI", "Output", "Library"};
    for (int i = 0; i < NumTabs; ++i) {
        buttons_[i].setButtonText(names[i]);
        buttons_[i].onClick = [this, i] {
            setActiveTab(static_cast<Tab>(i));
        };
        addAndMakeVisible(buttons_[i]);
    }
    updateButtonColors();
}

void SidebarTabBar::paint(juce::Graphics& g)
{
    g.setColour(Theme::Colors::Separator);
    g.fillRect(0, getHeight() - 1, getWidth(), 1);
}

void SidebarTabBar::resized()
{
    auto area = getLocalBounds();
    int tabW = area.getWidth() / NumTabs;
    for (int i = 0; i < NumTabs; ++i) {
        if (i == NumTabs - 1)
            buttons_[i].setBounds(area);
        else
            buttons_[i].setBounds(area.removeFromLeft(tabW));
    }
}

void SidebarTabBar::setActiveTab(Tab tab)
{
    if (activeTab_ == tab) return;
    activeTab_ = tab;
    updateButtonColors();
    for (auto* l : listeners_)
        l->tabChanged(tab);
}

void SidebarTabBar::updateButtonColors()
{
    for (int i = 0; i < NumTabs; ++i) {
        bool active = (i == static_cast<int>(activeTab_));
        buttons_[i].setColour(juce::TextButton::buttonColourId,
                              active ? Theme::Colors::Accent : Theme::Colors::ButtonBg);
        buttons_[i].setColour(juce::TextButton::textColourOffId,
                              active ? Theme::Colors::TextBright : Theme::Colors::Text);
    }
}

} // namespace erae
