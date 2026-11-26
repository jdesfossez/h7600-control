#include "Fixtures.h"
#include "h7600/Protocol.h"

#include <catch2/catch_test_macros.hpp>

using namespace h7600;

TEST_CASE("want requests match the bytes verified on the unit")
{
    CHECK(makeWant(1, Code::InfoWant) == Bytes{ 0xF0, 0x1C, 0x70, 0x01, 0x1A, 0xF7 });
    CHECK(makeWant(1, Code::ProgramWant) == Bytes{ 0xF0, 0x1C, 0x70, 0x01, 0x06, 0xF7 });
    CHECK(makeWant(1, Code::SigfileWantQuick) == Bytes{ 0xF0, 0x1C, 0x70, 0x01, 0x0B, 0xF7 });
    CHECK(makeWant(1, Code::SetupWant) == Bytes{ 0xF0, 0x1C, 0x70, 0x01, 0x07, 0xF7 });
    CHECK(makeWant(1, Code::ScreenWant) == Bytes{ 0xF0, 0x1C, 0x70, 0x01, 0x18, 0xF7 });
}

TEST_CASE("makeWant refuses codes that write the unit")
{
    for (auto c : { Code::SigfileDump, Code::SigfileDumpRemote, Code::ProgramDump, Code::SetupDump, Code::Keypress, Code::ParamSet })
        CHECK_THROWS(makeWant(1, c));
}

TEST_CASE("nibble round trip, high nibble first")
{
    Bytes in{ 0x00, 0x2A, 0xFF, 0x7E };
    auto n = nibblize(in);
    CHECK(n == Bytes{ 0x0, 0x0, 0x2, 0xA, 0xF, 0xF, 0x7, 0xE });
    CHECK(denibblize(n) == in);
}

TEST_CASE("bank change frame")
{
    CHECK(makeBankChange(1, false, 42) == Bytes{ 0xF0, 0x1C, 0x70, 0x01, 0x03, 0x00, 0x00, 0x02, 0x0A, 0xF7 });
    CHECK_THROWS(makeBankChange(1, false, 100));
}

TEST_CASE("parseFrame rejects non-Eventide data such as the stray 0xFF byte")
{
    CHECK_FALSE(parseFrame(Bytes{ 0xFF }));
    CHECK_FALSE(parseFrame(Bytes{ 0xF0, 0x7E, 0x70, 0x01, 0x06, 0xF7 }));
}

TEST_CASE("info dump is plain ASCII")
{
    auto f = loadFrame("info_dump_19.syx");
    CHECK(f.code == static_cast<std::uint8_t>(Code::InfoDump));
    auto text = payloadText(f);
    CHECK(text.find("Name: System ROM") != std::string::npos);
    CHECK(text.find("Revision: 5.22") != std::string::npos);
    CHECK(text.find("160 second sampler") != std::string::npos);
}

TEST_CASE("program and setup dumps are nibblized")
{
    auto p = loadFrame("program_dump_15.syx");
    CHECK(p.code == 0x15);
    CHECK(p.payload.size() == 10858);
    for (auto b : p.payload) REQUIRE(b < 0x10);
    auto bytes = denibblize(p.payload);
    std::string s(bytes.begin(), bytes.end());
    CHECK(s.find("H3000BreathingCanyon") != std::string::npos);

    auto s16 = loadFrame("setup_dump_16.syx");
    CHECK(s16.code == 0x16);
    CHECK(denibblize(s16.payload).size() == 1029);
}

TEST_CASE("parameter set: captured sequence-out message parses and rebuilds byte-exact")
{
    auto raw = loadFixture("param_set_3c_m_delay_76.syx");
    auto f = parseFrame(raw);
    REQUIRE(f);
    auto c = parseParamSet(*f);
    REQUIRE(c);
    CHECK(c->paramId == "1e0001");
    CHECK(c->value == "76");
    CHECK(c->moduleIndex() == 30);
    CHECK(makeParamSet(1, c->paramId, c->value) == raw);
    CHECK(paramIdForModule(3) == "030001");
    // Bytes sent and acknowledged (2E) by the unit to set m_delay to 50:
    CHECK(makeParamSet(1, "1e0001", "50") == Bytes{ 0xF0, 0x1C, 0x70, 0x01, 0x3C, 0x31, 0x65, 0x30, 0x30, 0x30, 0x31, 0x20, 0x35, 0x30, 0x20, 0xF7 });
}

TEST_CASE("parameter ack")
{
    auto f = loadFrame("ack_2e.syx");
    CHECK(f.code == static_cast<std::uint8_t>(Code::ParamAck));
    CHECK(f.payload.empty());
    CHECK(replyCodeFor(Code::ParamSet) == Code::ParamAck);
}

TEST_CASE("keypress echo decodes to key names")
{
    auto f = loadFrame("keypress_01_setup.syx");
    auto m = keyMask(f);
    REQUIRE(m);
    CHECK(*m == 0xFFFFF7FFu);
    CHECK(pressedKeys(*m) == std::vector<std::string>{ "SETUP" });
    CHECK(pressedKeys(0xFFFFFFFFu).empty());
    CHECK(pressedKeys(0xFBFFFFFFu) == std::vector<std::string>{ "SOFT1" });
    CHECK(pressedKeys(0xFFF7FFFFu) == std::vector<std::string>{ "PARAMETER" });
}
