#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "ui/BrowserView.h"
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
    juce::String getStatusText() const { return statusText; }

private:
    void refreshDevices();
    void showPage(int page);
    void setStatus(const juce::String&);
    void showSigfile(const h7600::Sigfile&);
    void log(const juce::String&);

    H7600ControlProcessor& owner;
    theme::LookAndFeel lnf;

    juce::ComboBox deviceBox;
    juce::Slider idSlider { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft };
    juce::ComboBox channelBox;
    juce::TextButton browseTab { "Browse" }, paramsTab { "Parameters" };
    juce::String programName, statusText;
    juce::Rectangle<int> ledArea;

    BrowserView browser;
    juce::Component paramsPage;
    juce::TextButton sigfileButton { "Request sigfile" };
    juce::TextEditor knobList;
    h7600::Sigfile current;
    int pendingLoad = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(H7600ControlEditor)
};
