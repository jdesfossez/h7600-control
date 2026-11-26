#include "h7600/PresetCatalog.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace h7600
{
using json = nlohmann::json;

bool Preset::tapTempo() const { return std::find(tempo.begin(), tempo.end(), "TT") != tempo.end(); }

const std::vector<Family>& families()
{
    static const std::vector<Family> f = {
        { "Start here", { 1, 2, 3, 4 } },
        { "Delays", { 5, 6, 7, 8 } },
        { "Reverbs", { 42, 43, 44, 45, 46, 47, 48, 49, 50 } },
        { "Pitch", { 36, 54, 55, 56, 57 } },
        { "Modulation", { 33, 35, 51 } },
        { "Dynamics & EQ", { 11, 12, 14 } },
        { "Guitar & instruments", { 9, 17, 18, 19, 20, 21 } },
        { "Multi & dual", { 10, 30, 66 } },
        { "Mix, master & post", { 15, 16, 23, 26, 38, 39 } },
        { "Voice", { 67, 68 } },
        { "Sound design", { 13, 22, 58, 59, 63 } },
        { "Samplers & synths", { 24, 34, 52, 53, 61 } },
        { "Px sound effects", { 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85 } },
        { "Utilities & more", { 62, 64, 65, 69, 70 } },
    };
    return f;
}

const std::vector<std::pair<std::string, char>>& effectTypes()
{
    static const std::vector<std::pair<std::string, char>> e = {
        { "Pitch", 'P' }, { "Reverb", 'R' }, { "Delay", 'D' }, { "EQ/Filter", 'E' },
        { "Modulation", 'M' }, { "Dynamics", 'Y' }, { "Sampler", 'S' }, { "Complex", 'C' },
    };
    return e;
}

const std::vector<std::string>& sourceTypes()
{
    static const std::vector<std::string> s = { "Vocals", "Guitar", "Drums", "Keyboard", "Surround", "Special FX" };
    return s;
}

namespace
{
std::optional<int> optInt(const json& j, const char* key)
{
    auto it = j.find(key);
    if (it == j.end() || it->is_null())
        return std::nullopt;
    return it->get<int>();
}

std::vector<std::string> strList(const json& j, const char* key)
{
    std::vector<std::string> out;
    if (auto it = j.find(key); it != j.end() && it->is_array())
        for (auto& v : *it)
            out.push_back(v.get<std::string>());
    return out;
}

std::string str(const json& j, const char* key)
{
    auto it = j.find(key);
    return it == j.end() || ! it->is_string() ? std::string{} : it->get<std::string>();
}

std::string lower(std::string_view s)
{
    std::string out(s);
    for (auto& c : out)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

bool startsWith(std::string_view s, std::string_view prefix) { return s.substr(0, prefix.size()) == prefix; }

bool anyStartsWith(const std::vector<std::string>& words, std::string_view t)
{
    return std::any_of(words.begin(), words.end(), [&](const std::string& w) { return startsWith(w, t); });
}

bool contains(const std::vector<std::string>& v, const std::string& s) { return std::find(v.begin(), v.end(), s) != v.end(); }
} // namespace

std::vector<std::string> searchWords(std::string_view s)
{
    std::vector<std::string> out;
    std::string cur;
    for (char ch : s)
    {
        auto c = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '#' || c == '+')
            cur.push_back(c);
        else if (! cur.empty())
            out.push_back(std::move(cur)), cur.clear();
    }
    if (! cur.empty())
        out.push_back(std::move(cur));
    return out;
}

bool withinOneEdit(std::string_view a, std::string_view b)
{
    const auto la = a.size(), lb = b.size();
    if ((la > lb ? la - lb : lb - la) > 1)
        return false;
    std::size_t i = 0, j = 0;
    int edits = 0;
    while (i < la && j < lb)
    {
        if (a[i] == b[j])
        {
            ++i, ++j;
            continue;
        }
        if (++edits > 1)
            return false;
        if (la > lb) ++i;
        else if (lb > la) ++j;
        else ++i, ++j;
    }
    return edits + static_cast<int>(la - i) + static_cast<int>(lb - j) <= 1;
}

std::optional<Catalog> Catalog::fromJson(std::string_view text)
{
    auto j = json::parse(text, nullptr, false);
    if (j.is_discarded() || ! j.contains("presets") || ! j.contains("banks"))
        return std::nullopt;
    Catalog c;
    for (auto& b : j["banks"])
        c.bankList.push_back({ b.value("number", 0), str(b, "name"), str(b, "description") });
    for (auto& p : j["presets"])
    {
        Preset x;
        x.number = p.value("number", 0);
        x.name = str(p, "name");
        x.bank = optInt(p, "bank");
        x.maxRateKhz = optInt(p, "max_rate_khz");
        x.inputs = optInt(p, "inputs");
        x.outputs = optInt(p, "outputs");
        x.effects = strList(p, "effects");
        x.sources = strList(p, "sources");
        x.tempo = strList(p, "tempo");
        x.tweak = str(p, "tweak");
        x.description = str(p, "description");
        x.indexOnly = p.value("index_only", false);
        c.presetList.push_back(std::move(x));
    }
    std::sort(c.presetList.begin(), c.presetList.end(), [](auto& a, auto& b) { return a.number < b.number; });
    return c;
}

const Bank* Catalog::bank(int number) const
{
    for (auto& b : bankList)
        if (b.number == number)
            return &b;
    return nullptr;
}

const Preset* Catalog::preset(int number) const
{
    auto it = std::lower_bound(presetList.begin(), presetList.end(), number, [](const Preset& p, int n) { return p.number < n; });
    return it != presetList.end() && it->number == number ? &*it : nullptr;
}

int Catalog::bankSize(int bankNumber) const
{
    return static_cast<int>(std::count_if(presetList.begin(), presetList.end(), [&](const Preset& p) { return p.bank == bankNumber; }));
}

std::vector<const Preset*> Catalog::variants(const Preset& p, std::size_t max) const
{
    std::vector<const Preset*> out;
    if (p.description.empty())
        return out;
    for (auto& q : presetList)
        if (&q != &p && q.bank == p.bank && q.description == p.description && out.size() < max)
            out.push_back(&q);
    return out;
}

void UserData::toggleFavorite(int n)
{
    if (! favorites.erase(n))
        favorites.insert(n);
}

void UserData::addTag(int n, const std::string& tag)
{
    if (tag.empty())
        return;
    auto& v = tags[n];
    if (! contains(v, tag))
        v.push_back(tag);
}

void UserData::removeTag(int n, const std::string& tag)
{
    auto it = tags.find(n);
    if (it == tags.end())
        return;
    auto& v = it->second;
    v.erase(std::remove(v.begin(), v.end(), tag), v.end());
    if (v.empty())
        tags.erase(it);
}

void UserData::noteLoaded(int n)
{
    recent.erase(std::remove(recent.begin(), recent.end(), n), recent.end());
    recent.insert(recent.begin(), n);
    if (recent.size() > 30)
        recent.resize(30);
}

PresetSearch::PresetSearch(const Catalog& c) : catalog(c) { reindex({}); }

std::vector<std::string> PresetSearch::tagsOf(const Preset& p, const UserData& user)
{
    std::vector<std::string> t = p.effects;
    t.insert(t.end(), p.sources.begin(), p.sources.end());
    if (p.tapTempo())
        t.push_back("Tap tempo");
    if (auto it = user.tags.find(p.number); it != user.tags.end())
        t.insert(t.end(), it->second.begin(), it->second.end());
    return t;
}

void PresetSearch::reindex(const UserData& user)
{
    entries.clear();
    entries.reserve(catalog.presets().size());
    for (auto& p : catalog.presets())
    {
        Entry e;
        e.preset = &p;
        e.name = lower(p.name);
        e.number = std::to_string(p.number);
        e.nameWords = searchWords(p.name);
        if (p.bank)
            if (auto* b = catalog.bank(*p.bank))
                e.bankWords = searchWords(b->name);
        e.descWords = searchWords(p.description + " " + p.tweak);
        for (auto& t : tagsOf(p, user))
            for (auto& w : searchWords(t))
                e.tagWords.push_back(w);
        entries.push_back(std::move(e));
    }
}

int PresetSearch::score(const Entry& e, const std::vector<std::string>& tokens) const
{
    int total = 0;
    for (auto& t : tokens)
    {
        int best = 0;
        if (startsWith(e.number, t)) best = 120;
        else if (startsWith(e.name, t)) best = 100;
        else if (anyStartsWith(e.nameWords, t)) best = 60;
        else if (e.name.find(t) != std::string::npos) best = 40;
        else if (anyStartsWith(e.tagWords, t)) best = 30;
        else if (anyStartsWith(e.bankWords, t)) best = 20;
        else if (anyStartsWith(e.descWords, t)) best = 8;
        else if (t.size() >= 4
                 && std::any_of(e.nameWords.begin(), e.nameWords.end(), [&](const std::string& w)
                                { return withinOneEdit(t, std::string_view(w).substr(0, t.size() + 1)) || withinOneEdit(t, w); }))
            best = 15;
        if (best == 0)
            return 0;
        total += best;
    }
    return total;
}

int PresetSearch::score(const Preset& p, std::string_view text) const
{
    auto tokens = searchWords(text);
    for (auto& e : entries)
        if (e.preset == &p)
            return tokens.empty() ? 1 : score(e, tokens);
    return 0;
}

bool PresetSearch::inScope(const Preset& p, const Scope& s, const UserData& user) const
{
    switch (s.kind)
    {
        case Scope::Kind::All: return true;
        case Scope::Kind::Favorites: return user.favorites.count(p.number) > 0;
        case Scope::Kind::Recent: return std::find(user.recent.begin(), user.recent.end(), p.number) != user.recent.end();
        case Scope::Kind::Family:
        {
            auto& fams = families();
            if (s.index < 0 || s.index >= static_cast<int>(fams.size()) || ! p.bank)
                return false;
            auto& b = fams[static_cast<std::size_t>(s.index)].banks;
            return std::find(b.begin(), b.end(), *p.bank) != b.end();
        }
        case Scope::Kind::Bank: return p.bank == s.index;
    }
    return true;
}

// skip: 0 none, 1 effects, 2 sources (so a chip row's counts ignore its own selection)
bool PresetSearch::passes(const Preset& p, const Query& q, int skip)
{
    if (skip != 1)
        for (auto& f : q.effects)
            if (! contains(p.effects, f))
                return false;
    if (skip != 2)
        for (auto& f : q.sources)
            if (! contains(p.sources, f))
                return false;
    if (q.only96k && p.maxRateKhz != 96)
        return false;
    if (q.onlyTempo && p.tempo.empty())
        return false;
    if (q.onlyStereoIn && p.inputs != 2)
        return false;
    return true;
}

SearchResult PresetSearch::run(const Query& q, const UserData& user) const
{
    const auto tokens = searchWords(q.text);
    std::vector<std::pair<const Preset*, int>> scored;
    for (auto& e : entries)
    {
        if (! inScope(*e.preset, q.scope, user))
            continue;
        int s = tokens.empty() ? 1 : score(e, tokens);
        if (s > 0)
            scored.emplace_back(e.preset, s);
    }

    SearchResult r;
    for (auto& [name, letter] : effectTypes())
        r.effectCounts[name] = 0;
    for (auto& s : sourceTypes())
        r.sourceCounts[s] = 0;
    for (auto& [p, s] : scored)
    {
        if (passes(*p, q, 1))
            for (auto& f : p->effects)
                if (r.effectCounts.count(f)) ++r.effectCounts[f];
        if (passes(*p, q, 2))
            for (auto& f : p->sources)
                if (r.sourceCounts.count(f)) ++r.sourceCounts[f];
    }

    std::vector<std::pair<const Preset*, int>> view;
    for (auto& x : scored)
        if (passes(*x.first, q, 0))
            view.push_back(x);

    auto byNumber = [](auto& a, auto& b) { return a.first->number < b.first->number; };
    if (q.sort == SortOrder::Name)
        std::stable_sort(view.begin(), view.end(), [](auto& a, auto& b) { return lower(a.first->name) < lower(b.first->name); });
    else if (q.sort == SortOrder::BestMatch && ! tokens.empty())
        std::stable_sort(view.begin(), view.end(), [](auto& a, auto& b)
                         { return a.second != b.second ? a.second > b.second : a.first->number < b.first->number; });
    else if (q.scope.kind == Scope::Kind::Recent)
    {
        auto pos = [&](int n) { return std::find(user.recent.begin(), user.recent.end(), n) - user.recent.begin(); };
        std::stable_sort(view.begin(), view.end(), [&](auto& a, auto& b) { return pos(a.first->number) < pos(b.first->number); });
    }
    else
        std::stable_sort(view.begin(), view.end(), byNumber);

    r.presets.reserve(view.size());
    for (auto& x : view)
        r.presets.push_back(x.first);
    return r;
}

} // namespace h7600
