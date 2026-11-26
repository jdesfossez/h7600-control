#pragma once
// Owns the MIDI ports to the H7600 (opened directly, bypassing the host's MIDI
// routing, which is unreliable for large SysEx) and turns raw messages into
// core-library events on the message thread.
//
// Robustness: requests time out; parameter changes are coalesced and wait for
// the unit's ack; if the interface disappears it is reopened when it comes
// back; if the unit stops answering (e.g. power cycle) the program is
// re-read once it answers again. Program changes made on the front panel are
// detected from the sequence-out echo and trigger a fresh sigfile read.

#include <juce_audio_devices/juce_audio_devices.h>

#include "h7600/ParamSender.h"
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
    bool isResponding() const { return responding; }
    juce::String getDeviceName() const { return deviceName; }

    void setDeviceId(int id);
    int getDeviceId() const { return deviceId; }

    /// MIDI channel 0..15 used for Program Change.
    void setMidiChannel(int ch) { midiChannel = juce::jlimit(0, 15, ch); }
    int getMidiChannel() const { return midiChannel; }

    /// Loads program `number` at run time: SysEx bank change + Program Change.
    /// Needs SETUP > MIDI > "MIDI map" = none on the unit. The sigfile is then
    /// requested to confirm which program actually loaded.
    void loadProgram(int number);

    /// Read-only: asks for the current program's quick sigfile.
    void requestSigfile();
    /// Run-time parameter change (not saved on the unit). Coalesced.
    void setParameter(const h7600::Knob& knob, double value);

    static juce::StringArray availableDevices();

    std::function<void(const juce::String&)> onStatus;
    std::function<void(const h7600::Sigfile&)> onSigfile;
    std::function<void(const h7600::ParamChange&)> onParamChange;
    std::function<void(const juce::StringArray&)> onKeys;
    std::function<void()> onConnectionChanged;

private:
    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage&) override;
    void timerCallback() override;
    void handleFrame(const h7600::Frame&);
    void send(const h7600::Bytes&);
    void status(const juce::String&);
    void scheduleRefresh(int delayMs);
    void watchdog();
    static std::int64_t nowMs() { return juce::Time::currentTimeMillis(); }

    std::unique_ptr<juce::MidiInput> input;
    std::unique_ptr<juce::MidiOutput> output;
    juce::String deviceName;
    std::uint8_t deviceId = 1;
    int midiChannel = 0;
    h7600::RequestTracker tracker;
    h7600::ParamSender sender;
    bool sigfilePending = false;
    bool responding = false;
    std::int64_t refreshAt = 0;
    std::int64_t nextWatchdog = 0;
    int lastSigfileEntries = 0;
    juce::WeakReference<H7600Link>::Master masterReference;
    friend class juce::WeakReference<H7600Link>;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(H7600Link)
};
