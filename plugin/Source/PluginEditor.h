#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"

/// Placeholder UI: MIDI device + device id, connection status, and a
/// "Request sigfile" button that lists the current program's knobs.
class H7600ControlEditor : public juce::AudioProcessorEditor
{
public:
    explicit H7600ControlEditor(H7600ControlProcessor&);
    ~H7600ControlEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void refreshDevices();
    void showSigfile(const h7600::Sigfile&);
    void log(const juce::String&);

    H7600ControlProcessor& owner;
    juce::Label title, statusLabel, deviceLabel, idLabel;
    juce::ComboBox deviceBox;
    juce::Slider idSlider { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft };
    juce::TextButton rescanButton { "Rescan" }, sigfileButton { "Request sigfile" };
    juce::TextEditor knobList;
    h7600::Sigfile current;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(H7600ControlEditor)
};
