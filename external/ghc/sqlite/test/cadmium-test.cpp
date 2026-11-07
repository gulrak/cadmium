#include <catch2/catch_test_macros.hpp>
#include <ghc/sqlite.hpp>

#include <cmath>
#include <string>
#include <random>

//#include "fmt/compile.h"
#include "tempdir.hpp"

namespace db = ghc::sqlite;

struct DBVersion
{
    int id{0};
    int schema_version{1};
};

struct DBTags
{
    int id{0};
    std::string name;
    std::string color;
};

struct DBProgram
{
    int id{0};
    std::string name;
    std::string origin; // "gamejam", "event", "magazine", "manual"
    std::string description;
    std::string release;
    std::string url;
    std::optional<int> year{0};

   // Rectangle rect{};
    std::vector<int> tags;
    std::vector<int> binaries;
};

struct DBBinaryConfig
{
    int id{0};
    int binary_id{0};
    std::string preset;
    std::string properties;
};

struct DBBinary
{
    int id{0};
    int program_id{0};
    std::string sha1;
    std::string release;
    std::string description;
    std::vector<uint8_t> data;

    std::vector<std::string> filenames;
    std::vector<int> tags;
    std::vector<DBBinaryConfig> configs;
};

struct DBFilename
{
    int id{0};
    int binary_id{0};
    std::string name;
};

struct DBProgramTag
{
    int id{0};
    int program_id{0};
    int tag_id{0};
};

struct DBBinaryTag
{
    int id{0};
    int binary_id{0};
    int tag_id{0};
};

struct DBBinaryConfigTag
{
    int id{0};
    int binary_config_id{0};
    int tag_id{0};
};

void registerTables()
{
    static bool registered = false;
    if (registered) return;
    registered = true;
    db::Table<DBVersion>("version")
        .column("id", &DBVersion::id)
        .column("schema_version", &DBVersion::schema_version)
        .registerTable();

    db::Table<DBProgram>("programs")
        .column("id", &DBProgram::id)
        .column("name", &DBProgram::name)
        .column("origin", &DBProgram::origin)
        .column("description", &DBProgram::description)
        .column("release", &DBProgram::release)
        .column("url", &DBProgram::url)
        .column("year", &DBProgram::year)
        .registerTable();

    db::Table<DBBinary>("binaries")
        .column("id", &DBBinary::id)
        .column("program_id", &DBBinary::program_id, "programs", "id", db::Action::ON_UPDATE_CASCADE, db::Action::ON_DELETE_CASCADE)
        .column("sha1", &DBBinary::sha1, true)
        .column("release", &DBBinary::release)
        .column("description", &DBBinary::description)
        .column("data", &DBBinary::data)
        .registerTable();

    db::Table<DBBinaryConfig>("binary_configs")
        .column("id", &DBBinaryConfig::id)
        .column("binary_id", &DBBinaryConfig::binary_id, "binaries", "id", db::Action::ON_UPDATE_CASCADE, db::Action::ON_DELETE_CASCADE)
        .column("preset", &DBBinaryConfig::preset)
        .column("properties", &DBBinaryConfig::properties)
        .registerTable();

    db::Table<DBFilename>("filenames")
        .column("id", &DBFilename::id)
        .column("binary_id", &DBFilename::binary_id, "binaries", "id", db::Action::ON_UPDATE_CASCADE, db::Action::ON_DELETE_CASCADE)
        .column("name", &DBFilename::name)
        .registerTable();

    db::Table<DBTags>("tags")
        .column("id", &DBTags::id)
        .column("name", &DBTags::name, true)
        .column("color", &DBTags::color)
        .registerTable();

    db::Table<DBProgramTag>("program_tags")
        .column("id", &DBProgramTag::id)
        .column("program_id", &DBProgramTag::program_id, "programs", "id", db::Action::ON_UPDATE_CASCADE, db::Action::ON_DELETE_CASCADE)
        .column("tag_id", &DBProgramTag::tag_id, "tags", "id", db::Action::ON_UPDATE_CASCADE, db::Action::ON_DELETE_CASCADE)
        .registerTable();

    db::Table<DBBinaryTag>("binary_tags")
        .column("id", &DBBinaryTag::id)
        .column("binary_id", &DBBinaryTag::binary_id, "binaries", "id", db::Action::ON_UPDATE_CASCADE, db::Action::ON_DELETE_CASCADE)
        .column("tag_id", &DBBinaryTag::tag_id, "tags", "id", db::Action::ON_UPDATE_CASCADE, db::Action::ON_DELETE_CASCADE)
        .registerTable();

    db::Table<DBBinaryConfigTag>("binary_config_tags")
        .column("id", &DBBinaryConfigTag::id)
        .column("binary_config_id", &DBBinaryConfigTag::binary_config_id, "binary_configs", "id", db::Action::ON_UPDATE_CASCADE, db::Action::ON_DELETE_CASCADE)
        .column("tag_id", &DBBinaryConfigTag::tag_id, "tags", "id", db::Action::ON_UPDATE_CASCADE, db::Action::ON_DELETE_CASCADE)
        .registerTable();
}

TEST_CASE("Cadmium-ORM: Database Create", "[database]")
{
    TemporaryDirectory t;
    fs::path dbFile = t.path() / "test.db";
    registerTables();
    db::Database db(dbFile.string());

    db::Registry::syncTables(db);

    auto progMeta = db::Registry::findMetadata<DBProgram>();
    CHECK(progMeta->createTableSql() == R"(CREATE TABLE IF NOT EXISTS "programs" ("id" INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, "name" TEXT NOT NULL, "origin" TEXT NOT NULL, "description" TEXT NOT NULL, "release" TEXT NOT NULL, "url" TEXT NOT NULL, "year" INTEGER);)");
    auto binCfgMeta = db::Registry::findMetadata<DBBinaryConfig>();
    CHECK(binCfgMeta->createTableSql() == R"(CREATE TABLE IF NOT EXISTS "binary_configs" ("id" INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, "binary_id" INTEGER NOT NULL, "preset" TEXT NOT NULL, "properties" TEXT NOT NULL, FOREIGN KEY("binary_id") REFERENCES "binaries"("id") ON UPDATE CASCADE ON DELETE CASCADE);)");

    db::Session session(db);
    DBVersion version{0,42};
    session.insert(version);
    CHECK(version.id == 1);

    auto versions = session.fetchAll<DBVersion>();
    CHECK(versions.size() == 1);
    CHECK(versions[0].schema_version == 42);

    //DBProgram program;
    //auto binaries = session.select<DBBinary>().limit(10).where(db::col(&DBBinary::program_id) == program.id);




    /*
    _pimpl->newTagId = _pimpl->connection->select_query<Select<TagsTable::field_t<"id">>>().where_one(TagsTable::field_t<"name">().like("new")).exec().value_or(0);
    _pimpl->unclassifiedTagId = _pimpl->connection->select_query<Select<TagsTable::field_t<"id">>>().where_one(TagsTable::field_t<"name">().like("???")).exec().value_or(0);
    auto binaries = _pimpl->storage.select(&DBBinary::id, where(c(&DBBinary::program_id) == program.id));
    auto configs = _pimpl->storage.get_all<DBBinaryConfig>(where(c(&DBBinaryConfig::binary_id) == binary.id));
    auto filenames = _pimpl->storage.select(&DBFilename::name, where(c(&DBFilename::binary_id) == binary.id));
    auto bid = _pimpl->storage.select(&DBBinary::id, where(c(&DBBinary::sha1) == digest));
     */
}

TEST_CASE("Cadmium-ORM: Registry sync creates tables and generates expected SQL", "[database]")
{
    using namespace ghc::sqlite;

    // Tables are registered earlier in this file
    TemporaryDirectory t;
    fs::path dbFile = t.path() / "test.db";
    registerTables();
    Database db(dbFile.string());

    // Will create all missing tables
    Registry::syncTables(db);

    auto progMeta = Registry::findMetadata<DBProgram>();
    REQUIRE(progMeta);
    auto binMeta = Registry::findMetadata<DBBinary>();
    REQUIRE(binMeta);

    // Spot‑check SQL generation for a non‑FK table
    CHECK(progMeta->createTableSql().find("CREATE TABLE IF NOT EXISTS \"programs\"") == 0);
    CHECK(progMeta->createTableSql().find("\"id\" INTEGER PRIMARY KEY AUTOINCREMENT") != std::string::npos);
    CHECK(progMeta->createTableSql().find("\"name\" TEXT") != std::string::npos);

    // Binaries has FKs + UNIQUE on sha1
    const auto binSQL = binMeta->createTableSql();
    CHECK(binSQL.find("\"program_id\" INTEGER") != std::string::npos);
    CHECK(binSQL.find("FOREIGN KEY(\"program_id\") REFERENCES \"programs\"(\"id\")") != std::string::npos);
    CHECK(((binSQL.find("\"sha1\" TEXT UNIQUE") != std::string::npos) || (binSQL.find("\"sha1\" TEXT") != std::string::npos)));
}

TEST_CASE("Cadmium-ORM: Insert program with/without optional fields", "[database]")
{
    using namespace ghc::sqlite;
    TemporaryDirectory t;
    fs::path dbFile = t.path() / "test.db";
    registerTables();
    Database db(dbFile.string());
    Registry::syncTables(db);

    Session session(db);

    DBProgram p1{};
    p1.name = "Demo";
    p1.origin = "event";
    p1.description = "First entry";
    p1.release = "1.0";
    p1.url = "https://example.invalid/demo";
    p1.year = std::nullopt; // optional field left null

    auto id1 = session.insert(p1);
    REQUIRE(id1 == p1.id);
    REQUIRE(id1 > 0);

    DBProgram p2{};
    p2.name = "Demo 2";
    p2.origin = "gamejam";
    p2.description = "Second entry";
    p2.release = "1.1";
    p2.url = "https://example.invalid/demo2";
    p2.year = 2024;

    auto id2 = session.insert(p2);
    REQUIRE(id2 == p2.id);
    REQUIRE(id2 == id1 + 1);
}

TEST_CASE("Cadmium-ORM: Insert binary with blob and filenames", "[database]")
{
    using namespace ghc::sqlite;
    TemporaryDirectory t;
    fs::path dbFile = t.path() / "test.db";
    registerTables();
    Database db(dbFile.string());
    Registry::syncTables(db);
    Session session(db);

    // Create a program first (FK target)
    DBProgram prog{};
    prog.name = "BlobProg";
    prog.origin = "manual";
    prog.description = "Has blobs";
    prog.release = "0.1";
    prog.url = "https://example.invalid/blob";
    session.insert(prog);
    REQUIRE(prog.id > 0);

    // Binary
    DBBinary bin{};
    bin.program_id = prog.id;
    bin.sha1 = "deadbeef";
    bin.release = "r1";
    bin.description = "payload";
    bin.data = Blob{0,1,2,3,4,5};
    session.insert(bin);
    REQUIRE(bin.id > 0);

    // Filenames child rows
    DBFilename f1{}; f1.binary_id = bin.id; f1.name = "fileA.bin";
    session.insert(f1);
    REQUIRE(f1.id > 0);

    DBFilename f2{}; f2.binary_id = bin.id; f2.name = "fileB.bin";
    session.insert(f2);
    REQUIRE(f2.id > 0);

    // Verify via SQL
    Statement q(session, "SELECT length(data) FROM binaries WHERE id=?;");
    q.bind(1, (int64_t)bin.id);
    REQUIRE(q.step());
    REQUIRE((int)q.column(0) == 6);

    Statement qf(session, "SELECT name FROM filenames WHERE binary_id=? ORDER BY id;");
    qf.bind(1, (int64_t)bin.id);
    REQUIRE(qf.step());
    CHECK(std::string(qf.column(0)) == "fileA.bin");
    REQUIRE(qf.step());
    CHECK(std::string(qf.column(0)) == "fileB.bin");
}

TEST_CASE("Cadmium-ORM: UNIQUE(sha1) enforced on binaries", "[database]")
{
    using namespace ghc::sqlite;
    TemporaryDirectory t;
    fs::path dbFile = t.path() / "test.db";
    registerTables();
    Database db(dbFile.string());
    Registry::syncTables(db);
    Session session(db);

    DBProgram prog{};
    prog.name = "U"; prog.origin = "o"; prog.description = "d"; prog.release = "r"; prog.url = "u";
    session.insert(prog);
    REQUIRE(prog.id > 0);

    DBBinary a{}; a.program_id = prog.id; a.sha1 = "d92c71b955b7634370571bd707715cf8bb0e2fb4"; a.release = "r"; a.description = ""; a.data = Blob{0,1,2,3,4};
    session.insert(a);
    REQUIRE(a.id > 0);

    DBBinary b{}; b.program_id = prog.id; b.sha1 = "d92c71b955b7634370571bd707715cf8bb0e2fb4"; b.release = "r2"; b.description = ""; b.data = Blob{5,6,7,8,9};
    REQUIRE_THROWS_AS(session.insert(b), Exception);
}

TEST_CASE("Cadmium-ORM: ON DELETE CASCADE from programs -> binaries -> filenames", "[database]")
{
    using namespace ghc::sqlite;
    TemporaryDirectory t;
    fs::path dbFile = t.path() / "test.db";
    registerTables();
    Database db(dbFile.string());
    Registry::syncTables(db);
    Session session(db);

    DBProgram p{}; p.name = "C"; p.origin = "o"; p.description = "d"; p.release = "r"; p.url = "u";
    session.insert(p);
    REQUIRE(p.id > 0);
    DBBinary b{}; b.program_id = p.id; b.sha1 = "s"; b.release = "r"; b.description = ""; b.data = {1,2,3,4};
    session.insert(b);
    REQUIRE(b.id > 0);
    DBFilename f{}; f.binary_id = b.id; f.name = "n";
    session.insert(f);
    REQUIRE(f.id > 0);

    // Delete program
    Statement del(session, "DELETE FROM programs WHERE id=?;");
    del.bind(1, (int64_t)p.id);
    REQUIRE_NOTHROW(del.execute());

    // Ensure cascade removed child rows
    auto cnt = static_cast<int64_t>(db.executeForValue("SELECT COUNT(*) FROM binaries WHERE program_id = " + std::to_string(p.id) + ";"));
    REQUIRE(cnt == 0);
    auto cnt2 = static_cast<int64_t>(db.executeForValue("SELECT COUNT(*) FROM filenames;"));
    REQUIRE(cnt2 == 0);
}

TEST_CASE("Cadmium-ORM: Tag link rows and config cascade", "[database]")
{
    using namespace ghc::sqlite;
    TemporaryDirectory t;
    fs::path dbFile = t.path() / "test.db";
    registerTables();
    Database db(dbFile.string());
    Registry::syncTables(db);
    Session session(db);

    // Tag
    DBTags tag{}; tag.name = "t1"; tag.color = "#fff";
    session.insert(tag);
    REQUIRE(tag.id > 0);

    // Program + tag link
    DBProgram p{}; p.name = "PT"; p.origin = "o"; p.description = "d"; p.release = "r"; p.url = "u";
    session.insert(p);
    REQUIRE(p.id > 0);
    DBProgramTag pt{}; pt.program_id = p.id; pt.tag_id = tag.id;
    session.insert(pt);
    REQUIRE(pt.id > 0);

    // Binary + config + tags
    DBBinary b{}; b.program_id = p.id; b.sha1 = "s2"; b.release = "r"; b.description = ""; b.data = {1,2,3,4};
    session.insert(b);
    REQUIRE(b.id > 0);
    DBBinaryConfig cfg{}; cfg.binary_id = b.id; cfg.preset = "default"; cfg.properties = "{}";
    session.insert(cfg);
    REQUIRE(cfg.id > 0);

    DBBinaryTag bt{}; bt.binary_id = b.id; bt.tag_id = tag.id;
    session.insert(bt);
    REQUIRE(bt.id > 0);
    DBBinaryConfigTag bct{}; bct.binary_config_id = cfg.id; bct.tag_id = tag.id;
    session.insert(bct);
    REQUIRE(bct.id > 0);

    // Delete tag, ensure link rows are removed
    Statement delTag(session, "DELETE FROM tags WHERE id=?;"); delTag.bind(1, (int64_t)tag.id);
    REQUIRE_NOTHROW(delTag.execute());

    auto cntPT = (int64_t)db.executeForValue("SELECT COUNT(*) FROM program_tags;");
    auto cntBT = (int64_t)db.executeForValue("SELECT COUNT(*) FROM binary_tags;");
    auto cntBCT = (int64_t)db.executeForValue("SELECT COUNT(*) FROM binary_config_tags;");
    REQUIRE(cntPT == 0);
    REQUIRE(cntBT == 0);
    REQUIRE(cntBCT == 0);
}

TEST_CASE("Cadmium-ORM: ON UPDATE CASCADE propagates program_id in binaries", "[database]")
{
    using namespace ghc::sqlite;
    TemporaryDirectory t;
    fs::path dbFile = t.path() / "test.db";
    registerTables();
    Database db(dbFile.string());
    Registry::syncTables(db);
    Session session(db);

    DBProgram p{}; p.name = "U"; p.origin = "o"; p.description = "d"; p.release = "r"; p.url = "u";
    session.insert(p);
    REQUIRE(p.id > 0);
    DBBinary b{}; b.program_id = p.id; b.sha1 = "s3"; b.release = "r"; b.description = ""; b.data = {1,2,3,4};
    session.insert(b);
    REQUIRE(b.id > 0);

    // Change the program id (simulate PK update)
    const int64_t newId = p.id + 100;
    Statement upd(session, "UPDATE programs SET id=? WHERE id=?;");
    upd.bind(1, newId); upd.bind(2, p.id);
    upd.execute();

    auto newFk = (int64_t)db.executeForValue("SELECT program_id FROM binaries WHERE id = " + std::to_string(b.id) + ";");
    REQUIRE(newFk == newId);
}

TEST_CASE("Cadmium-ORM: select for field by where", "[database]")
{
    using namespace ghc::sqlite;
    TemporaryDirectory t;
    fs::path dbFile = t.path() / "test.db";
    registerTables();
    Database db(dbFile.string());
    Registry::syncTables(db);
    Session session(db);

    DBProgram prog{};
    prog.name = "U"; prog.origin = "o"; prog.description = "d"; prog.release = "r"; prog.url = "u";
    session.insert(prog);
    REQUIRE(prog.id > 0);

    DBBinary a{}; a.program_id = prog.id; a.sha1 = "d92c71b955b7634370571bd707715cf8bb0e2fb4"; a.release = "r"; a.description = ""; a.data = Blob{0,1,2,3,4};
    session.insert(a);
    REQUIRE(a.id > 0);

    DBBinary b{}; b.program_id = prog.id; b.sha1 = "ae71a7b081a947f1760cdc147759803aea45e751"; b.release = "r2"; b.description = ""; b.data = Blob{5,6,7,8,9};
    session.insert(b);
    REQUIRE(b.id > 0);

    auto ids = session.select(col(&DBBinary::id)).where(col(&DBBinary::sha1) == "d92c71b955b7634370571bd707715cf8bb0e2fb4");
    REQUIRE(ids.size() == 1);
    REQUIRE(ids[0] == a.id);

    ids = session.select(col(&DBBinary::id)).where(like(col(&DBBinary::sha1), "d92c71b955%"));
    REQUIRE(ids.size() == 1);
    REQUIRE(ids[0] == a.id);

    auto id = session.select(col(&DBBinary::id)).where(like(col(&DBBinary::sha1), "d92c71b955%")).one();
    REQUIRE(id.has_value());
    REQUIRE(*id == a.id);
}

TEST_CASE("Cadmium-ORM: Manual UPSERT with RETURNING on tags", "[database][upsert]")
{
    using namespace ghc::sqlite;
    TemporaryDirectory t;
    fs::path dbFile = t.path() / "test.db";
    registerTables();
    Database db(dbFile.string());
    Registry::syncTables(db);
    Session session(db);

    // 1) First UPSERT creates a new row and returns its id
    const char* sql = R"(INSERT INTO tags(name, color)
VALUES (?, ?)
ON CONFLICT(name) DO UPDATE SET color = excluded.color
RETURNING id;)";

    Statement up1(session, sql);
    up1.bind(1, std::string("uniqueTag"));
    up1.bind(2, std::string("#111111"));
    REQUIRE(up1.step());
    const int64_t id1 = (int64_t)up1.column(0);
    REQUIRE(id1 > 0);

    // Verify color stored
    Statement q1(session, "SELECT color FROM tags WHERE id=?;");
    q1.bind(1, id1);
    REQUIRE(q1.step());
    CHECK(std::string(q1.column(0)) == "#111111");

    // 2) Second UPSERT with same name updates color and returns the same id
    Statement up2(session, sql);
    up2.bind(1, std::string("uniqueTag"));
    up2.bind(2, std::string("#222222"));
    REQUIRE(up2.step());
    const int64_t id2 = (int64_t)up2.column(0);
    CHECK(id2 == id1);

    Statement q2(session, "SELECT color FROM tags WHERE id=?;");
    q2.bind(1, id1);
    REQUIRE(q2.step());
    CHECK(std::string(q2.column(0)) == "#222222");

    // 3) Third UPSERT with a different name creates a new row (different id)
    Statement up3(session, sql);
    up3.bind(1, std::string("anotherTag"));
    up3.bind(2, std::string("#333333"));
    REQUIRE(up3.step());
    const int64_t id3 = (int64_t)up3.column(0);
    CHECK(id3 != id1);
}