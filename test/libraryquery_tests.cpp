#include <doctest/doctest.h>

#include <libraryquery.hpp>

namespace {

ProgramFacts program(std::string name, std::string release = {}, std::vector<std::string> authors = {})
{
    ProgramFacts facts;
    facts.name = std::move(name);
    facts.release = std::move(release);
    facts.authors = std::move(authors);
    facts.present = true;
    return facts;
}

bool matches(std::string_view query, const ProgramFacts& facts)
{
    return LibraryQuery::parse(query).matches(facts);
}

}

TEST_CASE("Library query: plain words search like before")
{
    const auto invaders = program("Space Invaders", "1978", {"David Winter"});
    CHECK(matches("", invaders));
    CHECK(matches("space", invaders));
    CHECK(matches("SPACE inv", invaders));
    CHECK(matches("winter", invaders));
    CHECK_FALSE(matches("invaders space", invaders));  // adjacent words are one phrase
    CHECK(matches("invaders and space", invaders));
    CHECK(matches("\"space invaders\"", invaders));
    CHECK(matches("pong or invaders", invaders));
    CHECK_FALSE(matches("-invaders", invaders));
    CHECK_FALSE(matches("not invaders", invaders));
    CHECK(matches("not (pong or tetris)", invaders));
}

TEST_CASE("Library query: release dates as ranges")
{
    const auto early = program("Early", "1978");
    const auto decade = program("Decade", "197x");
    const auto month = program("Month", "1990-05");
    const auto unknown = program("Unknown");
    CHECK(matches("release < 1980", early));
    CHECK(matches("release < 1980", decade));
    CHECK_FALSE(matches("release < 1979", decade));  // 197x might be 1979
    CHECK_FALSE(matches("release < 1980", unknown));
    CHECK(matches("release 1990..1999", month));
    CHECK(matches("release = 1990..1999", month));
    CHECK_FALSE(matches("release 1991..1999", month));
    CHECK(matches("release ..1979", early));
    CHECK(matches("release 1990..", month));
    CHECK(matches("release = 1990", month));
    CHECK(matches("release = 1990-05", month));
    CHECK_FALSE(matches("release = 1990-06", month));
    CHECK(matches("release >= 1990-05-01", program("Day", "1990-05-01")));
    CHECK(matches("release > 1977", early));
    CHECK(matches("release = none", unknown));
    CHECK(matches("release != none", early));
    CHECK(matches("year ~ 97", decade));
}

TEST_CASE("Library query: fields")
{
    auto facts = program("Octovore", "2015-11-02", {"Jackie Kircher"});
    facts.tags = {"new", "favourite"};
    facts.origin = "gamejam";
    facts.originReference = "Octojam2";
    facts.license = "CC0-1.0";
    CHECK(matches("author = \"jackie kircher\"", facts));
    CHECK(matches("author ~ kirch", facts));
    CHECK(matches("by ~ kirch", facts));
    CHECK_FALSE(matches("author = jackie", facts));
    CHECK(matches("tag = new", facts));
    CHECK(matches("-tag = old", facts));
    CHECK(matches("tag != old", facts));
    CHECK(matches("origin = \"game jam\"", facts));
    CHECK(matches("jam ~ octojam", facts));
    CHECK(matches("license = cc0-1.0", facts));
    CHECK(matches("copyright = none", facts));
    CHECK(matches("tag = new release = 2015 octo", facts));
    CHECK_FALSE(matches("tag = none", facts));
}

TEST_CASE("Library query: presence")
{
    auto facts = program("Missing Link");
    facts.present = false;
    CHECK(LibraryQuery::parse("missing").usesPresence());
    CHECK_FALSE(LibraryQuery::parse("\"missing\"").usesPresence());
    CHECK(matches("missing", facts));
    CHECK_FALSE(matches("present", facts));
    CHECK(matches("\"missing\"", facts));  // quoted, it is a word to search
}

TEST_CASE("Library query: errors")
{
    CHECK_THROWS_AS(LibraryQuery::parse("relase < 1980"), LibraryQuery::Error);
    CHECK_THROWS_AS(LibraryQuery::parse("release < 19"), LibraryQuery::Error);
    CHECK_THROWS_AS(LibraryQuery::parse("release < 1990..1999"), LibraryQuery::Error);
    CHECK_THROWS_AS(LibraryQuery::parse("name < x"), LibraryQuery::Error);
    CHECK_THROWS_AS(LibraryQuery::parse("(space"), LibraryQuery::Error);
    CHECK_THROWS_AS(LibraryQuery::parse("space)"), LibraryQuery::Error);
    CHECK_THROWS_AS(LibraryQuery::parse("\"space"), LibraryQuery::Error);
    CHECK_THROWS_AS(LibraryQuery::parse("< 1980"), LibraryQuery::Error);
    CHECK_THROWS_AS(LibraryQuery::parse("release <"), LibraryQuery::Error);
    CHECK_THROWS_AS(LibraryQuery::parse("a and"), LibraryQuery::Error);
    CHECK_THROWS_AS(LibraryQuery::parse("or a"), LibraryQuery::Error);
    CHECK_THROWS_AS(LibraryQuery::parse("not"), LibraryQuery::Error);
    try {
        LibraryQuery::parse("pong relase < 1980");
        FAIL("no error");
    }
    catch (const LibraryQuery::Error& error) {
        CHECK(error.position() == 5);
        CHECK(std::string(error.what()) == "unknown field 'relase'");
    }
}

namespace {

ProgramFacts twoRoms()
{
    auto facts = program("Octovore", "2015-11-02", {"Jackie Kircher"});
    RomFacts archive{.sha1 = "74936ffb0db233c722f5a39932d75240b6437a72", .size = 3549, .files = {}, .present = false};
    archive.configs.push_back({"schipc", {{"instructionsperframe", "120"}, {"wrapsprites", "false"}, {"screenrotation", "0°"}}});
    RomFacts local{.sha1 = "1ff6e2a8920c5b48def34348df65226285f39ce9", .size = 3549, .files = {"/roms/schip/Octovore (by Pangasaurus Rex)(2015).sc8"}, .present = true};
    local.configs.push_back({"schipc", {{"instructionsperframe", "30"}, {"wrapsprites", "false"}}});
    local.configs.push_back({"xo-chip", {{"instructionsperframe", "1000"}, {"wrapsprites", "true"}, {"startaddress", "512"}}});
    facts.roms = {archive, local};
    return facts;
}

const std::set<std::string> PROPERTIES = {"instructionsperframe", "wrapsprites", "screenrotation", "startaddress"};

std::vector<size_t> matching(std::string_view query, const ProgramFacts& facts)
{
    return LibraryQuery::parse(query, PROPERTIES).matchingRoms(facts);
}

}  // namespace

TEST_CASE("Library query: ROM and config fields")
{
    const auto facts = twoRoms();
    CHECK(matching("octovore", facts) == std::vector<size_t>{0, 1});
    CHECK_FALSE(LibraryQuery::parse("octovore", PROPERTIES).usesRomFields());
    CHECK(LibraryQuery::parse("variant = xo-chip", PROPERTIES).usesRomFields());
    CHECK(matching("variant = xo-chip", facts) == std::vector<size_t>{1});
    CHECK(matching("preset ~ schip", facts) == std::vector<size_t>{0, 1});
    CHECK(matching("instructionsPerFrame > 100", facts) == std::vector<size_t>{0, 1});
    CHECK(matching("instructionsPerFrame > 100 and variant = schipc", facts) == std::vector<size_t>{0});  // the same config
    CHECK(matching("wrapSprites = true", facts) == std::vector<size_t>{1});
    CHECK(matching("wrapSprites = yes", facts) == std::vector<size_t>{1});
    CHECK(matching("startAddress = 0x200", facts) == std::vector<size_t>{1});
    CHECK(matching("screenRotation = 0", facts) == std::vector<size_t>{0});
    CHECK(matching("startAddress = none", facts) == std::vector<size_t>{0, 1});  // the schipc configs have none
    CHECK(matching("size >= 3k", facts) == std::vector<size_t>{0, 1});
    CHECK(matching("size < 3k", facts).empty());
    CHECK(matching("sha1 = 1ff6", facts) == std::vector<size_t>{1});
    CHECK(matching("sha ~ 0db2", facts) == std::vector<size_t>{0});
    CHECK(matching("file = \"Octovore (by Pangasaurus Rex)(2015).sc8\"", facts) == std::vector<size_t>{1});
    CHECK(matching("file ~ pangasaurus", facts) == std::vector<size_t>{1});
    CHECK(matching("file = none", facts) == std::vector<size_t>{0});
    CHECK(matching("missing", facts) == std::vector<size_t>{0});
    CHECK(matching("present and author ~ kircher", facts) == std::vector<size_t>{1});
    CHECK(matching("-variant = xo-chip", facts) == std::vector<size_t>{0, 1});  // the other config of ROM 1 matches
    CHECK(matching("not (variant = xo-chip or missing)", facts) == std::vector<size_t>{1});
    CHECK(LibraryQuery::parse("variant = megachip", PROPERTIES).matches(facts) == false);
    CHECK_THROWS_AS(LibraryQuery::parse("size > big", PROPERTIES), LibraryQuery::Error);
    CHECK_THROWS_AS(LibraryQuery::parse("wrapSprites = true"), LibraryQuery::Error);  // no properties given
    CHECK_THROWS_AS(LibraryQuery::parse("variant < xo", PROPERTIES), LibraryQuery::Error);
}

TEST_CASE("Library query: opcode use")
{
    auto facts = twoRoms();
    facts.roms[0].analyzed = true;
    facts.roms[0].opcodes = {0x00E0, 0x00FF, 0xF175, 0xF285};
    facts.roms[1].analyzed = false;  // never analyzed, no opcodes known
    CHECK(matching("uses F?75", facts) == std::vector<size_t>{0});
    CHECK(matching("uses Fx75", facts) == std::vector<size_t>{0});
    CHECK(matching("uses f175", facts) == std::vector<size_t>{0});
    CHECK(matching("uses F075", facts).empty());
    CHECK(matching("uses 00FF and uses F?85", facts) == std::vector<size_t>{0});
    CHECK(matching("uses 00FD", facts).empty());
    CHECK(matching("analyzed -uses 00FD", facts) == std::vector<size_t>{0});
    CHECK(matching("-uses 00FD", facts) == std::vector<size_t>{0, 1});  // ROM 1 isn't analyzed, so it doesn't use it either
    CHECK(matching("-analyzed", facts) == std::vector<size_t>{1});
    CHECK(LibraryQuery::parse("uses 00E0").usesRomFields());
    CHECK(matches("\"uses\"", program("What it uses")));  // quoted, it is a word to search
    CHECK_THROWS_AS(LibraryQuery::parse("uses"), LibraryQuery::Error);
    CHECK_THROWS_AS(LibraryQuery::parse("uses F75"), LibraryQuery::Error);
    CHECK_THROWS_AS(LibraryQuery::parse("uses (F075)"), LibraryQuery::Error);
}

TEST_CASE("Library query: combined queries")
{
    const auto invaders = program("Space Invaders", "1978", {"David Winter"});
    CHECK(LibraryQuery::combine({}).empty());
    CHECK(LibraryQuery::combine({"  ", ""}).empty());
    CHECK(LibraryQuery::combine({"", "release < 1980"}) == "release < 1980");
    CHECK(LibraryQuery::combine({"pong or invaders", "winter"}) == "(pong or invaders) (winter)");
    CHECK(matches(LibraryQuery::combine({"pong or invaders", "winter"}), invaders));
    CHECK_FALSE(matches(LibraryQuery::combine({"pong or invaders", "earnest"}), invaders));
    CHECK_FALSE(matches(LibraryQuery::combine({"pong or tetris", "winter"}), invaders));  // the or stays inside
    CHECK(matches(LibraryQuery::combine({"-pong", "release < 1980"}), invaders));
    CHECK(LibraryQuery::quoted("xo-chip") == "\"xo-chip\"");
    auto tagged = invaders;
    tagged.tags = {"none", "???"};
    CHECK(matches("tag = " + LibraryQuery::quoted("none"), tagged));  // quoted, none is a name
    CHECK(matches("tag = " + LibraryQuery::quoted("???"), tagged));
    CHECK_THROWS_AS(LibraryQuery::quoted("say \"hi\""), LibraryQuery::Error);
}
