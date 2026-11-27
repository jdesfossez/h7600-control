#include "PatchEditPanel.h"

namespace
{
constexpr int kTop = 52; // header lines
const juce::String kUnplugged = "(unplugged)";
const juce::String kConstant = "(constant)";
}

void PatchEditPanel::show(h7600::PatchEditor* editor, const h7600::ModuleDb* database, int m)
{
    ed = editor;
    db = database;
    module = m;
    rows.clear();
    issueLines.clear();
    header = "Nothing selected";
    subHeader = "Click a module to edit it, or add one.";
    removeAllChildren();
    if (! ed || ! db) return;

    const auto& p = ed->patch();
    for (auto& i : ed->issues())
        if (i.severity == h7600::PatchIssue::Severity::Error && (i.module < 0 || i.module == module || module < 0))
            issueLines.add((i.module >= 0 ? juce::String(p.modules[(std::size_t) i.module].name()) + (i.field.empty() ? "" : "." + juce::String(i.field)) + ": " : juce::String())
                           + juce::String(i.message));

    if (module < 0 || module >= (int) p.modules.size())
    {
        resized();
        return;
    }
    const auto& mod = p.modules[(std::size_t) module];
    header = juce::String(mod.name());
    subHeader = juce::String(mod.type) + "  (sigfile line " + juce::String(module + 1) + ")";
    for (std::size_t fi = 0; fi < mod.fields.size(); ++fi)
    {
        const auto& f = mod.fields[fi];
        if (f.inputKind() == h7600::PortKind::UserObject) continue; // menu wiring: not edited here
        auto row = std::make_unique<Row>();
        const juce::String field = f.name;
        row->label = fi == 0 ? juce::String("name") : field;
        if (auto* rec = h7600::fieldRecord(*db, mod, f))
        {
            auto desc = juce::String(rec->get("DESCRIPTION"));
            auto lo = juce::String(rec->get("MINIMUM")), hi = juce::String(rec->get("MAXIMUM"));
            row->hint = desc != field ? desc : juce::String();
            if (lo.isNotEmpty() && ! lo.startsWith("=") && f.dbType != "ED_SIGNAL")
                row->hint << (row->hint.isEmpty() ? "" : ", ") << lo << ".." << hi;
        }
        if (f.isInput())
        {
            row->control = f.inputKind() == h7600::PortKind::Control;
            row->combo = std::make_unique<juce::ComboBox>();
            auto& cb = *row->combo;
            cb.addItem(row->control ? kConstant : kUnplugged, 1);
            auto sources = ed->sourcesFor((std::size_t) module, f.name);
            for (int s = 0; s < (int) sources.size(); ++s)
                cb.addItem(juce::String(sources[(std::size_t) s]), s + 2);
            const bool connected = p.resolve(f.token).has_value();
            if (connected) cb.setText(juce::String(f.token), juce::dontSendNotification);
            else cb.setSelectedId(1, juce::dontSendNotification);
            cb.onChange = [this, &cb, field]
            {
                if (cb.getSelectedId() == 1)
                    ed->unplug((std::size_t) module, field.toStdString());
                else
                {
                    auto src = cb.getText().unquoted();
                    auto& pp = ed->patch();
                    if (auto r = pp.resolve(src.toStdString()))
                        ed->connect(r->first, r->second->tag, (std::size_t) module, field.toStdString());
                }
                // Rebuilding destroys this combo box: do it after the callback returns.
                juce::Component::SafePointer<PatchEditPanel> safe(this);
                juce::MessageManager::callAsync([safe] { if (safe && safe->onEdited) safe->onEdited(); });
            };
            addAndMakeVisible(cb);
            if (row->control && ! connected)
            {
                row->text = std::make_unique<juce::TextEditor>();
                row->text->setText(juce::String(f.token), false);
            }
        }
        else
        {
            row->text = std::make_unique<juce::TextEditor>();
            row->text->setText(juce::String(f.text()), false);
        }
        if (row->text)
        {
            auto* te = row->text.get();
            te->setFont(theme::ui(13.0f));
            te->onReturnKey = [this, te, field] { commitText(field, *te); };
            te->onFocusLost = [this, te, field] { commitText(field, *te); };
            addAndMakeVisible(*te);
        }
        rows.push_back(std::move(row));
    }
    resized();
}

void PatchEditPanel::commitText(const juce::String& field, juce::TextEditor& te)
{
    if (! ed || module < 0) return;
    const auto& m = ed->patch().modules[(std::size_t) module];
    auto* f = m.field(field.toStdString());
    if (! f) return;
    auto value = te.getText().trim();
    if (value == juce::String(f->isInput() ? f->token : f->text())) return;
    const bool ok = ed->setField((std::size_t) module, field.toStdString(), value.toStdString());
    if (! ok) te.setText(juce::String(f->isInput() ? f->token : f->text()), false);
    // Called from the editor's own callback: rebuild after it returns.
    juce::Component::SafePointer<PatchEditPanel> safe(this);
    juce::MessageManager::callAsync([safe] { if (safe && safe->onEdited) safe->onEdited(); });
}

void PatchEditPanel::paint(juce::Graphics& g)
{
    g.setFont(theme::ui(18.0f, true));
    g.setColour(theme::ink);
    g.drawText(header, 0, 0, getWidth(), 26, juce::Justification::centredLeft);
    g.setFont(theme::ui(12.5f));
    g.setColour(theme::mute);
    g.drawText(subHeader, 0, 26, getWidth(), 18, juce::Justification::centredLeft);
    int y = kTop;
    g.setFont(theme::ui(12.5f));
    for (auto& l : issueLines)
    {
        g.setColour(theme::red);
        g.drawFittedText(l, 0, y, getWidth(), 32, juce::Justification::topLeft, 2);
        y += 34;
    }
    if (! issueLines.isEmpty()) y += 4;
    for (auto& r : rows)
    {
        g.setFont(theme::ui(13.0f, true));
        g.setColour(r->control ? juce::Colour(0xff6f9fd0) : theme::ink);
        g.drawText(r->label, 0, y, getWidth(), 18, juce::Justification::centredLeft);
        g.setFont(theme::ui(11.5f));
        g.setColour(theme::mute);
        g.drawText(r->hint, 0, y, getWidth(), 18, juce::Justification::centredRight);
        y += 20;
        if (r->combo) y += 30;
        if (r->text) y += 30;
        y += 6;
    }
}

void PatchEditPanel::resized()
{
    int y = kTop + 34 * issueLines.size() + (issueLines.isEmpty() ? 0 : 4);
    for (auto& r : rows)
    {
        y += 20;
        if (r->combo) { r->combo->setBounds(0, y, getWidth(), 26); y += 30; }
        if (r->text) { r->text->setBounds(0, y, getWidth(), 26); y += 30; }
        y += 6;
    }
    height = y;
}
