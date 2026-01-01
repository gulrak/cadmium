#include <catch2/catch_test_macros.hpp>
#include <ghc/sqlite.hpp>

#include <cmath>
#include <string>
#include <random>

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
