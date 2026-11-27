#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "ui/BrowserView.h"
#include "ui/ParamsView.h"
#include "ui/XrayView.h"
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

    enum class Page { Browse, Parameters, Xray, Lcd };
    void showPage(Page);

    // For the snapshot tool.
    BrowserView& getBrowser() { return browser; }
    ParamsView& getParams() { return params; }
    XrayView& getXray() { return xray; }
    juce::String getStatusText() const { return statusText; }

private:
    void refreshDevices();
    void setStatus(const juce::String&);
    void showSigfile(const h7600::Sigfile&);
    void connectionChanged();
    void databaseMenu();
    void databaseChanged();
    std::unique_ptr<juce::FileChooser> chooser;

    H7600ControlProcessor& owner;
    theme::LookAndFeel lnf;

    juce::ComboBox deviceBox;
    juce::Slider idSlider { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft };
    juce::ComboBox channelBox;
    juce::TextButton browseTab { "Browse" }, paramsTab { "Parameters" }, xrayTab { "X-ray" }, lcdTab { "LCD" };
    juce::String programName, statusText;
    juce::Rectangle<int> ledArea;

    BrowserView browser;
    ParamsView params;
    XrayView xray;
    LcdMirror lcdMirror;
    int pendingLoad = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(H7600ControlEditor)
};
