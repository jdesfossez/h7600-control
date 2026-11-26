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
