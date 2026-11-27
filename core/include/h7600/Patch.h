#pragma once
// Editable model of a program ("patch") as a quick sigfile, using the module database to name
// each argument and to know each module's outputs.
//
// A sigfile line is "<TYPE> <name> <arg> <arg> ...": the arguments are the module's ED_* records
// in order (MULTIPLE blocks expanded, see ModuleDb.h). An input argument (ED_SIGNAL, ED_CONTROL,
// ED_USEROBJ) is either a connection "<module>-<output tag>" (e.g. "adc-in1", "mix1-out",
// "info-obj") or, for a control input, a constant ("1.0000"). Tokens are kept exactly as read
// (quotes included), so an unedited patch serializes back to the same text.

#include "h7600/ModuleDb.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace h7600
{
enum class PortKind { Audio, Control, UserObject };

struct PatchField
{
    std::string name;   // record name with "~n" expanded, e.g. "in", "gain2", "obj1"
    std::string dbType; // ED_STRING, ED_SIGNAL, ED_CONTROL, ED_INTEGER, ED_REAL, ED_USEROBJ
    std::string token;  // as in the sigfile: "adc-in1", "1.0000", "\"post hicut: %5.0f Hz\""
    std::size_t record = 0; // index into DbModule::records

    bool isInput() const { return dbType == "ED_SIGNAL" || dbType == "ED_CONTROL" || dbType == "ED_USEROBJ"; }
    std::optional<PortKind> inputKind() const;
    bool isQuoted() const { return token.size() >= 2 && token.front() == '"' && token.back() == '"'; }
    /// Token without quotes.
    std::string text() const;
    /// The number, if the token is a numeric constant.
    std::optional<double> number() const;
};

struct PatchOutput
{
    std::string tag;   // e.g. "out", "in1", "obj", "left"
    PortKind kind = PortKind::Audio;
    std::string connectClass; // MONO, LEFT, RIGHT, MOD for audio outputs
};

struct PatchModule
{
    std::string type;                // as in the sigfile, upper case ("HIGHCUT")
    std::vector<PatchField> fields;  // fields[0] is the name (opstart_name)
    std::vector<PatchOutput> outputs;

    std::string name() const; // unquoted ("Sum L+R")
    PatchField* field(std::string_view name);
    const PatchField* field(std::string_view name) const;
    const PatchOutput* output(std::string_view tag) const;
    std::string line() const; // the sigfile line
};

struct Connection
{
    std::size_t from = 0, to = 0; // module indices
    std::string fromTag;          // output of `from`
    std::size_t field = 0;        // input field index in `to`
    PortKind kind = PortKind::Audio;
    bool feedback = false;        // source is not earlier in the file than the target
};

struct Patch
{
    std::string prefix;          // text before the first line (the unit sends "\r\n")
    std::string eol = "\r\n";
    std::vector<PatchModule> modules;

    std::optional<std::size_t> find(std::string_view name) const;
    std::string programName() const; // HEAD's obj_description, or ""
    /// Resolves "<module>-<tag>"; nullopt for constants and unknown references.
    std::optional<std::pair<std::size_t, const PatchOutput*>> resolve(std::string_view token) const;
    std::vector<Connection> connections() const;
};

/// Where a control source (a knob, an LFO, ...) ends up: following control connections, through
/// modules that only compute controls (C_MULTIPLY, C_TABLE, SKNOB3, ...), to the first module that
/// does something else with it (an audio module's ED_CONTROL input, a monitor, ...).
struct ControlTarget
{
    std::size_t module = 0;
    std::size_t field = 0;           // the input field in `module`
    std::vector<std::size_t> via;    // control modules in between, in order
};
std::vector<ControlTarget> controlTargets(const Patch& p, std::size_t source);

/// True for modules that are knobs on the unit's menus (KNOB, PERCENTKNOB, TEXTKNOB, SKNOB3, ...).
bool isKnobType(std::string_view type);

/// The knob modules whose controls reach `module` (directly or through control math).
std::vector<std::size_t> knobsSteering(const Patch& p, std::size_t module);

/// The database record behind a field (its DESCRIPTION, MINIMUM, MAXIMUM, DEFAULT), or nullptr.
const DbRecord* fieldRecord(const ModuleDb& db, const PatchModule& m, const PatchField& f);

/// Parses a quick sigfile (0B reply text). Fails if a type is unknown or the argument count does
/// not match the database.
std::optional<Patch> parsePatch(std::string_view text, const ModuleDb& db, std::string* error = nullptr);

/// The sigfile text: prefix, then each module line followed by eol.
std::string serialize(const Patch& p);

struct PatchIssue
{
    enum class Severity { Error, Warning };
    Severity severity = Severity::Error;
    int module = -1;    // index, or -1 for the whole patch
    std::string field;  // field name, or ""
    std::string message;
};

/// Checks a patch before it is sent to the unit: first module HEAD*, last TAIL; known types;
/// unique names (1..18 characters, no spaces, quotes or '-'); argument count; every connection
/// resolves to an output of the right kind (audio to ED_SIGNAL, control to ED_CONTROL, user
/// object to ED_USEROBJ); constants numeric and in range; at most 2 inputs and 2 outputs on the
/// HEAD (the H7600 is a two-in, two-out unit).
std::vector<PatchIssue> validate(const Patch& p, const ModuleDb& db);
inline bool hasErrors(const std::vector<PatchIssue>& v)
{
    for (auto& i : v)
        if (i.severity == PatchIssue::Severity::Error) return true;
    return false;
}

// ---- Editing -------------------------------------------------------------------------------

/// A new module of `type` with default arguments: inputs unpatched (audio to "<head>-null",
/// user objects to "<head>-nullobj", controls to their default constant).
std::optional<PatchModule> makeModule(const ModuleDb& db, std::string_view type, std::string_view name,
                                      std::string_view headName = "adc");

/// Sets a field's token. If the field is a repeat count (MULTIPLE "@field"), the module's fields
/// and outputs are re-expanded, keeping the values of fields that still exist.
bool setField(PatchModule& m, const ModuleDb& db, std::string_view field, std::string token,
              std::string_view headName = "adc");

/// A free name "<base><n>" (e.g. "filter3").
std::string uniqueName(const Patch& p, std::string_view base);

/// Inserts before TAIL (or at `at`), returns its index.
std::size_t insertModule(Patch& p, PatchModule m, std::optional<std::size_t> at = std::nullopt);
/// Removes a module and unpatches every input that used one of its outputs.
void removeModule(Patch& p, const ModuleDb& db, std::size_t index);
/// Sets an input to its unpatched value.
void unpatch(PatchModule& m, const ModuleDb& db, std::size_t field, std::string_view headName = "adc");
/// Renames a module and every reference to it.
bool renameModule(Patch& p, std::size_t index, std::string_view newName);

/// "<module>-<tag>", quoted when the module name has a space.
std::string referenceToken(std::string_view module, std::string_view tag);

/// Text for a field value: quoted for strings (embedded quotes removed), plain otherwise.
std::string fieldToken(const PatchField& f, std::string_view value);
} // namespace h7600
