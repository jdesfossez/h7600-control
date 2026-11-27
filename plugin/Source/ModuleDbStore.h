#pragma once
// The unit's module database (0E -> 0C), cached on the Mac. It is Eventide's data, so it is not
// shipped with the app: the user fetches it from their own H7600 once (about 2 minutes) or imports
// a saved copy. Cache: ~/Library/Application Support/H7600 Control/sigdbase.txt (macOS).

#include <juce_core/juce_core.h>

#include "h7600/ModuleDb.h"

class ModuleDbStore
{
public:
    ModuleDbStore();

    /// The database, or nullptr when none has been fetched or imported yet.
    const h7600::ModuleDb* get() const { return db ? &*db : nullptr; }
    juce::String describe() const;

    /// Takes the 0C payload text (or a .txt/.syx file's content); saves it to the cache if it parses.
    bool setFromText(const std::string& text, juce::String* error = nullptr);
    bool importFile(const juce::File& f, juce::String* error = nullptr);

    static juce::File cacheFile();

private:
    std::optional<h7600::ModuleDb> db;
};
