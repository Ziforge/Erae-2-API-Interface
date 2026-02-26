#pragma once

#include "Layout.h"
#include "Preset.h"
#include <vector>
#include <memory>
#include <algorithm>

namespace erae {

class MultiPageLayout {
public:
    static constexpr int MaxPages = 8; // Erae II API supports up to 8 pages

    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void pageChanged(int pageIndex) = 0;
    };

    MultiPageLayout()
    {
        // Start with one page
        pages_.push_back(std::make_unique<Layout>());
    }

    Layout& currentPage() { return *pages_[currentIndex_]; }
    const Layout& currentPage() const { return *pages_[currentIndex_]; }

    int currentPageIndex() const { return currentIndex_; }
    int numPages() const { return (int)pages_.size(); }

    Layout& getPage(int index) { return *pages_[index]; }

    void switchToPage(int index)
    {
        if (index >= 0 && index < (int)pages_.size() && index != currentIndex_) {
            currentIndex_ = index;
            notifyPageChanged();
        }
    }

    bool canAddPage() const { return (int)pages_.size() < MaxPages; }

    void addPage()
    {
        if (!canAddPage()) return;
        pages_.push_back(std::make_unique<Layout>());
        switchToPage((int)pages_.size() - 1);
    }

    void reset()
    {
        pages_.clear();
        currentIndex_ = 0;
        pages_.push_back(std::make_unique<Layout>());
        notifyPageChanged();
    }

    void removePage(int index)
    {
        if (pages_.size() <= 1) return; // Cannot delete last page
        if (index < 0 || index >= (int)pages_.size()) return;

        pages_.erase(pages_.begin() + index);
        if (currentIndex_ >= (int)pages_.size())
            currentIndex_ = (int)pages_.size() - 1;
        notifyPageChanged();
    }

    void duplicatePage(int index)
    {
        if (!canAddPage()) return;
        if (index < 0 || index >= (int)pages_.size()) return;

        auto newPage = std::make_unique<Layout>();
        // Clone all shapes from source page
        std::vector<std::unique_ptr<Shape>> cloned;
        for (auto& s : pages_[index]->shapes())
            cloned.push_back(s->clone());
        newPage->setShapes(std::move(cloned));

        pages_.insert(pages_.begin() + index + 1, std::move(newPage));
        switchToPage(index + 1);
    }

    // JSON serialization: v2 multi-page format
    juce::var toVar() const
    {
        auto root = new juce::DynamicObject();
        root->setProperty("version", 2);

        juce::Array<juce::var> pagesArr;
        for (auto& page : pages_)
            pagesArr.add(page->toVar());
        root->setProperty("pages", pagesArr);
        root->setProperty("current_page", currentIndex_);
        return juce::var(root);
    }

    // Deserialize: detects v1 (single page) and v2 (multi-page)
    void fromVar(const juce::var& data)
    {
        pages_.clear();
        currentIndex_ = 0;

        if (!data.isObject()) {
            pages_.push_back(std::make_unique<Layout>());
            return;
        }

        int version = (int)data.getProperty("version", 0);

        if (version >= 2) {
            // Multi-page format
            if (auto* pagesArr = data.getProperty("pages", {}).getArray()) {
                for (auto& pageData : *pagesArr) {
                    auto page = std::make_unique<Layout>();
                    if (pageData.getProperty("shapes", {}).getArray()) {
                        auto shapes = Preset::fromJSON(juce::JSON::toString(pageData));
                        if (!shapes.empty())
                            page->setShapes(std::move(shapes));
                    }
                    pages_.push_back(std::move(page));
                }
            }
            currentIndex_ = juce::jlimit(0, std::max(0, (int)pages_.size() - 1),
                                           (int)data.getProperty("current_page", 0));
        } else {
            // v1 single-page format: just a shapes array at top level
            auto page = std::make_unique<Layout>();
            auto json = juce::JSON::toString(data);
            auto shapes = Preset::fromJSON(json);
            if (!shapes.empty())
                page->setShapes(std::move(shapes));
            pages_.push_back(std::move(page));
        }

        if (pages_.empty())
            pages_.push_back(std::make_unique<Layout>());

        notifyPageChanged();
    }

    void addListener(Listener* l) { listeners_.push_back(l); }
    void removeListener(Listener* l)
    {
        listeners_.erase(std::remove(listeners_.begin(), listeners_.end(), l), listeners_.end());
    }

private:
    void notifyPageChanged()
    {
        for (auto* l : listeners_)
            l->pageChanged(currentIndex_);
    }

    std::vector<std::unique_ptr<Layout>> pages_;
    int currentIndex_ = 0;
    std::vector<Listener*> listeners_;
};

} // namespace erae
