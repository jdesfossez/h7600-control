#pragma once
// Mirror of the H7600's 240x64 LCD, from screen dumps (18 -> 17).
//
// Every dump makes the unit flash "sending screen bitmap", and the unit cannot
// announce screen changes itself, so there is no polling: one dump is asked
// when the tab opens, then once ~400 ms after the last event that can change
// the screen (front-panel keys/knobs, our acknowledged parameter changes,
// program reads), so a burst such as a knob being turned costs one dump.
// A Refresh button asks for one on demand.

#include <juce_gui_basics/juce_gui_basics.h>

#include "h7600/ScreenDump.h"
#include "Theme.h"

class LcdMirror : public juce::Component, private juce::Timer
{
public:
    /// Should return false if the request could not be sent now (busy).
    std::function<bool()> requestScreen;

    LcdMirror()
    {
        refresh.setWantsKeyboardFocus(false);
        refresh.onClick = [this] { due = juce::Time::getMillisecondCounter(); };
        addAndMakeVisible(refresh);
        startTimer(50);
    }

    /// Something may have changed on the unit's screen: refresh after it settles.
    void noteActivity()
    {
        if (isShowing())
            due = juce::Time::getMillisecondCounter() + settleMs;
    }

    void show(const h7600::ScreenDump& s)
    {
        dump = s;
        hasDump = true;
        repaint();
    }

    void visibilityChanged() override
    {
        if (isVisible())
            due = juce::Time::getMillisecondCounter(); // one dump when the tab opens
    }

    void resized() override
    {
        refresh.setBounds(getLocalBounds().reduced(24).removeFromTop(32).removeFromRight(120));
    }

    void paint(juce::Graphics& g) override
    {
        theme::card(g, getLocalBounds().toFloat());
        auto r = getLocalBounds().reduced(24);
        g.setColour(theme::mute);
        g.setFont(theme::ui(14.0f));
        g.drawText("Mirror of the H7600's screen. It updates after you use the unit or change a parameter (each update briefly shows \"sending screen bitmap\" on the unit).",
                   r.removeFromTop(32).withTrimmedRight(140), juce::Justification::centredLeft);
        r.removeFromTop(12);
        if (! hasDump)
        {
            g.drawText("Waiting for the first screen dump...", r, juce::Justification::centred);
            return;
        }
        // Fit the 240x64 screen, keeping square pixels.
        const int scale = juce::jmax(1, juce::jmin((r.getWidth() - 40) / dump.width, (r.getHeight() - 40) / dump.height));
        auto screen = juce::Rectangle<int>(dump.width * scale, dump.height * scale).withCentre(r.getCentre());
        auto bezel = screen.expanded(18).toFloat();
        g.setColour(juce::Colour(0xff0c0d0e));
        g.fillRoundedRectangle(bezel, 12.0f);
        g.setGradientFill(juce::ColourGradient(theme::lcdTop, 0, (float) screen.getY(), theme::lcd, 0, (float) screen.getBottom(), false));
        g.fillRect(screen.expanded(6));
        g.setColour(theme::lcdInk);
        const float dot = scale > 2 ? (float) scale - 0.6f : (float) scale;
        for (int y = 0; y < dump.height; ++y)
            for (int x = 0; x < dump.width; ++x)
                if (dump.pixel(x, y))
                    g.fillRect((float) (screen.getX() + x * scale), (float) (screen.getY() + y * scale), dot, dot);
    }

private:
    void timerCallback() override
    {
        if (due == 0 || ! isShowing() || ! requestScreen) return;
        if (juce::Time::getMillisecondCounter() >= due && requestScreen())
            due = 0; // if busy (param queue, other request), try again next tick
    }

    static constexpr juce::uint32 settleMs = 400;
    juce::TextButton refresh { "Refresh" };
    h7600::ScreenDump dump;
    bool hasDump = false;
    juce::uint32 due = 0;
};
