# SQLite C++20 ORM (ghc::sqlite)

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![SQLite3](https://img.shields.io/badge/SQLite3-3.47.0-blue.svg)](https://www.sqlite.org/)
[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
![platforms](https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux-blue)
[![Build Status](https://github.com/gulrak/ghc-sqlite/actions/workflows/ci.yml/badge.svg)](https://github.com/gulrak/ghc-sqlite/actions/workflows/ci.yml)

> :warning: **Still highly experimental**: The project is in active development and API may change,
> and there sure are bugs! Use at your own risk!

ghc::sqlite is a minimal, modern C++20 ORM for SQLite3. It it is based on SQLite3 code I wrote
in 2011 for some internal projects. This year, while working on my CHIP-8 environment Cadmium,
I decided to use an ORM for some database work and added enough feature to fulfill the needs
of that project.

## Features

- Describe your data as plain structs
- Register those structs as tables
- Auto-generate and sync schema
- Use type-safe, composable query expressions
- Perform CRUD via a simple `Session`

## Limitations

- All records are expected to have an `id` column of type `int`or `int64_t` as the first registered element
- The id is the primary key
- No support for composite primary- or foreign-keys
- No join queries
- No support for views, triggers
- lots of other things I didn't think of yet...

The library grows with my needs, and I might accept contributions, but I feel it is too early to
consider it production-ready or ready for general use. At this stage, it is more of a proof-of-concept.

So please don't ask me if or when it will support a specific feature yet, and don't put effort into huge
changes, as I might find them not fitting my direction and reject them. Be warned. :wink:

The library bundles SQLite3 3.47.0 (amalgamated) and exposes a small, RAII-friendly API.

---

## Table of Contents

<!-- TOC -->
* [What it looks like (Quick Start)](#what-it-looks-like-quick-start)
* [Installation](#installation)
* [Core Concepts](#core-concepts)
  * [Database and Session](#database-and-session)
  * [Defining Models (structs)](#defining-models-structs)
  * [Registering Tables](#registering-tables)
  * [Schema Sync](#schema-sync)
  * [Transactions](#transactions)
* [CRUD Operations](#crud-operations)
* [Querying](#querying)
  * [Selecting specific fields](#selecting-specific-fields)
* [Relationships and Constraints](#relationships-and-constraints)
  * [Foreign keys](#foreign-keys)
  * [Unique constraints](#unique-constraints)
  * [Optional (nullable) fields](#optional-nullable-fields)
  * [BLOB fields](#blob-fields)
* [Manual SQL (Statement)](#manual-sql-statement)
* [Error handling](#error-handling)
* [Threading & Concurrency](#threading--concurrency)
* [Logging](#logging)
* [Building & Running Tests](#building--running-tests)
* [FAQ](#faq)
* [License](#license)
<!-- TOC -->

---

## What it looks like (Quick Start)

```cpp
#include <ghc/sqlite.hpp>
namespace db = ghc::sqlite;

struct User {
    int64_t id;
    std::string name;
    std::string email;
};

// 1) Register your struct as a table
void registerTables() {
    db::Table<User>("users")
        .column("id", &User::id)     // INTEGER PRIMARY KEY AUTOINCREMENT
        .column("name", &User::name) // TEXT NOT NULL
        .column("email", &User::email) // TEXT NOT NULL
        .registerTable();
}

int main() {
    registerTables();

    db::Database database("example.db");
    auto session = database.session();

    // 2) Sync schema (creates tables if needed)
    db::Registry::syncTables(database);

    // 3) Insert
    User u{0, "Ada", "ada@example.org"};
    session.insert(u); // sets u.id to last insert row id

    // 4) Query
    using db::col;
    auto users = session.select<User>().where(col(&User::name) == std::string("Ada"));
    for (auto& row : users) {
        // row is User
    }

    // 5) Update
    u.email = "ada.lovelace@example.org";
    session.update(u);

    // 6) Fetch by id
    auto one = session.fetch<User>(u.id);

    return 0;
}
```

---

## Installation

Requirements:

- C++20 compatible compiler
- CMake 3.21+
- No external SQLite needed (amalgamation bundled)

Include in your CMake project by adding this repository and linking against the static library target `sqlite-static` (alias `ghc::sqlite`). Public headers live under `include/ghc/`.

---

## Core Concepts

### Database and Session

- `db::Database` manages one or more SQLite connections and is safe to use across threads. Constructor:
  - `Database(const std::string& path, uint8_t concurrency = 1)`
- `db::Session` is a lightweight view that executes statements and ORM operations on a `Database`.

```cpp
db::Database db{"app.db"};
db::Session session{db};

db.execute("PRAGMA journal_mode=WAL;");
auto val = db.executeForValue("SELECT 1;"); // ValueRef convertible to int/double/string
```

### Defining Models (structs)

Define plain C++ structs with fields you want to persist. Supported field types include:

- `int`, `int64_t`, other integral types → INTEGER
- `double`, `float` → REAL
- `std::string` → TEXT
- `std::vector<uint8_t>` (or `std::vector<int8_t>`) → BLOB
- `std::optional<T>` of the above → nullable columns

Every table must have an `id` field of type `int64_t` as the first registered column.

```cpp
struct Document {
    int64_t id;
    std::string title;
    std::vector<uint8_t> content;
    std::optional<std::string> description; // becomes TEXT NULL
    int64_t user_id; // foreign key to users.id (see below)
};
```

### Registering Tables

Use `db::Table<T>("table_name")` to declare columns and then `.registerTable()` once at startup.

```cpp
void registerTables() {
    db::Table<User>("users")
        .column("id", &User::id)
        .column("name", &User::name)
        .column("email", &User::email, /*isUnique=*/false)
        .registerTable();

    db::Table<Document>("documents")
        .column("id", &Document::id)
        .column("title", &Document::title)
        .column("content", &Document::content)
        // foreign key: ref table + ref column + actions
        .column("user_id", &Document::user_id, "users", "id",
                db::Action::ON_UPDATE_CASCADE, db::Action::ON_DELETE_CASCADE)
        .registerTable();
}
```

Notes:

- The first column you register must be the `id` and becomes `INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL`.
- For unique constraints on a single column: `.column("email", &User::email, /*isUnique=*/true)`.

### Schema Sync

Call `db::Registry::syncTables(db);` once after registration to create or evolve tables. It inspects SQLite schema and applies non-destructive changes when possible.

```cpp
registerTables();
db::Database db{"app.db"};
db::Registry::syncTables(db);
```

### Transactions

`db::Transaction` is RAII. Commit explicitly or it will roll back on destruction.

```cpp
db::Transaction tx{session};

User u{0, "Grace", "grace@example.org"};
session.insert(u);

tx.commit(); // or tx.commitAndReopen() to start a fresh transaction immediately
```

---

## CRUD Operations

All CRUD is performed via `db::Session`.

```cpp
db::Session session{db};

User u{0, "Linus", "linus@example.org"};

// Insert (sets u.id)
session.insert(u);

// Upsert-like helper (insert if id == 0 else update)
session.insertOrUpdate(u);

// Update
u.email = "linus.t@example.org";
session.update(u);

// Fetch by id
auto fetched = session.fetch<User>(u.id);

// Fetch all rows
auto allUsers = session.fetchAll<User>(); // std::vector<User>
```

---

## Querying

Use `session.select<T>()` with expression builders. To refer to a column, use `db::col(&T::member)`. Combine predicates using `&&`, `||`, and `!`. Comparison operators are overloaded. Use `db::like(col, pattern)` for `LIKE`.

```cpp
using db::col;

// All users with name == "Ada"
auto users = session.select<User>().where(col(&User::name) == std::string("Ada"));

// First (throws if empty)
const User& first = users.first();

// Optional single
auto maybeOne = users.one(); // std::optional<User>

// Value or default
User byDefault = users.valueOrDefault(User{0, "", ""});

// Complex expressions
auto result = session.select<User>()
    .where((col(&User::name) == std::string("Ada"))
        || db::like(col(&User::email), "%@example.org"))
    .order_by("name", /*desc=*/false)
    .limit(10)
    .offset(0);

for (auto& u : result) {
    // ...
}
```

### Selecting specific fields

Select a single field/column when you only need one value type.

```cpp
// Select all emails (QueryResult<std::string>)
auto emails = session.select<User, std::string>(db::col(&User::email))
                   .where(db::like(db::col(&User::email), "%@example.org"));

for (const auto& e : emails) {
    // use e (std::string)
}
```

---

## Relationships and Constraints

### Foreign keys

Use the `.column(name, &T::member, refTable, refColumn, onUpdate, onDelete)` overload.

```cpp
db::Table<Document>("documents")
    .column("id", &Document::id)
    .column("title", &Document::title)
    .column("content", &Document::content)
    .column("user_id", &Document::user_id, "users", "id",
            db::Action::ON_UPDATE_CASCADE, db::Action::ON_DELETE_CASCADE)
    .registerTable();
```

### Unique constraints

Pass `true` as the 3rd argument to the simple `.column` overload.

```cpp
db::Table<User>("users")
    .column("id", &User::id)
    .column("email", &User::email, /*isUnique=*/true)
    .registerTable();
```

### Optional (nullable) fields

Use `std::optional<T>` in your struct to allow NULL values.

```cpp
struct Profile {
    int64_t id;
    std::optional<std::string> nickname; // TEXT NULL
};

db::Table<Profile>("profiles")
    .column("id", &Profile::id)
    .column("nickname", &Profile::nickname)
    .registerTable();
```

### BLOB fields

Use `std::vector<uint8_t>` (or `std::vector<int8_t>`) for BLOB columns.

```cpp
struct BinaryAsset { int64_t id; std::vector<uint8_t> data; };
db::Table<BinaryAsset>("assets").column("id", &BinaryAsset::id)
    .column("data", &BinaryAsset::data)
    .registerTable();
```

---

## Manual SQL (Statement)

You can always drop down to raw SQL with `db::Statement`. Bind parameters by index, by name, or via stream operators.

```cpp
db::Statement insert{session, "INSERT INTO logs VALUES(?,?,?,?);"};
insert.bind(1, 1);
insert.bind(2, 3.14);
insert.bind(3, 1234);
insert.bind(4, "text");
insert.step();

// Stream style (supports db::null, db::step, db::execute)
db::Statement ins2{session, "INSERT INTO logs VALUES(?,?,?,?,?);"};
ins2 << 1 << 3.14 << 1234 << "hello" << db::null << db::execute;

// Reading results
db::Statement query{session, "SELECT id, value FROM logs ORDER BY id"};
int64_t id; double value;
while (query.step()) {
    query >> id >> value;
}
```

---

## Error handling

Operations throw `db::Exception` on failure.

```cpp
try {
    session.insert(u);
} catch (const db::Exception& ex) {
    // handle / log ex.what()
}
```

---

## Threading & Concurrency

`db::Database` can manage multiple SQLite connections for multi-threaded access. Set the `concurrency` in the constructor to the number of simultaneous connections you expect. Statements block if no free connection is available until one is released.

```cpp
db::Database db{"app.db", /*concurrency=*/4};
```

---

## Logging

The library uses it's own minimal logging framework. To facilitate your own logging, define
GHC_CUSTOM_LOGGER_PROVIDED and needs to provide the macros:
 * `INFO_LOG(fmt_str, ...)`
 * `ERROR_LOG(fmt_str, ...)`
 * `WARNING_LOG(fmt_str, ...)`
 * `DEBUG_LOG(fmt_str, ...)` (only expected to be expanding to actual logging on debug builds)


## Building & Running Tests

The project uses CMake and Catch2 v3. In CLion, prefer building targets:

- Library: `sqlite-static` (alias `ghc::sqlite`)
- Tests: `sqlite-tests`, `cadmium-tests`

From the build directory/profile (example shown for a Debug profile):

```bash
cmake --build cmake-build-debug --target sqlite-tests && ./cmake-build-debug/test/sqlite-tests
```

Run all with CTest:

```bash
ctest --output-on-failure
```

---

## FAQ

- Do I have to write CREATE TABLE SQL?  
  No. Register your structs and call `Registry::syncTables(db)`.

- How do I refer to columns in expressions?  
  Use `db::col(&T::member)`.

- Can I select only one field type?  
  Yes: `session.select(db::col(&SomeClass::field)).where(...);`

- How are NULLs represented?  
  Use `std::optional<T>` in structs. In query results, missing rows can be handled via `QueryResult::one()` returning `std::optional<T>`.

- Can I use raw SQL when needed?  
  Yes, via `db::Statement` with rich binding and extraction support.

---

## License

MIT — see [LICENSE](LICENSE).
