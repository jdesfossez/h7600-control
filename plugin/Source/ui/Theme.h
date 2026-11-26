#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Colours and fonts of the approved prototype (dark H7600 panel look).
namespace theme
{
inline const juce::Colour bg { 0xff151617 };
inline const juce::Colour panel { 0xff1f2022 };
inline const juce::Colour panel2 { 0xff26272a };
inline const juce::Colour ink { 0xffe9e7e2 };
inline const juce::Colour mute { 0xff9a9993 };
inline const juce::Colour line { 0xff34363a };
inline const juce::Colour lcdTop { 0xffc3cdb0 };
inline const juce::Colour lcd { 0xffb8c3a3 };
inline const juce::Colour lcdInk { 0xff1a2414 };
inline const juce::Colour lcdDim { 0xff55624a };
inline const juce::Colour red { 0xffd8392b };
inline const juce::Colour sel = ink;
inline const juce::Colour selInk = bg;
inline const juce::Colour softKey { 0xff2b2d30 };
inline const juce::Colour softKeyLine { 0xff44474c };

inline juce::FontOptions ui(float size = 15.0f, bool bold = false)
{
    return juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain);
}

/// LCD text. TODO: bundle the VT323 pixel font (OFL) once approved; a system
/// monospaced font stands in for now.
inline juce::FontOptions lcdFont(float size)
{
    return juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), size, juce::Font::plain);
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        setColour(juce::ResizableWindow::backgroundColourId, bg);
        setColour(juce::TextEditor::backgroundColourId, panel2);
        setColour(juce::TextEditor::textColourId, ink);
        setColour(juce::TextEditor::outlineColourId, line);
        setColour(juce::TextEditor::focusedOutlineColourId, ink);
        setColour(juce::TextEditor::highlightColourId, red.withAlpha(0.4f));
        setColour(juce::CaretComponent::caretColourId, ink);
        setColour(juce::ComboBox::backgroundColourId, panel2);
        setColour(juce::ComboBox::textColourId, ink);
        setColour(juce::ComboBox::outlineColourId, line);
        setColour(juce::ComboBox::arrowColourId, mute);
        setColour(juce::PopupMenu::backgroundColourId, panel);
        setColour(juce::PopupMenu::textColourId, ink);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, panel2);
        setColour(juce::PopupMenu::highlightedTextColourId, ink);
        setColour(juce::Label::textColourId, ink);
        setColour(juce::ListBox::backgroundColourId, panel);
        setColour(juce::ListBox::outlineColourId, line);
        setColour(juce::TextButton::buttonColourId, softKey);
        setColour(juce::TextButton::buttonOnColourId, sel);
        setColour(juce::TextButton::textColourOffId, ink);
        setColour(juce::TextButton::textColourOnId, selInk);
        setColour(juce::ScrollBar::thumbColourId, line);
        setColour(juce::Slider::textBoxTextColourId, ink);
        setColour(juce::Slider::textBoxBackgroundColourId, panel2);
        setColour(juce::Slider::textBoxOutlineColourId, line);
        setColour(juce::Slider::thumbColourId, ink);
        setColour(juce::Slider::trackColourId, red);
        setColour(juce::Slider::backgroundColourId, line);
        setColour(juce::Slider::rotarySliderFillColourId, red);
        setColour(juce::Slider::rotarySliderOutlineColourId, line);
    }

    juce::Font getTextButtonFont(juce::TextButton&, int) override { return juce::Font(ui(14.0f, true)); }

    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour& base, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced(0.5f);
        auto c = b.getToggleState() ? sel : base;
        if (over) c = c.brighter(0.08f);
        g.setColour(juce::Colours::black);
        if (! down) g.fillRoundedRectangle(r.translated(0, 2), 6.0f);
        g.setColour(c);
        g.fillRoundedRectangle(down ? r.translated(0, 1) : r, 6.0f);
        g.setColour(b.getToggleState() ? sel : softKeyLine);
        g.drawRoundedRectangle(down ? r.translated(0, 1) : r, 6.0f, 1.0f);
    }
};

inline void card(juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour(panel);
    g.fillRoundedRectangle(r, 10.0f);
    g.setColour(line);
    g.drawRoundedRectangle(r.reduced(0.5f), 10.0f, 1.0f);
}
} // namespace theme
