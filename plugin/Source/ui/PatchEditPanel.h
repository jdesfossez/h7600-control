#pragma once
// Field editors for one module of an edited patch (X-ray tab, edit mode): its name, settings
// (specifiers) and inputs (a source menu; control inputs can also take a constant). Every change
// goes through h7600::PatchEditor, so it can be undone. Only the Mac copy changes here.

#include <juce_gui_basics/juce_gui_basics.h>

#include "h7600/PatchEditor.h"
#include "Theme.h"

class PatchEditPanel : public juce::Component
{
public:
    std::function<void()> onEdited;

    void show(h7600::PatchEditor* editor, const h7600::ModuleDb* db, int module);
    void paint(juce::Graphics&) override;
    void resized() override;
    int preferredHeight() const { return height; }

private:
    struct Row
    {
        juce::String label, hint;
        bool control = false;
        std::unique_ptr<juce::TextEditor> text;
        std::unique_ptr<juce::ComboBox> combo;
    };
    void commitText(const juce::String& field, juce::TextEditor& te);
    h7600::PatchEditor* ed = nullptr;
    const h7600::ModuleDb* db = nullptr;
    int module = -1;
    juce::StringArray issueLines;
    juce::String header, subHeader;
    std::vector<std::unique_ptr<Row>> rows;
    int height = 0;
};
