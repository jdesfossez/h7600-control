#pragma once
// Parameter page: controls built from the loaded program's sigfile, grouped
// by its menu pages (soft-key tabs as on the unit). Changes are sent live
// (run time only, never saved on the unit); changes made on the unit's front
// panel (sequence out) move the controls without echoing back.

#include <juce_gui_basics/juce_gui_basics.h>

#include "h7600/Protocol.h"
#include "h7600/Sigfile.h"
#include "Theme.h"

class ParamControl : public juce::Component
{
public:
    explicit ParamControl(const h7600::Knob& k);

    std::function<void(double)> onChange;
    const h7600::Knob& knob() const { return model; }
    /// Value from the unit: updates the control without calling onChange.
    void setFromUnit(double v);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void changed(double v);
    h7600::Knob model;
    juce::Slider slider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::ComboBox choice;
};

class ParamsView : public juce::Component
{
public:
    ParamsView();

    std::function<void(const h7600::Knob&, double)> onChange;
    std::function<void()> onRefresh;

    void showSigfile(const h7600::Sigfile&);
    void applyFromUnit(const h7600::ParamChange&);
    void setConnected(bool connected, bool responding);

    /// As if the user moved the control of knob `name` (tools/tests).
    bool setKnobForTools(const std::string& name, double v);
    const h7600::Sigfile& currentSigfile() const { return sigfile; }

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void showPage(int index);

    h7600::Sigfile sigfile;
    std::vector<h7600::MenuPage> pages;
    int currentPage = 0;
    juce::OwnedArray<juce::TextButton> tabs;
    juce::OwnedArray<ParamControl> controls;
    juce::Component grid;
    juce::Viewport gridView;
    juce::TextButton refresh { "Read from unit" };
    juce::String note = "Connect the H7600 to see the loaded program's parameters.";
    juce::Rectangle<int> card;
};
