#include "h7600/PatchEditor.h"

namespace h7600
{
PatchEditor::PatchEditor(const ModuleDb& d, Patch original) : db(d), base(original), current(std::move(original)) {}

void PatchEditor::push()
{
    undoStack.push_back(current);
    if (undoStack.size() > 200) undoStack.erase(undoStack.begin());
    redoStack.clear();
}

std::string PatchEditor::headName() const
{
    return current.modules.empty() ? std::string("adc") : current.modules.front().name();
}

bool PatchEditor::setField(std::size_t module, std::string_view field, std::string_view value)
{
    if (module >= current.modules.size()) return false;
    auto* f = current.modules[module].field(field);
    if (! f) return false;
    if (field == "opstart_name") return rename(module, value);
    auto token = fieldToken(*f, value);
    if (token.empty() || token == f->token) return false;
    auto before = current;
    if (! h7600::setField(current.modules[module], db, field, token, headName()))
    {
        current = before;
        return false;
    }
    undoStack.push_back(std::move(before));
    redoStack.clear();
    return true;
}

bool PatchEditor::rename(std::size_t module, std::string_view name)
{
    if (module >= current.modules.size() || name.empty() || current.find(name)) return false;
    auto before = current;
    if (! renameModule(current, module, name)) return false;
    undoStack.push_back(std::move(before));
    redoStack.clear();
    return true;
}

std::optional<std::size_t> PatchEditor::add(std::string_view type)
{
    auto* dm = db.find(type);
    if (! dm) return std::nullopt;
    auto m = makeModule(db, type, uniqueName(current, dm->name), headName());
    if (! m) return std::nullopt;
    push();
    return insertModule(current, std::move(*m));
}

bool PatchEditor::remove(std::size_t module)
{
    // The HEAD and TAIL stay: every program needs them.
    if (module == 0 || module >= current.modules.size() || current.modules[module].type == "TAIL") return false;
    push();
    removeModule(current, db, module);
    return true;
}

bool PatchEditor::connect(std::size_t from, std::string_view tag, std::size_t to, std::string_view field)
{
    if (from >= current.modules.size() || to >= current.modules.size()) return false;
    auto* out = current.modules[from].output(tag);
    auto* f = current.modules[to].field(field);
    if (! out || ! f || ! f->isInput() || f->inputKind() != out->kind) return false;
    auto token = referenceToken(current.modules[from].name(), tag);
    if (token == f->token) return false;
    push();
    current.modules[to].field(field)->token = token;
    return true;
}

bool PatchEditor::unplug(std::size_t module, std::string_view field)
{
    if (module >= current.modules.size()) return false;
    auto& m = current.modules[module];
    for (std::size_t i = 1; i < m.fields.size(); ++i)
        if (m.fields[i].name == field && m.fields[i].isInput())
        {
            auto before = current;
            h7600::unpatch(current.modules[module], db, i, headName());
            if (serialize(before) == serialize(current)) return false;
            undoStack.push_back(std::move(before));
            redoStack.clear();
            return true;
        }
    return false;
}

bool PatchEditor::undo()
{
    if (undoStack.empty()) return false;
    redoStack.push_back(std::move(current));
    current = std::move(undoStack.back());
    undoStack.pop_back();
    return true;
}

bool PatchEditor::redo()
{
    if (redoStack.empty()) return false;
    undoStack.push_back(std::move(current));
    current = std::move(redoStack.back());
    redoStack.pop_back();
    return true;
}

void PatchEditor::revert()
{
    if (! modified()) return;
    push();
    current = base;
}

std::vector<std::string> PatchEditor::sourcesFor(std::size_t module, std::string_view field) const
{
    std::vector<std::string> out;
    if (module >= current.modules.size()) return out;
    auto* f = current.modules[module].field(field);
    if (! f || ! f->isInput()) return out;
    for (std::size_t i = 0; i < current.modules.size(); ++i)
    {
        if (i == module) continue;
        for (auto& o : current.modules[i].outputs)
        {
            if (o.kind != *f->inputKind()) continue;
            // The HEAD's placeholders are offered by unplug(), not as sources.
            if (i == 0 && o.tag.rfind("in", 0) != 0) continue;
            out.push_back(referenceToken(current.modules[i].name(), o.tag));
        }
    }
    return out;
}
} // namespace h7600
