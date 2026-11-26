#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

#include "H7600Link.h"

/// No audio processing: the plugin is a remote control for the hardware.
/// Audio passes through untouched.
class H7600ControlProcessor : public juce::AudioProcessor
{
public:
    H7600ControlProcessor();
    ~H7600ControlProcessor() override = default;

    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    H7600Link& getLink() { return link; }
    /// Opens the stored device, defaulting to the first available one.
    void connect(const juce::String& deviceName);

private:
    H7600Link link;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(H7600ControlProcessor)
};
