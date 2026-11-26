#pragma once
// Factory preset catalog (resources/presets.json) and the browser's search /
// filter logic. Mirrors the approved HTML prototype so the plugin behaves the
// same: ranking number prefix > name prefix > name word > substring > tags >
// bank > description, with 1-edit typo tolerance on name words.

#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace h7600
{
struct Bank
{
    int number = 0;
    std::string name;
    std::string description;
};

struct Preset
{
    int number = 0;
    std::string name;
    std::optional<int> bank;
    std::optional<int> maxRateKhz;
    std::optional<int> inputs;
    std::optional<int> outputs;
    std::vector<std::string> effects; // "Pitch", "Reverb", ...
    std::vector<std::string> sources; // "Vocals", "Guitar", ...
    std::vector<std::string> tempo;   // e.g. "TT" (tap tempo)
    std::string tweak;
    std::string description;
    bool indexOnly = false;

    bool tapTempo() const;
};

struct Family
{
    std::string name;
    std::vector<int> banks;
};

/// The bank families shown in the sidebar.
const std::vector<Family>& families();

/// Effect names in display order, with their one-letter badge.
const std::vector<std::pair<std::string, char>>& effectTypes();
/// Source names in display order.
const std::vector<std::string>& sourceTypes();

class Catalog
{
public:
    static std::optional<Catalog> fromJson(std::string_view json);

    const std::vector<Bank>& banks() const { return bankList; }
    const std::vector<Preset>& presets() const { return presetList; }
    const Bank* bank(int number) const;
    const Preset* preset(int number) const;
    int bankSize(int bankNumber) const;

    /// Presets in the same bank sharing the same description (variants).
    std::vector<const Preset*> variants(const Preset& p, std::size_t max = 8) const;

private:
    std::vector<Bank> bankList;
    std::vector<Preset> presetList;
};

/// Favorites, user tags and recents: kept on the computer, never on the unit.
struct UserData
{
    std::set<int> favorites;
    std::map<int, std::vector<std::string>> tags;
    std::vector<int> recent; // most recent first, at most 30

    void toggleFavorite(int number);
    void addTag(int number, const std::string& tag);
    void removeTag(int number, const std::string& tag);
    void noteLoaded(int number);
};

struct Scope
{
    enum class Kind { All, Favorites, Recent, Family, Bank } kind = Kind::All;
    int index = 0; // family index or bank number
    bool operator==(const Scope&) const = default;
};

enum class SortOrder { Number, Name, BestMatch };

struct Query
{
    std::string text;
    Scope scope;
    std::set<std::string> effects;
    std::set<std::string> sources;
    bool only96k = false;
    bool onlyStereoIn = false;
    bool onlyTempo = false;
    SortOrder sort = SortOrder::Number;
};

struct SearchResult
{
    std::vector<const Preset*> presets;
    std::map<std::string, int> effectCounts; // live counts for the chips
    std::map<std::string, int> sourceCounts;
};

class PresetSearch
{
public:
    explicit PresetSearch(const Catalog& catalog);

    /// Rebuilds the token index (call after user tags change).
    void reindex(const UserData& user);

    SearchResult run(const Query& q, const UserData& user) const;

    /// Ranking score of one preset for a query (0 = no match).
    int score(const Preset& p, std::string_view text) const;

    /// Auto tags shown for a preset: effects, sources, "Tap tempo", user tags.
    static std::vector<std::string> tagsOf(const Preset& p, const UserData& user);

private:
    struct Entry
    {
        const Preset* preset;
        std::string name;
        std::string number;
        std::vector<std::string> nameWords, bankWords, descWords, tagWords;
    };
    int score(const Entry& e, const std::vector<std::string>& tokens) const;
    bool inScope(const Preset& p, const Scope& s, const UserData& user) const;
    static bool passes(const Preset& p, const Query& q, int skip);

    const Catalog& catalog;
    std::vector<Entry> entries;
};

/// Lower-cased words split on anything but [a-z0-9#+].
std::vector<std::string> searchWords(std::string_view s);
/// True if a and b differ by at most one insertion, deletion or substitution.
bool withinOneEdit(std::string_view a, std::string_view b);

} // namespace h7600
