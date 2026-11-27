#include "h7600/Patch.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <set>
#include <tuple>

namespace h7600
{
namespace
{

bool isNumber(std::string_view s)
{
    if (s.empty()) return false;
    std::size_t i = 0;
    if (s[i] == '-' || s[i] == '+') ++i;
    bool digits = false, dot = false;
    for (; i < s.size(); ++i)
    {
        char c = s[i];
        if (std::isdigit(static_cast<unsigned char>(c))) digits = true;
        else if (c == '.' && ! dot) dot = true;
        else if ((c == 'e' || c == 'E') && digits && i + 1 < s.size())
        {
            ++i;
            if (s[i] == '-' || s[i] == '+') ++i;
            if (i >= s.size()) return false;
            for (; i < s.size(); ++i)
                if (! std::isdigit(static_cast<unsigned char>(s[i]))) return false;
            return true;
        }
        else return false;
    }
    return digits;
}

std::string formatNumber(double v)
{
    char buf[64];
    if (std::fabs(v - std::round(v)) < 1e-9 && std::fabs(v) < 1e15)
        std::snprintf(buf, sizeof buf, "%lld", static_cast<long long>(std::llround(v)));
    else
        std::snprintf(buf, sizeof buf, "%.4f", v);
    return buf;
}

bool isOperatorWord(std::string_view t)
{
    if (t.empty() || ! std::isupper(static_cast<unsigned char>(t[0]))) return false;
    for (char c : t)
        if (! (std::isupper(static_cast<unsigned char>(c)) || std::isdigit(static_cast<unsigned char>(c)) || c == '_'))
            return false;
    return true;
}

/// Tokens of one line: whitespace separated, a quoted string is one token (quotes kept).
std::vector<std::string> tokens(std::string_view s)
{
    std::vector<std::string> out;
    std::size_t i = 0;
    while (i < s.size())
    {
        if (std::isspace(static_cast<unsigned char>(s[i]))) { ++i; continue; }
        std::string t;
        if (s[i] == '"')
        {
            t.push_back(s[i++]);
            while (i < s.size() && s[i] != '"') t.push_back(s[i++]);
            if (i < s.size()) t.push_back(s[i++]);
        }
        else
            while (i < s.size() && ! std::isspace(static_cast<unsigned char>(s[i]))) t.push_back(s[i++]);
        out.push_back(std::move(t));
    }
    return out;
}

/// Evaluates the database's postfix expressions: "@nscales/16/#mul", "=10/524288/480/#div/...".
/// Operands are numbers or "@field"; unknown operators (#speed, #if, ...) give nullopt.
std::optional<double> evalExpr(std::string_view spec, const std::function<std::optional<double>(std::string_view)>& field)
{
    if (! spec.empty() && spec.front() == '=') spec.remove_prefix(1);
    if (spec.empty()) return std::nullopt;
    std::vector<double> st;
    std::size_t pos = 0;
    while (pos <= spec.size())
    {
        auto slash = spec.find('/', pos);
        auto t = spec.substr(pos, slash == std::string_view::npos ? std::string_view::npos : slash - pos);
        pos = slash == std::string_view::npos ? spec.size() + 1 : slash + 1;
        if (t.empty()) return std::nullopt;
        if (t.front() == '@')
        {
            auto v = field(t.substr(1));
            if (! v) return std::nullopt;
            st.push_back(*v);
        }
        else if (t.front() == '#')
        {
            if (t == "#neg" || t == "#trunc")
            {
                if (st.empty()) return std::nullopt;
                st.back() = t == "#neg" ? -st.back() : std::trunc(st.back());
                continue;
            }
            if (st.size() < 2) return std::nullopt;
            double b = st.back(); st.pop_back();
            double& a = st.back();
            if (t == "#add") a += b;
            else if (t == "#sub") a -= b;
            else if (t == "#mul") a *= b;
            else if (t == "#div") { if (b == 0) return std::nullopt; a /= b; }
            else return std::nullopt;
        }
        else if (isNumber(t))
            st.push_back(std::strtod(std::string(t).c_str(), nullptr));
        else
            return std::nullopt;
    }
    if (st.size() != 1) return std::nullopt;
    return st.front();
}

std::string unquote(std::string_view t)
{
    if (t.size() >= 2 && t.front() == '"' && t.back() == '"') t = t.substr(1, t.size() - 2);
    return std::string(t);
}

std::string expandName(const std::string& n, int rep)
{
    if (rep <= 0) return n;
    std::string out = n;
    for (auto p = out.find("~n"); p != std::string::npos; p = out.find("~n"))
        out.replace(p, 2, std::to_string(rep));
    return out;
}

std::string defaultToken(const DbRecord& r, std::string_view head)
{
    auto& t = r.type();
    auto& def = r.get("DEFAULT");
    if (t == "ED_STRING")
    {
        std::string s;
        for (char c : def)
            if (c != '"') s += c;
        return "\"" + s + "\"";
    }
    if (t == "ED_SIGNAL") return std::string(head) + "-null";
    if (t == "ED_USEROBJ") return std::string(head) + "-nullobj";
    if (isNumber(def)) return formatNumber(std::strtod(def.c_str(), nullptr));
    if (isNumber(r.get("MINIMUM"))) return formatNumber(std::strtod(r.get("MINIMUM").c_str(), nullptr));
    return "0";
}

// Walks a module's records, producing fields and outputs. `next(record, expandedName)` returns the
// token for an argument (or nullopt to stop with an error).
struct Expander
{
    using Next = std::function<std::optional<std::string>(std::size_t, const std::string&)>;
    Expander(const DbModule& mod, Next n) : m(mod), next(std::move(n)) {}
    const DbModule& m;
    Next next;
    std::vector<PatchField> fields;
    std::vector<PatchOutput> outputs;
    std::string error;

    std::optional<int> count(const std::string& spec) const
    {
        auto v = evalExpr(spec, [&](std::string_view n) -> std::optional<double>
        {
            auto it = std::find_if(fields.begin(), fields.end(), [&](auto& f) { return f.name == n; });
            return it == fields.end() ? std::nullopt : it->number();
        });
        if (! v) return std::nullopt;
        return std::clamp(static_cast<int>(*v), 0, 1000);
    }

    bool run(std::size_t b, std::size_t e, int rep)
    {
        for (std::size_t j = b; j < e; ++j)
        {
            auto& r = m.records[j];
            if (r.type() == "MULTIPLE")
            {
                int depth = 1;
                std::size_t k = j + 1;
                for (; k < e && depth > 0; ++k)
                {
                    if (m.records[k].type() == "MULTIPLE") ++depth;
                    if (m.records[k].type() == "MULTIPLEEND") --depth;
                }
                auto n = count(r.get("ACTION_SPECIFIER"));
                if (! n) { error = "bad repeat count " + r.get("ACTION_SPECIFIER"); return false; }
                for (int i = 1; i <= *n; ++i)
                    if (! run(j + 1, k - 1, i)) return false;
                j = k - 1;
                continue;
            }
            if (r.isArgument())
            {
                auto name = expandName(r.name(), rep);
                auto tok = next(j, name);
                if (! tok) { if (error.empty()) error = "missing argument " + name; return false; }
                fields.push_back({ name, r.type(), *tok, j });
            }
            else if (r.isOutput())
            {
                PatchOutput o;
                o.tag = expandName(r.get("CONNECT_TAG"), rep);
                o.kind = r.type() == "SIGNAL" ? PortKind::Audio : r.type() == "CONTROL" ? PortKind::Control : PortKind::UserObject;
                o.connectClass = r.get("CONNECT_CLASS");
                outputs.push_back(std::move(o));
            }
        }
        return true;
    }
};

std::optional<double> numberOf(const std::string& s)
{
    if (! isNumber(s)) return std::nullopt;
    return std::strtod(s.c_str(), nullptr);
}

std::string headNameOf(const Patch& p)
{
    return p.modules.empty() ? std::string {} : p.modules.front().name();
}
} // namespace

// ---- PatchField / PatchModule / Patch ---------------------------------------------------------

std::optional<PortKind> PatchField::inputKind() const
{
    if (dbType == "ED_SIGNAL") return PortKind::Audio;
    if (dbType == "ED_CONTROL") return PortKind::Control;
    if (dbType == "ED_USEROBJ") return PortKind::UserObject;
    return std::nullopt;
}

std::string PatchField::text() const { return isQuoted() ? token.substr(1, token.size() - 2) : token; }

std::optional<double> PatchField::number() const { return numberOf(token); }

std::string PatchModule::name() const { return fields.empty() ? std::string {} : fields.front().text(); }

PatchField* PatchModule::field(std::string_view n)
{
    for (auto& f : fields)
        if (f.name == n) return &f;
    return nullptr;
}

const PatchField* PatchModule::field(std::string_view n) const
{
    return const_cast<PatchModule*>(this)->field(n);
}

const PatchOutput* PatchModule::output(std::string_view tag) const
{
    for (auto& o : outputs)
        if (o.tag == tag) return &o;
    return nullptr;
}

std::string PatchModule::line() const
{
    std::string s = type;
    for (auto& f : fields) s += " " + f.token;
    return s;
}

std::optional<std::size_t> Patch::find(std::string_view name) const
{
    for (std::size_t i = 0; i < modules.size(); ++i)
        if (modules[i].name() == name) return i;
    return std::nullopt;
}

std::string Patch::programName() const
{
    if (modules.empty()) return {};
    if (auto* f = modules.front().field("obj_description")) return f->text();
    return {};
}

std::optional<std::pair<std::size_t, const PatchOutput*>> Patch::resolve(std::string_view token) const
{
    if (isNumber(token)) return std::nullopt;
    const auto t = unquote(token); // names with spaces are quoted: "Sum L+R-out"
    for (auto dash = t.find('-'); dash != std::string::npos; dash = t.find('-', dash + 1))
        if (auto idx = find(std::string_view(t).substr(0, dash)))
            if (auto* o = modules[*idx].output(std::string_view(t).substr(dash + 1)))
                return std::make_pair(*idx, o);
    return std::nullopt;
}

std::string referenceToken(std::string_view module, std::string_view tag)
{
    std::string r = std::string(module) + "-" + std::string(tag);
    return std::string(module).find(' ') != std::string::npos ? "\"" + r + "\"" : r;
}

std::vector<Connection> Patch::connections() const
{
    std::vector<Connection> out;
    for (std::size_t to = 0; to < modules.size(); ++to)
    {
        auto& m = modules[to];
        for (std::size_t fi = 1; fi < m.fields.size(); ++fi)
        {
            auto& f = m.fields[fi];
            if (! f.isInput()) continue;
            auto r = resolve(f.token);
            if (! r) continue;
            Connection c;
            c.from = r->first;
            c.to = to;
            c.fromTag = r->second->tag;
            c.field = fi;
            c.kind = r->second->kind;
            c.feedback = c.from >= c.to && c.from != 0; // the HEAD's outputs are the unit's inputs
            out.push_back(std::move(c));
        }
    }
    return out;
}

// ---- Control flow -----------------------------------------------------------------------

bool isKnobType(std::string_view type)
{
    return toUpper(type).find("KNOB") != std::string::npos;
}

std::vector<ControlTarget> controlTargets(const Patch& p, std::size_t source)
{
    std::vector<ControlTarget> out;
    if (source >= p.modules.size()) return out;
    const auto cs = p.connections();
    auto passesOn = [&](std::size_t m)
    {
        const auto& outs = p.modules[m].outputs;
        bool control = false, audio = false;
        for (auto& o : outs)
        {
            control |= o.kind == PortKind::Control;
            audio |= o.kind == PortKind::Audio;
        }
        return control && ! audio && m != 0;
    };
    struct Step { std::size_t module; std::vector<std::size_t> via; };
    std::vector<Step> todo { { source, {} } };
    std::set<std::size_t> seen { source };
    std::set<std::pair<std::size_t, std::size_t>> recorded;
    while (! todo.empty())
    {
        auto cur = todo.back();
        todo.pop_back();
        for (auto& c : cs)
        {
            if (c.from != cur.module || c.kind != PortKind::Control || c.to == cur.module) continue;
            if (passesOn(c.to))
            {
                if (seen.insert(c.to).second)
                {
                    auto via = cur.via;
                    via.push_back(c.to);
                    todo.push_back({ c.to, std::move(via) });
                }
            }
            else if (recorded.insert({ c.to, c.field }).second)
                out.push_back({ c.to, c.field, cur.via });
        }
    }
    std::sort(out.begin(), out.end(), [](auto& a, auto& b) { return std::tie(a.module, a.field) < std::tie(b.module, b.field); });
    return out;
}

std::vector<std::size_t> knobsSteering(const Patch& p, std::size_t module)
{
    std::vector<std::size_t> out;
    for (std::size_t k = 0; k < p.modules.size(); ++k)
    {
        if (! isKnobType(p.modules[k].type)) continue;
        for (auto& t : controlTargets(p, k))
            if (t.module == module || std::find(t.via.begin(), t.via.end(), module) != t.via.end())
            {
                out.push_back(k);
                break;
            }
    }
    return out;
}

const DbRecord* fieldRecord(const ModuleDb& db, const PatchModule& m, const PatchField& f)
{
    auto* dm = db.find(m.type);
    return dm && f.record < dm->records.size() ? &dm->records[f.record] : nullptr;
}

// ---- Graph -------------------------------------------------------------------------------

int PatchGraph::nodeOf(int module) const
{
    for (std::size_t i = 0; i < nodes.size(); ++i)
        if (nodes[i].module == module) return static_cast<int>(i);
    return -1;
}

std::string moduleAbbrev(std::string_view type)
{
    static const std::map<std::string, std::string> known {
        { "moddelay", "mdl" }, { "mix", "mix" }, { "ducker", "dck" }, { "stereomixer", "smx" },
        { "knob", "knb" }, { "percentknob", "pct" }, { "textknob", "txk" }, { "sknob3", "skn" },
        { "c_table", "tbl" }, { "c_multiply", "mul" }, { "midiclock", "clk" }, { "a_to_c", "a2c" },
        { "hmonitor", "hmn" },
    };
    auto t = toLower(type);
    if (auto it = known.find(t); it != known.end()) return it->second;
    std::string out;
    for (char c : t)
        if (c != '_' && out.size() < 3) out += c;
    return out;
}

PatchGraph buildGraph(const Patch& p)
{
    PatchGraph g;
    const auto n = p.modules.size();
    if (n == 0) return g;
    const auto cs = p.connections();
    std::vector<int> role(n, 0); // 0 hidden, 1 audio, 2 control
    for (std::size_t i = 1; i < n; ++i)
    {
        auto& m = p.modules[i];
        if (m.type == "TAIL") continue;
        bool audio = false, control = false;
        for (auto& o : m.outputs)
        {
            audio |= o.kind == PortKind::Audio;
            control |= o.kind == PortKind::Control;
        }
        for (auto& f : m.fields)
        {
            audio |= f.dbType == "ED_SIGNAL";
            control |= f.dbType == "ED_CONTROL";
        }
        role[i] = audio ? 1 : control ? 2 : 0;
    }
    // Signal depth over forward audio connections.
    std::vector<int> depth(n, 0);
    int maxDepth = 0;
    for (std::size_t i = 1; i < n; ++i)
    {
        if (role[i] != 1) continue;
        int d = 1;
        for (auto& c : cs)
            if (c.to == i && c.kind == PortKind::Audio && ! c.feedback && c.from < i && (c.from == 0 || role[c.from] == 1))
                d = std::max(d, c.from == 0 ? 1 : depth[c.from] + 1);
        depth[i] = d;
        maxDepth = std::max(maxDepth, d);
    }
    g.columns = maxDepth + 2;
    std::map<int, int> audioRows, controlRows;
    g.nodes.push_back({ GraphNode::kIn, true, 0, 0 });
    for (std::size_t i = 1; i < n; ++i)
        if (role[i] == 1)
            g.nodes.push_back({ static_cast<int>(i), true, depth[i], audioRows[depth[i]]++ });
    g.nodes.push_back({ GraphNode::kOut, true, g.columns - 1, 0 });
    for (std::size_t i = 1; i < n; ++i)
    {
        if (role[i] != 2) continue;
        int col = 0;
        for (auto& t : controlTargets(p, i))
            if (role[t.module] == 1) { col = depth[t.module]; break; }
            else if (t.module == 0) { col = g.columns - 1; }
        g.nodes.push_back({ static_cast<int>(i), false, col, controlRows[col]++ });
    }
    const int inNode = 0, outNode = g.nodeOf(GraphNode::kOut);
    for (auto& c : cs)
    {
        if (c.kind == PortKind::UserObject) continue;
        // "adc-null", "adc-zero" and the like are the HEAD's "unpatched" outputs, not connections.
        if (c.from == 0 && c.fromTag.rfind("in", 0) != 0) continue;
        int from = c.from == 0 ? (c.kind == PortKind::Audio ? inNode : -1) : g.nodeOf(static_cast<int>(c.from));
        int to = c.to == 0 ? (c.kind == PortKind::Audio ? outNode : -1) : g.nodeOf(static_cast<int>(c.to));
        if (from < 0 || to < 0) continue;
        const bool fb = c.from != 0 && c.to != 0 && c.feedback;
        g.edges.push_back({ from, to, c.kind, fb, c.to == 0 ? 0 : c.field });
    }
    return g;
}

// ---- Parse / serialize --------------------------------------------------------------------

std::optional<Patch> parsePatch(std::string_view text, const ModuleDb& db, std::string* error)
{
    auto fail = [&](std::string why) -> std::optional<Patch>
    {
        if (error) *error = std::move(why);
        return std::nullopt;
    };
    Patch p;
    p.eol = text.find("\r\n") != std::string_view::npos ? "\r\n" : "\n";

    // Group lines into module statements (a statement starts with an upper-case type word).
    struct Stmt { std::vector<std::string> toks; int line; };
    std::vector<Stmt> stmts;
    std::size_t pos = 0;
    int lineNo = 0;
    bool seenFirst = false;
    while (pos < text.size())
    {
        auto nl = text.find('\n', pos);
        auto end = nl == std::string_view::npos ? text.size() : nl;
        auto raw = text.substr(pos, end - pos);
        if (! raw.empty() && raw.back() == '\r') raw.remove_suffix(1);
        ++lineNo;
        auto toks = tokens(raw);
        if (toks.empty())
        {
            if (! seenFirst) p.prefix.append(text.substr(pos, (nl == std::string_view::npos ? end : nl + 1) - pos));
        }
        else if (isOperatorWord(toks[0]))
        {
            seenFirst = true;
            stmts.push_back({ std::move(toks), lineNo });
        }
        else if (! stmts.empty())
            stmts.back().toks.insert(stmts.back().toks.end(), toks.begin(), toks.end());
        else
            return fail("text before the first module at line " + std::to_string(lineNo));
        pos = nl == std::string_view::npos ? text.size() : nl + 1;
    }
    if (stmts.empty()) return fail("no modules");

    for (auto& s : stmts)
    {
        auto* dm = db.find(s.toks[0]);
        if (! dm) return fail("line " + std::to_string(s.line) + ": unknown module type " + s.toks[0]);
        std::size_t k = 1;
        Expander ex(*dm, [&](std::size_t, const std::string&) -> std::optional<std::string>
            {
                if (k >= s.toks.size()) return std::nullopt;
                return s.toks[k++];
            });
        if (! ex.run(0, dm->records.size(), 0))
            return fail("line " + std::to_string(s.line) + " (" + s.toks[0] + "): " + ex.error);
        if (k != s.toks.size())
            return fail("line " + std::to_string(s.line) + " (" + s.toks[0] + "): " + std::to_string(s.toks.size() - k)
                        + " more arguments than the database describes");
        PatchModule m;
        m.type = s.toks[0];
        m.fields = std::move(ex.fields);
        m.outputs = std::move(ex.outputs);
        p.modules.push_back(std::move(m));
    }
    return p;
}

std::string serialize(const Patch& p)
{
    std::string s = p.prefix;
    for (auto& m : p.modules) s += m.line() + p.eol;
    return s;
}

// ---- Validation ---------------------------------------------------------------------------

std::vector<PatchIssue> validate(const Patch& p, const ModuleDb& db)
{
    std::vector<PatchIssue> out;
    auto err = [&](int m, std::string f, std::string msg)
    { out.push_back({ PatchIssue::Severity::Error, m, std::move(f), std::move(msg) }); };
    auto warn = [&](int m, std::string f, std::string msg)
    { out.push_back({ PatchIssue::Severity::Warning, m, std::move(f), std::move(msg) }); };

    if (p.modules.empty()) { err(-1, "", "the patch has no modules"); return out; }
    if (p.modules.front().type.rfind("HEAD", 0) != 0) err(0, "", "the first module must be a HEAD");
    if (p.modules.back().type != "TAIL") err(static_cast<int>(p.modules.size()) - 1, "", "the last module must be a TAIL");

    std::set<std::string> names;
    for (std::size_t i = 0; i < p.modules.size(); ++i)
    {
        auto& m = p.modules[i];
        const int mi = static_cast<int>(i);
        if (i > 0 && m.type.rfind("HEAD", 0) == 0) err(mi, "", "only the first module can be a HEAD");
        if (i + 1 < p.modules.size() && m.type == "TAIL") err(mi, "", "TAIL must be the last module");
        auto* dm = db.find(m.type);
        if (! dm) { err(mi, "", "unknown module type " + m.type); continue; }

        const auto n = m.name();
        const bool quoted = m.fields.front().isQuoted();
        if (n.empty() || n.size() > 18) err(mi, "name", "names must be 1 to 18 characters");
        if (n.find_first_of("\t\"-") != std::string::npos || (! quoted && n.find(' ') != std::string::npos))
            err(mi, "name", "names cannot contain quotes or '-' (spaces only in a quoted name)");
        if (! names.insert(n).second) err(mi, "name", "the name " + n + " is used twice");

        // Same shape as the database describes for the current repeat counts.
        std::size_t k = 0;
        Expander ex(*dm, [&](std::size_t, const std::string&) -> std::optional<std::string>
            {
                if (k >= m.fields.size()) return std::nullopt;
                return m.fields[k++].token;
            });
        if (! ex.run(0, dm->records.size(), 0) || k != m.fields.size())
        {
            err(mi, "", "the arguments do not match the database (" + (ex.error.empty() ? "too many" : ex.error) + ")");
            continue;
        }

        for (std::size_t fi = 1; fi < m.fields.size(); ++fi)
        {
            auto& f = m.fields[fi];
            auto& rec = dm->records[f.record];
            if (f.isInput())
            {
                auto kind = *f.inputKind();
                if (auto r = p.resolve(f.token))
                {
                    if (r->second->kind != kind)
                    {
                        static const char* names[] = { "an audio", "a control", "a user object" };
                        err(mi, f.name, std::string("needs ") + names[static_cast<int>(kind)] + " connection, "
                            + f.token + " is " + names[static_cast<int>(r->second->kind)] + " output");
                    }
                }
                else if (kind == PortKind::Control && isNumber(f.token))
                {
                    // fall through to the range check below
                }
                else
                {
                    err(mi, f.name, f.token + " is not an output of any module");
                    continue;
                }
                if (! (kind == PortKind::Control && isNumber(f.token))) continue;
            }
            if (f.dbType == "ED_STRING")
            {
                if (fi > 0 && ! f.isQuoted()) err(mi, f.name, "text values must be quoted");
                auto size = numberOf(rec.get("STORAGE_SIZE"));
                if (size && *size > 0 && f.text().size() > static_cast<std::size_t>(*size))
                    warn(mi, f.name, "longer than " + rec.get("STORAGE_SIZE") + " characters");
                continue;
            }
            auto v = f.number();
            if (! v) { err(mi, f.name, f.token + " is not a number"); continue; }
            if (f.dbType == "ED_INTEGER" && std::fabs(*v - std::round(*v)) > 1e-9)
                err(mi, f.name, "must be a whole number");
            auto bound = [&](std::string_view key) -> std::optional<double>
            {
                // Numbers, "@field" or postfix formulas; formulas that depend on the sample rate
                // (#speed) or use conditionals are not checked.
                return evalExpr(rec.get(key), [&](std::string_view n) -> std::optional<double>
                {
                    auto* g = m.field(n);
                    return g ? g->number() : std::nullopt;
                });
            };
            auto lo = bound("MINIMUM"), hi = bound("MAXIMUM");
            if (lo && hi && *lo > *hi) std::swap(lo, hi);
            // A bound computed from other fields is only a warning: factory programs exceed some
            // (Ambience's HMENUPAGE textnum is 1 with a computed maximum of 0).
            const bool computed = ! numberOf(rec.get("MINIMUM")) || ! numberOf(rec.get("MAXIMUM"));
            if ((lo && *v < *lo - 1e-6) || (hi && *v > *hi + 1e-6))
                out.push_back({ computed ? PatchIssue::Severity::Warning : PatchIssue::Severity::Error, mi, f.name,
                                f.token + " is out of range (" + (lo ? formatNumber(*lo) : "?") + " to "
                                    + (hi ? formatNumber(*hi) : "?") + ")" });
        }
        if (i == 0)
            for (auto* io : { "ninputs", "noutputs" })
                if (auto* f = m.field(io); f && f->number() && *f->number() > 2)
                    err(mi, io, "the H7600 has two inputs and two outputs");
    }
    return out;
}

// ---- Editing ------------------------------------------------------------------------------

std::string fieldToken(const PatchField& f, std::string_view value)
{
    if (f.dbType == "ED_STRING" && f.name != "opstart_name")
    {
        std::string s = "\"";
        for (char c : value)
            if (c != '"' && c != '\r' && c != '\n') s += c;
        return s + "\"";
    }
    std::string s;
    for (char c : value)
        if (! std::isspace(static_cast<unsigned char>(c)) && c != '"') s += c;
    return s;
}

std::optional<PatchModule> makeModule(const ModuleDb& db, std::string_view type, std::string_view name,
                                      std::string_view headName)
{
    auto* dm = db.find(type);
    if (! dm) return std::nullopt;
    Expander ex(*dm, [&](std::size_t r, const std::string& n) -> std::optional<std::string>
        {
            if (n == "opstart_name") return std::string(name);
            return defaultToken(dm->records[r], headName);
        });
    if (! ex.run(0, dm->records.size(), 0)) return std::nullopt;
    PatchModule m;
    m.type = toUpper(dm->name);
    m.fields = std::move(ex.fields);
    m.outputs = std::move(ex.outputs);
    return m;
}

bool setField(PatchModule& m, const ModuleDb& db, std::string_view fieldName, std::string token, std::string_view headName)
{
    auto* f = m.field(fieldName);
    auto* dm = db.find(m.type);
    if (! f || ! dm) return false;
    f->token = std::move(token);
    std::map<std::string, std::string> old;
    for (auto& g : m.fields) old[g.name] = g.token;
    Expander ex(*dm, [&](std::size_t r, const std::string& n) -> std::optional<std::string>
        {
            if (auto it = old.find(n); it != old.end()) return it->second;
            return defaultToken(dm->records[r], headName);
        });
    if (! ex.run(0, dm->records.size(), 0)) return false;
    m.fields = std::move(ex.fields);
    m.outputs = std::move(ex.outputs);
    return true;
}

std::string uniqueName(const Patch& p, std::string_view base)
{
    std::string b(base.substr(0, 15));
    for (int i = 1;; ++i)
    {
        auto n = b + std::to_string(i);
        if (! p.find(n)) return n;
    }
}

std::size_t insertModule(Patch& p, PatchModule m, std::optional<std::size_t> at)
{
    std::size_t idx = at ? std::min(*at, p.modules.size())
                         : (! p.modules.empty() && p.modules.back().type == "TAIL" ? p.modules.size() - 1 : p.modules.size());
    p.modules.insert(p.modules.begin() + static_cast<std::ptrdiff_t>(idx), std::move(m));
    return idx;
}

void unpatch(PatchModule& m, const ModuleDb& db, std::size_t field, std::string_view headName)
{
    auto* dm = db.find(m.type);
    if (! dm || field >= m.fields.size()) return;
    m.fields[field].token = defaultToken(dm->records[m.fields[field].record], headName);
}

void removeModule(Patch& p, const ModuleDb& db, std::size_t index)
{
    if (index >= p.modules.size()) return;
    const std::string head = index == 0 ? std::string("adc") : headNameOf(p);
    for (std::size_t i = 0; i < p.modules.size(); ++i)
    {
        if (i == index) continue;
        for (std::size_t fi = 1; fi < p.modules[i].fields.size(); ++fi)
            if (p.modules[i].fields[fi].isInput())
                if (auto r = p.resolve(p.modules[i].fields[fi].token); r && r->first == index)
                    unpatch(p.modules[i], db, fi, head);
    }
    p.modules.erase(p.modules.begin() + static_cast<std::ptrdiff_t>(index));
}

bool renameModule(Patch& p, std::size_t index, std::string_view newName)
{
    if (index >= p.modules.size() || newName.empty() || p.find(newName)) return false;
    std::vector<std::pair<PatchField*, std::string>> refs;
    for (auto& m : p.modules)
        for (std::size_t fi = 1; fi < m.fields.size(); ++fi)
            if (m.fields[fi].isInput())
                if (auto r = p.resolve(m.fields[fi].token); r && r->first == index)
                    refs.emplace_back(&m.fields[fi], r->second->tag);
    const std::string n(newName);
    p.modules[index].fields.front().token = n.find(' ') != std::string::npos ? "\"" + n + "\"" : n;
    for (auto& [f, tag] : refs) f->token = referenceToken(n, tag);
    return true;
}
} // namespace h7600
