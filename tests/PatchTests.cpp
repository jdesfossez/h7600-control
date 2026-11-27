#include "External.h"
#include "h7600/ModuleDb.h"
#include "h7600/Patch.h"

#include <catch2/catch_test_macros.hpp>

using namespace h7600;

namespace
{
std::string handmade(const std::string& name)
{
    auto t = readText(std::filesystem::path(H7600_FIXTURES_DIR) / "handmade" / name);
    REQUIRE(t);
    return *t;
}

const ModuleDb& testDb()
{
    static ModuleDb db = [] {
        std::string err;
        auto d = ModuleDb::parse(handmade("test_db.txt"), &err);
        INFO(err);
        REQUIRE(d);
        return *d;
    }();
    return db;
}

Patch testPatch()
{
    std::string err;
    auto p = parsePatch(handmade("test_gain.sig"), testDb(), &err);
    INFO(err);
    REQUIRE(p);
    return *p;
}

bool hasIssue(const std::vector<PatchIssue>& v, const std::string& field, const std::string& text)
{
    for (auto& i : v)
        if (i.field == field && i.message.find(text) != std::string::npos) return true;
    return false;
}
} // namespace

TEST_CASE("Module database: records, lookup, classes")
{
    auto& db = testDb();
    CHECK(db.modules().size() == 5);
    auto* g = db.find("GAIN");
    REQUIRE(g);
    CHECK(g->name == "gain");
    CHECK(g->className == "MIXER");
    CHECK(g->records.front().type() == "OPERATOR");
    CHECK(g->records.back().type() == "OPERATOREND");
    CHECK(g->records[4].get("DESCRIPTION") == "level dB");
    CHECK(db.classes() == std::vector<std::string> { "SYSTEM", "INTERFACE", "MIXER" });
    CHECK_FALSE(db.find("nosuch"));
}

TEST_CASE("Module database: rejects malformed text")
{
    std::string err;
    CHECK_FALSE(ModuleDb::parse("", &err));
    CHECK_FALSE(ModuleDb::parse("{\r\n   {\r\n\tTYPE:OPERATOR\r\n", &err));
    CHECK(err.find("truncated") != std::string::npos);
    CHECK_FALSE(ModuleDb::parse("{\r\n   {\r\n\tTYPE:CONTROL\r\n\t}\r\n}\r\n", &err));
    CHECK_FALSE(ModuleDb::parse("hello\r\n", &err));
}

TEST_CASE("Patch: parse names every argument and output")
{
    auto p = testPatch();
    REQUIRE(p.modules.size() == 6);
    CHECK(p.programName() == "Test Gain");
    auto& head = p.modules[0];
    CHECK(head.name() == "adc");
    REQUIRE(head.field("out2"));
    CHECK(head.field("out2")->token == "g2-out");
    CHECK(head.field("obj1")->token == "page-obj");
    CHECK(head.output("in1"));
    CHECK(head.output("in2"));
    CHECK(head.output("nullobj")->kind == PortKind::UserObject);
    auto& knob = p.modules[2];
    CHECK(knob.field("label")->text() == "level: %4.1f dB");
    CHECK(knob.field("default")->number() == -6);
    CHECK(knob.output("out")->kind == PortKind::Control);
}

TEST_CASE("Patch: round trip is byte-identical")
{
    auto text = handmade("test_gain.sig");
    CHECK(serialize(testPatch()) == text);
}

TEST_CASE("Patch: connections and feedback")
{
    auto p = testPatch();
    auto cs = p.connections();
    int audio = 0, control = 0, obj = 0;
    for (auto& c : cs)
    {
        if (c.kind == PortKind::Audio) ++audio;
        if (c.kind == PortKind::Control) ++control;
        if (c.kind == PortKind::UserObject) ++obj;
        if (c.from == 0) CHECK_FALSE(c.feedback);
    }
    CHECK(audio == 4);   // adc-in1, adc-in2 into the gains; g1/g2 into the HEAD's outputs
    CHECK(control == 2); // lvl into both gains
    CHECK(obj == 2);     // page into the HEAD, lvl into the page
    int fb = 0;
    for (auto& c : cs) fb += c.feedback;
    CHECK(fb == 4); // HEAD outputs and its menu, all defined later in the file

    p.modules[3].field("in")->token = "g2-out"; // g1 now fed by g2, which comes after it
    bool found = false;
    for (auto& c : p.connections())
        if (c.to == 3 && c.from == 4) found = c.feedback;
    CHECK(found);
}

TEST_CASE("Patch: a valid patch has no issues")
{
    auto issues = validate(testPatch(), testDb());
    for (auto& i : issues) UNSCOPED_INFO(i.field << ": " << i.message);
    CHECK(issues.empty());
}

TEST_CASE("Patch: validation errors")
{
    auto& db = testDb();
    SECTION("wrong connection kind")
    {
        auto p = testPatch();
        p.modules[3].field("in")->token = "lvl-out";
        CHECK(hasIssue(validate(p, db), "in", "needs an audio connection"));
    }
    SECTION("unknown output")
    {
        auto p = testPatch();
        p.modules[3].field("level")->token = "nosuch-out";
        CHECK(hasIssue(validate(p, db), "level", "not an output"));
    }
    SECTION("constant on an audio input")
    {
        auto p = testPatch();
        p.modules[3].field("in")->token = "0.5";
        CHECK(hasIssue(validate(p, db), "in", "not an output"));
    }
    SECTION("constant out of range, including a range that names another field")
    {
        auto p = testPatch();
        p.modules[3].field("level")->token = "30";
        CHECK(hasIssue(validate(p, db), "level", "out of range"));
        p.modules[2].field("default")->token = "5"; // above the knob's own max (0)
        CHECK(hasIssue(validate(p, db), "default", "out of range"));
    }
    SECTION("names")
    {
        auto p = testPatch();
        p.modules[4].fields[0].token = "g1";
        CHECK(hasIssue(validate(p, db), "name", "used twice"));
        p.modules[4].fields[0].token = "a_name_that_is_far_too_long";
        CHECK(hasIssue(validate(p, db), "name", "1 to 18"));
        p.modules[4].fields[0].token = "bad-name";
        CHECK(hasIssue(validate(p, db), "name", "'-'"));
    }
    SECTION("two in, two out")
    {
        auto p = testPatch();
        REQUIRE(setField(p.modules[0], db, "noutputs", "3"));
        CHECK(p.modules[0].field("out3"));
        CHECK(hasIssue(validate(p, db), "noutputs", "two inputs and two outputs"));
    }
    SECTION("HEAD first, TAIL last")
    {
        auto p = testPatch();
        std::swap(p.modules[0], p.modules[1]);
        std::swap(p.modules[4], p.modules[5]);
        auto v = validate(p, db);
        CHECK(hasIssue(v, "", "first module must be a HEAD"));
        CHECK(hasIssue(v, "", "last module must be a TAIL"));
    }
    SECTION("wrong argument count")
    {
        auto p = testPatch();
        p.modules[3].fields.pop_back();
        CHECK(hasIssue(validate(p, db), "", "do not match the database"));
    }
}

TEST_CASE("Patch: parse rejects unknown types and wrong argument counts")
{
    std::string err;
    CHECK_FALSE(parsePatch("\r\nHEADM adc 2 2 adc-in1 adc-in2 \"x\" \"\" 0\r\nFOO f\r\nTAIL t\r\n", testDb(), &err));
    CHECK(err.find("unknown module type FOO") != std::string::npos);
    CHECK_FALSE(parsePatch("\r\nHEADM adc 2 2 adc-in1 adc-in2 \"x\" \"\" 0\r\nGAIN g adc-in1\r\nTAIL t\r\n", testDb(), &err));
    CHECK(err.find("missing argument level") != std::string::npos);
    CHECK_FALSE(parsePatch("\r\nHEADM adc 2 2 adc-in1 adc-in2 \"x\" \"\" 0 extra\r\nTAIL t\r\n", testDb(), &err));
    CHECK(err.find("more arguments") != std::string::npos);
}

TEST_CASE("Patch: editing")
{
    auto& db = testDb();
    auto p = testPatch();

    SECTION("add a module with defaults before TAIL, patch it in")
    {
        auto m = makeModule(db, "gain", uniqueName(p, "g"), "adc");
        REQUIRE(m);
        CHECK(m->type == "GAIN");
        CHECK(m->name() == "g3");
        CHECK(m->line() == "GAIN g3 adc-null 0");
        auto idx = insertModule(p, *m);
        CHECK(idx == 5);
        CHECK(p.modules.back().type == "TAIL");
        p.modules[idx].field("in")->token = "g1-out";
        p.modules[0].field("out1")->token = "g3-out";
        CHECK(validate(p, db).empty());
    }
    SECTION("remove a module unpatches what used it")
    {
        removeModule(p, db, 3); // g1
        CHECK(p.modules[0].field("out1")->token == "adc-null");
        CHECK(validate(p, db).empty());
    }
    SECTION("rename updates references")
    {
        REQUIRE(renameModule(p, 2, "level"));
        CHECK(p.modules[3].field("level")->token == "level-out");
        CHECK(p.modules[1].field("obj1")->token == "level-obj");
        CHECK_FALSE(renameModule(p, 2, "g1"));
        CHECK(validate(p, db).empty());
    }
    SECTION("changing a repeat count re-expands, keeping values")
    {
        REQUIRE(setField(p.modules[1], db, "nentries", "2"));
        CHECK(p.modules[1].field("obj1")->token == "lvl-obj");
        CHECK(p.modules[1].field("obj2")->token == "adc-nullobj");
        REQUIRE(setField(p.modules[1], db, "nentries", "0"));
        CHECK_FALSE(p.modules[1].field("obj1"));
        CHECK(validate(p, db).empty());
    }
    SECTION("string tokens are quoted, quotes removed")
    {
        auto& f = *p.modules[0].field("obj_description");
        f.token = fieldToken(f, "My \"best\" patch");
        CHECK(f.token == "\"My best patch\"");
        CHECK(p.programName() == "My best patch");
    }
}

// ---- With the real database and factory sigfiles (not in the repository) --------------------

TEST_CASE("Full database: every factory sigfile round-trips and validates")
{
    auto text = fullDatabaseText();
    if (! text) SKIP("module database not available (set H7600_SIGDBASE)");
    std::string err;
    auto db = ModuleDb::parse(*text, &err);
    INFO(err);
    REQUIRE(db);
    CHECK(db->modules().size() == 225);
    CHECK(db->classes().size() == 16);

    auto files = externalSigfiles();
    if (files.empty()) SKIP("no factory sigfiles (set H7600_SIGFILES_DIR)");
    for (auto& f : files)
    {
        INFO(f.filename().string());
        auto t = sigfileText(f);
        REQUIRE(t);
        auto p = parsePatch(*t, *db, &err);
        INFO(err);
        REQUIRE(p);
        CHECK(serialize(*p) == *t);
        for (auto& i : validate(*p, *db))
            if (i.severity == PatchIssue::Severity::Error)
                FAIL_CHECK(p->modules[static_cast<std::size_t>(std::max(i.module, 0))].name() << "." << i.field << ": " << i.message);
    }
}

TEST_CASE("Full database: every module can be created and parsed back")
{
    auto text = fullDatabaseText();
    if (! text) SKIP("module database not available (set H7600_SIGDBASE)");
    auto db = ModuleDb::parse(*text);
    REQUIRE(db);
    for (auto& dm : db->modules())
    {
        INFO(dm.name);
        auto m = makeModule(*db, dm.name, "x1");
        REQUIRE(m);
        Patch p;
        p.prefix = "\r\n";
        p.modules.push_back(*makeModule(*db, "headm", "adc"));
        p.modules.push_back(*m);
        p.modules.push_back(*makeModule(*db, "tail", "t"));
        std::string err;
        auto back = parsePatch(serialize(p), *db, &err);
        INFO(err);
        REQUIRE(back);
        CHECK(serialize(*back) == serialize(p));
    }
}

TEST_CASE("Patch: control targets and the knobs that steer a module")
{
    auto p = testPatch();
    auto t = controlTargets(p, 2); // lvl
    REQUIRE(t.size() == 2);
    CHECK(p.modules[t[0].module].name() == "g1");
    CHECK(p.modules[t[0].module].fields[t[0].field].name == "level");
    CHECK(t[0].via.empty());
    CHECK(knobsSteering(p, 3) == std::vector<std::size_t> { 2 });
    CHECK(knobsSteering(p, 1).empty());
    auto* rec = fieldRecord(testDb(), p.modules[3], p.modules[3].fields[t[0].field]);
    REQUIRE(rec);
    CHECK(rec->get("DESCRIPTION") == "level dB");
}

TEST_CASE("Full database: knobs reach their targets through control math")
{
    auto text = fullDatabaseText();
    if (! text) SKIP("module database not available (set H7600_SIGDBASE)");
    auto f = std::filesystem::path(H7600_FIXTURES_DIR) / "sigfile_quick_0a_ducked_delays.syx";
    if (! std::filesystem::exists(f)) SKIP("Ducked Delays capture not available");
    auto db = ModuleDb::parse(*text);
    REQUIRE(db);
    auto p = parsePatch(*sigfileText(f), *db);
    REQUIRE(p);
    // TEXTKNOB dly_1 -> C_TABLE table1 -> C_MULTIPLY math1 -> SKNOB3 dly1 (merge in) -> MODDELAY delay1
    auto t = controlTargets(*p, *p->find("dly_1"));
    REQUIRE(t.size() == 1);
    CHECK(p->modules[t[0].module].name() == "delay1");
    CHECK(t[0].via.size() == 3);
    auto steer = knobsSteering(*p, *p->find("delay1"));
    std::vector<std::string> names;
    for (auto k : steer) names.push_back(p->modules[k].name());
    CHECK(names == std::vector<std::string> { "dly_1", "dly1", "gliderate" });
}
