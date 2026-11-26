#include "h7600/Sigfile.h"
#include "h7600/Protocol.h"

#include <cctype>
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
