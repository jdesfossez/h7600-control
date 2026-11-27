#pragma once
// Parameter page: controls built from the loaded program's sigfile, grouped
// by its menu pages (soft-key tabs as on the unit). Changes are sent live
// (run time only, never saved on the unit); changes made on the unit's front
// panel (sequence out) move the controls without echoing back.

#include <juce_gui_basics/juce_gui_basics.h>

#include "h7600/ModuleDb.h"
#include "h7600/Patch.h"
#include "h7600/Protocol.h"
#include "h7600/Sigfile.h"
#include "Theme.h"

class ParamControl : public juce::Component
{
public:
    ParamControl(const h7600::Knob& k, juce::String info);

    std::function<void(double)> onChange;
    const h7600::Knob& knob() const { return model; }
    /// Value from the unit: updates the control without calling onChange.
    void setFromUnit(double v);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void changed(double v);
    h7600::Knob model;
    juce::String info; // range, and what the knob drives (from the module database)
    juce::Slider slider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::ComboBox choice;
};

class ParamsView : public juce::Component
{
public:
    ParamsView();

    std::function<void(const h7600::Knob&, double)> onChange;
    std::function<void()> onRefresh;
    /// "Module database" menu: fetch from the unit, import a file.
    std::function<void()> onDatabaseMenu;

    /// The module database, if any (nullptr): adds range and target info to each control.
    void setModuleDb(const h7600::ModuleDb* db);
    void setDatabaseStatus(const juce::String& s) { dbStatus = s; repaint(); }

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
    juce::String infoFor(const h7600::Knob& k) const;
    void rebuildPatch();

    h7600::Sigfile sigfile;
    std::vector<h7600::MenuPage> pages;
    int currentPage = 0;
    juce::OwnedArray<juce::TextButton> tabs;
    juce::OwnedArray<ParamControl> controls;
    juce::Component grid;
    juce::Viewport gridView;
    juce::TextButton refresh { "Read from unit" };
    juce::TextButton dbButton { "Module database" };
    const h7600::ModuleDb* db = nullptr;
    std::optional<h7600::Patch> patch;
    juce::String dbStatus;
    juce::String note = "Connect the H7600 to see the loaded program's parameters.";
    juce::Rectangle<int> card;
};
