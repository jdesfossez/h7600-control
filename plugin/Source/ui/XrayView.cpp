#include "XrayView.h"

namespace
{
const juce::Colour controlBlue { 0xff2d5b86 };

/// LCD-style soft key: outlined, inverted when on.
class SoftKeyLnf : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool) override
    {
        auto r = b.getLocalBounds().toFloat().reduced(1.0f);
        g.setColour(b.getToggleState() ? theme::lcdInk : (over ? theme::lcdTop : theme::lcd));
        g.fillRect(r);
        g.setColour(theme::lcdInk);
        g.drawRect(r, 1.5f);
    }
    void drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        g.setFont(theme::lcdFont(15.0f));
        g.setColour(b.getToggleState() ? theme::lcd : theme::lcdInk);
        g.drawText(b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
    }
};
SoftKeyLnf& softKeyLnf()
{
    static SoftKeyLnf l;
    return l;
}
} // namespace

// Paints the inspector's text lines above the knob controls.
struct XrayView::Content : juce::Component
{
    explicit Content(XrayView& v) : owner(v) {}
    void paint(juce::Graphics& g) override
    {
        int y = 0;
        for (int i = 0; i < owner.inspectorLines.size(); ++i)
        {
            auto line = owner.inspectorLines[i];
            if (line.startsWith("#"))
            {
                g.setFont(theme::ui(18.0f, true));
                g.setColour(theme::ink);
                g.drawText(line.substring(1), 0, y, getWidth(), 26, juce::Justification::centredLeft);
                y += 28;
            }
            else if (line.startsWith("="))
            {
                y += 6;
                g.setFont(theme::ui(12.0f, true));
                g.setColour(theme::mute);
                g.drawText(line.substring(1).toUpperCase(), 0, y, getWidth(), 18, juce::Justification::centredLeft);
                y += 20;
            }
            else
            {
                const bool ctrl = line.startsWith("~");
                auto text = ctrl ? line.substring(1) : line;
                auto name = text.upToFirstOccurrenceOf("\t", false, false);
                auto value = text.fromFirstOccurrenceOf("\t", false, false);
                g.setFont(theme::ui(13.0f));
                g.setColour(ctrl ? juce::Colour(0xff6f9fd0) : theme::ink);
                g.drawText(name, 0, y, 120, 18, juce::Justification::centredLeft);
                g.setColour(theme::mute);
                g.drawFittedText(value, 124, y, getWidth() - 124, 18, juce::Justification::centredLeft, 1);
                y += 19;
            }
        }
    }
    int textHeight() const
    {
        int y = 0;
        for (auto& l : owner.inspectorLines)
            y += l.startsWith("#") ? 28 : l.startsWith("=") ? 26 : 19;
        return y;
    }
    XrayView& owner;
};

XrayView::XrayView()
{
    for (auto* k : { &audKey, &bothKey, &ctrlKey })
    {
        k->setLookAndFeel(&softKeyLnf());
        k->setWantsKeyboardFocus(false);
        addAndMakeVisible(*k);
    }
    audKey.onClick = [this] { setMode(Mode::Audio); };
    bothKey.onClick = [this] { setMode(Mode::AudioAndControl); };
    ctrlKey.onClick = [this] { setMode(Mode::Control); };
    contentPainter = std::make_unique<Content>(*this);
    inspectorView.setViewedComponent(contentPainter.get(), false);
    inspectorView.setScrollBarsShown(true, false);
    addAndMakeVisible(inspectorView);
    setMode(Mode::AudioAndControl);
    problem = "Read a program from the unit to see its structure.";

    for (auto* b : { &editBtn, &undoBtn, &redoBtn, &addBtn, &delBtn, &revertBtn, &runBtn })
    {
        b->setWantsKeyboardFocus(false);
        addChildComponent(*b);
    }
    editBtn.setVisible(true);
    editBtn.setTooltip("Edit a copy of this program on the Mac. The unit only changes when you press Run on unit.");
    editBtn.onClick = [this] { if (editor) stopEditing(); else startEditing(); };
    undoBtn.onClick = [this] { if (editor && editor->undo()) editsChanged(); };
    redoBtn.onClick = [this] { if (editor && editor->redo()) editsChanged(); };
    revertBtn.onClick = [this] { if (editor) { editor->revert(); editsChanged(); } };
    addBtn.onClick = [this] { addModuleMenu(); };
    delBtn.onClick = [this]
    {
        if (editor && selected > 0 && editor->remove((std::size_t) selected)) { selected = -1; editsChanged(); }
    };
    runBtn.setColour(juce::TextButton::buttonColourId, theme::red.darker(0.3f));
    runBtn.onClick = [this] { confirmRun(); };
    editPanel.onEdited = [this] { editsChanged(); };
    editView.setViewedComponent(&editPanel, false);
    editView.setScrollBarsShown(true, false);
    addChildComponent(editView);
}

void XrayView::startEditing()
{
    if (! patch || ! db) return;
    editor = std::make_unique<h7600::PatchEditor>(*db, *patch);
    editNote = "Editing a copy on this Mac. The unit is unchanged until Run on unit.";
    editsChanged();
    resized();
}

void XrayView::stopEditing()
{
    if (editor && editor->modified())
    {
        juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon, "Stop editing",
            "Discard the edits made to this copy? The unit is not affected either way.", "Discard", "Keep editing", nullptr,
            juce::ModalCallbackFunction::create([this](int ok)
            {
                if (! ok) return;
                editor.reset();
                editNote.clear();
                showSigfile(sigfile);
                resized();
            }));
        return;
    }
    editor.reset();
    editNote.clear();
    showSigfile(sigfile);
    resized();
}

void XrayView::editsChanged()
{
    if (! editor) return;
    patch = editor->patch();
    rebuild();
    updateEditButtons();
}

void XrayView::updateEditButtons()
{
    const bool on = editor != nullptr;
    editBtn.setButtonText(on ? "Stop editing" : "Edit a copy");
    editBtn.setEnabled(on || (patch && db));
    for (auto* b : { &undoBtn, &redoBtn, &addBtn, &delBtn, &revertBtn, &runBtn })
        b->setVisible(on);
    inspectorView.setVisible(! on);
    editView.setVisible(on);
    if (! on) return;
    undoBtn.setEnabled(editor->canUndo());
    redoBtn.setEnabled(editor->canRedo());
    revertBtn.setEnabled(editor->modified());
    delBtn.setEnabled(selected > 0 && selected < (int) editor->patch().modules.size() - 1);
    runBtn.setEnabled(! h7600::hasErrors(editor->issues()));
}

void XrayView::addModuleMenu()
{
    if (! editor || ! db) return;
    juce::PopupMenu menu;
    std::vector<std::string> types;
    for (auto& cls : db->classes())
    {
        if (cls == "SYSTEM") continue; // HEAD and TAIL variants
        juce::PopupMenu sub;
        for (auto& m : db->modules())
            if (m.className == cls)
            {
                types.push_back(m.name);
                sub.addItem((int) types.size(), juce::String(m.name));
            }
        menu.addSubMenu(juce::String(cls).toLowerCase(), sub);
    }
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&addBtn), [this, types](int r)
    {
        if (r <= 0 || ! editor) return;
        if (auto idx = editor->add(types[(std::size_t) r - 1]))
        {
            selected = (int) *idx;
            editsChanged();
        }
    });
}

void XrayView::confirmRun()
{
    if (! editor) return;
    auto issues = editor->issues();
    if (h7600::hasErrors(issues)) return;
    juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::WarningIcon, "Run on the unit",
        "The H7600 will replace the program it is running with this edited copy.\n\n"
        "Nothing is saved on the unit: loading any program brings the stored ones back. "
        "A copy of what is sent is kept on this Mac.", "Run on unit", "Cancel", nullptr,
        juce::ModalCallbackFunction::create([this](int ok)
        {
            if (! ok || ! editor || ! onRun) return;
            editNote = "Sending to the unit...";
            runBtn.setEnabled(false);
            repaint();
            onRun(editor->text(), editor->patch().programName());
        }));
}

void XrayView::runFinished(bool ok, const juce::String& message)
{
    editNote = message;
    if (ok && editor) editor->markRun();
    updateEditButtons();
    repaint();
}

void XrayView::setMode(Mode m)
{
    mode = m;
    audKey.setToggleState(m == Mode::Audio, juce::dontSendNotification);
    bothKey.setToggleState(m == Mode::AudioAndControl, juce::dontSendNotification);
    ctrlKey.setToggleState(m == Mode::Control, juce::dontSendNotification);
    layoutGraph();
    repaint();
}

void XrayView::setModuleDb(const h7600::ModuleDb* d)
{
    db = d;
    if (! sigfile.text.empty()) showSigfile(sigfile);
}

void XrayView::showSigfile(const h7600::Sigfile& sf)
{
    if (editor)
    {
        // Keep showing the edited copy; remember what the unit runs now.
        sigfile = sf;
        repaint();
        return;
    }
    const bool sameProgram = sf.text.size() > 0 && sf.programName == sigfile.programName;
    const auto keep = selected >= 0 && patch && selected < (int) patch->modules.size() ? patch->modules[(std::size_t) selected].name() : std::string();
    sigfile = sf;
    patch.reset();
    problem.clear();
    if (! db)
        problem = "X-ray needs the module database: Parameters > Module database.";
    else
    {
        std::string err;
        patch = h7600::parsePatch(sf.text, *db, &err);
        if (! patch) problem = "Could not read this program with the module database: " + juce::String(err);
    }
    selected = -1;
    if (patch && sameProgram && ! keep.empty())
        if (auto i = patch->find(keep)) selected = (int) *i;
    rebuild();
}

void XrayView::rebuild()
{
    graph = patch ? h7600::buildGraph(*patch) : h7600::PatchGraph {};
    if (patch && selected >= (int) patch->modules.size()) selected = -1;
    layoutGraph();
    buildInspector();
    updateEditButtons();
    repaint();
}

void XrayView::applyFromUnit(const h7600::ParamChange& c)
{
    auto idx = c.moduleIndex();
    if (! idx) return;
    const double v = juce::String(c.value).getDoubleValue();
    for (auto& k : sigfile.knobs)
        if (k.moduleIndex == *idx) k.value = v;
    for (auto* pc : knobControls)
        if (pc->knob().moduleIndex == *idx) pc->setFromUnit(v);
}

bool XrayView::visible(const h7600::GraphNode& n) const
{
    if (mode == Mode::Audio) return n.audio;
    if (mode == Mode::Control) return ! n.audio && n.module >= 0;
    return true;
}

juce::String XrayView::nodeName(const h7600::GraphNode& n) const
{
    if (n.module == h7600::GraphNode::kIn) return "IN";
    if (n.module == h7600::GraphNode::kOut) return "OUT";
    return juce::String(patch->modules[(std::size_t) n.module].name());
}

juce::String XrayView::nodeAbbrev(const h7600::GraphNode& n) const
{
    if (n.module < 0) return n.module == h7600::GraphNode::kIn ? "in" : "out";
    return juce::String(h7600::moduleAbbrev(patch->modules[(std::size_t) n.module].type));
}

void XrayView::resized()
{
    auto r = getLocalBounds();
    inspector = r.removeFromRight(juce::jmax(300, juce::jmin(340, r.getWidth() / 4)));
    r.removeFromRight(14);
    keysRow = r.removeFromBottom(40);
    r.removeFromBottom(10);
    lcd = r;
    graphArea = lcd.reduced(24, 16).withTrimmedTop(30);
    auto keys = keysRow.withSizeKeepingCentre(juce::jmin(keysRow.getWidth(), 3 * 130 + 2 * 16), 34);
    for (auto* k : { &audKey, &bothKey, &ctrlKey })
    {
        k->setBounds(keys.removeFromLeft(130));
        keys.removeFromLeft(16);
    }
    auto ir = inspector.reduced(16).withTrimmedBottom(150);
    auto bar = ir.removeFromTop(32);
    if (editor)
    {
        editBtn.setBounds(bar.removeFromLeft(bar.getWidth() / 2 - 3));
        bar.removeFromLeft(6);
        revertBtn.setBounds(bar);
        ir.removeFromTop(6);
        auto row = ir.removeFromTop(30);
        const int w = (row.getWidth() - 3 * 6) / 4;
        for (auto* b : { &undoBtn, &redoBtn, &addBtn, &delBtn })
        {
            b->setBounds(row.removeFromLeft(w));
            row.removeFromLeft(6);
        }
        ir.removeFromTop(6);
        runBtn.setBounds(ir.removeFromTop(32));
        ir.removeFromTop(8);
    }
    else
    {
        editBtn.setBounds(bar.removeFromLeft(140));
        ir.removeFromTop(8);
    }
    inspectorView.setBounds(ir);
    editView.setBounds(ir);
    layoutGraph();
    layoutInspector();
}

void XrayView::layoutGraph()
{
    placed.clear();
    if (! patch || graphArea.isEmpty()) return;
    const float nodeW = 38.0f, nodeH = 20.0f;
    const int cols = juce::jmax(2, graph.columns);
    auto area = graphArea.toFloat();
    auto colX = [&](int c) { return area.getX() + nodeW / 2 + (area.getWidth() - nodeW) * (float) c / (float) (cols - 1); };

    // Audio nodes: rows spread evenly in their column.
    std::map<int, int> perColumn;
    int maxAudioRows = 1;
    for (auto& n : graph.nodes)
        if (n.audio) maxAudioRows = juce::jmax(maxAudioRows, ++perColumn[n.column]);

    // The audio graph takes the top 42 % (more when a column is crowded: 36 px per row).
    juce::Rectangle<float> audioArea = area, controlArea = area;
    if (mode == Mode::AudioAndControl)
    {
        audioSplit = juce::jlimit(area.getHeight() * 0.42f, area.getHeight() * 0.7f, 14.0f + 36.0f * (float) (maxAudioRows + 1));
        audioArea = area.withHeight(audioSplit);
        controlArea = area.withTrimmedTop(audioSplit + 16.0f);
    }
    for (int i = 0; i < (int) graph.nodes.size(); ++i)
    {
        auto& n = graph.nodes[(std::size_t) i];
        if (! n.audio || ! visible(n) || mode == Mode::Control) continue;
        const int count = n.module < 0 ? 1 : perColumn[n.column] - (n.column == 0 ? 1 : 0) - (n.column == cols - 1 ? 1 : 0);
        const int row = n.row;
        const float y = audioArea.getY() + 14.0f + (audioArea.getHeight() - 14.0f) * (float) (row + 1) / (float) (juce::jmax(1, count) + 1);
        placed.push_back({ i, juce::Rectangle<float>(nodeW, nodeH).withCentre({ colX(n.column), y }) });
    }

    // Control nodes: lanes under their column, up to 3 per row, packed left to right.
    if (mode != Mode::Audio)
    {
        std::map<int, std::vector<int>> lanes;
        for (int i = 0; i < (int) graph.nodes.size(); ++i)
            if (! graph.nodes[(std::size_t) i].audio) lanes[graph.nodes[(std::size_t) i].column].push_back(i);
        const float cellW = nodeW + 34.0f;
        int maxRows = 1;
        for (auto& [c, v] : lanes) maxRows = juce::jmax(maxRows, ((int) v.size() + 2) / 3);
        const float cellH = juce::jlimit(30.0f, 46.0f, (controlArea.getHeight() - 14.0f) / (float) maxRows);
        float laneEnd = controlArea.getX() - 1000.0f;
        for (auto& [c, v] : lanes)
        {
            const int perRow = juce::jmin(3, (int) v.size());
            const float laneW = cellW * (float) perRow;
            float x0 = juce::jlimit(controlArea.getX(), juce::jmax(controlArea.getX(), controlArea.getRight() - laneW),
                                    juce::jmax(colX(c) - laneW / 2, laneEnd + 8.0f));
            for (int k = 0; k < (int) v.size(); ++k)
            {
                const float x = x0 + cellW * (float) (k % perRow) + cellW / 2;
                const float y = controlArea.getY() + 14.0f + cellH * (float) (k / perRow) + nodeH / 2;
                placed.push_back({ v[(std::size_t) k], juce::Rectangle<float>(nodeW, nodeH).withCentre({ x, y }) });
            }
            laneEnd = x0 + laneW;
        }
    }
}

void XrayView::paint(juce::Graphics& g)
{
    // LCD panel
    g.setColour(juce::Colours::black);
    g.fillRoundedRectangle(lcd.toFloat().expanded(6), 12.0f);
    juce::ColourGradient grad(theme::lcdTop, 0, (float) lcd.getY(), theme::lcd, 0, (float) lcd.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(lcd.toFloat(), 8.0f);

    auto head = lcd.reduced(24, 10).removeFromTop(26);
    g.setColour(theme::lcdInk);
    g.setFont(theme::lcdFont(18.0f));
    juce::String title = sigfile.programName.empty() ? juce::String("no program") : juce::String(sigfile.programName);
    if (programNumber >= 0) title = juce::String(programNumber) + " " + title;
    g.drawText(title, head, juce::Justification::centredLeft);
    if (patch && selected >= 0)
        g.drawText(juce::String(patch->modules[(std::size_t) selected].name()), head, juce::Justification::centredRight);
    else if (editor)
        g.drawText(editor->modified() ? "EDITED COPY *" : "EDITED COPY", head, juce::Justification::centredRight);
    if (editNote.isNotEmpty())
    {
        g.setColour(theme::lcdInk);
        g.setFont(theme::lcdFont(14.0f));
        g.drawText(editNote, lcd.reduced(24, 8).removeFromBottom(18), juce::Justification::centredLeft);
    }
    g.setColour(theme::lcdDim);
    g.drawHorizontalLine(head.getBottom() + 2, (float) head.getX(), (float) head.getRight());

    if (! patch)
    {
        g.setColour(theme::lcdDim);
        g.setFont(theme::lcdFont(17.0f));
        g.drawFittedText(problem, graphArea.reduced(40), juce::Justification::centred, 3);
    }
    else
    {
        auto boxOf = [&](int node) -> const juce::Rectangle<float>*
        {
            for (auto& p : placed)
                if (p.node == node) return &p.box;
            return nullptr;
        };
        const int selNode = selected >= 0 ? graph.nodeOf(selected) : -1;
        // Edges
        for (auto& e : graph.edges)
        {
            if (mode == Mode::Audio && e.kind != h7600::PortKind::Audio) continue;
            if (mode == Mode::Control && e.kind != h7600::PortKind::Control) continue;
            auto* a = boxOf(e.from);
            auto* b = boxOf(e.to);
            if (! a || ! b) continue;
            const bool hot = selNode >= 0 && (e.from == selNode || e.to == selNode);
            const float alpha = selNode >= 0 && ! hot ? 0.3f : 1.0f;
            juce::Path p;
            if (e.feedback)
            {
                auto s = a->getCentre().withY(a->getY()), t = b->getCentre().withY(b->getY());
                const float lift = 28.0f + std::abs(s.x - t.x) * 0.12f;
                p.startNewSubPath(s);
                p.cubicTo(s.x, s.y - lift, t.x, t.y - lift, t.x, t.y);
            }
            else if (e.kind == h7600::PortKind::Control)
            {
                auto s = a->getCentre().withY(a->getY());
                auto t = b->getCentre().withY(b->getBottom());
                if (! graph.nodes[(std::size_t) e.to].audio && b->getY() > a->getY()) t = b->getCentre().withY(b->getY());
                const float dy = (t.y - s.y) * 0.5f;
                p.startNewSubPath(s);
                p.cubicTo(s.x, s.y + dy, t.x, t.y - dy, t.x, t.y);
            }
            else
            {
                auto s = a->getCentre().withX(a->getRight()), t = b->getCentre().withX(b->getX());
                const float dx = juce::jmax(20.0f, (t.x - s.x) * 0.5f);
                p.startNewSubPath(s);
                p.cubicTo(s.x + dx, s.y, t.x - dx, t.y, t.x, t.y);
            }
            auto colour = hot ? theme::red : (e.kind == h7600::PortKind::Control ? controlBlue : theme::lcdDim);
            g.setColour(colour.withMultipliedAlpha(alpha));
            juce::PathStrokeType stroke(hot ? 2.0f : 1.3f);
            if (e.feedback || e.kind == h7600::PortKind::Control)
            {
                juce::Path dashed;
                const float dots[] = { e.feedback ? 2.0f : 5.0f, 3.0f };
                stroke.createDashedStroke(dashed, p, dots, 2);
                g.fillPath(dashed);
            }
            else
                g.strokePath(p, stroke);
        }
        // Nodes
        for (auto& pl : placed)
        {
            auto& n = graph.nodes[(std::size_t) pl.node];
            const bool sel = n.module >= 0 && n.module == selected;
            g.setColour(theme::lcdDim);
            g.setFont(theme::lcdFont(12.0f));
            g.drawText(nodeName(n).substring(0, 10), pl.box.withY(pl.box.getY() - 14).withHeight(13).expanded(26, 0),
                       juce::Justification::centredBottom);
            if (sel)
            {
                g.setColour(theme::lcdInk);
                g.fillRect(pl.box);
            }
            g.setColour(theme::lcdInk);
            g.drawRect(pl.box, n.audio ? 1.6f : 1.0f);
            g.setColour(sel ? theme::lcd : theme::lcdInk);
            g.setFont(theme::lcdFont(15.0f));
            g.drawText(nodeAbbrev(n), pl.box, juce::Justification::centred);
        }
        if (mode == Mode::AudioAndControl)
        {
            g.setColour(theme::lcdDim.withAlpha(0.5f));
            const float y = (float) graphArea.getY() + audioSplit + 8.0f;
            for (float x = (float) graphArea.getX(); x < (float) graphArea.getRight(); x += 6.0f)
                g.fillRect(x, y, 2.0f, 1.0f);
        }
    }

    // Inspector panel
    theme::card(g, inspector.toFloat());
    auto logBox = inspector.reduced(16).removeFromBottom(136);
    g.setColour(juce::Colour(0xff111213));
    g.fillRoundedRectangle(logBox.toFloat(), 6.0f);
    g.setColour(theme::mute);
    g.setFont(theme::ui(11.0f, true));
    auto lr = logBox.reduced(10, 6);
    g.drawText("SENT (3C, RUN TIME ONLY)", lr.removeFromTop(16), juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
    g.setColour(juce::Colour(0xff8fbf7f));
    for (int i = juce::jmax(0, sentLog.size() - 6); i < sentLog.size(); ++i)
        g.drawText(sentLog[i], lr.removeFromTop(17), juce::Justification::centredLeft);
}

void XrayView::mouseDown(const juce::MouseEvent& e)
{
    if (! patch || ! lcd.contains(e.getPosition())) return;
    for (auto& p : placed)
        if (p.box.expanded(4).contains(e.position))
        {
            auto& n = graph.nodes[(std::size_t) p.node];
            if (n.module >= 0) select(n.module);
            return;
        }
    select(-1);
}

bool XrayView::selectModule(const std::string& name)
{
    if (! patch) return false;
    auto i = patch->find(name);
    if (! i) return false;
    select((int) *i);
    return true;
}

void XrayView::select(int module)
{
    selected = module;
    buildInspector();
    updateEditButtons();
    repaint();
}

void XrayView::log(const juce::String& line)
{
    sentLog.add(line);
    if (sentLog.size() > 50) sentLog.removeRange(0, sentLog.size() - 50);
    repaint(inspector);
}

void XrayView::buildInspector()
{
    if (editor)
    {
        editPanel.setSize(juce::jmax(10, editView.getWidth() - 12), 10);
        editPanel.show(editor.get(), db, selected);
        editPanel.setSize(juce::jmax(10, editView.getWidth() - 12), juce::jmax(10, editPanel.preferredHeight()));
        return;
    }
    knobControls.clear();
    inspectorLines.clear();
    if (! patch || selected < 0 || selected >= (int) patch->modules.size())
    {
        inspectorLines.add("#Nothing selected");
        inspectorLines.add("Click a module\t");
        layoutInspector();
        return;
    }
    auto& m = patch->modules[(std::size_t) selected];
    inspectorLines.add("#" + juce::String(m.name()));
    juce::String kind = juce::String(m.type);
    if (auto* dm = db ? db->find(m.type) : nullptr) kind << "  (" << juce::String(dm->className).toLowerCase() << ")";
    inspectorLines.add("type\t" + kind);
    inspectorLines.add("sigfile line\t" + juce::String(selected + 1));
    inspectorLines.add("=inputs");
    int inputs = 0;
    for (std::size_t i = 1; i < m.fields.size(); ++i)
    {
        auto& f = m.fields[i];
        if (! f.isInput() || f.inputKind() == h7600::PortKind::UserObject) continue;
        juce::String v = juce::String(f.token);
        if (auto* rec = db ? h7600::fieldRecord(*db, m, f) : nullptr; rec && ! patch->resolve(f.token))
            v << "  (" << juce::String(rec->get("MINIMUM")) << ".." << juce::String(rec->get("MAXIMUM")) << ")";
        inspectorLines.add((f.inputKind() == h7600::PortKind::Control ? "~" : "") + juce::String(f.name) + "\t" + v);
        ++inputs;
    }
    if (inputs == 0) inspectorLines.add("none\t");
    juce::StringArray settings;
    for (std::size_t i = 1; i < m.fields.size(); ++i)
        if (! m.fields[i].isInput()) settings.add(juce::String(m.fields[i].name) + "\t" + juce::String(m.fields[i].token));
    if (! settings.isEmpty())
    {
        inspectorLines.add("=settings");
        inspectorLines.addArray(settings);
    }
    auto knobs = h7600::knobsSteering(*patch, (std::size_t) selected);
    if (h7600::isKnobType(m.type)) knobs.insert(knobs.begin(), (std::size_t) selected);
    if (! knobs.empty()) inspectorLines.add("=knobs steering it");
    for (auto k : knobs)
        if (auto* knob = sigfile.findKnobByModule((int) k + 1))
        {
            auto* c = knobControls.add(new ParamControl(*knob, {}));
            c->onChange = [this, c](double v)
            {
                for (auto& kn : sigfile.knobs)
                    if (kn.moduleIndex == c->knob().moduleIndex) kn.value = v;
                log("3C " + juce::String(c->knob().paramId()) + " " + juce::String(h7600::valueText(c->knob(), v)));
                if (onChange) onChange(c->knob(), v);
            };
            contentPainter->addAndMakeVisible(c);
        }
    layoutInspector();
}

void XrayView::layoutInspector()
{
    auto* content = static_cast<Content*>(contentPainter.get());
    const int w = inspectorView.getWidth() - 12;
    int y = content->textHeight() + 6;
    for (auto* c : knobControls)
    {
        c->setBounds(0, y, w, 78);
        y += 86;
    }
    content->setSize(juce::jmax(10, w), y);
    content->repaint();
}
