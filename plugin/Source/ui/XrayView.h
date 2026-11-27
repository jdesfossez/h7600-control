#pragma once
// X-ray: a read-only picture of the loaded program, from its quick sigfile and the module
// database. A sage LCD panel shows the modules as a graph (audio modules in columns by signal
// depth, IN on the left, OUT on the right; control modules in lanes under the audio module they
// steer; feedback as dotted arcs). Clicking a module shows its fields in the inspector on the
// right, with a control for every knob that steers it (directly or through control math). Those
// controls send run-time parameter changes, like the Parameters page; nothing is saved.

#include <juce_gui_basics/juce_gui_basics.h>

#include "h7600/ModuleDb.h"
#include "h7600/Patch.h"
#include "h7600/Sigfile.h"
#include "ParamsView.h"
#include "Theme.h"

class XrayView : public juce::Component
{
public:
    enum class Mode { Audio, AudioAndControl, Control };

    XrayView();

    std::function<void(const h7600::Knob&, double)> onChange;

    void setModuleDb(const h7600::ModuleDb* db);
    void showSigfile(const h7600::Sigfile&);
    void applyFromUnit(const h7600::ParamChange&);
    void setProgramNumber(int n) { programNumber = n; repaint(); }

    void setMode(Mode);
    /// Selects a module by name (tools); returns false if it is not drawn.
    bool selectModule(const std::string& name);

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;

protected:
    /// The current patch, if the sigfile parsed against the database (for the editor subclass).
    std::optional<h7600::Patch> patch;
    const h7600::ModuleDb* db = nullptr;
    h7600::Sigfile sigfile;
    int selected = -1; // module index
    void rebuild();          // patch changed: graph, layout, inspector
    void select(int module);
    juce::Rectangle<int> lcdBounds() const { return lcd; }
    juce::Rectangle<int> inspectorBounds() const { return inspector; }
    juce::String problem;   // why there is no graph
    void log(const juce::String& line);

private:
    struct Placed
    {
        int node = 0;
        juce::Rectangle<float> box;
    };
    void layoutGraph();
    void buildInspector();
    void layoutInspector();
    bool visible(const h7600::GraphNode&) const;
    juce::String nodeName(const h7600::GraphNode&) const;
    juce::String nodeAbbrev(const h7600::GraphNode&) const;

    h7600::PatchGraph graph;
    std::vector<Placed> placed;
    Mode mode = Mode::AudioAndControl;
    int programNumber = -1;
    float audioSplit = 0; // height of the audio graph in aud+ctrl mode

    juce::Rectangle<int> lcd, graphArea, inspector, keysRow;
    juce::TextButton audKey { "aud only" }, bothKey { "aud+ctrl" }, ctrlKey { "ctrl only" };

    // Inspector
    juce::Viewport inspectorView;
    juce::Component inspectorContent;
    juce::OwnedArray<ParamControl> knobControls;
    juce::StringArray inspectorLines; // painted by the content component
    juce::StringArray sentLog;
    struct Content;
    std::unique_ptr<juce::Component> contentPainter;
};
