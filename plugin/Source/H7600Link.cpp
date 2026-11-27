#include "H7600Link.h"

namespace
{
bool is(const h7600::Frame& f, h7600::Code c) { return f.code == static_cast<std::uint8_t>(c); }
}

H7600Link::H7600Link()
{
    sender.onFailed = [this](const std::string& id, const std::string& v)
    { status("The unit did not acknowledge " + juce::String(id) + " = " + juce::String(v)); };
    startTimer(4); // the parameter sender may send every 7 ms
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
    run.reset();
    dbFetching = false;
    sentValues.clear();
    verifyAt = 0;
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
    if (dbFetching) { status("Wait until the module database has arrived"); return; }
    sentValues.clear();
    verifyAt = 0;
    for (auto& m : h7600::makeProgramSelect(deviceId, midiChannel, number))
        send(m);
    status("Loading " + juce::String(number) + "...");
    scheduleRefresh(1200);
}

void H7600Link::requestSigfile()
{
    if (! isOpen()) { status("Not connected"); return; }
    if (sigfilePending || dbFetching) return;
    sigfilePending = true;
    verifyGeneration = setGeneration;
    tracker.expect(h7600::Code::SigfileWantQuick, nowMs(), 8000,
        [this](const h7600::Frame& f)
        {
            sigfilePending = false;
            if (auto sf = h7600::parseSigfile(h7600::payloadText(f)))
                deliverSigfile(*sf);
            else
                status("Could not read the program description from the unit");
        },
        [this](h7600::Code)
        {
            sigfilePending = false;
            if (run) finishRun(false, "The unit did not answer after the program was sent");
            responding = false;
            status("The H7600 is not answering. Check MIDI cables, SETUP > MIDI (MIDI enabled, system exclusive on) and the device ID.");
            if (onConnectionChanged) onConnectionChanged();
        });
    send(h7600::makeWant(deviceId, h7600::Code::SigfileWantQuick));
}

void H7600Link::deliverSigfile(const h7600::Sigfile& sf)
{
    lastSigfileEntries = (int) sf.entries.size();
    if (run && run->verifyAt == 0)
    {
        auto norm = [](std::string s)
        {
            std::string o;
            for (char c : s)
                if (c != '\r' && c != '\0') o += c;
            while (! o.empty() && (o.back() == '\n' || o.back() == ' ')) o.pop_back();
            while (! o.empty() && o.front() == '\n') o.erase(o.begin());
            return o;
        };
        const bool same = norm(sf.text) == norm(run->text);
        finishRun(same, same ? juce::String("The unit runs the edited program (not saved on the unit)")
                             : juce::String("The unit runs something else than what was sent (it may have rejected it): \"")
                                   + juce::String(sf.programName) + "\" is loaded");
    }
    if (verifyGeneration != setGeneration)
    {
        // The user moved a control while this was on its way: its values are stale.
        verifyAt = nowMs() + 600;
        return;
    }
    if (! sentValues.empty())
    {
        auto bad = h7600::knobMismatches(sf, sentValues);
        sentValues.clear();
        if (! bad.empty())
        {
            auto& k = *bad.front().knob;
            status("The unit acknowledged but kept " + juce::String(k.label().empty() ? k.name : k.label()) + " at "
                   + juce::String(k.displayValue()) + " (sent " + juce::String(bad.front().sent) + ")"
                   + (bad.size() > 1 ? " and " + juce::String((int) bad.size() - 1) + " more" : juce::String()));
        }
    }
    if (onSigfile) onSigfile(sf);
}

bool H7600Link::runSigfile(const std::string& text, std::function<void(bool, const juce::String&)> done)
{
    auto fail = [&](const juce::String& m) { if (done) done(false, m); return false; };
    if (! isOpen()) return fail("Not connected");
    if (dbFetching || run || sigfilePending) return fail("The unit is busy; try again in a moment");
    h7600::Bytes frame;
    try { frame = h7600::makeSigfileLoad(deviceId, text); }
    catch (const std::exception& e) { return fail(e.what()); }
    sender.clear();
    sentValues.clear();
    verifyAt = 0;
    run = RunPending { text, std::move(done), nowMs() + 2500 };
    send(frame);
    status("Sent the edited program to the unit (run time only); reading it back...");
    return true;
}

void H7600Link::finishRun(bool ok, const juce::String& msg)
{
    if (! run) return;
    auto done = std::move(run->done);
    run.reset();
    status(msg);
    if (done) done(ok, msg);
}

bool H7600Link::fetchModuleDatabase()
{
    if (! isOpen()) { status("Not connected"); return false; }
    if (dbFetching) return false;
    if (! sender.idle() || sigfilePending || screenPending)
    {
        status("The unit is busy; try again in a moment");
        return false;
    }
    dbFetching = true;
    dbBytes = 0;
    dbLastProgress = nowMs();
    tracker.expect(h7600::Code::SigdbaseWant, nowMs(), 5 * 60 * 1000,
        [this](const h7600::Frame& f)
        {
            dbFetching = false;
            if (onDatabaseProgress) onDatabaseProgress(1.0);
            if (onDatabase) onDatabase(h7600::payloadText(f));
            scheduleRefresh(300);
        },
        [this](h7600::Code)
        {
            dbFetching = false;
            status("The module database did not arrive within 5 minutes");
            if (onDatabaseProgress) onDatabaseProgress(-1.0);
        });
    send(h7600::makeWant(deviceId, h7600::Code::SigdbaseWant));
    status("Asked the H7600 for its module database (about 2 minutes; it answers nothing else meanwhile)");
    return true;
}

bool H7600Link::requestScreen()
{
    if (! isOpen() || screenPending || sigfilePending || dbFetching || ! sender.idle())
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
    if (dbFetching) { status("Wait until the module database has arrived"); return; }
    const auto id = knob.paramId();
    const auto text = h7600::valueText(knob, value);
    sender.set(id, text);
    auto it = std::find_if(sentValues.begin(), sentValues.end(), [&](auto& p) { return p.first == id; });
    if (it != sentValues.end()) it->second = text;
    else sentValues.emplace_back(id, text);
    ++setGeneration;
    verifyAt = nowMs() + 800; // read back once the user has stopped for a moment
}

void H7600Link::handlePartialSysexMessage(juce::MidiInput*, const juce::uint8*, int n, double)
{
    juce::WeakReference<H7600Link> weak(this);
    juce::MessageManager::callAsync([weak, n]
    {
        auto* self = weak.get();
        if (! self || ! self->dbFetching) return;
        self->dbBytes += n;
        if (nowMs() - self->dbLastProgress < 500) return;
        self->dbLastProgress = nowMs();
        const double frac = juce::jlimit(0.0, 0.99, (double) self->dbBytes / 286448.0); // size on fw 5.22
        if (self->onDatabaseProgress) self->onDatabaseProgress(frac);
        self->status("Receiving the module database: " + juce::String(juce::roundToInt(frac * 100)) + " %");
    });
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
        if (wasKnown && ! is(f, h7600::Code::SigfileDumpRemote))
            scheduleRefresh(500);
    }
    const bool screenEvent = is(f, h7600::Code::Keypress) || is(f, h7600::Code::ParamSet)
                          || is(f, h7600::Code::ParamAck) || is(f, h7600::Code::SigfileDumpRemote);
    if (screenEvent && onScreenMayChange)
        onScreenMayChange();
    if (run && (is(f, h7600::Code::Ok) || is(f, h7600::Code::Error)))
    {
        // Tech Note 34's answers to 0A (firmware 5.22 sent neither in our test).
        if (is(f, h7600::Code::Error))
            finishRun(false, "The unit rejected the program: " + juce::String(h7600::payloadText(f)));
        else
            run->verifyAt = nowMs() + 300;
        return;
    }
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
        // ENTER / SELECT on the panel may have loaded a program: re-read it.
        for (auto& k : h7600::pressedKeys(*mask))
            if (k == "ENTER" || k == "SELECT")
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
    if (run && run->verifyAt != 0 && now >= run->verifyAt && ! sigfilePending)
    {
        run->verifyAt = 0; // the next sigfile is the read-back
        requestSigfile();
    }
    if (! dbFetching) // the unit is deaf until the database has arrived
    {
        if (refreshAt != 0 && now >= refreshAt && sender.idle())
        {
            refreshAt = 0;
            requestSigfile();
        }
        if (verifyAt != 0 && now >= verifyAt && sender.idle() && ! sigfilePending)
        {
            verifyAt = 0;
            requestSigfile();
        }
    }
    if (now >= nextWatchdog)
    {
        nextWatchdog = now + 2000;
        watchdog();
    }
}
