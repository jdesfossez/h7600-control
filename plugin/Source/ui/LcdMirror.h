#pragma once
// Live mirror of the H7600's 240x64 LCD, from screen dumps (18 -> 17).
// Polls only while visible; each dump costs ~1.3 s of MIDI bandwidth.

#include <juce_gui_basics/juce_gui_basics.h>

#include "h7600/ScreenDump.h"
#include "Theme.h"

class LcdMirror : public juce::Component, private juce::Timer
{
public:
    /// Should return false if the request could not be sent now.
    std::function<bool()> requestScreen;

    LcdMirror() { startTimer(250); }

    void show(const h7600::ScreenDump& s)
    {
        dump = s;
        hasDump = true;
        lastUpdate = juce::Time::getMillisecondCounter();
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        theme::card(g, getLocalBounds().toFloat());
        auto r = getLocalBounds().reduced(24);
        g.setColour(theme::mute);
        g.setFont(theme::ui(14.0f));
        g.drawText("Live view of the H7600's screen. It refreshes about once a second and pauses while parameters are being sent.",
                   r.removeFromTop(24), juce::Justification::centredLeft);
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
        if (! isShowing() || ! requestScreen) return;
        const auto now = juce::Time::getMillisecondCounter();
        if (now - lastAsk > 1000 && requestScreen())
            lastAsk = now;
    }

    h7600::ScreenDump dump;
    bool hasDump = false;
    juce::uint32 lastUpdate = 0, lastAsk = 0;
};
