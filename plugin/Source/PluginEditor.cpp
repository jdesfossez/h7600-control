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
    idSlider.setValue(link.getDeviceId(), juce::dontSendNotification);
    idSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 40, 26);
    idSlider.onValueChange = [this] { owner.getLink().setDeviceId((int) idSlider.getValue()); };
    addAndMakeVisible(idSlider);

    for (int ch = 1; ch <= 16; ++ch)
        channelBox.addItem("Ch " + juce::String(ch), ch);
    channelBox.setSelectedId(link.getMidiChannel() + 1, juce::dontSendNotification);
    channelBox.onChange = [this] { owner.getLink().setMidiChannel(channelBox.getSelectedId() - 1); };
    addAndMakeVisible(channelBox);

    for (auto* t : { &browseTab, &paramsTab, &lcdTab })
    {
        t->setClickingTogglesState(false);
        t->setWantsKeyboardFocus(false);
        addAndMakeVisible(*t);
    }
    browseTab.onClick = [this] { showPage(Page::Browse); };
    paramsTab.onClick = [this] { showPage(Page::Parameters); };
    lcdTab.onClick = [this] { showPage(Page::Lcd); };
    lcdMirror.requestScreen = [this] { return owner.getLink().requestScreen(); };
    addChildComponent(lcdMirror);

    browser.onLoad = [this](int number)
    {
        pendingLoad = number;
        owner.getLink().loadProgram(number);
    };
    addAndMakeVisible(browser);

    params.onChange = [this](const h7600::Knob& k, double v) { owner.getLink().setParameter(k, v); };
    params.onRefresh = [this] { owner.getLink().requestSigfile(); };
    addChildComponent(params);

    link.onStatus = [this](const juce::String& s) { setStatus(s); };
    link.onSigfile = [this](const h7600::Sigfile& sf) { showSigfile(sf); };
    link.onParamChange = [this](const h7600::ParamChange& c) { params.applyFromUnit(c); };
    link.onConnectionChanged = [this] { connectionChanged(); };
    link.onScreen = [this](const h7600::ScreenDump& d) { lcdMirror.show(d); };
    link.onScreenMayChange = [this] { lcdMirror.noteActivity(); };

    refreshDevices();
    if (! link.isOpen() && owner.shouldAutoConnect())
    {
        auto devices = H7600Link::availableDevices();
        if (devices.contains("UM-ONE")) owner.connect("UM-ONE");
        else if (! devices.isEmpty()) owner.connect(devices[0]);
        refreshDevices();
    }
    setStatus(link.isOpen() ? "Connected to " + link.getDeviceName() : "Not connected");
    if (link.isOpen()) link.requestSigfile(); // editor reopened: show the current program

    setResizable(true, true);
    setResizeLimits(1000, 640, 2400, 1600);
    setSize(1320, 820);
    showPage(Page::Browse);
}

H7600ControlEditor::~H7600ControlEditor()
{
    auto& link = owner.getLink();
    link.onStatus = nullptr;
    link.onSigfile = nullptr;
    link.onParamChange = nullptr;
    link.onConnectionChanged = nullptr;
    link.onScreen = nullptr;
    link.onScreenMayChange = nullptr;
    setLookAndFeel(nullptr);
}

void H7600ControlEditor::showPage(Page page)
{
    browser.setVisible(page == Page::Browse);
    params.setVisible(page == Page::Parameters);
    lcdMirror.setVisible(page == Page::Lcd);
    browseTab.setToggleState(page == Page::Browse, juce::dontSendNotification);
    paramsTab.setToggleState(page == Page::Parameters, juce::dontSendNotification);
    lcdTab.setToggleState(page == Page::Lcd, juce::dontSendNotification);
    if (page == Page::Browse) browser.focusSearch();
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

void H7600ControlEditor::connectionChanged()
{
    auto& link = owner.getLink();
    params.setConnected(link.isOpen(), link.isResponding());
    refreshDevices();
    repaint();
}

void H7600ControlEditor::showSigfile(const h7600::Sigfile& sf)
{
    programName = sf.programName;
    params.showSigfile(sf);
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
            setStatus("Asked for " + juce::String(pendingLoad) + " but the unit has \"" + juce::String(sf.programName)
                      + "\" loaded. Set SETUP > MIDI > MIDI map to none on the H7600.");
        pendingLoad = -1;
    }

    repaint();
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
    auto sub = header.removeFromLeft(300);
    g.drawText(programName.isEmpty() ? juce::String("Control") : juce::String(juce::CharPointer_UTF8("Control  \xc2\xb7  ")) + programName, sub, juce::Justification::centredLeft);

    const bool on = owner.getLink().isOpen() && owner.getLink().isResponding();
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
    header.removeFromLeft(80 + 300);
    auto right = header.withSizeKeepingCentre(header.getWidth(), 32);
    lcdTab.setBounds(right.removeFromRight(70));
    right.removeFromRight(6);
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
    params.setBounds(r);
    lcdMirror.setBounds(r);
}
