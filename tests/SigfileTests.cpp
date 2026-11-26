#include "Fixtures.h"
#include "h7600/Sigfile.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace h7600;

static Sigfile load(const std::string& name)
{
    auto f = loadFrame(name);
    REQUIRE(f.code == static_cast<std::uint8_t>(Code::SigfileDumpRemote));
    auto sf = parseSigfile(payloadText(f));
    REQUIRE(sf);
    return *sf;
}

// Captured while trim was temporarily -12 and m_delay 75 during the 3C test.
TEST_CASE("Breathing Canyon sigfile")
{
    auto sf = load("sigfile_quick_0a_breathing_canyon.syx");
    CHECK(sf.programName == "H3000BreathingCanyonEmpty");
    CHECK(sf.author == "italo deangelis02");
    CHECK(sf.entries.size() == 57);

    auto* dly = sf.findKnob("Km_dlys");
    REQUIRE(dly);
    CHECK(dly->kind == KnobKind::PercentKnob);
    CHECK(dly->moduleIndex == 30);
    CHECK(dly->paramId() == "1e0001"); // matches the unit's own sequence-out message
    CHECK(dly->min == 0);
    CHECK(dly->max == 100);
    CHECK(dly->value == 75);
    CHECK(dly->label() == "m_delay");
    CHECK(dly->displayValue() == "m_delay:  75 %");

    auto* trim = sf.findKnob("trim");
    REQUIRE(trim);
    CHECK(trim->paramId() == "030001"); // verified on the unit: set to -12 and back
    CHECK(trim->min == -20);
    CHECK(trim->value == -12);

    auto* r1 = sf.findKnob("Krate1");
    REQUIRE(r1);
    CHECK(r1->step == Catch::Approx(0.01));
    CHECK(r1->value == Catch::Approx(0.66));

    CHECK(sf.knobs.size() == 26);
}

TEST_CASE("Ducked Delays sigfile with TEXTKNOBs")
{
    auto sf = load("sigfile_quick_0a_ducked_delays.syx");
    CHECK(sf.programName == "Ducked Delays");
    CHECK(sf.author == "sdg");
    auto* t = sf.findKnob("dly_1");
    REQUIRE(t);
    CHECK(t->kind == KnobKind::TextKnob);
    CHECK(t->options.size() == 28);
    CHECK(t->displayValue() == "t_delay1: 1/8 note"); // as shown on the LCD
    auto* lvl = sf.findKnob("v");
    REQUIRE(lvl);
    CHECK(lvl->value == -3);
}

TEST_CASE("multi-line entries and comments (DSP4000 tech note style)")
{
    const char* text =
        "HEAD Hed LMix-out RMix-out \"Universal Matrix\" \"Matrix\" 2 Setup-obj info-obj\n"
        "MENUPAGE Setup \"Operation\" \" Setup\" 7 no-obj MatName-obj FactName-obj no-obj no-obj\n"
        "MatKnob-obj WideKnob-obj\n"
        "TEXTKNOB MatKnob \"%s\" \"shnam\" 3 1 \"Normal L/R\" \"Reverse R/L\"\n"
        "\"Mono sum\" ; a comment\n"
        "KNOB WideKnob \"%2.1f (1 is normal)\" \"shnam\" 0.0000 2.0000 0.1000 1.0000\n"
        "TAIL JayRose\n";
    auto sf = parseSigfile(text);
    REQUIRE(sf);
    CHECK(sf->programName == "Universal Matrix");
    CHECK(sf->entries.size() == 5);
    CHECK(sf->entries[1].tokens.size() == 11);
    auto* m = sf->findKnob("MatKnob");
    REQUIRE(m);
    CHECK(m->options == std::vector<std::string>{ "Normal L/R", "Reverse R/L", "Mono sum" });
    CHECK(m->displayValue() == "Reverse R/L");
    CHECK(sf->findKnob("WideKnob")->moduleIndex == 4);
}

TEST_CASE("non-sigfile text is rejected")
{
    CHECK_FALSE(parseSigfile("hello world"));
}

TEST_CASE("menu pages follow the HEAD menus, depth first")
{
    auto sf = load("sigfile_quick_0a_breathing_canyon.syx");
    auto pages = sf.pages();
    REQUIRE(pages.size() == 4);
    CHECK(pages[0].title == "master params");
    CHECK(pages[0].tab == "main");
    std::vector<std::string> names;
    for (auto i : pages[0].knobs) names.push_back(sf.knobs[i].name);
    CHECK(names == std::vector<std::string>{ "Km_dlys", "Km_rates", "Km_depths", "Km_fdbck", "kdrylev", "kwetlev", "trim" });
    CHECK(pages[1].tab == "rates");
    CHECK(pages[1].knobs.size() == 6);
    CHECK(pages[2].tab == "depths");
    CHECK(pages[3].tab == "delays");
    // Every knob is placed exactly once.
    std::size_t total = 0;
    for (auto& p : pages) total += p.knobs.size();
    CHECK(total == sf.knobs.size());
}

TEST_CASE("Ducked Delays pages include TEXTKNOBs")
{
    auto sf = load("sigfile_quick_0a_ducked_delays.syx");
    auto pages = sf.pages();
    REQUIRE(pages.size() >= 2);
    CHECK(pages[0].tab == "delay");
    CHECK(pages[1].tab == "ducking");
    bool hasText = false;
    for (auto i : pages[0].knobs) hasText |= sf.knobs[i].kind == KnobKind::TextKnob;
    CHECK(hasText);
}

TEST_CASE("value text and quantize")
{
    auto sf = load("sigfile_quick_0a_breathing_canyon.syx");
    auto& rate = *sf.findKnob("Krate1");
    CHECK(valueText(rate, 0.661) == "0.66");
    CHECK(valueText(rate, 500) == "100.00");
    auto& dly = *sf.findKnob("Km_dlys");
    CHECK(valueText(dly, 49.6) == "50"); // matches the bytes the unit accepted
    auto& trim = *sf.findKnob("trim");
    CHECK(valueText(trim, -12.2) == "-12");
    CHECK(valueText(trim, 5) == "0");
    CHECK(quantize(trim, -25) == -20);
}
