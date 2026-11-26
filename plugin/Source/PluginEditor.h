#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "ui/BrowserView.h"
#include "ui/ParamsView.h"
#include "ui/LcdMirror.h"
#include "ui/Theme.h"

/// Header (brand, loaded program, connection LED, MIDI settings, page tabs)
/// over the preset browser or the parameter page.
class H7600ControlEditor : public juce::AudioProcessorEditor
{
public:
    explicit H7600ControlEditor(H7600ControlProcessor&);
    ~H7600ControlEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    BrowserView& getBrowser() { return browser; }
    void showPageForTools(int p) { showPage(p); }
    ParamsView& getParams() { return params; }
    juce::String getStatusText() const { return statusText; }

private:
    void refreshDevices();
    void showPage(int page);
    void setStatus(const juce::String&);
    void showSigfile(const h7600::Sigfile&);
    void connectionChanged();

    H7600ControlProcessor& owner;
    theme::LookAndFeel lnf;

    juce::ComboBox deviceBox;
    juce::Slider idSlider { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft };
    juce::ComboBox channelBox;
    juce::TextButton browseTab { "Browse" }, paramsTab { "Parameters" }, lcdTab { "LCD" };
    juce::String programName, statusText;
    juce::Rectangle<int> ledArea;

    BrowserView browser;
    ParamsView params;
    LcdMirror lcdMirror;
    int pendingLoad = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(H7600ControlEditor)
};
