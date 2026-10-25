#include <catch2/catch_test_macros.hpp>
#include <ghc/sqlite.hpp>

#include <cmath>
#include <functional>
#include <future>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <random>
#include <thread>

#include "tempdir.hpp"

namespace db = ghc::sqlite;


TEST_CASE("Database Create", "[database]")
{
    TemporaryDirectory t;
    fs::path dbFile = t.path() / "test.db";
    db::Database db(dbFile.string());
    REQUIRE(exists(dbFile));
}

TEST_CASE("Database Basic", "[database]")
{
    TemporaryDirectory t;
    fs::path dbFile = t.path() / "test.db";
    db::Database db(dbFile.string());
    db.execute("CREATE TABLE test_table (id INTEGER PRIMARY KEY, floatval REAL, intval INTEGER, textval TEXT);");
    std::string result = db.executeForValue("SELECT name FROM sqlite_master WHERE type='table';");
    REQUIRE("test_table" == result);
}

TEST_CASE("Database Statement", "[database]")
{
    TemporaryDirectory t;
    fs::path dbFile = t.path() / "test.db";
    db::Database db{dbFile.string()};
    db::Session session{db};
    db.execute("CREATE TABLE test_table (id INTEGER PRIMARY KEY, floatval REAL, intval INTEGER, textval TEXT);");
    db::Statement stmt{session, "INSERT INTO test_table VALUES(1, 3.14, 1234, 'huzzli');"};
    stmt.step();
    REQUIRE(1 == (int64_t)db.executeForValue("SELECT id FROM test_table ORDER BY id ASC LIMIT 1;"));
    db::Statement query{session, "SELECT id, floatval, intval, textval FROM test_table;"};
    if (query.step()) {
        REQUIRE(1 == (int64_t)query.column(0));
        REQUIRE(std::fabs(3.14 - (double)query.column(1)) < 0.0001);
        REQUIRE(1234 == (int)query.column(2));
        REQUIRE("huzzli" == static_cast<const std::string&>(query.column(3)));
    }
}

TEST_CASE("Database Statement Bind", "[database]")
{
    TemporaryDirectory t;
    fs::path dbFile = t.path() / "test.db";
    db::Database db(dbFile.string());
    db::Session session{db};
    db.execute("CREATE TABLE test_table (id INTEGER PRIMARY KEY, floatval REAL, intval INTEGER, textval TEXT);");
    db::Statement stmt{session, "INSERT INTO test_table VALUES(?,?,?,?);"};
    stmt.bind(1, 1);
    stmt.bind(2, 3.14);
    stmt.bind(3, 1234);
    stmt.bind(4, "huzzli");
    stmt.step();
    REQUIRE(1 == (int64_t)db.executeForValue("SELECT id FROM test_table ORDER BY id ASC LIMIT 1;"));
    db::Statement query{session, "SELECT id, floatval, intval, textval FROM test_table;"};
    if (query.step()) {
        REQUIRE(1 == (int64_t)query.column(0));
        REQUIRE(std::fabs(3.14 - (double)query.column(1)) < 0.0001);
        REQUIRE(1234 == (int)query.column(2));
        REQUIRE("huzzli" == static_cast<const std::string&>(query.column(3)));
    }
}

TEST_CASE("Database Statement Bind by Stream Operator", "[database]")
{
    TemporaryDirectory t;
    fs::path dbFile = t.path() / "test.db";
    db::Database db(dbFile.string());
    db::Session session{db};
    int64_t id;
    int intval;
    double floatval;
    std::string textval;
    db.execute("CREATE TABLE test_table (id INTEGER PRIMARY KEY, floatval REAL, intval INTEGER, textval TEXT, nullval INTEGER);");
    db::Statement stmt{session, "INSERT INTO test_table VALUES(?,?,?,?,?);"};
    stmt << 1 << 3.14 << 1234 << "huzzli" << db::null << db::execute;
    REQUIRE(1 == (int64_t)db.executeForValue("SELECT id FROM test_table ORDER BY id ASC LIMIT 1;"));
    db::Statement query{session, "SELECT id, floatval, intval, textval FROM test_table;"};
    if (query.step()) {
        query >> id >> floatval >> intval >> textval;
        REQUIRE(1 == id);
        REQUIRE(std::fabs(3.14 - floatval) < 0.0001);
        REQUIRE(1234 == intval);
        REQUIRE("huzzli" == textval);
        REQUIRE(query.column(4).isNull());
    }
}

TEST_CASE("Database Simple Encryption", "[database]")
{
#ifdef WITH_ENCRYPTION
    TemporaryDirectory t;
    fs::path dbFile = t.path() / "test.db";
    db::Database db(dbFile.string(), "1234", "5678");
    db.execute("CREATE TABLE test_table (id INTEGER PRIMARY KEY, floatval REAL, intval INTEGER, textval TEXT);");
    std::string result = db.executeForValue("SELECT name FROM sqlite_master WHERE type='table';");
    REQUIRE("test_table" == result);
#endif
}

TEST_CASE("Database Reconnect Encryption", "[database]")
{
#ifdef WITH_ENCRYPTION
    TemporaryDirectory t;
    fs::path dbFile = t.path() / "test.db";
    {
        db::Database db(dbFile.string(), "1234", "5678");
        db.execute("CREATE TABLE test_table (id INTEGER PRIMARY KEY, floatval REAL, intval INTEGER, textval TEXT);");
    }
    {
        db::Database db(dbFile.string(), "1234", "5678");
        std::string result = db.executeForValue("SELECT name FROM sqlite_master WHERE type='table';");
        REQUIRE("test_table" == result);
    }
#endif
}

struct User {
    int64_t id;
    std::string name;
    std::string email;
};

struct Document {
    int64_t id;
    std::string title;
    std::vector<uint8_t> content;
    int64_t user_id;
};

void register_example_structs() {
    // Method 1: Manual registration (recommended for clarity)
    db::Table<User>("users")
        .column("id", &User::id)
        .column("name", &User::name)
        .column("email", &User::email)
        .registerTable();

    db::Table<Document>("documents")
        .column("id", &Document::id)
        .column("title", &Document::title)
        .column("content", &Document::content)
        .column("user_id", &Document::user_id)
        .registerTable();
}

TEST_CASE("ORM Table Registration", "[database]")
{
    register_example_structs();
    auto userMeta = db::Registry::findMetadata<User>();
    CHECK(userMeta->createTableSql() == R"(CREATE TABLE IF NOT EXISTS "users" ("id" INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, "name" TEXT NOT NULL, "email" TEXT NOT NULL);)");
}

// Regression tests for the fixes from the October 2026 review

namespace {

struct SyncParent
{
    int id{0};
    std::string name;
};

struct SyncChild
{
    int id{0};
    int parent_id{0};
    std::string key;
    std::optional<int> year;
};

struct MappedRecord
{
    int id{0};
    std::string name;
    int value{0};
    std::optional<int> year;
    db::Blob data;
};

struct TxRecord
{
    int id{0};
    std::string name;
};

struct ExprRecord
{
    int id{0};
    int group{0};
    std::string name;
};

struct KeywordOrder
{
    int id{0};
    int group{0};
    std::string select;
};

struct PagedRecord
{
    int id{0};
    std::string name;
};

struct RemoveParent
{
    int id{0};
    std::string name;
};

struct RemoveChild
{
    int id{0};
    int parent_id{0};
};

struct BlobRecord
{
    int id{0};
    db::Blob data;
};

struct GrownParent
{
    int id{0};
    std::string name;
};

struct GrownRecord
{
    int id{0};
    std::string name;
    std::string license;
    int64_t count{0};
    double rating{0.0};
    db::Blob data;
    std::optional<int> year;
    std::optional<int> parent_id;
};

struct GrownUnique
{
    int id{0};
    std::string name;
    std::string key;
};

struct RelaxedRecord
{
    int id{0};
    std::string name;
    std::optional<db::Blob> data;
    std::optional<int> size;
};

struct RelaxedChild
{
    int id{0};
    int record_id{0};
};

struct WalRecord
{
    int id{0};
    std::string name;
};

struct NullableRecord
{
    int id{0};
    std::optional<int> year;
    std::optional<std::string> nickname;
};

std::string captureCout(const std::function<void()>& func)
{
    std::ostringstream out;
    auto* old = std::cout.rdbuf(out.rdbuf());
    try {
        func();
    }
    catch (...) {
        std::cout.rdbuf(old);
        throw;
    }
    std::cout.rdbuf(old);
    return out.str();
}

}  // namespace

TEST_CASE("ORM schema sync reports no differences for its own tables", "[orm]")
{
    TemporaryDirectory t;
    db::Table<SyncParent>("sync_parents").column("id", &SyncParent::id).column("name", &SyncParent::name).registerTable();
    db::Table<SyncChild>("sync_children")
        .column("id", &SyncChild::id)
        .column("parent_id", &SyncChild::parent_id, "sync_parents", "id", db::Action::ON_UPDATE_CASCADE, db::Action::ON_DELETE_CASCADE)
        .column("key", &SyncChild::key, true)
        .column("year", &SyncChild::year)
        .registerTable();
    db::Database db((t.path() / "test.db").string());
    db::Registry::syncTables(db);
    auto report = captureCout([&] { db::Registry::syncTables(db); });
    CHECK(report.find("mismatch") == std::string::npos);
    CHECK(report.find("Missing") == std::string::npos);
    CHECK(report.find("Extra") == std::string::npos);
    CHECK(1 == (int)db.executeForValue("SELECT COUNT(*) FROM sqlite_master WHERE type='index' AND tbl_name='sync_children';"));
}

TEST_CASE("ORM maps columns by name instead of table order", "[orm]")
{
    TemporaryDirectory t;
    db::Database db((t.path() / "test.db").string());
    db.execute(R"(CREATE TABLE "mapped" ("id" INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, "legacy" TEXT, "year" INTEGER, "value" INTEGER NOT NULL, "data" BLOB, "name" TEXT NOT NULL);)");
    db::Table<MappedRecord>("mapped")
        .column("id", &MappedRecord::id)
        .column("name", &MappedRecord::name)
        .column("value", &MappedRecord::value)
        .column("year", &MappedRecord::year)
        .column("data", &MappedRecord::data)
        .registerTable();
    auto session = db.session();
    MappedRecord a{0, "abc", 42, 1980, {1, 2, 3}};
    session.insert(a);
    session.insert(MappedRecord{0, "def", 7, std::nullopt, {}});
    auto all = session.fetchAll<MappedRecord>();
    REQUIRE(all.size() == 2);
    CHECK(all[0].name == "abc");
    CHECK(all[0].value == 42);
    CHECK(all[0].year == 1980);
    CHECK(all[0].data == db::Blob{1, 2, 3});
    CHECK(all[1].name == "def");
    CHECK(all[1].value == 7);
    CHECK_FALSE(all[1].year.has_value());
    CHECK(session.fetch<MappedRecord>(a.id).value == 42);
    auto years = session.select(db::col(&MappedRecord::year)).orderBy("id").where(db::col(&MappedRecord::id) > 0);
    REQUIRE(years.size() == 2);
    CHECK(years[0] == 1980);
    CHECK_FALSE(years[1].has_value());
    CHECK(session.update(MappedRecord{a.id, "abc", 1, 1981, {}}) == 1);
    CHECK(session.update(MappedRecord{4711, "none", 1, std::nullopt, {}}) == 0);
}

TEST_CASE("ORM transaction survives an automatic rollback", "[orm]")
{
    TemporaryDirectory t;
    db::Table<TxRecord>("tx_records").column("id", &TxRecord::id).column("name", &TxRecord::name, true).registerTable();
    db::Database db((t.path() / "test.db").string());
    db::Registry::syncTables(db);
    auto session = db.session();
    CHECK_THROWS_AS(([&] {
                        db::Transaction transaction(session);
                        session.insert(TxRecord{0, "a"});
                        // Stand-in for SQLite rolling back on its own, e.g. after SQLITE_FULL.
                        db::Statement(session, "ROLLBACK").step();
                        session.insert(TxRecord{0, "a"});
                        session.insert(TxRecord{0, "a"});
                    }()),
                    db::ConstraintViolationException);
    {
        db::Transaction transaction(session);
        session.insert(TxRecord{0, "b"});
    }
    CHECK(session.fetchAll<TxRecord>().size() == 1);
}

TEST_CASE("ORM combines expressions with logical operators", "[orm]")
{
    TemporaryDirectory t;
    db::Table<ExprRecord>("expr_records").column("id", &ExprRecord::id).column("grp", &ExprRecord::group).column("name", &ExprRecord::name).registerTable();
    db::Database db((t.path() / "test.db").string());
    db::Registry::syncTables(db);
    auto session = db.session();
    for (auto [group, name] : {std::pair{1, "a"}, {1, "b"}, {2, "a"}, {3, "c"}}) {
        session.insert(ExprRecord{0, group, name});
    }
    const auto& group = db::col(&ExprRecord::group);
    const auto& name = db::col(&ExprRecord::name);
    CHECK(session.select<ExprRecord>().where(group == 1 && name == std::string("a")).size() == 1);
    CHECK(session.select<ExprRecord>().where(group == 3 || name == std::string("b")).size() == 2);
    CHECK(session.select<ExprRecord>().where(!(group == 1)).size() == 2);
    CHECK(session.select(db::col(&ExprRecord::id)).where((group == 1 || group == 2) && !(name == std::string("b"))).size() == 2);
}

TEST_CASE("ORM quotes keyword identifiers", "[orm]")
{
    TemporaryDirectory t;
    db::Table<KeywordOrder>("order").column("id", &KeywordOrder::id).column("group", &KeywordOrder::group).column("sel\"ect", &KeywordOrder::select, true).registerTable();
    db::Database db((t.path() / "test.db").string());
    db::Registry::syncTables(db);
    auto session = db.session();
    KeywordOrder a{0, 1, "x"};
    session.insert(a);
    session.insert(KeywordOrder{0, 2, "y"});
    CHECK(session.update(KeywordOrder{a.id, 3, "z"}) == 1);
    auto rows = session.select<KeywordOrder>().orderBy("group", true).where(db::col(&KeywordOrder::group) > 0 && db::like(db::col(&KeywordOrder::select), "%"));
    REQUIRE(rows.size() == 2);
    CHECK(rows[0].group == 3);
    CHECK(rows[0].select == "z");
}

TEST_CASE("ORM releases a session copy destroyed on another thread", "[orm]")
{
    TemporaryDirectory t;
    auto database = std::make_unique<db::Database>((t.path() / "test.db").string(), 2);
    std::optional<db::Session> handoff;
    std::promise<void> handedOver, released;
    // Keep the owner thread alive until the copy is gone, so its thread id can't be reused.
    std::thread owner([&] {
        {
            db::Session session(*database);
            handoff = session;
        }
        handedOver.set_value();
        released.get_future().wait();
    });
    handedOver.get_future().wait();
    std::thread([&] { handoff.reset(); }).join();
    released.set_value();
    owner.join();
    // With the connection still booked for the owner thread, this would wait forever.
    auto destroyed = std::async(std::launch::async, [&] { database.reset(); });
    REQUIRE(destroyed.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
}

TEST_CASE("ORM selects all rows without a WHERE clause", "[orm]")
{
    TemporaryDirectory t;
    db::Table<PagedRecord>("paged_records").column("id", &PagedRecord::id).column("name", &PagedRecord::name).registerTable();
    db::Database db((t.path() / "test.db").string());
    db::Registry::syncTables(db);
    auto session = db.session();
    for (auto name : {"d", "b", "a", "c"}) {
        session.insert(PagedRecord{0, name});
    }
    CHECK(session.select<PagedRecord>().all().size() == 4);
    auto sorted = session.select<PagedRecord>().orderBy("name").limit(2).all();
    REQUIRE(sorted.size() == 2);
    CHECK(sorted[0].name == "a");
    CHECK(sorted[1].name == "b");
    auto names = session.select(db::col(&PagedRecord::name)).orderBy("name", true).all();
    REQUIRE(names.size() == 4);
    CHECK(names[0] == "d");
}

TEST_CASE("ORM supports an offset without a limit", "[orm]")
{
    TemporaryDirectory t;
    db::Table<PagedRecord>("paged_records").column("id", &PagedRecord::id).column("name", &PagedRecord::name).registerTable();
    db::Database db((t.path() / "test.db").string());
    db::Registry::syncTables(db);
    auto session = db.session();
    for (auto name : {"a", "b", "c"}) {
        session.insert(PagedRecord{0, name});
    }
    auto rest = session.select<PagedRecord>().orderBy("name").offset(1).all();
    REQUIRE(rest.size() == 2);
    CHECK(rest[0].name == "b");
}

TEST_CASE("ORM removes rows by id and cascades", "[orm]")
{
    TemporaryDirectory t;
    db::Table<RemoveParent>("remove_parents").column("id", &RemoveParent::id).column("name", &RemoveParent::name).registerTable();
    db::Table<RemoveChild>("remove_children")
        .column("id", &RemoveChild::id)
        .column("parent_id", &RemoveChild::parent_id, "remove_parents", "id", db::Action::ON_UPDATE_CASCADE, db::Action::ON_DELETE_CASCADE)
        .registerTable();
    db::Database db((t.path() / "test.db").string());
    db::Registry::syncTables(db);
    auto session = db.session();
    RemoveParent a{0, "a"}, b{0, "b"};
    session.insert(a);
    session.insert(b);
    session.insert(RemoveChild{0, a.id});
    session.insert(RemoveChild{0, a.id});
    session.insert(RemoveChild{0, b.id});
    CHECK(session.remove<RemoveParent>(a.id) == 1);
    CHECK(session.remove<RemoveParent>(a.id) == 0);
    CHECK(session.fetchAll<RemoveParent>().size() == 1);
    CHECK(session.fetchAll<RemoveChild>().size() == 1);
    CHECK(session.remove(b) == 1);
    CHECK(session.fetchAll<RemoveChild>().empty());
}

TEST_CASE("ORM filters optional columns by value and NULL", "[orm]")
{
    TemporaryDirectory t;
    db::Table<NullableRecord>("nullable_records").column("id", &NullableRecord::id).column("year", &NullableRecord::year).column("nickname", &NullableRecord::nickname).registerTable();
    db::Database db((t.path() / "test.db").string());
    db::Registry::syncTables(db);
    auto session = db.session();
    session.insert(NullableRecord{0, 1980, "a"});
    session.insert(NullableRecord{0, 1990, std::nullopt});
    session.insert(NullableRecord{0, std::nullopt, "c"});
    const auto& year = db::col(&NullableRecord::year);
    const auto& nickname = db::col(&NullableRecord::nickname);
    CHECK(session.select<NullableRecord>().where(year == 1980).size() == 1);
    CHECK(session.select<NullableRecord>().where(year > 1970).size() == 2);
    CHECK(session.select<NullableRecord>().where(nickname == std::string("c")).size() == 1);
    CHECK(session.select<NullableRecord>().where(db::isNull(year)).size() == 1);
    CHECK(session.select<NullableRecord>().where(db::isNotNull(year) && db::isNull(nickname)).size() == 1);
    CHECK(session.select(db::col(&NullableRecord::id)).where(db::isNotNull(nickname)).size() == 2);
}

TEST_CASE("ORM writes while another connection is reading", "[orm]")
{
    TemporaryDirectory t;
    db::Table<WalRecord>("wal_records").column("id", &WalRecord::id).column("name", &WalRecord::name).registerTable();
    db::Database db((t.path() / "test.db").string(), 2);
    db::Registry::syncTables(db);
    {
        auto session = db.session();
        session.insert(WalRecord{0, "a"});
        session.insert(WalRecord{0, "b"});
    }
    std::promise<void> reading, written;
    std::thread reader([&] {
        auto session = db.session();
        db::Statement query(session, "SELECT name FROM wal_records");
        query.step();  // keeps a read transaction open until the statement is done
        reading.set_value();
        written.get_future().wait();
        while (query.step()) {
        }
    });
    reading.get_future().wait();
    {
        auto session = db.session();
        CHECK_NOTHROW(session.insert(WalRecord{0, "c"}));
    }
    written.set_value();
    reader.join();
    CHECK(db.session().fetchAll<WalRecord>().size() == 3);
}

TEST_CASE("ORM stores an empty blob as empty instead of NULL", "[orm]")
{
    TemporaryDirectory t;
    db::Table<BlobRecord>("blob_records").column("id", &BlobRecord::id).column("data", &BlobRecord::data).registerTable();
    db::Database db((t.path() / "test.db").string());
    db::Registry::syncTables(db);
    auto session = db.session();
    BlobRecord empty{0, {}};
    REQUIRE_NOTHROW(session.insert(empty));
    CHECK(session.fetch<BlobRecord>(empty.id).data.empty());
    CHECK(0 == (int)db.executeForValue("SELECT COUNT(*) FROM blob_records WHERE data IS NULL;"));
}

TEST_CASE("ORM schema sync adds columns missing in an existing table", "[orm]")
{
    TemporaryDirectory t;
    db::Table<GrownParent>("grown_parents").column("id", &GrownParent::id).column("name", &GrownParent::name).registerTable();
    db::Table<GrownRecord>("grown_records")
        .column("id", &GrownRecord::id)
        .column("name", &GrownRecord::name)
        .column("license", &GrownRecord::license)
        .column("count", &GrownRecord::count)
        .column("rating", &GrownRecord::rating)
        .column("data", &GrownRecord::data)
        .column("year", &GrownRecord::year)
        .column("parent_id", &GrownRecord::parent_id, "grown_parents", "id", db::Action::ON_UPDATE_CASCADE, db::Action::ON_DELETE_CASCADE)
        .registerTable();
    db::Database db((t.path() / "test.db").string());
    db.execute("CREATE TABLE grown_records (id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, name TEXT NOT NULL);");
    db.execute("INSERT INTO grown_records (name) VALUES ('old');");
    db::Registry::syncTables(db);
    auto report = captureCout([&] { db::Registry::syncTables(db); });
    CHECK(report.find("Missing") == std::string::npos);
    CHECK(report.find("mismatch") == std::string::npos);
    auto session = db.session();
    auto records = session.fetchAll<GrownRecord>();
    REQUIRE(records.size() == 1);
    CHECK(records[0].name == "old");
    CHECK(records[0].license.empty());
    CHECK(records[0].count == 0);
    CHECK(records[0].rating == 0.0);
    CHECK(records[0].data.empty());
    CHECK_FALSE(records[0].year);
    CHECK_FALSE(records[0].parent_id);
    GrownParent parent{0, "parent"};
    session.insert(parent);
    records[0].parent_id = parent.id;
    session.update(records[0]);
    CHECK(session.remove(parent) == 1);
    CHECK(session.fetchAll<GrownRecord>().empty());
}

TEST_CASE("ORM schema sync refuses to add a UNIQUE column to an existing table", "[orm]")
{
    TemporaryDirectory t;
    db::Table<GrownUnique>("grown_uniques").column("id", &GrownUnique::id).column("name", &GrownUnique::name).column("key", &GrownUnique::key, true).registerTable();
    db::Database db((t.path() / "test.db").string());
    db.execute("CREATE TABLE grown_uniques (id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, name TEXT NOT NULL);");
    CHECK_THROWS_AS(db::Registry::syncTables(db), db::Exception);
}

TEST_CASE("ORM schema sync rebuilds a table whose column became optional", "[orm]")
{
    TemporaryDirectory t;
    db::Table<RelaxedRecord>("relaxed_records").column("id", &RelaxedRecord::id).column("name", &RelaxedRecord::name).column("data", &RelaxedRecord::data).column("size", &RelaxedRecord::size).registerTable();
    db::Table<RelaxedChild>("relaxed_children")
        .column("id", &RelaxedChild::id)
        .column("record_id", &RelaxedChild::record_id, "relaxed_records", "id", db::Action::ON_UPDATE_CASCADE, db::Action::ON_DELETE_CASCADE)
        .registerTable();
    db::Database db((t.path() / "test.db").string());
    db.execute("CREATE TABLE relaxed_records (id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, name TEXT NOT NULL, data BLOB NOT NULL);");
    db.execute("CREATE TABLE relaxed_children (id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, record_id INTEGER NOT NULL, "
               "FOREIGN KEY(record_id) REFERENCES relaxed_records(id) ON UPDATE CASCADE ON DELETE CASCADE);");
    db.execute("INSERT INTO relaxed_records (name, data) VALUES ('old', X'0102');");
    db.execute("INSERT INTO relaxed_children (record_id) VALUES (1);");
    db::Registry::syncTables(db);
    auto report = captureCout([&] { db::Registry::syncTables(db); });
    CHECK(report.find("mismatch") == std::string::npos);
    CHECK(report.find("Rebuilding") == std::string::npos);
    auto session = db.session();
    auto records = session.fetchAll<RelaxedRecord>();
    REQUIRE(records.size() == 1);
    CHECK(records[0].name == "old");
    REQUIRE(records[0].data);
    CHECK(*records[0].data == db::Blob{1, 2});
    CHECK_FALSE(records[0].size);
    CHECK(session.fetchAll<RelaxedChild>().size() == 1);
    RelaxedRecord missing{0, "missing", std::nullopt, 128};
    REQUIRE_NOTHROW(session.insert(missing));
    CHECK_FALSE(session.fetch<RelaxedRecord>(missing.id).data);
    CHECK(0 == (int)db.executeForValue("SELECT COUNT(*) FROM sqlite_master WHERE name = 'relaxed_records_rebuild';"));
    // the foreign key of the child table still works after the rebuild
    CHECK(session.remove(records[0]) == 1);
    CHECK(session.fetchAll<RelaxedChild>().empty());
}
