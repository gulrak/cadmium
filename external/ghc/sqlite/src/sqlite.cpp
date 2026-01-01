///////////////////////////////////////////////////////////////////////////
//
//  FILE:
//  nucleus/database.cpp
//
//  Copyright (c) 2011-2014 Schümann Development. All rights reserved.
//
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
///////////////////////////////////////////////////////////////////////////

#include <ghc/sqlite.hpp>

#include <ghc/logger.hpp>
#include "sqlite3/sqlite3.h"

#include <algorithm>
#include <iostream>

#ifdef WITH_ENCRYPTION
#if SQLITE_VERSION_NUMBER>=3032000
#error "Custom encryption is not supported on this version of SQLite3"
#endif
extern "C" {
int sqlite3_key_v2(sqlite3* db, const char* zDbName, const void* pKey, int nKey);
}
#endif

#include <cstdint>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

inline std::uint64_t thisThreadId() noexcept {
    return static_cast<std::uint64_t>(::GetCurrentThreadId());
}

#elif defined(__APPLE__)
#include <pthread.h>

inline std::uint64_t thisThreadId() noexcept {
    std::uint64_t tid = 0;
    // pthread_threadid_np returns 0 on success
    (void)::pthread_threadid_np(nullptr, &tid);
    return tid;
}

#elif defined(__linux__)
#include <unistd.h>
#include <sys/syscall.h>

inline std::uint64_t thisThreadId() noexcept {
    // syscall(SYS_gettid) returns pid_t (signed), but thread ids are non-negative.
    return static_cast<std::uint64_t>(::syscall(SYS_gettid));
}

#else
#error "thisThreadId(): unsupported platform (need Windows, macOS, or Linux)."
#endif

#define SQLITE3X(call)                                    \
    do {                                                  \
        if (sqlite3_##call != SQLITE_OK)                  \
            throw Exception(sqlite3_errmsg(_connection)); \
    } while (0)

#define SQLITE3(call, connection, ...)                            \
    do {                                                          \
        if (sqlite3_##call(connection, __VA_ARGS__) != SQLITE_OK) \
            throw Exception(sqlite3_errmsg(connection));          \
    } while (0)

#define SQLITE3_STMT(call, connection, ...)              \
    do {                                                 \
        if (sqlite3_##call(__VA_ARGS__) != SQLITE_OK)    \
            throw Exception(sqlite3_errmsg(connection)); \
    } while (0)

namespace ghc::sqlite {

//-------------------------------------------------------------------------------------------------

struct Session::SharedSession
{
    Database& _db;
    sqlite3* _connection;

    explicit SharedSession(Database& db)
        : _db(db)
        , _connection(_db.acquireConnection())
    {
    }

    ~SharedSession()
    {
        if (_connection) {
            _db.releaseConnection(_connection);
        }
    }
};

ConstraintViolationException::ConstraintViolationException(const std::string& msg)
    : Exception(msg)
{
}

Session::Session(Database& db)
    : _sharedSession(std::make_shared<SharedSession>(db))
{
}

Session::~Session() {}

sqlite3* Session::connection() const
{
    return _sharedSession->_connection;
}

class StatementHandle
{
public:
    StatementHandle(sqlite3_stmt* stmt)
        : _stmt(stmt)
    {
    }
    ~StatementHandle()
    {
        if (_stmt) {
            sqlite3_finalize(_stmt);
        }
    }

    sqlite3_stmt* get() const { return _stmt; }

private:
    sqlite3_stmt* _stmt;
};

//-------------------------------------------------------------------------------------------------

Transaction::Transaction(Session& session)
    : _session(session)

{
    Statement stmt(_session, "BEGIN TRANSACTION;");
    stmt.step();
    _inTransaction = true;
}

Transaction::~Transaction()
{
    if (_inTransaction) {
        Statement stmt(_session, "ROLLBACK TRANSACTION;");
        stmt.step();
        _inTransaction = false;
    }
}

void Transaction::commit()
{
    if (!_inTransaction) {
        throw Exception("Commit without open transaction!");
    }
    Statement stmt(_session, "COMMIT TRANSACTION;");
    stmt.step();
    _inTransaction = false;
}

void Transaction::commitAndReopen()
{
    commit();
    Statement stmt(_session, "BEGIN TRANSACTION;");
    stmt.step();
    _inTransaction = true;
}

//-------------------------------------------------------------------------------------------------

ValueRef::ValueRef(const Session& session, const std::shared_ptr<StatementHandle>& stmt, int idx)
    : _session(session)
    , _stmt(stmt)
    , _idx(idx)
{
    _type = sqlite3_column_type(_stmt->get(), idx);
}

ValueRef::operator bool() const
{
    return sqlite3_column_int(_stmt->get(), _idx) != 0;
}

ValueRef::operator int() const
{
    return sqlite3_column_int(_stmt->get(), _idx);
}

ValueRef::operator int64_t() const
{
    return sqlite3_column_int64(_stmt->get(), _idx);
}

ValueRef::operator double() const
{
    return sqlite3_column_double(_stmt->get(), _idx);
}

std::string_view ValueRef::asStringView() const
{
    return reinterpret_cast<const char*>(sqlite3_column_text(_stmt->get(), _idx));
}

ValueRef::operator std::string() const
{
    const char* ptr = reinterpret_cast<const char*>(sqlite3_column_text(_stmt->get(), _idx));
    int len = sqlite3_column_bytes(_stmt->get(), _idx);
    return ptr ? std::string(ptr, ptr + len) : std::string();
}

ValueRef::operator Blob() const
{
    const auto* ptr = static_cast<const std::uint8_t*>(sqlite3_column_blob(_stmt->get(), _idx));
    int len = sqlite3_column_bytes(_stmt->get(), _idx);
    return ptr && len > 0 ? Blob(ptr, ptr + len) : Blob{};
}

bool ValueRef::isNull() const
{
    return _type == SQLITE_NULL;
}

//-------------------------------------------------------------------------------------------------

Statement::Statement(Session& session, const std::string& query)
    : _session(session)
    , _resultColumns(0)
    , _bindColumnIdx(1)
    , _resultColumnIdx(0)
{
    sqlite3_stmt* raw_stmt;
    SQLITE3(prepare_v2, connection(), query.c_str(), -1, &raw_stmt, nullptr);
    _stmt = std::make_shared<StatementHandle>(raw_stmt);
}

Statement::~Statement() = default;

void Statement::reset()
{
    sqlite3_reset(_stmt->get());
    _resultTypes.clear();
    _bindColumnIdx = 1;
    _resultColumnIdx = 0;
    _resultColumns = 0;
}

int Statement::parameterIndexForName(const std::string& name)
{
    int rc = sqlite3_bind_parameter_index(_stmt->get(), name.c_str());
    if (rc == 0) {
        throw Exception(std::string("No parameter named '") + name + "'");
    }
    return rc;
}

void Statement::bind(int idx, const int& value)
{
    SQLITE3_STMT(bind_int, connection(), _stmt->get(), idx, value);
}

void Statement::bind(int idx, const int64_t& value)
{
    SQLITE3_STMT(bind_int64, connection(), _stmt->get(), idx, value);
}

void Statement::bind(int idx, const double& value)
{
    SQLITE3_STMT(bind_double, connection(), _stmt->get(), idx, value);
}

void Statement::bind(int idx, const char* value)
{
    SQLITE3_STMT(bind_text, connection(), _stmt->get(), idx, value, -1, SQLITE_TRANSIENT);
}

void Statement::bind(int idx, const std::string& value)
{
    SQLITE3_STMT(bind_text, connection(), _stmt->get(), idx, value.c_str(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
}

void Statement::bind(int idx, const Blob& value)
{
    SQLITE3_STMT(bind_blob, connection(), _stmt->get(), idx, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
}

void Statement::bindNull(int idx)
{
    SQLITE3_STMT(bind_null, connection(), _stmt->get(), idx);
}

void Statement::bind(int idx, const Parameter& value)
{
    std::visit(
        [this, idx](const auto& val) {
            if constexpr (std::is_same_v<std::decay_t<decltype(val)>, std::nullptr_t>) {
                this->bindNull(idx);
            }
            else {
                this->bind(idx, val);
            }
        },
        value);
}

bool Statement::step()
{
    _resultColumnIdx = -1;
    int rc = sqlite3_step(_stmt->get());
    if (rc == SQLITE_DONE) {
        return false;
    }
    else if (rc == SQLITE_ROW) {
        _resultColumnIdx = 0;
        if (!_resultColumns) {
            _resultColumns = sqlite3_column_count(_stmt->get());
            if (_resultColumns) {
                _resultTypes.reserve(static_cast<size_t>(_resultColumns));
                for (int i = 0; i < _resultColumns; ++i) {
                    _resultTypes.push_back(sqlite3_column_type(_stmt->get(), i));
                }
            }
        }
        return true;
    }
    else {
        if (rc != SQLITE_OK) {
            throw Exception(sqlite3_errmsg(connection()));
        }
        return false;
    }
}

int64_t Statement::execute()
{
    int rc = sqlite3_step(_stmt->get());
    if (rc == SQLITE_DONE) {
        return sqlite3_last_insert_rowid(connection());
    }
    if (rc == SQLITE_ROW) {
        throw Exception("ghc::sqlite::Statement::execute() called for a query statement with results!");
    }
    if (rc == SQLITE_CONSTRAINT) {
        throw ConstraintViolationException(sqlite3_errmsg(connection()));
    }
    if (rc != SQLITE_OK) {
        throw Exception(sqlite3_errmsg(connection()));
    }
    return 0;
}

ValueRef Statement::column(int idx) const
{
    return {_session, _stmt, idx};
}

std::string Statement::columnName(int idx) const
{
    return sqlite3_column_name(_stmt->get(), idx);
}

size_t Statement::columnCount() const
{
    return static_cast<size_t>(_resultColumns);
}
size_t Statement::parameterCount() const
{
    return static_cast<size_t>(sqlite3_bind_parameter_count(_stmt->get()));
}

int Statement::changes() const
{
    return sqlite3_changes(connection());
}

sqlite3* Statement::connection() const
{
    return _session.connection();
}

Statement& Statement::operator<<(Statement& (*f)(Statement&))
{
    return f(*this);
}

int64_t Statement::operator<<(int64_t (*f)(Statement&))
{
    return f(*this);
}

Statement& null(Statement& stmt)
{
    stmt.bindNull(stmt._bindColumnIdx++);
    return stmt;
}

bool step(Statement& stmt)
{
    return stmt.step();
}

int64_t execute(Statement& stmt)
{
    return stmt.execute();
}

//-------------------------------------------------------------------------------------------------

Database::Database(const std::string& path, uint8_t concurrency)
{
    for (int i = 0; i < concurrency; ++i) {
        ::sqlite3* connection;
        if (sqlite3_open(path.c_str(), &connection) != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(connection);
            sqlite3_close(connection);
            throw Exception(msg);
        }
        _freeConnections.push_back(connection);
    }
    for (auto& connnection : _freeConnections) {
        sqlite3_busy_timeout(connnection, 1000);
        sqlite3_exec(connnection, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
    }
}

#ifdef WITH_ENCRYPTION
Database::Database(const std::string& path, const std::string& hexUMK, const std::string& hexDLK, uint8_t concurrency)
{
    for (int i = 0; i < concurrency; ++i) {
        ::sqlite3* connection;
        if (sqlite3_open(path.c_str(), &connection) != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(connection);
            sqlite3_close(connection);
            throw Exception(msg);
        }
        sqlite3_key_v2(connection, nullptr, (hexUMK + ":" + hexDLK).c_str(), static_cast<int>(hexUMK.size() + hexDLK.size() + 1));
        _freeConnections.push_back(connection);
    }
}
#endif

Database::~Database()
{
    bool loop = false;
    do {
        {
            std::unique_lock<std::mutex> lock(_mutex);
            for (::sqlite3* connection : _freeConnections) {
                sqlite3_close(connection);
            }
            _freeConnections.clear();
            loop = !_usedConnections.empty();
        }
        if (loop) {
            ERROR_LOG("Database destructor reached with {} free connections still active!", _freeConnections.size());
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } while (loop);
}

Session Database::session()
{
    return Session(*this);
}

int Database::execute(const std::string& query)
{
    Session session(*this);
    Statement stmt(session, query);
    stmt.step();
    return stmt.changes();
}

ValueRef Database::executeForValue(const std::string& query)
{
    Session session(*this);
    Statement stmt(session, query);
    if (!stmt.step()) {
        throw Exception("No record returned!");
    }
    return stmt.column(0);
}

int64_t Database::lastInsertRowId()
{
    return sqlite3_last_insert_rowid(acquireConnection());
}

::sqlite3* Database::acquireConnection()
{
    std::unique_lock<std::mutex> lock(_mutex);
    std::thread::id tid = std::this_thread::get_id();
    auto uci = _usedConnections.find(tid);
    DEBUG_LOG("trying to acquire db connection for thread {}", thisThreadId());
    if (uci != _usedConnections.end()) {
        ++(uci->second.first);
        return uci->second.second;
    }
    else {
        _condition.wait(lock, [this] { return !_freeConnections.empty(); });
        ::sqlite3* connection = _freeConnections.front();
        _usedConnections[tid] = std::make_pair(1, connection);
        _freeConnections.pop_front();
        return connection;
    }
}

void Database::releaseConnection(::sqlite3* connection)
{
    std::unique_lock<std::mutex> lock(_mutex);
    std::thread::id tid = std::this_thread::get_id();
    auto uci = _usedConnections.find(tid);
    DEBUG_LOG("releasing db connection for thread {}", thisThreadId());
    if (uci != _usedConnections.end()) {
        if (--(uci->second.first) == 0) {
            _usedConnections.erase(tid);
            _freeConnections.push_back(connection);
            _condition.notify_one();
        }
    }
    else {
        _freeConnections.push_back(connection);
        _condition.notify_one();
    }
}

const std::string& BaseTableMetadata::insertSql() const
{
    if (_insertSql.empty()) {
        std::string sql = "INSERT INTO \"" + tableName() + "\" (";
        std::string values;
        bool first = true;
        for (size_t i = 1; i < fields().size(); ++i) {  // Skip id field
            if (!first) {
                sql += ", ";
                values += ", ";
            }
            first = false;
            sql += "\"" + fields()[i]->name + "\"";
            values += "?";
        }
        sql += ") VALUES (" + values + ")";
        _insertSql = sql;
    }
    return _insertSql;
}

const std::string& BaseTableMetadata::updateSql() const
{
    if (_updateSql.empty()) {
        std::string sql = "UPDATE \"" + tableName() + "\" SET ";
        bool first = true;
        for (size_t i = 1; i < fields().size(); ++i) {  // Skip id field
            if (!first) {
                sql += ", ";
            }
            first = false;
            sql += "\"" + fields()[i]->name + "\" = ?";
        }
        sql += " WHERE id = ?";
        _updateSql = sql;
    }
    return _updateSql;
}
bool BaseTableMetadata::hasField(const std::string& fieldName) const
{
    for (const auto& field : fields()) {
        if (field->name == fieldName) {
            return true;
        }
    }
    return false;
}

void Registry::doSyncTables(Database& db) const
{
    auto session = db.session();
    Statement stmt(session, "SELECT name FROM sqlite_master WHERE type='table';");
    std::unordered_set<std::string> existingTables;
    while (stmt.step()) {
        existingTables.insert(stmt.column(0));
    }

    for (const auto& table : _tables) {
        if (existingTables.find(table->tableName()) == existingTables.end()) {
            std::cout << "Missing table '" << table->tableName() << "'. Create with:\n";
            std::cout << table->createTableSql() << "\n";
            Transaction transaction(session);
            Statement createStmt(session, table->createTableSql());
            createStmt.step();
            for (auto& field : table->fields()) {
                if (field->isUnique) {
                    Statement createIdxStmt(session, "CREATE UNIQUE INDEX \"" + table->tableName() + "_" + field->name + "\" ON \"" + table->tableName() + "\" (" + field->name + ");");
                    createIdxStmt.step();
                }
            }
            transaction.commit();
        }
        else {
            auto schema = getSQLiteTableSchema(db, table->tableName());
            analyzeTableDifferences(db, table->tableName(), schema, table->fields());
        }
    }
}

std::string Registry::escapeIdent(const std::string& s)
{
    std::string out;
    out.reserve(s.size() * 2);
    out.push_back('"');
    for (char c : s) {
        if (c == '"')
            out.push_back('"');
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

void Registry::fetchForeignKeyInfo(Session& session, const std::string& tableName, std::unordered_map<std::string, ForeignKeyInfo>& foreignKeys)
{
    Statement fkStmt(session, "PRAGMA foreign_key_list(\"" + tableName + "\");");
    while (fkStmt.step()) {
        ForeignKeyInfo fki;
        std::string localColumn = std::string(fkStmt.column(3));
        fki.refTable = std::string(fkStmt.column(2));
        fki.refColumn = std::string(fkStmt.column(4));
        // TODO: maybe support SET NULL/DEFAULT/RESTRICT later
        fki.on_update = std::string(fkStmt.column(5)) == "CASCADE" ? Action::ON_UPDATE_CASCADE : Action::NO_ACTION;
        fki.on_delete = std::string(fkStmt.column(6)) == "CASCADE" ? Action::ON_DELETE_CASCADE : Action::NO_ACTION;
        foreignKeys.emplace(localColumn, fki);
    }
}

std::vector<Registry::SQLiteTableInfo> Registry::getSQLiteTableSchema(Database& db, const std::string& tableName)
{
    std::vector<SQLiteTableInfo> columns;
    std::unordered_map<std::string, ForeignKeyInfo> foreignKeys;
    auto session = db.session();
    fetchForeignKeyInfo(session, tableName, foreignKeys);
    Statement stmt(session, "PRAGMA table_info(\"" + tableName + "\");");
    while (stmt.step()) {
        SQLiteTableInfo info;
        bool notNull;
        stmt >> info.name >> info.type >> notNull;
        info.isOptional = !notNull;
        if ((int)stmt.column(5) > 0) {
            auto it = foreignKeys.find(info.name);
            if (it != foreignKeys.end()) {
                info.isUnique = false;
                info.fkInfo = it->second;
            }
        }
        if (!stmt.column(4).isNull()) {
            info.defaultValue = std::string(stmt.column(4));
        }
        columns.push_back(info);
    }
    return columns;
}
std::set<std::vector<std::string>> Registry::getSQLiteUniqueIndexes(Database& db, const std::string& tableName)
{
    std::set<std::vector<std::string>> uniques;
    auto session = db.session();
    Statement idxList(session, "PRAGMA index_list(\"" + tableName + "\");");
    while (idxList.step()) {
        const std::string idxName = std::string(idxList.column(1));
        const int unique = (int)idxList.column(2);
        if (!unique)
            continue;
        // Fetch columns for this index
        Statement idxInfo(session, "PRAGMA index_info(\"" + idxName + "\");");
        std::vector<std::string> cols;
        while (idxInfo.step()) {
            cols.emplace_back(std::string(idxInfo.column(2)));  // column name
        }
        if (!cols.empty()) {
            uniques.insert(cols);
        }
    }
    return uniques;
}

void Registry::analyzeTableDifferences(Database& db, const std::string& tableName, const std::vector<SQLiteTableInfo>& existing, const std::vector<std::unique_ptr<BaseFieldInfo>>& required)
{
    std::cout << "Analyzing table '" << tableName << "':\n";
    auto existingUniqueIdxCols = getSQLiteUniqueIndexes(db, tableName);
    // Check for missing columns
    for (const auto& reqCol : required) {
        auto it = std::find_if(existing.begin(), existing.end(), [&reqCol](const SQLiteTableInfo& info) { return info.name == reqCol->name; });

        if (it == existing.end()) {
            std::cout << "Missing column: " << reqCol->name << "\n";
            std::cout << "ALTER TABLE \"" << tableName << "\" ADD COLUMN \"" << reqCol->name << "\" ";
            switch (reqCol->sqlType) {
                case SqlType::INTEGER:
                    std::cout << "INTEGER";
                    break;
                case SqlType::REAL:
                    std::cout << "REAL";
                    break;
                case SqlType::TEXT:
                    std::cout << "TEXT";
                    break;
                case SqlType::BLOB:
                    std::cout << "BLOB";
                    break;
                default:
                    break;
            }
            if (!reqCol->isOptional)
                std::cout << " NOT NULL";
            std::cout << ";\n";
        }
        else {
            if (it->isOptional != reqCol->isOptional) {
                std::cout << "Column '" << it->name << "' optional mismatch: " << it->isOptional << " != " << reqCol->isOptional << "\n";
            }
            if (it->isUnique != reqCol->isUnique) {
                std::cout << "Column '" << it->name << "' unique mismatch: " << it->isUnique << " != " << reqCol->isUnique << "\n";
            }
            if (it->fkInfo != reqCol->fkInfo) {
                std::cout << "Column '" << it->name << "' foreign key mismatch: " << it->fkInfo.has_value() << " != " << reqCol->fkInfo.has_value() << "\n";
            }
        }
    }

    // Check for extra columns
    for (const auto& existCol : existing) {
        auto it = std::find_if(required.begin(), required.end(), [&existCol](const std::unique_ptr<BaseFieldInfo>& info) { return info->name == existCol.name; });

        if (it == required.end()) {
            std::cout << "Extra column found: " << existCol.name << "\n";
        }
    }
}

}  // namespace ghc::sqlite
