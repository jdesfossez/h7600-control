#include "ParamsView.h"

ParamControl::ParamControl(const h7600::Knob& k) : model(k)
{
    if (model.kind == h7600::KnobKind::TextKnob)
    {
        for (int i = 0; i < (int) model.options.size(); ++i)
            choice.addItem(model.options[(std::size_t) i], i + 1);
        choice.setSelectedId((int) model.value + 1, juce::dontSendNotification);
        choice.onChange = [this] { changed(choice.getSelectedId() - 1); };
        addAndMakeVisible(choice);
    }
    else
    {
        double lo = std::min(model.min, model.max), hi = std::max(model.min, model.max);
        slider.setRange(lo, hi > lo ? hi : lo + 1, model.step > 0 ? model.step : 0);
        slider.setValue(model.value, juce::dontSendNotification);
        slider.setDoubleClickReturnValue(true, model.value);
        slider.setColour(juce::Slider::trackColourId, theme::red);
        slider.setColour(juce::Slider::backgroundColourId, theme::line);
        slider.setColour(juce::Slider::thumbColourId, theme::ink);
        slider.onValueChange = [this] { changed(slider.getValue()); };
        addAndMakeVisible(slider);
    }
}

void ParamControl::changed(double v)
{
    model.value = h7600::quantize(model, v);
    repaint();
    if (onChange) onChange(model.value);
}

void ParamControl::setFromUnit(double v)
{
    model.value = v;
    if (model.kind == h7600::KnobKind::TextKnob)
        choice.setSelectedId((int) v + 1, juce::dontSendNotification);
    else
        slider.setValue(v, juce::dontSendNotification);
    repaint();
}

void ParamControl::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour(theme::panel2);
    g.fillRoundedRectangle(r, 8.0f);
    g.setColour(theme::line);
    g.drawRoundedRectangle(r.reduced(0.5f), 8.0f, 1.0f);
    auto t = getLocalBounds().reduced(12, 8).removeFromTop(22);
    g.setFont(theme::ui(14.0f, true));
    g.setColour(theme::ink);
    g.drawFittedText(juce::String(model.label().empty() ? model.name : model.label()), t, juce::Justification::centredLeft, 1);
    // Value as the unit formats it (printf format from the sigfile), in LCD style.
    g.setFont(theme::lcdFont(17.0f));
    g.setColour(theme::lcd);
    auto shown = juce::String(model.displayValue());
    if (auto colon = shown.indexOfChar(':'); colon >= 0) shown = shown.substring(colon + 1).trim();
    g.drawFittedText(shown, t, juce::Justification::centredRight, 1);
}

void ParamControl::resized()
{
    auto r = getLocalBounds().reduced(12, 8);
    r.removeFromTop(26);
    if (model.kind == h7600::KnobKind::TextKnob) choice.setBounds(r.withSizeKeepingCentre(r.getWidth(), 28));
    else slider.setBounds(r);
}

ParamsView::ParamsView()
{
    refresh.onClick = [this] { if (onRefresh) onRefresh(); };
    addAndMakeVisible(refresh);
    gridView.setViewedComponent(&grid, false);
    gridView.setScrollBarsShown(true, false);
    addAndMakeVisible(gridView);
}

void ParamsView::setConnected(bool connected, bool responding)
{
    if (! connected) note = "Not connected. Pick the MIDI interface in the header.";
    else if (! responding && sigfile.knobs.empty()) note = "Waiting for the H7600 to answer...";
    else return;
    if (! sigfile.knobs.empty()) return;
    repaint();
}

void ParamsView::showSigfile(const h7600::Sigfile& sf)
{
    const bool sameProgram = sf.programName == sigfile.programName && ! pages.empty();
    const auto keepPage = currentPage;
    sigfile = sf;
    pages = sf.pages();
    note = "Changes are sent live and are not saved on the unit.";
    tabs.clear();
    for (int i = 0; i < (int) pages.size(); ++i)
    {
        auto* b = tabs.add(new juce::TextButton(juce::String(pages[(std::size_t) i].tab)));
        b->setTooltip(juce::String(pages[(std::size_t) i].title));
        b->setWantsKeyboardFocus(false);
        b->onClick = [this, i] { showPage(i); };
        addAndMakeVisible(b);
    }
    showPage(sameProgram && keepPage < (int) pages.size() ? keepPage : 0);
    resized();
    repaint();
}

void ParamsView::showPage(int index)
{
    currentPage = index;
    for (int i = 0; i < tabs.size(); ++i)
        tabs[i]->setToggleState(i == index, juce::dontSendNotification);
    controls.clear();
    if (index >= 0 && index < (int) pages.size())
        for (auto k : pages[(std::size_t) index].knobs)
        {
            auto* c = controls.add(new ParamControl(sigfile.knobs[k]));
            c->onChange = [this, c](double v)
            {
                // keep our copy in sync so a page switch shows the new value
                for (auto& kn : sigfile.knobs)
                    if (kn.moduleIndex == c->knob().moduleIndex) kn.value = v;
                if (onChange) onChange(c->knob(), v);
            };
            grid.addAndMakeVisible(c);
        }
    resized();
}

void ParamsView::applyFromUnit(const h7600::ParamChange& change)
{
    auto idx = change.moduleIndex();
    if (! idx) return;
    const double v = juce::String(change.value).getDoubleValue();
    for (auto& kn : sigfile.knobs)
        if (kn.moduleIndex == *idx) kn.value = v;
    for (auto* c : controls)
        if (c->knob().moduleIndex == *idx) c->setFromUnit(v);
}

void ParamsView::paint(juce::Graphics& g)
{
    theme::card(g, card.toFloat());
    auto r = card.reduced(16);
    auto top = r.removeFromTop(40);
    g.setColour(theme::ink);
    g.setFont(theme::ui(22.0f, true));
    g.drawText(sigfile.programName.empty() ? juce::String("No program read yet") : juce::String(sigfile.programName),
               top.withTrimmedRight(180), juce::Justification::centredLeft);
    g.setColour(theme::mute);
    g.setFont(theme::ui(14.0f));
    g.drawText(note, r.removeFromTop(22), juce::Justification::centredLeft);
    if (! pages.empty() && currentPage < (int) pages.size())
    {
        g.setColour(theme::mute);
        g.setFont(theme::ui(13.0f));
        g.drawText(juce::String(pages[(std::size_t) currentPage].title).toUpperCase(), gridView.getBounds().translated(0, -24).withHeight(20),
                   juce::Justification::centredLeft);
    }
}

void ParamsView::resized()
{
    card = getLocalBounds();
    auto r = card.reduced(16);
    auto top = r.removeFromTop(40);
    refresh.setBounds(top.removeFromRight(160).withSizeKeepingCentre(160, 32));
    r.removeFromTop(30);
    auto tabRow = r.removeFromTop(44);
    const int tw = tabs.isEmpty() ? 0 : juce::jmin(150, (tabRow.getWidth() - 8 * (tabs.size() - 1)) / tabs.size());
    for (auto* t : tabs)
    {
        t->setBounds(tabRow.removeFromLeft(tw).reduced(0, 4));
        tabRow.removeFromLeft(8);
    }
    r.removeFromTop(30);
    gridView.setBounds(r);

    const int cols = juce::jmax(1, r.getWidth() / 330);
    const int gap = 12, h = 78;
    const int w = (r.getWidth() - 14 - gap * (cols - 1)) / cols;
    int i = 0;
    for (auto* c : controls)
    {
        c->setBounds((i % cols) * (w + gap), (i / cols) * (h + gap), w, h);
        ++i;
    }
    grid.setSize(r.getWidth() - 14, ((controls.size() + cols - 1) / cols) * (h + gap));
}

bool ParamsView::setKnobForTools(const std::string& name, double v)
{
    for (int p = 0; p < (int) pages.size(); ++p)
        for (auto k : pages[(std::size_t) p].knobs)
            if (sigfile.knobs[k].name == name)
            {
                showPage(p);
                for (auto* c : controls)
                    if (c->knob().name == name)
                    {
                        if (c->onChange) c->onChange(h7600::quantize(c->knob(), v));
                        c->setFromUnit(h7600::quantize(c->knob(), v));
                        return true;
                    }
            }
    return false;
}
