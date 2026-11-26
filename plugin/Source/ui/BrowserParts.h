#pragma once
// Building blocks of the preset browser: banks sidebar, filter chip rows,
// LCD detail panel and the tag chips. Each is a plain painted component.

#include <juce_gui_basics/juce_gui_basics.h>

#include "h7600/PresetCatalog.h"
#include "Theme.h"

// ---- Sidebar ------------------------------------------------------------------

class BankSidebar : public juce::Component
{
public:
    struct Row
    {
        h7600::Scope scope;
        juce::String label;
        int count = 0;
        bool family = false; // bold family header
        bool separatorAfter = false;
    };

    std::function<void(h7600::Scope)> onSelect;

    void setRows(std::vector<Row> r, h7600::Scope selected)
    {
        rows = std::move(r);
        current = selected;
        setSize(getWidth(), totalHeight());
        repaint();
    }

    int totalHeight() const
    {
        int h = 8;
        for (auto& r : rows) h += rowHeight(r) + (r.separatorAfter ? 17 : 0);
        return h + 8;
    }

    void paint(juce::Graphics& g) override
    {
        int y = 8;
        for (auto& r : rows)
        {
            auto box = juce::Rectangle<int>(8, y, getWidth() - 16, rowHeight(r));
            const bool on = r.scope == current;
            if (on)
            {
                g.setColour(theme::sel);
                g.fillRoundedRectangle(box.toFloat(), 6.0f);
            }
            else if (box.contains(hover))
            {
                g.setColour(theme::panel2);
                g.fillRoundedRectangle(box.toFloat(), 6.0f);
            }
            const bool indent = r.scope.kind == h7600::Scope::Kind::Bank;
            auto text = box.withTrimmedLeft(indent ? 22 : 10).withTrimmedRight(10);
            g.setFont(theme::ui(r.family ? 15.0f : 14.5f, r.family));
            g.setColour(on ? theme::selInk : (indent ? theme::mute : theme::ink));
            g.drawFittedText(r.label, text.withTrimmedRight(36), juce::Justification::centredLeft, 1);
            g.setFont(theme::ui(13.5f));
            g.setColour(on ? theme::selInk.withAlpha(0.7f) : theme::mute);
            g.drawText(juce::String(r.count), text, juce::Justification::centredRight);
            y += box.getHeight();
            if (r.separatorAfter)
            {
                g.setColour(theme::line);
                g.fillRect(14, y + 8, getWidth() - 28, 1);
                y += 17;
            }
        }
    }

    void mouseMove(const juce::MouseEvent& e) override { hover = e.getPosition(); repaint(); }
    void mouseExit(const juce::MouseEvent&) override { hover = { -1, -1 }; repaint(); }
    void mouseUp(const juce::MouseEvent& e) override
    {
        int y = 8;
        for (auto& r : rows)
        {
            int h = rowHeight(r);
            if (e.y >= y && e.y < y + h)
            {
                if (onSelect) onSelect(r.scope);
                return;
            }
            y += h + (r.separatorAfter ? 17 : 0);
        }
    }

private:
    static int rowHeight(const Row& r) { return r.scope.kind == h7600::Scope::Kind::Bank ? 30 : 34; }
    std::vector<Row> rows;
    h7600::Scope current;
    juce::Point<int> hover { -1, -1 };
};

// ---- Chip row ------------------------------------------------------------------

class ChipRow : public juce::Component
{
public:
    struct Chip
    {
        juce::String key, label;
        int count = -1; // -1: no count shown
        bool active = false;
        bool enabled = true;
        bool removable = false; // user tag with ×
        bool accent = false;    // red outline (user tags)
    };

    explicit ChipRow(juce::String rowLabel = {}) : label(std::move(rowLabel)) {}

    std::function<void(const juce::String&)> onClick;
    std::function<void(const juce::String&)> onRemove;

    void setChips(std::vector<Chip> c) { chips = std::move(c); layout(); repaint(); }

    int heightForWidth(int w) { layoutFor(w); return contentHeight; }

    void resized() override { layout(); }

    void paint(juce::Graphics& g) override
    {
        if (label.isNotEmpty())
        {
            g.setFont(theme::ui(13.0f));
            g.setColour(theme::mute);
            g.drawText(label, 0, 0, labelWidth, chipHeight, juce::Justification::centredLeft);
        }
        for (std::size_t i = 0; i < chips.size(); ++i)
        {
            auto& c = chips[i];
            auto r = rects[i].toFloat().reduced(0.5f);
            float alpha = c.enabled || c.active ? 1.0f : 0.35f;
            if (c.active)
            {
                g.setColour(theme::sel);
                g.fillRoundedRectangle(r, r.getHeight() / 2);
            }
            else
            {
                g.setColour(theme::panel2.withAlpha(alpha));
                if (label.isEmpty()) g.fillRoundedRectangle(r, r.getHeight() / 2);
                g.setColour((c.accent ? theme::red : (rects[i].contains(hover) && c.enabled ? theme::ink : theme::line)).withAlpha(alpha));
                g.drawRoundedRectangle(r, r.getHeight() / 2, 1.0f);
            }
            auto ink = (c.active ? theme::selInk : theme::ink).withAlpha(alpha);
            auto t = rects[i].reduced(11, 0);
            g.setFont(theme::ui(14.0f));
            g.setColour(ink);
            auto labelW = juce::GlyphArrangement::getStringWidthInt(juce::Font(theme::ui(14.0f)), c.label);
            g.drawText(c.label, t.removeFromLeft(labelW + 1), juce::Justification::centredLeft);
            if (c.count >= 0)
            {
                g.setFont(theme::ui(12.0f));
                g.setColour(c.active ? ink.withAlpha(0.7f) : theme::mute.withAlpha(alpha));
                g.drawText(juce::String(c.count), t.withTrimmedLeft(4), juce::Justification::centredLeft);
            }
            if (c.removable)
            {
                g.setColour(theme::mute);
                g.drawText(juce::CharPointer_UTF8("\xc3\x97"), t, juce::Justification::centredRight);
            }
        }
    }

    void mouseMove(const juce::MouseEvent& e) override { hover = e.getPosition(); repaint(); }
    void mouseExit(const juce::MouseEvent&) override { hover = { -1, -1 }; repaint(); }
    void mouseUp(const juce::MouseEvent& e) override
    {
        for (std::size_t i = 0; i < chips.size(); ++i)
            if (rects[i].contains(e.getPosition()))
            {
                auto& c = chips[i];
                if (c.removable && e.x > rects[i].getRight() - 22) { if (onRemove) onRemove(c.key); }
                else if ((c.enabled || c.active) && onClick) onClick(c.key);
                return;
            }
    }

private:
    void layout() { layoutFor(getWidth()); }
    void layoutFor(int w)
    {
        rects.clear();
        int x = label.isNotEmpty() ? labelWidth : 0, y = 0;
        juce::Font f(theme::ui(14.0f)), fc(theme::ui(12.0f));
        for (auto& c : chips)
        {
            int cw = 22 + juce::GlyphArrangement::getStringWidthInt(f, c.label)
                   + (c.count >= 0 ? 5 + juce::GlyphArrangement::getStringWidthInt(fc, juce::String(c.count)) : 0)
                   + (c.removable ? 16 : 0);
            if (x + cw > w && x > (label.isNotEmpty() ? labelWidth : 0))
            {
                x = label.isNotEmpty() ? labelWidth : 0;
                y += chipHeight + 6;
            }
            rects.emplace_back(x, y, cw, chipHeight);
            x += cw + 6;
        }
        contentHeight = chips.empty() ? 0 : y + chipHeight;
    }

    static constexpr int chipHeight = 30;
    static constexpr int labelWidth = 64;
    juce::String label;
    std::vector<Chip> chips;
    std::vector<juce::Rectangle<int>> rects;
    int contentHeight = 0;
    juce::Point<int> hover { -1, -1 };
};

// ---- LCD detail ------------------------------------------------------------------

class LcdPanel : public juce::Component
{
public:
    std::function<void(int)> onVariant;

    void show(const h7600::Preset* p, const h7600::Catalog* c)
    {
        preset = p;
        catalog = c;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto outer = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff0c0d0e));
        g.fillRoundedRectangle(outer, 10.0f);
        auto r = outer.reduced(12.0f);
        g.setGradientFill(juce::ColourGradient(theme::lcdTop, r.getX(), r.getY(), theme::lcd, r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle(r, 4.0f);
        auto t = r.reduced(14.0f, 12.0f);
        variantRects.clear();

        if (preset == nullptr)
        {
            g.setColour(theme::lcdInk);
            g.setFont(theme::lcdFont(28.0f));
            g.drawText("NO SELECTION", t.removeFromTop(36), juce::Justification::centredLeft);
            g.setFont(theme::lcdFont(16.0f));
            g.drawFittedText("Type to search, use the arrow keys to move, Enter to load.", t.toNearestInt(), juce::Justification::topLeft, 4);
            return;
        }
        auto& p = *preset;
        g.setColour(theme::lcdInk);
        g.setFont(theme::lcdFont(28.0f));
        g.drawFittedText(juce::String(p.name).toUpperCase(), t.removeFromTop(36).toNearestInt(), juce::Justification::centredLeft, 1);

        auto row = [&](const juce::String& k, const juce::String& v)
        {
            auto line = t.removeFromTop(25);
            g.setFont(theme::lcdFont(17.0f));
            g.setColour(theme::lcdDim);
            g.drawText(k, line, juce::Justification::centredLeft);
            g.setColour(theme::lcdInk);
            g.drawFittedText(v, line.withTrimmedLeft(100).toNearestInt(), juce::Justification::centredRight, 1);
            g.setColour(theme::lcdDim);
            for (float x = line.getX(); x < line.getRight(); x += 6) g.fillRect(x, line.getBottom() - 1, 3.0f, 1.0f);
        };
        const h7600::Bank* bank = p.bank && catalog ? catalog->bank(*p.bank) : nullptr;
        row("PROGRAM", juce::String(p.number));
        row("BANK", p.bank ? juce::String(*p.bank) + " " + (bank ? juce::String(bank->name) : juce::String()) : "?");
        row("MAX RATE", p.maxRateKhz ? juce::String(*p.maxRateKhz) + " kHz" : "?");
        row("I/O", p.inputs ? juce::String(*p.inputs) + " in / " + juce::String(p.outputs.value_or(0)) + " out" : "?");
        if (! p.tempo.empty()) row("TEMPO", p.tapTempo() ? "Tap tempo" : "Central timer");

        g.setFont(theme::lcdFont(16.0f));
        if (! p.tweak.empty())
        {
            t.removeFromTop(8);
            auto box = t.removeFromTop(textHeight(p.tweak, t.getWidth() - 12) + 10);
            g.setColour(theme::lcdDim);
            g.drawRect(box, 1.0f);
            g.setColour(theme::lcdInk);
            drawWrapped(g, p.tweak, box.reduced(6, 5));
        }
        t.removeFromTop(10);
        juce::String desc = p.description.empty() ? (p.indexOnly ? "Listed in the manual's index only; no description."
                                                                  : "No description in the manual.")
                                                  : juce::String(p.description);
        auto dh = textHeight(desc, t.getWidth());
        g.setColour(theme::lcdInk);
        drawWrapped(g, desc, t.removeFromTop(dh));

        if (catalog)
        {
            auto vars = catalog->variants(p);
            if (! vars.empty())
            {
                t.removeFromTop(10);
                g.setColour(theme::lcdInk);
                g.drawText("VARIANTS:", t.removeFromTop(20), juce::Justification::centredLeft);
                for (auto* v : vars)
                {
                    if (t.getHeight() < 20) break;
                    auto line = t.removeFromTop(20);
                    juce::String s = juce::String(v->number) + " " + juce::String(v->name);
                    auto w = (float) juce::GlyphArrangement::getStringWidth(juce::Font(theme::lcdFont(16.0f)), s);
                    g.drawText(s, line, juce::Justification::centredLeft);
                    g.fillRect(line.getX(), line.getBottom() - 3, w, 1.0f);
                    variantRects.push_back({ line.withWidth(w).toNearestInt(), v->number });
                }
            }
        }
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        for (auto& [r, n] : variantRects)
            if (r.contains(e.getPosition()) && onVariant) onVariant(n);
    }

private:
    static juce::TextLayout layoutFor(const juce::String& s, float w)
    {
        juce::AttributedString a;
        a.append(s, juce::Font(theme::lcdFont(16.0f)), theme::lcdInk);
        a.setLineSpacing(1.0f);
        juce::TextLayout l;
        l.createLayout(a, w);
        return l;
    }
    static float textHeight(const juce::String& s, float w) { return layoutFor(s, w).getHeight() + 2; }
    static void drawWrapped(juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r) { layoutFor(s, r.getWidth()).draw(g, r); }

    const h7600::Preset* preset = nullptr;
    const h7600::Catalog* catalog = nullptr;
    std::vector<std::pair<juce::Rectangle<int>, int>> variantRects;
};
