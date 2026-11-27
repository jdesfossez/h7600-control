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
#include "h7600/ScreenDump.h"
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
    /// Read-only: asks for the LCD bitmap. Skipped while parameter changes are
    /// queued (a dump takes ~1.3 s of MIDI bandwidth). Returns false if skipped.
    bool requestScreen();

    /// Run-time parameter change (not saved on the unit). Coalesced and rate-capped. A moment
    /// after the last change the program is read back (0B) to confirm, since the unit's ack
    /// does not prove a value was taken.
    void setParameter(const h7600::Knob& knob, double value);

    /// Read-only: asks for the module database (0E -> 0C). About 2 minutes on fw 5.22, during
    /// which the unit answers nothing else, so every other request waits. Progress and the
    /// result come through onStatus / onDatabaseProgress / onDatabase.
    bool fetchModuleDatabase();
    bool isFetchingDatabase() const { return dbFetching; }

    /// Sends an edited sigfile with 0A: the unit compiles it and runs it in place of the current
    /// program (run time only, nothing saved). Then reads the program back (0B) and calls `done`
    /// with whether the unit runs exactly what was sent. The only 0A path in the app; the UI calls
    /// it after an explicit confirmation.
    bool runSigfile(const std::string& text, std::function<void(bool ok, const juce::String& message)> done);

    static juce::StringArray availableDevices();

    std::function<void(const juce::String&)> onStatus;
    std::function<void(const h7600::Sigfile&)> onSigfile;
    std::function<void(const h7600::ParamChange&)> onParamChange;
    std::function<void()> onConnectionChanged;
    std::function<void(const h7600::ScreenDump&)> onScreen;
    std::function<void(const std::string& text)> onDatabase;
    std::function<void(double fraction)> onDatabaseProgress; // 0..1, estimated from bytes received
    /// Something happened that can change the unit's screen: a front-panel
    /// key or knob, one of our parameter changes acknowledged, a program read.
    std::function<void()> onScreenMayChange;

private:
    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage&) override;
    void handlePartialSysexMessage(juce::MidiInput*, const juce::uint8*, int, double) override;
    void deliverSigfile(const h7600::Sigfile&);
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
    bool screenPending = false;
    bool responding = false;
    std::int64_t refreshAt = 0;
    std::int64_t nextWatchdog = 0;
    int lastSigfileEntries = 0;
    // Read-back after parameter changes.
    std::vector<std::pair<std::string, std::string>> sentValues; // latest per param id
    std::int64_t verifyAt = 0;
    int setGeneration = 0, verifyGeneration = -1;
    // Sigfile run (0A) waiting for its read-back.
    struct RunPending
    {
        std::string text;
        std::function<void(bool, const juce::String&)> done;
        std::int64_t verifyAt = 0;
    };
    std::optional<RunPending> run;
    void finishRun(bool ok, const juce::String& msg);
    // Module database download.
    bool dbFetching = false;
    std::int64_t dbBytes = 0, dbLastProgress = 0;
    juce::WeakReference<H7600Link>::Master masterReference;
    friend class juce::WeakReference<H7600Link>;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(H7600Link)
};
