// Renders the plugin editor to PNGs for visual review:
//   H7600Snapshot <out-dir>
#include <juce_audio_utils/juce_audio_utils.h>

#include "../Source/PluginEditor.h"
#include "../Source/PluginProcessor.h"

static void save(juce::Component& c, const juce::File& f)
{
    auto img = c.createComponentSnapshot(c.getLocalBounds(), true, 2.0f);
    f.deleteFile();
    juce::FileOutputStream out(f);
    juce::PNGImageFormat().writeImageToStream(img, out);
    std::printf("wrote %s\n", f.getFullPathName().toRawUTF8());
}

static void pump(int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil(ms); }

// Live mode (talks to the unit, run-time only): H7600Snapshot <out-dir> --live <n1> <n2> ...
// Loads each program through the browser's load path and snapshots the result.
static int live(const juce::File& dir, juce::StringArray numbers)
{
    H7600ControlProcessor proc;
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    auto* editor = dynamic_cast<H7600ControlEditor*>(ed.get());
    ed->setSize(1320, 820);
    ed->addToDesktop(0);
    ed->setVisible(true);
    pump(500);
    std::printf("connected: %d (%s)\n", (int) proc.getLink().isOpen(), proc.getLink().getDeviceName().toRawUTF8());
    for (auto& n : numbers)
    {
        editor->getBrowser().setSearchText(n);
        pump(100);
        editor->getBrowser().loadSelected();
        pump(5000);
        std::printf("after load %s: %s\n", n.toRawUTF8(), editor->getStatusText().toRawUTF8());
        save(*ed, dir.getChildFile("live_load_" + n + ".png"));
    }
    // Parameter page for the last program.
    editor->showPageForTools(1);
    pump(300);
    save(*ed, dir.getChildFile("live_params.png"));
    editor->showPageForTools(2);
    pump(4000);
    save(*ed, dir.getChildFile("live_lcd.png"));
    return 0;
}

// Live parameter test: H7600Snapshot <out-dir> --param <knob> <value>
// Moves a control through the UI path, re-reads the program and prints the value.
static int liveParam(const juce::File& dir, const juce::String& knob, double value)
{
    H7600ControlProcessor proc;
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    auto* editor = dynamic_cast<H7600ControlEditor*>(ed.get());
    ed->setSize(1320, 820);
    ed->addToDesktop(0);
    ed->setVisible(true);
    editor->showPageForTools(1);
    pump(4000);
    auto& params = editor->getParams();
    auto* before = params.currentSigfile().findKnob(knob.toStdString());
    if (! before) { std::printf("knob %s not found in \"%s\"\n", knob.toRawUTF8(), params.currentSigfile().programName.c_str()); return 1; }
    std::printf("program %s, %s before: %g\n", params.currentSigfile().programName.c_str(), knob.toRawUTF8(), before->value);
    params.setKnobForTools(knob.toStdString(), value);
    pump(800);
    save(*ed, dir.getChildFile("live_param_" + knob + ".png"));
    proc.getLink().requestSigfile();
    pump(4000);
    auto* after = params.currentSigfile().findKnob(knob.toStdString());
    std::printf("%s read back from unit: %g\n", knob.toRawUTF8(), after ? after->value : -999.0);
    ed->removeFromDesktop();
    return 0;
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc > 4 && juce::String(argv[2]) == "--param")
        return liveParam(juce::File(juce::String(argv[1])), argv[3], juce::String(argv[4]).getDoubleValue());
    if (argc > 2 && juce::String(argv[2]) == "--live")
    {
        juce::StringArray nums;
        for (int i = 3; i < argc; ++i) nums.add(argv[i]);
        return live(juce::File(juce::String(argv[1])), nums);
    }
    setenv("H7600_NO_AUTOCONNECT", "1", 1);
    juce::File dir(argc > 1 ? juce::String(argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    dir.createDirectory();

    H7600ControlProcessor proc;
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    ed->setSize(1320, 820);
    ed->addToDesktop(0);
    ed->setVisible(true);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(200);
    save(*ed, dir.getChildFile("browser_all.png"));

    auto* editor = dynamic_cast<H7600ControlEditor*>(ed.get());
    editor->getBrowser().setSearchText("ambience");
    juce::MessageManager::getInstance()->runDispatchLoopUntil(200);
    save(*ed, dir.getChildFile("browser_search_ambience.png"));
    ed->removeFromDesktop();
    return 0;
}
