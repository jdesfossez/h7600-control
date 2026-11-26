#include "h7600/PresetCatalog.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <string>

using namespace h7600;

static const Catalog& catalog()
{
    static const Catalog c = []
    {
        std::ifstream f(std::string(H7600_RESOURCES_DIR) + "/presets.json");
        std::string s{ std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>() };
        auto parsed = Catalog::fromJson(s);
        REQUIRE(parsed);
        return *parsed;
    }();
    return c;
}

static std::vector<int> numbers(const SearchResult& r, std::size_t n = 5)
{
    std::vector<int> out;
    for (std::size_t i = 0; i < r.presets.size() && i < n; ++i)
        out.push_back(r.presets[i]->number);
    return out;
}

TEST_CASE("catalog has 75 banks and 1114 presets")
{
    auto& c = catalog();
    CHECK(c.banks().size() == 75);
    CHECK(c.presets().size() == 1114);
    auto* amb = c.preset(4210);
    REQUIRE(amb);
    CHECK(amb->name == "Ambience");
    CHECK(amb->bank == 42);
    CHECK(c.preset(513)->name == "Ducked Delays");
    CHECK(c.preset(9999) == nullptr);
    CHECK(c.bank(42) != nullptr);
}

TEST_CASE("every family bank exists in the catalog")
{
    for (auto& f : families())
        for (int b : f.banks)
            CHECK(catalog().bank(b) != nullptr);
}

TEST_CASE("words and one-edit distance")
{
    CHECK(searchWords("H3000 Breathing-Canyon #2") == std::vector<std::string>{ "h3000", "breathing", "canyon", "#2" });
    CHECK(withinOneEdit("ambience", "ambiance"));
    CHECK(withinOneEdit("ambence", "ambience"));
    CHECK(withinOneEdit("ambiencee", "ambience"));
    CHECK_FALSE(withinOneEdit("ambeince", "ambience"));
    CHECK_FALSE(withinOneEdit("abc", "abcde"));
}

TEST_CASE("search ranking: number prefix first, then name prefix")
{
    PresetSearch s(catalog());
    UserData u;
    Query q;
    q.text = "4210";
    q.sort = SortOrder::BestMatch;
    auto r = s.run(q, u);
    REQUIRE(! r.presets.empty());
    CHECK(r.presets[0]->number == 4210);

    q.text = "ambience";
    r = s.run(q, u);
    REQUIRE(! r.presets.empty());
    CHECK(r.presets[0]->name.rfind("Ambience", 0) == 0); // name prefix beats word/substring
    CHECK(s.score(*catalog().preset(4210), "ambience") == 100);
    CHECK(s.score(*catalog().preset(4210), "4210") == 120);
}

TEST_CASE("typo tolerance on name words")
{
    PresetSearch s(catalog());
    UserData u;
    Query q;
    q.text = "ambiance"; // one substitution
    q.sort = SortOrder::BestMatch;
    auto r = s.run(q, u);
    auto ns = numbers(r, 50);
    CHECK(std::find(ns.begin(), ns.end(), 4210) != ns.end());
    CHECK(s.score(*catalog().preset(4210), "ambiance") == 15);
}

TEST_CASE("every token must match")
{
    PresetSearch s(catalog());
    UserData u;
    Query q;
    q.text = "ducked zzzz";
    CHECK(s.run(q, u).presets.empty());
}

TEST_CASE("scopes: bank, family, favorites, recent")
{
    PresetSearch s(catalog());
    UserData u;
    Query q;
    q.scope = { Scope::Kind::Bank, 42 };
    auto r = s.run(q, u);
    CHECK(static_cast<int>(r.presets.size()) == catalog().bankSize(42));
    for (auto* p : r.presets) CHECK(p->bank == 42);

    q.scope = { Scope::Kind::Family, 2 }; // Reverbs 42-50
    r = s.run(q, u);
    for (auto* p : r.presets) CHECK((*p->bank >= 42 && *p->bank <= 50));

    u.toggleFavorite(513);
    u.toggleFavorite(4210);
    q.scope = { Scope::Kind::Favorites, 0 };
    CHECK(numbers(s.run(q, u)) == std::vector<int>{ 513, 4210 });
    u.toggleFavorite(513);
    CHECK(numbers(s.run(q, u)) == std::vector<int>{ 4210 });

    u.noteLoaded(513);
    u.noteLoaded(4210);
    u.noteLoaded(513);
    q.scope = { Scope::Kind::Recent, 0 };
    CHECK(numbers(s.run(q, u)) == std::vector<int>{ 513, 4210 }); // most recent first
}

TEST_CASE("filters and live chip counts")
{
    PresetSearch s(catalog());
    UserData u;
    Query q;
    auto all = s.run(q, u);
    CHECK(all.presets.size() == 1114);
    const int reverbCount = all.effectCounts.at("Reverb");
    CHECK(reverbCount > 0);

    q.effects = { "Reverb" };
    auto r = s.run(q, u);
    CHECK(static_cast<int>(r.presets.size()) == reverbCount);
    for (auto* p : r.presets) CHECK(std::find(p->effects.begin(), p->effects.end(), "Reverb") != p->effects.end());
    // The effect row's own counts ignore the effect selection.
    CHECK(r.effectCounts.at("Reverb") == reverbCount);

    q.only96k = true;
    for (auto* p : s.run(q, u).presets) CHECK(p->maxRateKhz == 96);
    q.only96k = false;
    q.onlyStereoIn = true;
    for (auto* p : s.run(q, u).presets) CHECK(p->inputs == 2);
}

TEST_CASE("user tags are searchable after reindex")
{
    PresetSearch s(catalog());
    UserData u;
    u.addTag(513, "julienfave");
    s.reindex(u);
    Query q;
    q.text = "julienfave";
    CHECK(numbers(s.run(q, u)) == std::vector<int>{ 513 });
    u.removeTag(513, "julienfave");
    s.reindex(u);
    CHECK(s.run(q, u).presets.empty());
}

TEST_CASE("sort by name")
{
    PresetSearch s(catalog());
    UserData u;
    Query q;
    q.scope = { Scope::Kind::Bank, 5 };
    q.sort = SortOrder::Name;
    auto r = s.run(q, u);
    for (std::size_t i = 1; i < r.presets.size(); ++i)
        CHECK(r.presets[i - 1]->name <= r.presets[i]->name);
}
