#include "PluginEditor.h"

H7600ControlEditor::H7600ControlEditor(H7600ControlProcessor& p)
    : AudioProcessorEditor(&p), owner(p)
{
    title.setText("H7600 Control", juce::dontSendNotification);
    title.setFont(juce::FontOptions(20.0f, juce::Font::bold));
    addAndMakeVisible(title);

    deviceLabel.setText("MIDI device", juce::dontSendNotification);
    idLabel.setText("Device ID", juce::dontSendNotification);
    addAndMakeVisible(deviceLabel);
    addAndMakeVisible(idLabel);

    addAndMakeVisible(deviceBox);
    deviceBox.onChange = [this]
    {
        auto name = deviceBox.getText();
        if (name.isNotEmpty() && name != owner.getLink().getDeviceName())
            owner.connect(name);
    };

    idSlider.setRange(0, 127, 1);
    idSlider.setValue(owner.getLink().getDeviceId(), juce::dontSendNotification);
    idSlider.onValueChange = [this] { owner.getLink().setDeviceId((int) idSlider.getValue()); };
    addAndMakeVisible(idSlider);

    rescanButton.onClick = [this] { refreshDevices(); };
    sigfileButton.onClick = [this] { owner.getLink().requestSigfile(); };
    addAndMakeVisible(rescanButton);
    addAndMakeVisible(sigfileButton);

    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    addAndMakeVisible(statusLabel);

    knobList.setMultiLine(true);
    knobList.setReadOnly(true);
    knobList.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));
    addAndMakeVisible(knobList);

    auto& link = owner.getLink();
    link.onStatus = [this](const juce::String& s) { statusLabel.setText(s, juce::dontSendNotification); };
    link.onSigfile = [this](const h7600::Sigfile& sf) { showSigfile(sf); };
    link.onParamChange = [this](const h7600::ParamChange& c)
    {
        juce::String name = c.paramId;
        if (auto idx = c.moduleIndex())
            if (auto* k = current.findKnobByModule(*idx))
                name = k->name;
        log("unit changed " + name + " -> " + c.value);
    };
    link.onKeys = [this](const juce::StringArray& keys)
    {
        if (! keys.isEmpty()) log("key: " + keys.joinIntoString("+"));
    };

    refreshDevices();
    if (! link.isOpen())
    {
        // Prefer an interface that looks like the H7600's, else the first one.
        auto devices = H7600Link::availableDevices();
        if (devices.contains("UM-ONE")) owner.connect("UM-ONE");
        else if (! devices.isEmpty()) owner.connect(devices[0]);
        refreshDevices();
    }
    statusLabel.setText(link.isOpen() ? "Connected to " + link.getDeviceName() : "Not connected",
                        juce::dontSendNotification);
    setSize(640, 480);
}

H7600ControlEditor::~H7600ControlEditor()
{
    auto& link = owner.getLink();
    link.onStatus = nullptr;
    link.onSigfile = nullptr;
    link.onParamChange = nullptr;
    link.onKeys = nullptr;
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
    juce::String text;
    text << "Program: " << sf.programName << "\n\n";
    for (auto& k : sf.knobs)
    {
        text << juce::String(k.paramId()).paddedRight(' ', 8)
             << juce::String(k.name).paddedRight(' ', 14)
             << juce::String(k.displayValue()).paddedRight(' ', 30);
        if (k.kind == h7600::KnobKind::TextKnob)
            text << "(" << (int) k.options.size() << " choices)";
        else
            text << "[" << k.min << " .. " << k.max << " step " << k.step << "]";
        text << "\n";
    }
    knobList.setText(text, false);
}

void H7600ControlEditor::log(const juce::String& s)
{
    knobList.moveCaretToEnd();
    knobList.insertTextAtCaret(s + "\n");
}

void H7600ControlEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void H7600ControlEditor::resized()
{
    auto r = getLocalBounds().reduced(12);
    title.setBounds(r.removeFromTop(28));
    r.removeFromTop(6);
    auto row = r.removeFromTop(26);
    deviceLabel.setBounds(row.removeFromLeft(80));
    deviceBox.setBounds(row.removeFromLeft(200));
    row.removeFromLeft(6);
    rescanButton.setBounds(row.removeFromLeft(70));
    row.removeFromLeft(12);
    idLabel.setBounds(row.removeFromLeft(70));
    idSlider.setBounds(row.removeFromLeft(110));
    r.removeFromTop(8);
    row = r.removeFromTop(26);
    sigfileButton.setBounds(row.removeFromLeft(140));
    row.removeFromLeft(10);
    statusLabel.setBounds(row);
    r.removeFromTop(8);
    knobList.setBounds(r);
}
