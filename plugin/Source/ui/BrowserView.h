#pragma once
// Preset browser: banks sidebar | search, filters, results | LCD detail.
// Keyboard: typing focuses search, arrows move, Enter loads (only Enter, the
// Load button or a double-click load; moving only shows info).

#include "../UserStore.h"
#include "BrowserParts.h"

class BrowserView : public juce::Component, private juce::ListBoxModel
{
public:
    BrowserView(const h7600::Catalog&, UserStore&);
    ~BrowserView() override;

    /// Called when the user asks to load a preset.
    std::function<void(int number)> onLoad;

    void setStatus(const juce::String& s) { status.setText(s, juce::dontSendNotification); }
    /// Call after a successful load to update recents.
    void noteLoaded(int number);
    void focusSearch() { search.grabKeyboardFocus(); }
    /// Sets the search text as if typed (also used by the snapshot tool).
    void setSearchText(const juce::String& t) { search.setText(t, true); }
    /// Same as pressing Enter.
    void loadSelected() { loadCurrent(); }

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;

private:
    class SearchBox : public juce::TextEditor
    {
    public:
        std::function<bool(const juce::KeyPress&)> onKey;
        bool keyPressed(const juce::KeyPress& k) override { return (onKey && onKey(k)) || juce::TextEditor::keyPressed(k); }
    };

    // ListBoxModel
    int getNumRows() override { return (int) result.presets.size(); }
    void paintListBoxItem(int row, juce::Graphics&, int w, int h, bool selected) override;
    void selectedRowsChanged(int row) override;
    void returnKeyPressed(int row) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;

    void refresh(bool keepSelection = false);
    void rebuildSidebar();
    void rebuildChips();
    void showDetail();
    void move(int delta);
    void loadCurrent();
    void setScope(h7600::Scope);
    const h7600::Preset* current() const;
    void saveUser();

    const h7600::Catalog& catalog;
    UserStore& store;
    h7600::PresetSearch searcher;
    h7600::Query query;
    h7600::SearchResult result;

    juce::Viewport sidebarView;
    BankSidebar sidebar;
    SearchBox search;
    ChipRow fxRow { "Effect" }, srcRow { "Source" }, optRow { "Only" };
    juce::Label count;
    juce::ComboBox sort;
    juce::ListBox list { "Presets", this };
    LcdPanel lcd;
    juce::TextButton loadButton { juce::CharPointer_UTF8("Load \xe2\x8f\x8e") }, favButton { "Favorite" },
        prevButton { juce::CharPointer_UTF8("\xe2\x97\x80 Prev") }, nextButton { juce::CharPointer_UTF8("Next \xe2\x96\xb6") };
    ChipRow tagRow;
    juce::TextEditor tagInput;
    juce::TextButton tagAdd { "Add" };
    juce::Label status;

    juce::Rectangle<int> navCard, mainCard, tagsCard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BrowserView)
};
