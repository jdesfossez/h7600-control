#include "BrowserView.h"

using h7600::Scope;

BrowserView::BrowserView(const h7600::Catalog& c, UserStore& s)
    : catalog(c), store(s), searcher(c)
{
    searcher.reindex(store.data());
    setWantsKeyboardFocus(true);

    sidebar.onSelect = [this](Scope sc) { setScope(sc); };
    sidebarView.setViewedComponent(&sidebar, false);
    sidebarView.setScrollBarsShown(true, false);
    addAndMakeVisible(sidebarView);

    search.setTextToShowWhenEmpty("Search names, descriptions, banks, tags", theme::mute);
    search.setFont(juce::Font(theme::ui(17.0f)));
    search.setIndents(34, 11);
    search.setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    search.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    search.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    search.onTextChange = [this]
    {
        query.text = search.getText().trim().toStdString();
        if (! query.text.empty() && query.sort == h7600::SortOrder::Number)
            sort.setSelectedId(3, juce::dontSendNotification), query.sort = h7600::SortOrder::BestMatch;
        if (query.text.empty() && query.sort == h7600::SortOrder::BestMatch)
            sort.setSelectedId(1, juce::dontSendNotification), query.sort = h7600::SortOrder::Number;
        refresh();
    };
    search.onFocusLost = [this] { repaint(); };
    search.onKey = [this](const juce::KeyPress& k)
    {
        if (k == juce::KeyPress::downKey) { move(1); return true; }
        if (k == juce::KeyPress::upKey) { move(-1); return true; }
        if (k == juce::KeyPress::pageDownKey) { move(10); return true; }
        if (k == juce::KeyPress::pageUpKey) { move(-10); return true; }
        if (k == juce::KeyPress::returnKey) { loadCurrent(); return true; }
        if (k == juce::KeyPress::escapeKey) { search.clear(); search.onTextChange(); return true; }
        return false;
    };
    addAndMakeVisible(search);

    auto toggle = [this](std::set<std::string>& set, const juce::String& key)
    {
        auto k = key.toStdString();
        if (! set.erase(k)) set.insert(k);
        refresh();
    };
    fxRow.onClick = [this, toggle](const juce::String& k) { toggle(query.effects, k); };
    srcRow.onClick = [this, toggle](const juce::String& k) { toggle(query.sources, k); };
    optRow.onClick = [this](const juce::String& k)
    {
        if (k == "hi96") query.only96k = ! query.only96k;
        if (k == "stereo") query.onlyStereoIn = ! query.onlyStereoIn;
        if (k == "tempo") query.onlyTempo = ! query.onlyTempo;
        refresh();
    };
    addAndMakeVisible(fxRow);
    addAndMakeVisible(srcRow);
    addAndMakeVisible(optRow);

    count.setColour(juce::Label::textColourId, theme::mute);
    count.setFont(juce::Font(theme::ui(14.0f)));
    addAndMakeVisible(count);
    sort.addItem("Number", 1);
    sort.addItem("Name", 2);
    sort.addItem("Best match", 3);
    sort.setSelectedId(1, juce::dontSendNotification);
    sort.onChange = [this]
    {
        query.sort = sort.getSelectedId() == 2 ? h7600::SortOrder::Name
                   : sort.getSelectedId() == 3 ? h7600::SortOrder::BestMatch
                                               : h7600::SortOrder::Number;
        refresh(true);
    };
    addAndMakeVisible(sort);

    list.setRowHeight(38);
    list.setColour(juce::ListBox::backgroundColourId, theme::panel);
    list.setColour(juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    list.setWantsKeyboardFocus(true);
    addAndMakeVisible(list);

    lcd.onVariant = [this](int n)
    {
        for (int i = 0; i < (int) result.presets.size(); ++i)
            if (result.presets[(std::size_t) i]->number == n) { list.selectRow(i); return; }
    };
    addAndMakeVisible(lcd);

    loadButton.setColour(juce::TextButton::buttonColourId, theme::red);
    loadButton.onClick = [this] { loadCurrent(); };
    favButton.onClick = [this]
    {
        if (auto* p = current())
        {
            store.data().toggleFavorite(p->number);
            saveUser();
            rebuildSidebar();
            if (query.scope.kind == Scope::Kind::Favorites) refresh(true);
            else { list.repaint(); showDetail(); }
        }
    };
    prevButton.onClick = [this] { move(-1); };
    nextButton.onClick = [this] { move(1); };
    for (auto* b : { &loadButton, &favButton, &prevButton, &nextButton })
    {
        b->setWantsKeyboardFocus(false);
        addAndMakeVisible(*b);
    }

    tagRow.onRemove = [this](const juce::String& tag)
    {
        if (auto* p = current())
        {
            store.data().removeTag(p->number, tag.toStdString());
            saveUser();
            searcher.reindex(store.data());
            showDetail();
        }
    };
    addAndMakeVisible(tagRow);
    tagInput.setTextToShowWhenEmpty("Add your own tag", theme::mute);
    tagInput.setFont(juce::Font(theme::ui(15.0f)));
    auto addTag = [this]
    {
        auto v = tagInput.getText().trim();
        if (auto* p = current(); p && v.isNotEmpty())
        {
            store.data().addTag(p->number, v.toStdString());
            saveUser();
            searcher.reindex(store.data());
            tagInput.clear();
            showDetail();
        }
    };
    tagInput.onReturnKey = addTag;
    tagAdd.onClick = addTag;
    addAndMakeVisible(tagInput);
    addAndMakeVisible(tagAdd);

    status.setColour(juce::Label::textColourId, theme::mute);
    status.setFont(juce::Font(theme::ui(14.0f)));
    addAndMakeVisible(status);

    rebuildSidebar();
    refresh();
}

BrowserView::~BrowserView() { sidebarView.setViewedComponent(nullptr, false); }

const h7600::Preset* BrowserView::current() const
{
    int r = list.getSelectedRow();
    return r >= 0 && r < (int) result.presets.size() ? result.presets[(std::size_t) r] : nullptr;
}

void BrowserView::saveUser() { store.save(); }

void BrowserView::refresh(bool keepSelection)
{
    const auto* prev = keepSelection ? current() : nullptr;
    result = searcher.run(query, store.data());
    list.updateContent();
    int row = 0;
    if (prev)
        for (int i = 0; i < (int) result.presets.size(); ++i)
            if (result.presets[(std::size_t) i] == prev) row = i;
    if (result.presets.empty()) list.deselectAllRows();
    else list.selectRow(row);
    list.repaint();

    juce::String c = juce::String((int) result.presets.size()) + (result.presets.size() == 1 ? " preset" : " presets");
    if (! query.text.empty())
        c += juce::String(juce::CharPointer_UTF8(" matching \xe2\x80\x9c")) + juce::String(query.text) + juce::String(juce::CharPointer_UTF8("\xe2\x80\x9d"));
    count.setText(c, juce::dontSendNotification);
    rebuildChips();
    showDetail();
}

void BrowserView::rebuildChips()
{
    std::vector<ChipRow::Chip> fx, src, opt;
    for (auto& [name, letter] : h7600::effectTypes())
    {
        int n = result.effectCounts.count(name) ? result.effectCounts.at(name) : 0;
        bool on = query.effects.count(name) > 0;
        fx.push_back({ name, name, n, on, n > 0 || on });
    }
    for (auto& name : h7600::sourceTypes())
    {
        int n = result.sourceCounts.count(name) ? result.sourceCounts.at(name) : 0;
        bool on = query.sources.count(name) > 0;
        src.push_back({ name, name, n, on, n > 0 || on });
    }
    opt.push_back({ "hi96", "Runs at 96 kHz", -1, query.only96k, true });
    opt.push_back({ "stereo", "Stereo in", -1, query.onlyStereoIn, true });
    opt.push_back({ "tempo", "Tempo sync", -1, query.onlyTempo, true });
    fxRow.setChips(std::move(fx));
    srcRow.setChips(std::move(src));
    optRow.setChips(std::move(opt));
    resized();
}

void BrowserView::rebuildSidebar()
{
    std::vector<BankSidebar::Row> rows;
    auto& u = store.data();
    rows.push_back({ { Scope::Kind::All, 0 }, "All presets", (int) catalog.presets().size() });
    rows.push_back({ { Scope::Kind::Favorites, 0 }, juce::CharPointer_UTF8("\xe2\x98\x85 Favorites"), (int) u.favorites.size() });
    rows.push_back({ { Scope::Kind::Recent, 0 }, "Recently loaded", (int) u.recent.size(), false, true });
    auto& fams = h7600::families();
    for (int i = 0; i < (int) fams.size(); ++i)
    {
        auto& f = fams[(std::size_t) i];
        int total = 0;
        for (int b : f.banks) total += catalog.bankSize(b);
        rows.push_back({ { Scope::Kind::Family, i }, f.name, total, true });
        bool open = query.scope == Scope { Scope::Kind::Family, i }
                 || (query.scope.kind == Scope::Kind::Bank && std::find(f.banks.begin(), f.banks.end(), query.scope.index) != f.banks.end());
        if (open)
            for (int b : f.banks)
                if (auto* bank = catalog.bank(b))
                    rows.push_back({ { Scope::Kind::Bank, b }, bank->name, catalog.bankSize(b) });
    }
    sidebar.setRows(std::move(rows), query.scope);
    sidebar.setSize(sidebarView.getMaximumVisibleWidth(), sidebar.totalHeight());
}

void BrowserView::setScope(Scope s)
{
    query.scope = s;
    rebuildSidebar();
    refresh();
}

void BrowserView::showDetail()
{
    auto* p = current();
    lcd.show(p, &catalog);
    std::vector<ChipRow::Chip> chips;
    if (p)
    {
        auto& u = store.data();
        std::vector<std::string> user;
        if (auto it = u.tags.find(p->number); it != u.tags.end()) user = it->second;
        for (auto& t : h7600::PresetSearch::tagsOf(*p, u))
            if (std::find(user.begin(), user.end(), t) == user.end())
                chips.push_back({ t, t, -1, false, true });
        for (auto& t : user)
        {
            ChipRow::Chip c { t, t, -1, false, true };
            c.removable = c.accent = true;
            chips.push_back(c);
        }
        favButton.setButtonText(u.favorites.count(p->number) ? juce::CharPointer_UTF8("\xe2\x98\x85 Favorite") : juce::CharPointer_UTF8("Favorite"));
    }
    tagRow.setChips(std::move(chips));
    resized();
}

void BrowserView::move(int d)
{
    if (result.presets.empty()) return;
    int r = juce::jlimit(0, (int) result.presets.size() - 1, list.getSelectedRow() + d);
    list.selectRow(r);
    list.scrollToEnsureRowIsOnscreen(r);
}

void BrowserView::loadCurrent()
{
    if (auto* p = current(); p && onLoad)
        onLoad(p->number);
}

void BrowserView::noteLoaded(int number)
{
    store.data().noteLoaded(number);
    saveUser();
    rebuildSidebar();
    if (query.scope.kind == Scope::Kind::Recent) refresh(true);
}

void BrowserView::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= (int) result.presets.size()) return;
    auto& p = *result.presets[(std::size_t) row];
    if (selected) g.fillAll(theme::sel);
    g.setColour(theme::line);
    g.fillRect(0, h - 1, w, 1);
    auto ink = selected ? theme::selInk : theme::ink;
    auto mute = selected ? theme::selInk.withAlpha(0.7f) : theme::mute;

    auto r = juce::Rectangle<int>(0, 0, w, h).reduced(8, 0);
    g.setFont(theme::ui(15.0f));
    g.setColour(mute);
    g.drawText(juce::String(p.number), r.removeFromLeft(52), juce::Justification::centredRight);
    r.removeFromLeft(10);

    // badges on the right
    auto badges = r.removeFromRight((int) p.effects.size() * 21);
    for (auto& e : p.effects)
    {
        char letter = '?';
        for (auto& [name, l] : h7600::effectTypes()) if (name == e) letter = l;
        auto b = badges.removeFromLeft(21).withSizeKeepingCentre(18, 18).toFloat();
        g.setColour(ink.withAlpha(0.75f));
        g.drawRoundedRectangle(b.reduced(0.5f), 4.0f, 1.0f);
        g.setFont(theme::ui(11.0f, true));
        g.drawText(juce::String::charToString((juce::juce_wchar) letter), b, juce::Justification::centred);
    }
    r.removeFromRight(10);

    juce::String name = p.name;
    if (store.data().favorites.count(p.number))
    {
        g.setColour(theme::red);
        g.setFont(theme::ui(15.0f, true));
        g.drawText(juce::CharPointer_UTF8("\xe2\x98\x85"), r.removeFromLeft(20), juce::Justification::centredLeft);
    }
    juce::Font bold(theme::ui(15.0f, true));
    auto nameW = juce::jmin(r.getWidth(), juce::GlyphArrangement::getStringWidthInt(bold, name) + 2);
    g.setFont(bold);
    g.setColour(ink);
    g.drawText(name, r.removeFromLeft(nameW), juce::Justification::centredLeft, true);
    if (p.bank)
        if (auto* b = catalog.bank(*p.bank))
        {
            g.setFont(theme::ui(14.0f));
            g.setColour(mute);
            g.drawText(b->name, r.withTrimmedLeft(8), juce::Justification::centredLeft, true);
        }
}

void BrowserView::selectedRowsChanged(int) { showDetail(); }
void BrowserView::returnKeyPressed(int) { loadCurrent(); }
void BrowserView::listBoxItemDoubleClicked(int, const juce::MouseEvent&) { loadCurrent(); }

bool BrowserView::keyPressed(const juce::KeyPress& k)
{
    if (k == juce::KeyPress::downKey) { move(1); return true; }
    if (k == juce::KeyPress::upKey) { move(-1); return true; }
    if (k == juce::KeyPress::returnKey) { loadCurrent(); return true; }
    // Typing anywhere goes to the search box.
    auto c = k.getTextCharacter();
    if (c == '/' || (c >= ' ' && c < 0x7f && ! k.getModifiers().isCommandDown()))
    {
        search.grabKeyboardFocus();
        if (c != '/') search.insertTextAtCaret(juce::String::charToString(c));
        else search.selectAll();
        return true;
    }
    return false;
}

void BrowserView::paint(juce::Graphics& g)
{
    theme::card(g, navCard.toFloat());
    theme::card(g, mainCard.toFloat());
    theme::card(g, tagsCard.toFloat());
    // search box (the editor itself is transparent so the icon shows)
    auto sb = search.getBounds().toFloat();
    g.setColour(theme::panel2);
    g.fillRoundedRectangle(sb, 8.0f);
    g.setColour(search.hasKeyboardFocus(true) ? theme::ink : theme::line);
    g.drawRoundedRectangle(sb.reduced(0.5f), 8.0f, 1.0f);
    g.setColour(theme::mute);
    g.setFont(theme::ui(17.0f));
    g.drawText(juce::CharPointer_UTF8("\xe2\x8c\x95"), search.getBounds().withWidth(34), juce::Justification::centred);
    g.setColour(theme::line);
    g.fillRect(list.getX(), list.getY() - 1, list.getWidth(), 1);
    g.setColour(theme::ink);
    g.setFont(theme::ui(15.0f, true));
    g.drawText("Tags", tagsCard.reduced(12).removeFromTop(20), juce::Justification::centredLeft);
}

void BrowserView::resized()
{
    auto r = getLocalBounds();
    const int gap = 14;
    navCard = r.removeFromLeft(240);
    r.removeFromLeft(gap);
    auto aside = r.removeFromRight(380);
    r.removeFromRight(gap);
    mainCard = r;

    sidebarView.setBounds(navCard.reduced(1, 6));
    sidebar.setSize(sidebarView.getMaximumVisibleWidth(), sidebar.totalHeight());

    auto m = mainCard.reduced(14);
    search.setBounds(m.removeFromTop(46));
    m.removeFromTop(12);
    for (auto* row : { &fxRow, &srcRow, &optRow })
    {
        int h = row->heightForWidth(m.getWidth());
        row->setBounds(m.removeFromTop(h));
        m.removeFromTop(6);
    }
    auto meta = m.removeFromTop(30);
    sort.setBounds(meta.removeFromRight(130).reduced(0, 2));
    count.setBounds(meta);
    m.removeFromTop(6);
    list.setBounds(m);

    // LCD + soft keys
    auto soft = aside.removeFromTop(juce::jmax(340, aside.getHeight() - 210));
    auto keys = soft.removeFromBottom(56);
    lcd.setBounds(soft);
    keys = keys.reduced(12, 6);
    const int kw = (keys.getWidth() - 3 * 8) / 4;
    for (auto* b : { &loadButton, &favButton, &prevButton, &nextButton })
    {
        b->setBounds(keys.removeFromLeft(kw));
        keys.removeFromLeft(8);
    }
    aside.removeFromTop(12);
    tagsCard = aside;
    auto t = aside.reduced(12);
    t.removeFromTop(26);
    status.setBounds(t.removeFromBottom(24));
    auto in = t.removeFromBottom(36);
    tagAdd.setBounds(in.removeFromRight(60));
    in.removeFromRight(6);
    tagInput.setBounds(in);
    t.removeFromBottom(8);
    tagRow.setBounds(t.removeFromTop(juce::jmin(t.getHeight(), tagRow.heightForWidth(t.getWidth()))));
    repaint();
}
