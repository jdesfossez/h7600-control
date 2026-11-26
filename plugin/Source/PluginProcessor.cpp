#include "PluginProcessor.h"
#include "PluginEditor.h"

H7600ControlProcessor::H7600ControlProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

bool H7600ControlProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

juce::AudioProcessorEditor* H7600ControlProcessor::createEditor() { return new H7600ControlEditor(*this); }

void H7600ControlProcessor::connect(const juce::String& deviceName)
{
    link.open(deviceName);
}

void H7600ControlProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    juce::ValueTree state("H7600Control");
    state.setProperty("midiDevice", link.getDeviceName(), nullptr);
    state.setProperty("deviceId", link.getDeviceId(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, dest);
}

void H7600ControlProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
    {
        auto state = juce::ValueTree::fromXml(*xml);
        link.setDeviceId(state.getProperty("deviceId", 1));
        auto name = state.getProperty("midiDevice").toString();
        if (name.isNotEmpty())
            juce::MessageManager::callAsync([this, name] { connect(name); });
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new H7600ControlProcessor(); }
