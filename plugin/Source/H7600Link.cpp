#include "H7600Link.h"

H7600Link::H7600Link() { startTimer(100); }

H7600Link::~H7600Link()
{
    stopTimer();
    close();
    masterReference.clear();
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
        close();
        status("Cannot open MIDI device \"" + name + "\"");
        return false;
    }
    deviceName = name;
    input->start();
    status("Connected to " + name + ", device id " + juce::String(deviceId));
    return true;
}

void H7600Link::close()
{
    if (input) input->stop();
    input.reset();
    output.reset();
    tracker.clear();
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

void H7600Link::requestSigfile()
{
    if (! isOpen()) { status("Not connected"); return; }
    tracker.expect(h7600::Code::SigfileWantQuick, nowMs(), 10000,
        [this](const h7600::Frame& f)
        {
            if (auto sf = h7600::parseSigfile(h7600::payloadText(f)))
            {
                status("Program: " + juce::String(sf->programName) + " (" + juce::String((int) sf->knobs.size()) + " knobs)");
                if (onSigfile) onSigfile(*sf);
            }
            else
                status("Could not parse sigfile");
        },
        [this](h7600::Code) { status("No reply to sigfile request (is SYSTEM EXCLUSIVE on?)"); });
    send(h7600::makeWant(deviceId, h7600::Code::SigfileWantQuick));
    status("Requesting sigfile...");
}

void H7600Link::setParameter(const h7600::Knob& knob, const juce::String& value)
{
    if (! isOpen()) return;
    tracker.expect(h7600::Code::ParamSet, nowMs(), 3000, {}, [this](h7600::Code) { status("Parameter change not acknowledged"); });
    send(h7600::makeParamSet(deviceId, knob.paramId(), value.toStdString()));
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
    if (tracker.onFrame(f))
        return;
    if (auto c = h7600::parseParamSet(f))
    {
        if (onParamChange) onParamChange(*c);
    }
    else if (auto mask = h7600::keyMask(f))
    {
        juce::StringArray keys;
        for (auto& k : h7600::pressedKeys(*mask)) keys.add(k);
        if (onKeys) onKeys(keys);
    }
}

void H7600Link::timerCallback() { tracker.poll(nowMs()); }
