#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <BinaryData.h>

h7600::Catalog H7600ControlProcessor::loadCatalog()
{
    auto parsed = h7600::Catalog::fromJson(std::string_view(BinaryData::presets_json, (std::size_t) BinaryData::presets_jsonSize));
    jassert(parsed);
    return parsed ? std::move(*parsed) : h7600::Catalog {};
}

H7600ControlProcessor::H7600ControlProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      catalog(loadCatalog())
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
    state.setProperty("midiChannel", link.getMidiChannel(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, dest);
}

void H7600ControlProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
    {
        auto state = juce::ValueTree::fromXml(*xml);
        link.setDeviceId(state.getProperty("deviceId", 1));
        link.setMidiChannel(state.getProperty("midiChannel", 0));
        auto name = state.getProperty("midiDevice").toString();
        if (name.isNotEmpty())
            juce::MessageManager::callAsync([this, name] { connect(name); });
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new H7600ControlProcessor(); }
