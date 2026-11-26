#include "PluginEditor.h"

H7600ControlEditor::H7600ControlEditor(H7600ControlProcessor& p)
    : AudioProcessorEditor(&p), owner(p), browser(p.getCatalog(), p.getUserStore())
{
    setLookAndFeel(&lnf);
    auto& link = owner.getLink();

    deviceBox.setTooltip("MIDI interface wired to the H7600");
    deviceBox.onChange = [this]
    {
        auto name = deviceBox.getText();
        if (name.isNotEmpty() && name != owner.getLink().getDeviceName())
            owner.connect(name);
    };
    addAndMakeVisible(deviceBox);

    idSlider.setRange(0, 127, 1);
    idSlider.setTextValueSuffix("");
    idSlider.setValue(link.getDeviceId(), juce::dontSendNotification);
    idSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 40, 26);
    idSlider.onValueChange = [this] { owner.getLink().setDeviceId((int) idSlider.getValue()); };
    addAndMakeVisible(idSlider);

    for (int ch = 1; ch <= 16; ++ch)
        channelBox.addItem("Ch " + juce::String(ch), ch);
    channelBox.setSelectedId(link.getMidiChannel() + 1, juce::dontSendNotification);
    channelBox.onChange = [this] { owner.getLink().setMidiChannel(channelBox.getSelectedId() - 1); };
    addAndMakeVisible(channelBox);

    for (auto* t : { &browseTab, &paramsTab })
    {
        t->setClickingTogglesState(false);
        t->setWantsKeyboardFocus(false);
        addAndMakeVisible(*t);
    }
    browseTab.onClick = [this] { showPage(0); };
    paramsTab.onClick = [this] { showPage(1); };

    browser.onLoad = [this](int number)
    {
        pendingLoad = number;
        owner.getLink().loadProgram(number);
    };
    addAndMakeVisible(browser);

    sigfileButton.onClick = [this] { owner.getLink().requestSigfile(); };
    paramsPage.addAndMakeVisible(sigfileButton);
    knobList.setMultiLine(true);
    knobList.setReadOnly(true);
    knobList.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain)));
    paramsPage.addAndMakeVisible(knobList);
    addChildComponent(paramsPage);

    link.onStatus = [this](const juce::String& s) { setStatus(s); };
    link.onSigfile = [this](const h7600::Sigfile& sf) { showSigfile(sf); };
    link.onParamChange = [this](const h7600::ParamChange& c)
    {
        juce::String name = c.paramId;
        if (auto idx = c.moduleIndex())
            if (auto* k = current.findKnobByModule(*idx)) name = k->name;
        log("unit changed " + name + " -> " + c.value);
    };
    link.onKeys = [this](const juce::StringArray& keys)
    {
        if (! keys.isEmpty()) log("key: " + keys.joinIntoString("+"));
    };

    refreshDevices();
    if (! link.isOpen() && ! juce::SystemStats::getEnvironmentVariable("H7600_NO_AUTOCONNECT", {}).isNotEmpty())
    {
        auto devices = H7600Link::availableDevices();
        if (devices.contains("UM-ONE")) owner.connect("UM-ONE");
        else if (! devices.isEmpty()) owner.connect(devices[0]);
        refreshDevices();
    }
    setStatus(link.isOpen() ? "Connected to " + link.getDeviceName() : "Not connected");

    setResizable(true, true);
    setResizeLimits(1000, 640, 2400, 1600);
    setSize(1320, 820);
    showPage(0);
}

H7600ControlEditor::~H7600ControlEditor()
{
    auto& link = owner.getLink();
    link.onStatus = nullptr;
    link.onSigfile = nullptr;
    link.onParamChange = nullptr;
    link.onKeys = nullptr;
    setLookAndFeel(nullptr);
}

void H7600ControlEditor::showPage(int page)
{
    browser.setVisible(page == 0);
    paramsPage.setVisible(page == 1);
    browseTab.setToggleState(page == 0, juce::dontSendNotification);
    paramsTab.setToggleState(page == 1, juce::dontSendNotification);
    if (page == 0) browser.focusSearch();
}

void H7600ControlEditor::setStatus(const juce::String& s)
{
    statusText = s;
    browser.setStatus(s);
    repaint();
}

void H7600ControlEditor::refreshDevices()
{
    deviceBox.clear(juce::dontSendNotification);
    auto devices = H7600Link::availableDevices();
    for (int i = 0; i < devices.size(); ++i)
        deviceBox.addItem(devices[i], i + 1);
    auto connected = owner.getLink().getDeviceName();
    if (devices.contains(connected))
        deviceBox.setText(connected, juce::dontSendNotification);
}

void H7600ControlEditor::showSigfile(const h7600::Sigfile& sf)
{
    current = sf;
    programName = sf.programName;
    if (pendingLoad >= 0)
    {
        // Confirm the load against the catalog name (the sigfile name can carry a suffix).
        auto* p = owner.getCatalog().preset(pendingLoad);
        auto squash = [](juce::String s) { return s.removeCharacters(" -_").toLowerCase(); };
        bool ok = p != nullptr && squash(sf.programName).startsWith(squash(p->name));
        if (ok)
        {
            browser.noteLoaded(pendingLoad);
            setStatus("Loaded " + juce::String(pendingLoad) + " " + juce::String(p->name));
        }
        else
            setStatus("Asked for " + juce::String(pendingLoad) + ", the unit has \"" + juce::String(sf.programName)
                      + "\" loaded. Is SETUP > MIDI > MIDI map set to none?");
        pendingLoad = -1;
    }

    juce::String text;
    text << "Program: " << sf.programName << "\n\n";
    for (auto& k : sf.knobs)
    {
        text << juce::String(k.paramId()).paddedRight(' ', 8) << juce::String(k.name).paddedRight(' ', 14)
             << juce::String(k.displayValue()).paddedRight(' ', 30);
        if (k.kind == h7600::KnobKind::TextKnob) text << "(" << (int) k.options.size() << " choices)";
        else text << "[" << k.min << " .. " << k.max << " step " << k.step << "]";
        text << "\n";
    }
    knobList.setText(text, false);
    repaint();
}

void H7600ControlEditor::log(const juce::String& s)
{
    knobList.moveCaretToEnd();
    knobList.insertTextAtCaret(s + "\n");
}

void H7600ControlEditor::paint(juce::Graphics& g)
{
    g.fillAll(theme::bg);
    auto header = getLocalBounds().reduced(16, 0).removeFromTop(64);
    g.setColour(theme::ink);
    g.setFont(theme::ui(22.0f, true));
    g.drawText("H7600", header.removeFromLeft(80), juce::Justification::centredLeft);
    g.setColour(theme::mute);
    g.setFont(theme::ui(15.0f));
    auto sub = header.removeFromLeft(380);
    g.drawText(programName.isEmpty() ? juce::String("Control") : juce::String(juce::CharPointer_UTF8("Control  \xc2\xb7  ")) + programName, sub, juce::Justification::centredLeft);

    const bool on = owner.getLink().isOpen();
    auto led = ledArea.withSizeKeepingCentre(ledArea.getWidth(), 20);
    auto dot = led.removeFromLeft(10).withSizeKeepingCentre(10, 10).toFloat();
    if (on)
    {
        g.setColour(theme::red.withAlpha(0.35f));
        g.fillEllipse(dot.expanded(3));
    }
    g.setColour(on ? theme::red : juce::Colour(0xff555555));
    g.fillEllipse(dot);
    g.setColour(theme::mute);
    g.setFont(theme::ui(14.0f));
    g.drawFittedText(statusText, led.withTrimmedLeft(8), juce::Justification::centredLeft, 1);
}

void H7600ControlEditor::resized()
{
    auto r = getLocalBounds().reduced(16, 0);
    auto header = r.removeFromTop(64);
    header.removeFromLeft(80 + 380);
    auto right = header.withSizeKeepingCentre(header.getWidth(), 32);
    paramsTab.setBounds(right.removeFromRight(110));
    right.removeFromRight(6);
    browseTab.setBounds(right.removeFromRight(90));
    right.removeFromRight(16);
    channelBox.setBounds(right.removeFromRight(80));
    right.removeFromRight(6);
    idSlider.setBounds(right.removeFromRight(110));
    right.removeFromRight(6);
    deviceBox.setBounds(right.removeFromRight(150));
    right.removeFromRight(12);
    ledArea = right;

    r.removeFromBottom(16);
    browser.setBounds(r);
    paramsPage.setBounds(r);
    auto pr = paramsPage.getLocalBounds();
    sigfileButton.setBounds(pr.removeFromTop(34).removeFromLeft(160));
    pr.removeFromTop(10);
    knobList.setBounds(pr);
}
