#pragma once
// Owns the MIDI ports to the H7600 (opened directly, bypassing the host's MIDI
// routing, which is unreliable for large SysEx) and turns raw messages into
// core-library events on the message thread.

#include <juce_audio_devices/juce_audio_devices.h>

#include "h7600/Protocol.h"
#include "h7600/RequestTracker.h"
#include "h7600/Sigfile.h"

class H7600Link : private juce::MidiInputCallback, private juce::Timer
{
public:
    H7600Link();
    ~H7600Link() override;

    /// Opens the input and output whose name matches (e.g. "UM-ONE").
    bool open(const juce::String& deviceName);
    void close();
    bool isOpen() const { return input != nullptr && output != nullptr; }
    juce::String getDeviceName() const { return deviceName; }

    void setDeviceId(int id) { deviceId = static_cast<std::uint8_t>(juce::jlimit(0, 127, id)); }
    int getDeviceId() const { return deviceId; }

    /// Read-only: asks for the current program's quick sigfile.
    void requestSigfile();
    /// Run-time parameter change (not saved on the unit).
    void setParameter(const h7600::Knob& knob, const juce::String& value);

    static juce::StringArray availableDevices();

    std::function<void(const juce::String&)> onStatus;
    std::function<void(const h7600::Sigfile&)> onSigfile;
    std::function<void(const h7600::ParamChange&)> onParamChange;
    std::function<void(const juce::StringArray&)> onKeys;

private:
    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage&) override;
    void timerCallback() override;
    void handleFrame(const h7600::Frame&);
    void send(const h7600::Bytes&);
    void status(const juce::String&);
    static std::int64_t nowMs() { return juce::Time::currentTimeMillis(); }

    std::unique_ptr<juce::MidiInput> input;
    std::unique_ptr<juce::MidiOutput> output;
    juce::String deviceName;
    std::uint8_t deviceId = 1;
    h7600::RequestTracker tracker;
    juce::WeakReference<H7600Link>::Master masterReference;
    friend class juce::WeakReference<H7600Link>;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(H7600Link)
};
