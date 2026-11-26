#include "h7600/Sigfile.h"
#include "h7600/Protocol.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <map>
#include <set>
#include <cstdio>
#include <cstdlib>
#include <utility>

namespace h7600
{
namespace
{
struct Token
{
    std::string text;
    bool quoted = false;
    bool lineStart = false;
};

std::vector<Token> tokenize(std::string_view s)
{
    std::vector<Token> out;
    bool atLineStart = true;
    std::size_t i = 0;
    while (i < s.size())
    {
        char c = s[i];
        if (c == '\n' || c == '\r')
        {
            atLineStart = true;
            ++i;
            continue;
        }
        if (c == ' ' || c == '\t')
        {
            ++i;
            continue;
        }
        if (c == ';') // comment to end of line
        {
            while (i < s.size() && s[i] != '\n')
                ++i;
            continue;
        }
        Token t;
        t.lineStart = atLineStart;
        atLineStart = false;
        if (c == '"')
        {
            t.quoted = true;
            ++i;
            while (i < s.size() && s[i] != '"')
                t.text.push_back(s[i++]);
            ++i; // closing quote
        }
        else
        {
            while (i < s.size() && ! std::isspace(static_cast<unsigned char>(s[i])))
                t.text.push_back(s[i++]);
        }
        out.push_back(std::move(t));
    }
    return out;
}

bool isOperatorWord(const Token& t)
{
    if (t.quoted || t.text.empty() || ! std::isupper(static_cast<unsigned char>(t.text[0])))
        return false;
    for (char c : t.text)
        if (! (std::isupper(static_cast<unsigned char>(c)) || std::isdigit(static_cast<unsigned char>(c)) || c == '_'))
            return false;
    return true;
}

double num(const std::string& s) { return std::strtod(s.c_str(), nullptr); }

std::optional<Knob> knobFrom(const SigEntry& e)
{
    const auto& t = e.tokens;
    Knob k;
    k.moduleIndex = e.moduleIndex;
    if ((e.type == "KNOB" || e.type == "PERCENTKNOB") && t.size() >= 7)
    {
        k.kind = e.type == "KNOB" ? KnobKind::Knob : KnobKind::PercentKnob;
        k.name = t[0];
        k.format = t[1];
        k.unit = t[2];
        k.min = num(t[3]);
        k.max = num(t[4]);
        k.step = num(t[5]);
        k.value = num(t[6]);
        return k;
    }
    if (e.type == "TEXTKNOB" && t.size() >= 5)
    {
        // TEXTKNOB name "fmt" "unit" <count> <value> "opt0" "opt1" ...
        k.kind = KnobKind::TextKnob;
        k.name = t[0];
        k.format = t[1];
        k.unit = t[2];
        auto count = static_cast<int>(num(t[3]));
        k.value = num(t[4]);
        for (std::size_t i = 5; i < t.size() && static_cast<int>(k.options.size()) < count; ++i)
            k.options.push_back(t[i]);
        k.min = 0;
        k.max = count > 0 ? count - 1 : 0;
        k.step = 1;
        return k;
    }
    return std::nullopt;
}
} // namespace

std::string Knob::paramId() const { return paramIdForModule(moduleIndex); }

std::string Knob::label() const
{
    auto end = format.find_first_of(":%");
    std::string s = format.substr(0, end);
    while (! s.empty() && s.back() == ' ')
        s.pop_back();
    return s;
}

std::string Knob::displayValue() const
{
    if (kind == KnobKind::TextKnob)
    {
        auto idx = static_cast<std::size_t>(value);
        std::string opt = idx < options.size() ? options[idx] : std::string{};
        auto pos = format.find("%s");
        return pos == std::string::npos ? opt : format.substr(0, pos) + opt + format.substr(pos + 2);
    }
    char buf[128];
    std::snprintf(buf, sizeof buf, format.c_str(), value);
    return buf;
}

const Knob* Sigfile::findKnob(std::string_view n) const
{
    for (auto& k : knobs)
        if (k.name == n)
            return &k;
    return nullptr;
}

const Knob* Sigfile::findKnobByModule(int idx) const
{
    for (auto& k : knobs)
        if (k.moduleIndex == idx)
            return &k;
    return nullptr;
}

std::vector<MenuPage> Sigfile::pages() const
{
    std::map<std::string, const SigEntry*> byName;
    for (auto& e : entries)
        byName[e.name()] = &e;
    std::map<std::string, std::size_t> knobByName;
    for (std::size_t i = 0; i < knobs.size(); ++i)
        knobByName[knobs[i].name] = i;

    auto objName = [](const std::string& tok) -> std::string
    {
        const std::string suffix = "-obj";
        if (tok.size() > suffix.size() && tok.compare(tok.size() - suffix.size(), suffix.size(), suffix) == 0)
            return tok.substr(0, tok.size() - suffix.size());
        return {};
    };

    std::vector<MenuPage> out;
    std::set<std::string> visited;
    std::set<std::size_t> placed;
    std::function<void(const SigEntry&)> visit = [&](const SigEntry& menu)
    {
        if (! visited.insert(menu.name()).second)
            return;
        MenuPage page;
        page.name = menu.name();
        page.title = menu.tokens.size() > 1 ? menu.tokens[1] : menu.name();
        page.tab = menu.tokens.size() > 2 ? menu.tokens[2] : page.title;
        while (! page.tab.empty() && page.tab.front() == ' ') page.tab.erase(page.tab.begin());
        std::vector<const SigEntry*> children;
        for (std::size_t i = 4; i < menu.tokens.size(); ++i)
        {
            auto n = objName(menu.tokens[i]);
            if (n.empty()) continue;
            if (auto k = knobByName.find(n); k != knobByName.end())
            {
                if (placed.insert(k->second).second) page.knobs.push_back(k->second);
            }
            else if (auto e = byName.find(n); e != byName.end() && e->second->type == "MENUPAGE")
                children.push_back(e->second);
        }
        if (! page.knobs.empty()) out.push_back(std::move(page));
        for (auto* c : children) visit(*c);
    };

    if (! entries.empty())
        for (auto& tok : entries.front().tokens)
            if (auto n = objName(tok); ! n.empty())
                if (auto e = byName.find(n); e != byName.end() && e->second->type == "MENUPAGE")
                    visit(*e->second);
    // Menus not reachable from HEAD (unusual) still get a page.
    for (auto& e : entries)
        if (e.type == "MENUPAGE") visit(e);

    MenuPage other { "other", "other params", "other", {} };
    for (std::size_t i = 0; i < knobs.size(); ++i)
        if (! placed.count(i)) other.knobs.push_back(i);
    if (! other.knobs.empty()) out.push_back(std::move(other));
    return out;
}

double quantize(const Knob& k, double v)
{
    double lo = std::min(k.min, k.max), hi = std::max(k.min, k.max);
    v = std::clamp(v, lo, hi);
    if (k.step > 0)
        v = k.min + std::round((v - k.min) / k.step) * k.step;
    return std::clamp(v, lo, hi);
}

std::string valueText(const Knob& k, double v)
{
    v = quantize(k, v);
    int decimals = 0;
    for (double s = k.step; decimals < 6 && s > 0 && std::fabs(s - std::round(s)) > 1e-9; s *= 10)
        ++decimals;
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*f", decimals, v);
    std::string out = buf;
    if (out == "-0") out = "0";
    return out;
}

std::optional<Sigfile> parseSigfile(std::string_view text)
{
    auto toks = tokenize(text);
    Sigfile sf;
    for (auto& t : toks)
    {
        if (t.lineStart && isOperatorWord(t))
        {
            SigEntry e;
            e.type = t.text;
            e.moduleIndex = static_cast<int>(sf.entries.size()) + 1;
            sf.entries.push_back(std::move(e));
        }
        else if (! sf.entries.empty())
        {
            sf.entries.back().tokens.push_back(t.text);
        }
    }
    if (sf.entries.empty() || sf.entries.front().type.rfind("HEAD", 0) != 0)
        return std::nullopt;

    // HEAD* <name> <args...> "<program name>" ...: the first quoted token.
    for (auto& t : toks)
        if (t.quoted)
        {
            sf.programName = t.text;
            break;
        }
    if (sf.entries.back().type == "TAIL" && ! sf.entries.back().tokens.empty())
        sf.author = sf.entries.back().tokens[0];

    for (auto& e : sf.entries)
        if (auto k = knobFrom(e))
            sf.knobs.push_back(std::move(*k));
    return sf;
}

} // namespace h7600
