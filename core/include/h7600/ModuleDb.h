#pragma once
// The unit's module ("operator") database, as returned for SIGDBASE_WANT (0E -> 0C).
//
// The 0C payload is plain ASCII (CR LF lines): a sequence of module groups "{ ... }", each holding
// field records "{ KEY:VALUE ... }". A module opens with a TYPE:OPERATOR record (OPERATOR_NAME,
// OPERATOR_CLASS) and closes with TYPE:OPERATOREND. In between:
//  - ED_* records (ED_STRING, ED_SIGNAL, ED_CONTROL, ED_INTEGER, ED_REAL, ED_USEROBJ) are the
//    module's arguments in a sigfile line, in order (the first, opstart_name, is its name);
//  - SIGNAL / CONTROL / USEROBJ records with a CONNECT_TAG are its outputs ("<name>-<tag>");
//  - MULTIPLE ... MULTIPLEEND repeats the records between them ACTION_SPECIFIER times ("@field"
//    names an earlier integer argument); "~n" in names and tags becomes the 1-based repeat index;
//  - SWITCH / CASE / SWITCHEND bracket optional outputs (they never contain arguments).
// Firmware 5.22 lists 225 modules. The database is Eventide's data: the app fetches it from the
// user's own unit and this repository does not include it.

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace h7600
{
struct DbRecord
{
    std::vector<std::pair<std::string, std::string>> values; // in order; ATTRIBUTE may repeat

    /// First value for key, or "" if absent.
    const std::string& get(std::string_view key) const;
    bool has(std::string_view key) const;
    std::vector<std::string> all(std::string_view key) const;
    const std::string& type() const { return get("TYPE"); }
    const std::string& name() const { return get("NAME"); }
    bool isArgument() const { return type().rfind("ED_", 0) == 0; }
    bool isOutput() const;
};

struct DbModule
{
    std::string name;      // lower case, e.g. "highcut"; sigfiles use it upper case
    std::string className; // e.g. "FILTER"
    std::vector<DbRecord> records; // every record, OPERATOR and OPERATOREND included
};

class ModuleDb
{
public:
    /// Parses the 0C payload text. Returns nullopt (and a reason) if it is not a database.
    static std::optional<ModuleDb> parse(std::string_view text, std::string* error = nullptr);

    const std::vector<DbModule>& modules() const { return mods; }
    bool empty() const { return mods.empty(); }
    /// Case-insensitive lookup by module name or sigfile type ("HIGHCUT").
    const DbModule* find(std::string_view type) const;
    /// Class names in database order, each once.
    std::vector<std::string> classes() const;

private:
    std::vector<DbModule> mods;
};

std::string toLower(std::string_view s);
std::string toUpper(std::string_view s);
} // namespace h7600
