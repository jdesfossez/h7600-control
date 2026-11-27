// Developer tool: renders the plugin editor to PNG files.
//
//   H7600Snapshot <out-dir>                      offline: browser views, no MIDI
//   H7600Snapshot <out-dir> --screenshots        read-only: README screenshots of the
//                                                program loaded on the unit (one screen dump)
//   H7600Snapshot <out-dir> --fixtures <dir>     offline: the same views from messages captured
//                                                from a unit (tests/fixtures), no MIDI
//   H7600Snapshot <out-dir> --live <n>...        loads each program through the browser path
//   H7600Snapshot <out-dir> --param <knob> <v>   moves a control, reads the value back
//   H7600Snapshot <out-dir> --lcd <seconds>      counts screen dumps with the LCD tab idle
//
// Everything sent to the unit is run time only (loads, parameter changes, reads);
// nothing is saved on it.
#include <juce_audio_utils/juce_audio_utils.h>

#include "../Source/PluginEditor.h"
#include "../Source/PluginProcessor.h"

namespace
{
using Page = H7600ControlEditor::Page;

void pump(int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil(ms); }

/// A processor and its editor on the desktop (needed for fonts and focus),
/// at the editor's default size.
struct Harness
{
    explicit Harness(bool connect)
    {
        proc.setAutoConnect(connect);
        ed.reset(dynamic_cast<H7600ControlEditor*>(proc.createEditor()));
        ed->setSize(1320, 820);
        ed->addToDesktop(0);
        ed->setVisible(true);
    }
    ~Harness() { ed->removeFromDesktop(); }

    void save(const juce::File& f)
    {
        auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 2.0f);
        f.deleteFile();
        juce::FileOutputStream out(f);
        juce::PNGImageFormat().writeImageToStream(img, out);
        std::printf("wrote %s\n", f.getFullPathName().toRawUTF8());
    }

    H7600ControlProcessor proc;
    std::unique_ptr<H7600ControlEditor> ed;
};

int offline(const juce::File& dir)
{
    Harness h(false);
    pump(200);
    h.save(dir.getChildFile("browser_all.png"));
    h.ed->getBrowser().setSearchText("ambience");
    pump(200);
    h.save(dir.getChildFile("browser_search_ambience.png"));
    return 0;
}

// Read-only: shows what is loaded on the unit, never loads or changes anything.
int screenshots(const juce::File& dir, const juce::String& search)
{
    Harness h(true);
    pump(4000); // connect + quick sigfile of the loaded program
    std::printf("connected: %d (%s), status: %s\n", (int) h.proc.getLink().isOpen(),
                h.proc.getLink().getDeviceName().toRawUTF8(), h.ed->getStatusText().toRawUTF8());
    h.ed->showPage(Page::Parameters);
    pump(300);
    h.save(dir.getChildFile("parameters.png"));
    h.ed->showPage(Page::Lcd); // asks for one screen dump
    pump(3000);
    h.save(dir.getChildFile("lcd.png"));
    h.ed->showPage(Page::Browse);
    h.ed->getBrowser().setSearchText(search); // moving the selection only shows info
    pump(300);
    h.save(dir.getChildFile("browse.png"));
    return 0;
}

h7600::Bytes readFile(const juce::File& f)
{
    juce::MemoryBlock mb;
    f.loadFileAsData(mb);
    auto* p = static_cast<const std::uint8_t*>(mb.getData());
    return { p, p + mb.getSize() };
}

// Feeds captured replies through the link's callbacks, as if the unit sent them.
int fixtures(const juce::File& dir, const juce::File& fixturesDir, const juce::String& sigfile, const juce::String& search)
{
    Harness h(false);
    auto& link = h.proc.getLink();
    auto sf = h7600::parseFrame(readFile(fixturesDir.getChildFile(sigfile)));
    auto screen = h7600::parseFrame(readFile(fixturesDir.getChildFile("screen_dump_17.syx")));
    if (! sf || ! screen)
    {
        std::printf("fixtures not found in %s\n", fixturesDir.getFullPathName().toRawUTF8());
        return 1;
    }
    if (auto parsed = h7600::parseSigfile(h7600::payloadText(*sf)); parsed && link.onSigfile)
        link.onSigfile(*parsed);
    h.ed->showPage(Page::Parameters);
    pump(300);
    h.save(dir.getChildFile("parameters.png"));
    h.ed->showPage(Page::Xray); // needs the module database in the app's cache
    pump(300);
    h.save(dir.getChildFile("xray.png"));
    for (auto* name : { "delay1", "mix1", "Km_dlys", "plex" })
        if (h.ed->getXray().selectModule(name))
        {
            pump(200);
            h.save(dir.getChildFile("xray_selected.png"));
            break;
        }
    h.ed->getXray().setMode(XrayView::Mode::Audio);
    pump(200);
    h.save(dir.getChildFile("xray_audio.png"));
    h.ed->getXray().setMode(XrayView::Mode::Control);
    pump(200);
    h.save(dir.getChildFile("xray_control.png"));
    h.ed->getXray().setMode(XrayView::Mode::AudioAndControl);
    // Edit mode, offline: add a highcut after the left input. Nothing is sent.
    auto& xr = h.ed->getXray();
    xr.startEditing();
    if (auto* ed = xr.getEditor())
        if (auto idx = ed->add("highcut"))
        {
            ed->connect(0, "in1", *idx, "in");
            xr.editsChanged();
            xr.selectModule(ed->patch().modules[*idx].name());
            pump(300);
            h.save(dir.getChildFile("xray_edit.png"));
        }
    h.ed->showPage(Page::Lcd);
    if (auto d = h7600::parseScreenDump(*screen); d && link.onScreen)
        link.onScreen(*d);
    pump(300);
    h.save(dir.getChildFile("lcd.png"));
    h.ed->showPage(Page::Browse);
    h.ed->getBrowser().setSearchText(search);
    pump(300);
    h.save(dir.getChildFile("browse.png"));
    return 0;
}

int live(const juce::File& dir, const juce::StringArray& numbers)
{
    Harness h(true);
    pump(500);
    std::printf("connected: %d (%s)\n", (int) h.proc.getLink().isOpen(), h.proc.getLink().getDeviceName().toRawUTF8());
    for (auto& n : numbers)
    {
        h.ed->getBrowser().setSearchText(n);
        pump(100);
        h.ed->getBrowser().loadSelected();
        pump(5000);
        std::printf("after load %s: %s\n", n.toRawUTF8(), h.ed->getStatusText().toRawUTF8());
        h.save(dir.getChildFile("live_load_" + n + ".png"));
    }
    h.ed->showPage(Page::Parameters);
    pump(300);
    h.save(dir.getChildFile("live_params.png"));
    h.ed->showPage(Page::Lcd);
    pump(4000);
    h.save(dir.getChildFile("live_lcd.png"));
    return 0;
}

int liveParam(const juce::File& dir, const juce::String& knob, double value)
{
    Harness h(true);
    h.ed->showPage(Page::Parameters);
    pump(4000);
    auto& params = h.ed->getParams();
    auto* before = params.currentSigfile().findKnob(knob.toStdString());
    if (! before)
    {
        std::printf("knob %s not found in \"%s\"\n", knob.toRawUTF8(), params.currentSigfile().programName.c_str());
        return 1;
    }
    std::printf("program %s, %s before: %g\n", params.currentSigfile().programName.c_str(), knob.toRawUTF8(), before->value);
    params.setKnobForTools(knob.toStdString(), value);
    pump(800);
    h.save(dir.getChildFile("live_param_" + knob + ".png"));
    h.proc.getLink().requestSigfile();
    pump(4000);
    auto* after = params.currentSigfile().findKnob(knob.toStdString());
    std::printf("%s read back from unit: %g\n", knob.toRawUTF8(), after ? after->value : -999.0);
    return 0;
}

int lcdOnly(const juce::File& dir, int seconds)
{
    Harness h(true);
    pump(3000); // connect + initial program read
    int dumps = 0;
    auto& link = h.proc.getLink();
    auto prev = link.onScreen;
    link.onScreen = [&dumps, prev](const h7600::ScreenDump& d) { ++dumps; if (prev) prev(d); };
    h.ed->showPage(Page::Lcd);
    pump(seconds * 1000);
    std::printf("screen dumps in %d s with the LCD tab open and no activity: %d\n", seconds, dumps);
    h.save(dir.getChildFile("live_lcd.png"));
    return 0;
}
} // namespace

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::File dir(argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile(argv[1])
                            : juce::File::getCurrentWorkingDirectory());
    dir.createDirectory();
    const juce::String mode = argc > 2 ? argv[2] : "";

    if (mode == "--screenshots")
        return screenshots(dir, argc > 3 ? argv[3] : "canyon");
    if (mode == "--fixtures" && argc > 3)
        return fixtures(dir, juce::File::getCurrentWorkingDirectory().getChildFile(argv[3]),
                        argc > 4 ? argv[4] : "sigfile_quick_0a_ducked_delays.syx", argc > 5 ? argv[5] : "canyon");
    if (mode == "--lcd" && argc > 3)
        return lcdOnly(dir, juce::String(argv[3]).getIntValue());
    if (mode == "--param" && argc > 4)
        return liveParam(dir, argv[3], juce::String(argv[4]).getDoubleValue());
    if (mode == "--live")
    {
        juce::StringArray numbers;
        for (int i = 3; i < argc; ++i) numbers.add(argv[i]);
        return live(dir, numbers);
    }
    if (mode.isNotEmpty())
    {
        std::printf("usage: H7600Snapshot <out-dir> [--screenshots [search] | --fixtures <dir> [sigfile] [search] | --live <n>... | --param <knob> <value> | --lcd <seconds>]\n");
        return 2;
    }
    return offline(dir);
}
