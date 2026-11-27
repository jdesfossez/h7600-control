#include "h7600/ModuleDb.h"

#include <algorithm>
#include <cctype>

namespace h7600
{
namespace
{
const std::string kEmpty;

std::string_view trim(std::string_view s)
{
    while (! s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    while (! s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
    return s;
}
} // namespace

std::string toLower(std::string_view s)
{
    std::string o(s);
    for (auto& c : o) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return o;
}

std::string toUpper(std::string_view s)
{
    std::string o(s);
    for (auto& c : o) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return o;
}

const std::string& DbRecord::get(std::string_view key) const
{
    for (auto& [k, v] : values)
        if (k == key) return v;
    return kEmpty;
}

bool DbRecord::has(std::string_view key) const
{
    return std::any_of(values.begin(), values.end(), [&](auto& kv) { return kv.first == key; });
}

std::vector<std::string> DbRecord::all(std::string_view key) const
{
    std::vector<std::string> out;
    for (auto& [k, v] : values)
        if (k == key) out.push_back(v);
    return out;
}

bool DbRecord::isOutput() const
{
    auto& t = type();
    return (t == "SIGNAL" || t == "CONTROL" || t == "USEROBJ") && has("CONNECT_TAG");
}

std::optional<ModuleDb> ModuleDb::parse(std::string_view text, std::string* error)
{
    auto fail = [&](std::string why) -> std::optional<ModuleDb>
    {
        if (error) *error = std::move(why);
        return std::nullopt;
    };
    ModuleDb db;
    int depth = 0;
    std::vector<DbRecord> group;
    DbRecord rec;
    int lineNo = 0;
    std::size_t pos = 0;
    while (pos <= text.size())
    {
        auto nl = text.find('\n', pos);
        auto line = trim(text.substr(pos, nl == std::string_view::npos ? std::string_view::npos : nl - pos));
        pos = nl == std::string_view::npos ? text.size() + 1 : nl + 1;
        ++lineNo;
        if (line.empty() || line == std::string_view("\0", 1)) continue;
        if (line == "{")
        {
            if (++depth > 2) return fail("nesting deeper than two levels at line " + std::to_string(lineNo));
            if (depth == 1) group.clear();
            else rec = {};
        }
        else if (line == "}")
        {
            if (depth == 2) group.push_back(std::move(rec));
            else if (depth == 1)
            {
                if (group.empty() || group.front().type() != "OPERATOR" || group.back().type() != "OPERATOREND")
                    return fail("module without OPERATOR/OPERATOREND records ending at line " + std::to_string(lineNo));
                DbModule m;
                m.name = toLower(group.front().get("OPERATOR_NAME"));
                m.className = group.front().get("OPERATOR_CLASS");
                m.records = std::move(group);
                group.clear();
                db.mods.push_back(std::move(m));
            }
            else
                return fail("unbalanced '}' at line " + std::to_string(lineNo));
            --depth;
        }
        else
        {
            auto colon = line.find(':');
            if (depth != 2 || colon == std::string_view::npos)
                return fail("unexpected text at line " + std::to_string(lineNo));
            rec.values.emplace_back(std::string(line.substr(0, colon)), std::string(line.substr(colon + 1)));
        }
    }
    if (depth != 0) return fail("truncated database (unclosed '{')");
    if (db.mods.empty()) return fail("no modules found");
    return db;
}

const DbModule* ModuleDb::find(std::string_view type) const
{
    auto n = toLower(type);
    for (auto& m : mods)
        if (m.name == n) return &m;
    return nullptr;
}

std::vector<std::string> ModuleDb::classes() const
{
    std::vector<std::string> out;
    for (auto& m : mods)
        if (std::find(out.begin(), out.end(), m.className) == out.end()) out.push_back(m.className);
    return out;
}
} // namespace h7600
