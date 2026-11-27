#pragma once
// Edits a copy of a patch on the computer, with undo/redo. Nothing here talks to the unit: the
// result is sent only by an explicit "Run on unit" (see makeSigfileLoad), and only after
// validate() finds no errors.

#include "h7600/ModuleDb.h"
#include "h7600/Patch.h"

#include <string>
#include <vector>

namespace h7600
{
class PatchEditor
{
public:
    PatchEditor(const ModuleDb& db, Patch original);

    const Patch& patch() const { return current; }
    const Patch& original() const { return base; }
    bool modified() const { return serialize(current) != serialize(base); }
    std::string text() const { return serialize(current); }
    std::vector<PatchIssue> issues() const { return validate(current, db); }

    // Every edit is one undo step; each returns false (and changes nothing) if it does not apply.
    bool setField(std::size_t module, std::string_view field, std::string_view value);
    bool rename(std::size_t module, std::string_view name);
    /// Adds a module of `type` before TAIL with default arguments; returns its index.
    std::optional<std::size_t> add(std::string_view type);
    bool remove(std::size_t module);
    /// Connects output `tag` of module `from` to input `field` of module `to` (kinds must match).
    bool connect(std::size_t from, std::string_view tag, std::size_t to, std::string_view field);
    /// Back to the unpatched value (audio "<head>-null", control default, user object "<head>-nullobj").
    bool unplug(std::size_t module, std::string_view field);

    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }
    bool undo();
    bool redo();
    /// Discards every edit.
    void revert();
    /// The unit now runs the edited patch: it becomes the new original (undo history kept).
    void markRun() { base = current; }

    /// Outputs that can feed `field` of `module`: "<module>-<tag>" tokens of the right kind.
    std::vector<std::string> sourcesFor(std::size_t module, std::string_view field) const;

private:
    void push();
    std::string headName() const;
    const ModuleDb& db;
    Patch base, current;
    std::vector<Patch> undoStack, redoStack;
};
} // namespace h7600
