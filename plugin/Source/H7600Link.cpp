#include "H7600Link.h"

H7600Link::H7600Link()
{
    sender.onFailed = [this](const std::string& id, const std::string& v)
    { status("The unit did not acknowledge " + juce::String(id) + " = " + juce::String(v)); };
    startTimer(10);
}

H7600Link::~H7600Link()
{
    stopTimer();
    close();
    masterReference.clear();
}

void H7600Link::setDeviceId(int id)
{
    deviceId = static_cast<std::uint8_t>(juce::jlimit(0, 127, id));
    sender.setDeviceId(deviceId);
}

juce::StringArray H7600Link::availableDevices()
{
    juce::StringArray names;
    for (auto& d : juce::MidiInput::getAvailableDevices())
        for (auto& o : juce::MidiOutput::getAvailableDevices())
            if (d.name == o.name)
                names.addIfNotAlreadyThere(d.name);
    return names;
}

bool H7600Link::open(const juce::String& name)
{
    close();
    deviceName = name; // remembered even if opening fails, so the watchdog can retry
    juce::String inId, outId;
    for (auto& d : juce::MidiInput::getAvailableDevices())
        if (d.name == name) inId = d.identifier;
    for (auto& d : juce::MidiOutput::getAvailableDevices())
        if (d.name == name) outId = d.identifier;
    if (inId.isNotEmpty() && outId.isNotEmpty())
    {
        input = juce::MidiInput::openDevice(inId, this);
        output = juce::MidiOutput::openDevice(outId);
    }
    if (! isOpen())
    {
        input.reset();
        output.reset();
        status("Cannot open MIDI device \"" + name + "\"");
        if (onConnectionChanged) onConnectionChanged();
        return false;
    }
    input->start();
    status("Connected to " + name + ", device id " + juce::String(deviceId));
    if (onConnectionChanged) onConnectionChanged();
    scheduleRefresh(300); // read the loaded program
    return true;
}

void H7600Link::close()
{
    if (input) input->stop();
    input.reset();
    output.reset();
    tracker.clear();
    sender.clear();
    sigfilePending = false;
    screenPending = false;
    responding = false;
}

void H7600Link::send(const h7600::Bytes& b)
{
    if (output)
        output->sendMessageNow(juce::MidiMessage(b.data(), static_cast<int>(b.size())));
}

void H7600Link::status(const juce::String& s)
{
    if (onStatus) onStatus(s);
}

void H7600Link::scheduleRefresh(int delayMs) { refreshAt = nowMs() + delayMs; }

void H7600Link::loadProgram(int number)
{
    if (! isOpen()) { status("Not connected"); return; }
    for (auto& m : h7600::makeProgramSelect(deviceId, midiChannel, number))
        send(m);
    status("Loading " + juce::String(number) + "...");
    scheduleRefresh(1200);
}

void H7600Link::requestSigfile()
{
    if (! isOpen()) { status("Not connected"); return; }
    if (sigfilePending) return;
    sigfilePending = true;
    tracker.expect(h7600::Code::SigfileWantQuick, nowMs(), 8000,
        [this](const h7600::Frame& f)
        {
            sigfilePending = false;
            if (auto sf = h7600::parseSigfile(h7600::payloadText(f)))
            {
                lastSigfileEntries = (int) sf->entries.size();
                if (onSigfile) onSigfile(*sf);
            }
            else
                status("Could not read the program description from the unit");
        },
        [this](h7600::Code)
        {
            sigfilePending = false;
            responding = false;
            status("The H7600 is not answering. Check MIDI cables, SETUP > MIDI (MIDI enabled, system exclusive on) and the device ID.");
            if (onConnectionChanged) onConnectionChanged();
        });
    send(h7600::makeWant(deviceId, h7600::Code::SigfileWantQuick));
}

bool H7600Link::requestScreen()
{
    if (! isOpen() || screenPending || sigfilePending || ! sender.idle())
        return false;
    screenPending = true;
    tracker.expect(h7600::Code::ScreenWant, nowMs(), 5000,
        [this](const h7600::Frame& f)
        {
            screenPending = false;
            if (auto s = h7600::parseScreenDump(f))
                if (onScreen) onScreen(*s);
        },
        [this](h7600::Code) { screenPending = false; });
    send(h7600::makeWant(deviceId, h7600::Code::ScreenWant));
    return true;
}

void H7600Link::setParameter(const h7600::Knob& knob, double value)
{
    if (! isOpen()) return;
    sender.set(knob.paramId(), h7600::valueText(knob, value));
}

void H7600Link::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& m)
{
    // MIDI thread: copy and hop to the message thread.
    h7600::Bytes bytes(m.getRawData(), m.getRawData() + m.getRawDataSize());
    juce::WeakReference<H7600Link> weak(this);
    juce::MessageManager::callAsync([weak, raw = std::move(bytes)]
    {
        if (auto* self = weak.get())
            if (auto f = h7600::parseFrame(raw))
                self->handleFrame(*f);
    });
}

void H7600Link::handleFrame(const h7600::Frame& f)
{
    if (f.deviceId != deviceId && deviceId != h7600::kBroadcastDevice)
        return;
    if (! responding)
    {
        // First reply after connecting or after the unit came back.
        bool wasKnown = lastSigfileEntries > 0;
        responding = true;
        if (onConnectionChanged) onConnectionChanged();
        if (wasKnown && f.code != static_cast<std::uint8_t>(h7600::Code::SigfileDumpRemote))
            scheduleRefresh(500);
    }
    const bool screenEvent = f.code == static_cast<std::uint8_t>(h7600::Code::Keypress)
                          || f.code == static_cast<std::uint8_t>(h7600::Code::ParamSet)
                          || f.code == static_cast<std::uint8_t>(h7600::Code::ParamAck)
                          || f.code == static_cast<std::uint8_t>(h7600::Code::SigfileDumpRemote);
    if (screenEvent && onScreenMayChange)
        onScreenMayChange();
    if (sender.onFrame(f, nowMs()))
        return;
    if (tracker.onFrame(f))
        return;
    if (auto c = h7600::parseParamSet(f))
    {
        // Module ids beyond the program's own (e.g. 4005xxxx) are the unit's
        // program browser: a load from the front panel follows.
        auto idx = c->moduleIndex();
        if (! idx || *idx > lastSigfileEntries)
            scheduleRefresh(1500);
        else if (onParamChange)
            onParamChange(*c);
    }
    else if (auto mask = h7600::keyMask(f))
    {
        juce::StringArray keys;
        for (auto& k : h7600::pressedKeys(*mask)) keys.add(k);
        if (onKeys) onKeys(keys);
        // ENTER / SELECT on the panel may have loaded a program: re-read it.
        if (keys.contains("ENTER") || keys.contains("SELECT"))
            scheduleRefresh(1500);
    }
}

void H7600Link::watchdog()
{
    if (deviceName.isEmpty()) return;
    const bool present = availableDevices().contains(deviceName);
    if (isOpen() && ! present)
    {
        auto name = deviceName;
        close();
        deviceName = name;
        status("MIDI interface \"" + name + "\" disconnected; waiting for it to come back...");
        if (onConnectionChanged) onConnectionChanged();
    }
    else if (! isOpen() && present)
        open(deviceName);
}

void H7600Link::timerCallback()
{
    const auto now = nowMs();
    tracker.poll(now);
    if (isOpen())
        if (auto m = sender.poll(now))
            send(*m);
    if (refreshAt != 0 && now >= refreshAt && sender.idle())
    {
        refreshAt = 0;
        requestSigfile();
    }
    if (now >= nextWatchdog)
    {
        nextWatchdog = now + 2000;
        watchdog();
    }
}
