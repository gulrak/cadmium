//---------------------------------------------------------------------------------------
// ghc/sqlite.hpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2011-2025, Steffen Schümann <s.schuemann@pobox.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
//---------------------------------------------------------------------------------------
#pragma once

#include <condition_variable>
#include <cstddef>
#include <list>
#include <map>
#include <mutex>
#include <optional>
#include <ranges>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#define GHC_SQLITE_VERSION "1.0.0"
#define GHC_SQLITE_VERSION_MAJOR 1
#define GHC_SQLITE_VERSION_MINOR 0
#define GHC_SQLITE_VERSION_PATCH 0

extern "C" {
struct sqlite3;
struct sqlite3_stmt;
}

namespace ghc::sqlite {
struct BaseFieldInfo;

class Database;
class Session;
class Statement;
class StatementHandle;
class Transaction;
class BaseTableMetadata;

template <typename T>
struct is_std_optional_impl : std::false_type
{
};

template <typename U>
struct is_std_optional_impl<std::optional<U>> : std::true_type
{
};

template <typename T>
inline constexpr bool is_std_optional_v = is_std_optional_impl<std::remove_cv_t<std::remove_reference_t<T>>>::value;

template <typename T>
concept StdOptional = is_std_optional_v<T>;

template <class T>
concept OrmRecord = std::is_aggregate_v<T> && std::is_standard_layout_v<T> && requires(T t) {
    requires std::is_integral_v<std::remove_cvref_t<decltype(t.id)>>;
    { t.id = std::remove_cvref_t<decltype(t.id)>{} };
};

template <typename T>
concept ValidColumnType = std::is_arithmetic_v<T> || std::is_same_v<T, std::string> || std::is_same_v<T, const char*> || std::is_same_v<T, std::vector<uint8_t>>;

using Blob = std::vector<uint8_t>;
using Parameter = std::variant<std::nullptr_t, int, int64_t, double, std::string, Blob>;

namespace detail {

template <std::ranges::input_range R>
std::string join(R&& r, std::string_view delimiter);

}

/// An exception class for Exceptions during database activities.
class Exception : public std::runtime_error
{
public:
    explicit Exception(const std::string& msg);
};

class ConstraintViolationException : public Exception
{
public:
    explicit ConstraintViolationException(const std::string& msg);
};

// Concepts to keep expressions from multiple tables from mixing.
template <class A, class B>
concept SameTable = std::is_same_v<typename A::table_type, typename B::table_type>;

template <class T>
class QueryResult
{
public:
    using value_type = T;
    using vector_type = std::vector<T>;
    using iterator = typename vector_type::iterator;
    using const_iterator = typename vector_type::const_iterator;

    QueryResult();
    explicit QueryResult(vector_type v);

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

    iterator begin() noexcept;
    iterator end() noexcept;
    const_iterator begin() const noexcept;
    const_iterator end() const noexcept;

    T& operator[](std::size_t i) noexcept;
    const T& operator[](std::size_t i) const noexcept;

    template <class... Args>
    T& emplace_back(Args&&... args);
    void push_back(T v);

    const vector_type& asVector() const& noexcept;
    vector_type& asVector() & noexcept;

    vector_type intoVector() && noexcept;

    explicit operator const vector_type&() const& noexcept;
    explicit operator vector_type() && noexcept;

    template <class FT>
        requires std::convertible_to<FT, T>
    T valueOrDefault(FT&& defaultValue) const;

    std::optional<T> one() const;

    const T& expectOne() const;

    const T& first() const;

private:
    vector_type _data;
};

template <class Expr>
struct ExprBase
{
};

// Leaf: `col OP value`
template <class Col, class Val, class OpTag>
struct Cmp : ExprBase<Cmp<Col, Val, OpTag>>
{
    using table_type = typename Col::table_type;
    const Col& col;
    Val val;  // store by value; cheap and safe
};

// Unary: NOT
template <class E>
struct Not : ExprBase<Not<E>>
{
    using table_type = typename E::table_type;
    E e;
};

// Binary: AND / OR
template <class L, class R, class Tag>
struct Bin : ExprBase<Bin<L, R, Tag>>
{
    static_assert(SameTable<L, R>, "AND/OR across different tables is not allowed");
    using table_type = typename L::table_type;
    L l;
    R r;
};

// clang-format off
struct OpEq {};
struct OpNe {};
struct OpLt {};
struct OpLe {};
struct OpGt {};
struct OpGe {};
struct OpAnd {};
struct OpOr {};
struct OpLike {};
// clang-format on

enum class SqlType { NULL_TYPE = 0, INTEGER = 1, REAL = 2, TEXT = 3, BLOB = 4 };
enum class Action { NO_ACTION, ON_CONFLICT_IGNORE, ON_CONFLICT_ABORT, ON_UPDATE_CASCADE, ON_DELETE_CASCADE };

template <typename T>
constexpr SqlType getSqlType();

struct ForeignKeyInfo
{
    auto operator<=>(const ForeignKeyInfo&) const = default;
    std::string refTable;
    std::string refColumn;
    Action on_update{Action::NO_ACTION};
    Action on_delete{Action::NO_ACTION};
};

struct BaseFieldInfo
{
    std::string tableTypeName;
    std::string name;
    SqlType sqlType;
    size_t offset;
    bool isOptional;
    bool isUnique;
    std::optional<ForeignKeyInfo> fkInfo;

    BaseFieldInfo(std::string tableType,
                  std::string n,
                  SqlType type,
                  size_t off,
                  bool isOptional_,
                  bool isUnique_,
                  const std::string& refTable = std::string(),
                  const std::string& refColumn = std::string(),
                  Action updateAction = Action::NO_ACTION,
                  Action deleteAction = Action::NO_ACTION);
    virtual ~BaseFieldInfo() = default;

    virtual void bind(Statement& stmt, const void* obj) = 0;
    virtual void fetch(Statement& stmt, void* obj, int columnIndex) = 0;
};

template <typename StructType, typename FieldType>
struct FieldInfo final : BaseFieldInfo
{
    using table_type = StructType;
    using field_type = FieldType;
    using base_type = BaseFieldInfo;
    using base_type::BaseFieldInfo;
    FieldInfo(std::string name, size_t offset, bool isUnique_ = false);
    FieldInfo(std::string name, size_t offset, const std::string& refTable = std::string(), const std::string& refColumn = std::string(), Action updateAction = Action::NO_ACTION, Action deleteAction = Action::NO_ACTION);

    // Helper to get/set field value
    FieldType& get(StructType& obj) const;
    const FieldType& get(const StructType& obj) const;

    void bind(Statement& stmt, const void* obj) override;
    void fetch(Statement& stmt, void* obj, int columnIndex) override;
};

template <typename T>
struct SelectBuilder
{
    Session& db;
    std::string table;
    std::string orderName{};
    bool orderDesc = false;
    std::optional<int> lim{}, off{};

    template <class E>
    QueryResult<T> where(const E& e);
    SelectBuilder& orderBy(const char* col, bool desc = false);
    SelectBuilder& limit(int n);
    SelectBuilder& one();
    SelectBuilder& offset(int n);
};

template <typename T, typename FT>
struct SelectFieldBuilder
{
    Session& db;
    std::string table;
    std::string columnToFetch;
    std::string orderName{};
    bool orderDesc = false;
    std::optional<int> lim{}, off{};

    template <class E>
    QueryResult<FT> where(const E& e);
    SelectFieldBuilder& orderBy(const char* col, bool desc = false);
    SelectFieldBuilder& limit(int n);
    SelectFieldBuilder& one();
    SelectFieldBuilder& offset(int n);
};

struct SqlAndParams
{
    std::string sql;
    std::vector<Parameter> params;
};

void emitComparison(std::string& out, std::vector<Parameter>& ps, std::string_view col, const Parameter& p, const char* op);

template <class Col, class Val, class OpTag>
void toSql(const Cmp<Col, Val, OpTag>& e, std::string& out, std::vector<Parameter>& ps);

template <class L, class R, class Tag>
void toSql(const Bin<L, R, Tag>& e, std::string& out, std::vector<Parameter>& ps);

template <class E>
void toSql(const Not<E>& e, std::string& out, std::vector<Parameter>& ps);

class Session
{
public:
    explicit Session(Database& db);
    ~Session();

    template <OrmRecord T>
    int64_t insertOrUpdate(T& obj);
    template <OrmRecord T>
    int64_t insert(T& obj);
    template <OrmRecord T>
    int64_t insert(const T& obj);
    template <OrmRecord T>
    int64_t update(const T& obj);
    template <OrmRecord T>
    T fetch(int64_t id);
    template <OrmRecord T>
    SelectBuilder<T> select();
    template <OrmRecord T, typename FT>
    SelectFieldBuilder<T, FT> select(const FieldInfo<T, FT>& field);
    template <OrmRecord T>
    std::vector<T> fetchAll();

private:
    friend class Transaction;
    friend class Statement;
    sqlite3* connection() const;
    // int64_t doInsert(const BaseTableMetadata& metadata, const char* obj);
    struct SharedSession;
    std::shared_ptr<SharedSession> _sharedSession;
};

/// A transaction guard class.
///
/// Create an instance of nucleus::db::Transaction to open a transaction. On destruction the transaction is
/// rolled back, unless it was committed. An additional commitAndReopen method allows to use transactions to
/// group batches of operations for performance reasons without the need to recreate the transaction object
/// or force a huge commit around all operations.
class Transaction
{
public:
    /// Creates a transaction guard for the given database.
    /// @param session The database session to open the transaction on.
    explicit Transaction(Session& session);

    /// make it not-copyable
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    /// Transaction destructor. Emits a rollback if the transaction is open and no commit was called.
    ~Transaction();
    /// Commit the current transaction.
    /// If this method is called, the destructor of this transaction instance degrades to a no-op.
    void commit();
    /// Commit and reopen the transaction. This commits the running transaction and is ideal to do
    /// grouped bulk inserts and flush the groups into the database.
    void commitAndReopen();

private:
    Session& _session;
    bool _inTransaction;
};

/// A row value access facade.
///
/// This class is used to ease access to the row values. It implements cast operators to allow automatic cast
/// of row values to specific data types. A nucleus::db:ValueRef instance is valid only for the live-time of
/// the corresponding nucleus::db::Statement instance.
class ValueRef
{
public:
    /// Cast operator to fetch a column value as an int.
    /// @return The bool value of the referred column.
    operator bool() const;
    /// Cast operator to fetch a column value as an int.
    /// @return The integer value of the referred column.
    operator int() const;
    /// Cast operator to fetch a column value as an int64_t.
    /// @return The int64_t value of the referred column.
    operator int64_t() const;
    /// Cast operator to fetch a column value as an double.
    /// @return The double value of the referred column.
    operator double() const;
    /// Cast operator to fetch a column value as a std::string.
    /// @return The text value of the referred column.
    operator std::string() const;
    /// Cast operator to fetch a column value as a std::vector<uint8_t>.
    /// @return The blob value of the referred column.
    operator Blob() const;
    /// Accessor to a non-copying string_view.
    /// @warning The lifetime of the string_view is limited until step() or reset() is called on the Statement or the Statement goes out of scope!
    /// @return The a const char* to the text value of the referred column.
    std::string_view asStringView() const;

    /// Test if this value contains a null value.
    /// @return true if the value is a null value‚.
    bool isNull() const;

    template <StdOptional T>
    operator T() const;

private:
    friend Statement;
    /// Creates an instance with an sqlite3 statement handle and a column index.
    /// This constructor is used by the ghc::db::Statement class.
    ValueRef(const Session& session, const std::shared_ptr<StatementHandle>& stmt, int idx);
    Session _session;
    std::shared_ptr<StatementHandle> _stmt;
    int _idx;
    int _type;
};

/// A class to encapsulate prepared SQL statements.
///
/// The Statement class uses a ghc::db::Database instance to fetch a thread specific connection handle
/// that can be shared between this and other ghc::db::Statement instances used in the same thread.
/// The class has to be instantiated and used from the same thread.
class Statement
{
public:
    /// The constructor retrieves a connection from the given database and creates a prepared SQL statement for the query.
    Statement(Session& session, const std::string& query);

    /// make it non-copyable/non-movable
    Statement(const Statement&) = delete;
    Statement(Statement&&) = delete;
    Statement& operator=(const Statement&) = delete;
    Statement& operator=(Statement&&) = delete;

    /// The destructor releases the database connection handle.
    ~Statement();

    /// Reset the prepared statement to be used for a new call. After a reset new values can be bound to the statement.
    void reset();

    /// Return the parameter index of a parameter with the given name.
    /// @param name The name of the parameter
    /// @return The index of the parameter usable for a bind.
    /// @throw ghc::db::Exception if there is no parameter of that name
    int parameterIndexForName(const std::string& name);

    /// Bind an integer to the statement.
    /// @param idx Index of the placeholder ('?') to bind this value to.
    void bind(int idx, const int& value);
    /// Bind an int64_t to the statement.
    /// @param idx Index of the placeholder ('?') to bind this value to.
    void bind(int idx, const int64_t& value);
    /// Bind a double to the statement.
    /// @param idx Index of the placeholder ('?') to bind this value to.
    void bind(int idx, const double& value);
    /// Bind a string from a const char* to the statement.
    /// @param idx Index of the placeholder ('?') to bind this value to.
    void bind(int idx, const char* value);
    /// Bind a string from a std::string to the statement.
    /// @param idx Index of the placeholder ('?') to bind this value to.
    void bind(int idx, const std::string& value);
    /// Bind a string from a std::string to the statement.
    /// @param idx Index of the placeholder ('?') to bind this value to.
    /// @param value Reference to a blob represented as a vector of uint8_t.
    void bind(int idx, const Blob& value);
    /// Bind a null-value to the statement.
    /// @param idx Index of the placeholder ('?') to bind this value to.
    void bindNull(int idx);

    template <typename T>
    void bind(const std::string& name, const T& value);

    void bind(int idx, const Parameter& value);

    /// Bind a value to the statement.
    /// This variant of binding parameters has the advantage that no indexes need to be given, but it should
    /// not be mixed with the index based binding (you can do so, but should must be knowing what you are doing).
    /// An internal parameter counter is used that is reset by ghc::db::Statement::reset, so rebinding
    /// for the next run with this operator is supported.
    /// @param value The value to bind.
    /// @return This statement to cascade multiple binds like the normal stream operations.
    template <typename T>
        requires ValidColumnType<T>
    Statement& operator<<(const T& value);
    template <size_t N>
    Statement& operator<<(const char (&lit)[N]);

    Statement& operator<<(std::nullptr_t)
    {
        if (_bindColumnIdx > parameterCount()) {
            throw Exception("Too many parameters bound");
        }
        bindNull(_bindColumnIdx++);
        return *this;
    }

    Statement& operator<<(Statement& (*f)(Statement&));
    int64_t operator<<(int64_t (*f)(Statement&));

    /// Execute the statement or step to the next result row.
    /// @return true if there was (another) result available.
    bool step();

    /// Execute the non-query statement and return the row id if this was an insert.
    /// @return the row id of the last insert or 0 if this was not an insert.
    /// @throw ghc::db::Exception if this statement was a query resulting in rows
    int64_t execute();

    /// Fetch a value from a column of the current row.
    /// @param idx The column index to fetch the value from.
    /// @return A ghc::db::ValueRef as an access proxy to a row value.
    ValueRef column(int idx) const;

    /// Return the column name of the result column. The result is undefined if the column is the result of
    /// an expression without an AS clause.
    /// @param idx The column index to fetch the name from.
    /// @return The name of the column, if available.
    std::string columnName(int idx) const;

    /// Return the number of columns if this was a query, or 0
    /// @return The number of columns in the result of the query or 0 if there is no result.
    size_t columnCount() const;

    /// Return the number of parameters in the statement.
    /// @return The number of parameters in the statement.
    size_t parameterCount() const;

    /// Return number off affected rows
    /// @return affected rows or 0
    int changes() const;

    template <typename T>
    Statement& operator>>(T& value);

private:
    friend Statement& null(Statement& stmt);
    sqlite3* connection() const;
    Session& _session;
    std::shared_ptr<StatementHandle> _stmt;
    int _resultColumns;
    int _bindColumnIdx;
    int _resultColumnIdx;
    std::vector<int> _resultTypes;
};

/// A stand-in for NULL in stream bind operations.
Statement& null(Statement& stmt);

/// A function used as function pointer in stream-operator based binding, to directly execute the query and
/// return true if there is a first result row available.
bool step(Statement& stmt);

/// A function used as function pointer in stream-operator based binding, to directly execute the statement.
int64_t execute(Statement& stmt);

/// An abstraction of SQLite3 database connection handling.
///
/// ghc::db::Database is a class that handles connections to an sqlite3 database. It can be used from different
/// threads, and handles concurrent access between different threads with a dynamic connection per thread
/// reservation, threads creating statements while no connection handles are dedicated to them and no free ones
/// are available are blocked until a connection is free. The total number of usable connections is configured
/// in the constructor call.
class Database
{
public:
    /// Construct a database connection manager with a given path to an sqlite database file with a given concurrency.
    /// The database file doesn't need to exist, the file will be created if it is not existing.
    /// @param path The path of the database file to be used.
    /// @param concurrency The number of connections to the database to open and use for thread sharing. For
    ///                    multiple nested statements from a single thread, no concurrent connections are needed.
    explicit Database(const std::string& path, uint8_t concurrency = 1);
#ifdef WITH_ENCRYPTION
    Database(const std::string& path, const std::string& hexUMK, const std::string& hexDLK, uint8_t concurrency = 1);
#endif
    /// The destructor, closing all connections. An instance must not be deleted while there are still
    /// statements or transactions referring to the database.
    ~Database();

    /// make it non-copyable
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    Session session();

    /// Shortcut to execute a statement without using a ghc::db::Statement.
    /// @param query The SQL statement to execute.
    /// @return The number of affected rows.
    /// @throw A ghc::db::Exception is thrown in case of an error.
    int execute(const std::string& query);

    /// Shortcut to execute a query with a single return value (e.g. a `SELECT COUNT(*)` statement without using a ghc::db::Statement.
    /// @param query The SQL statement to execute.
    /// @return A ghc::db::ValueRef for the first column of the first row of the query.
    /// @throw A ghc::db::Exception is thrown in case of an error.
    ValueRef executeForValue(const std::string& query);

private:
    friend class Statement;
    friend class Session;
    int64_t lastInsertRowId();
    ::sqlite3* acquireConnection();
    void releaseConnection(::sqlite3* connection);
    typedef std::list<::sqlite3*> FreeConnectionList;
    typedef std::map<std::thread::id, std::pair<int, ::sqlite3*>> UsedConnectionMap;
    FreeConnectionList _freeConnections;
    UsedConnectionMap _usedConnections;
    std::mutex _mutex;
    std::condition_variable _condition;
};

class BaseTableMetadata
{
public:
    virtual ~BaseTableMetadata() = default;
    virtual const std::string& tableName() const = 0;
    virtual const std::vector<std::unique_ptr<BaseFieldInfo>>& fields() const = 0;
    virtual std::string createTableSql() const = 0;
    const std::string& insertSql() const;
    const std::string& updateSql() const;

    // Find field by member pointer with type checking
    template <typename StructType, typename FieldType>
    const FieldInfo<StructType, FieldType>* findTypedFieldByMember(FieldType StructType::* member) const
    {
        auto offset = reinterpret_cast<size_t>(&(static_cast<StructType*>(nullptr)->*member));
        for (const auto& field : fields()) {
            if (field->offset == offset) {
                if (auto* typedField = dynamic_cast<const FieldInfo<StructType, FieldType>*>(field.get())) {
                    return typedField;
                }
            }
        }
        return nullptr;
    }

    // Generic version that returns BaseFieldInfo
    template <typename StructType, typename FieldType>
    const BaseFieldInfo* findFieldByMember(FieldType StructType::* member) const
    {
        auto offset = reinterpret_cast<size_t>(&(static_cast<StructType*>(nullptr)->*member));

        for (const auto& field : fields()) {
            if (field->offset == offset) {
                return field.get();
            }
        }
        return nullptr;
    }

    bool hasField(const std::string& fieldName) const;

protected:
    mutable std::string _insertSql;
    mutable std::string _updateSql;
};

template <typename T>
class TableMetadata final : public BaseTableMetadata
{
public:
    TableMetadata(const std::string& name, std::vector<std::unique_ptr<BaseFieldInfo>> fields)
        : _tableName(name)
        , _fields(std::move(fields))
    {
        // Verify first field is 'id' of type int or int64_t
        if (_fields.empty() || _fields[0]->name != "id" || _fields[0]->sqlType != SqlType::INTEGER) {
            throw std::runtime_error("First field must be 'id' of type int64_t");
        }
    }

    const std::string& tableName() const override { return _tableName; }
    const std::vector<std::unique_ptr<BaseFieldInfo>>& fields() const override { return _fields; }

    // Generate CREATE TABLE SQL
    [[nodiscard]] std::string createTableSql() const override
    {
        std::string sql = "CREATE TABLE IF NOT EXISTS \"" + _tableName + "\" (";

        for (const auto& field : _fields) {
            if (sql.back() != '(')
                sql += ", ";

            sql += "\"" + field->name + "\" ";

            switch (field->sqlType) {
                case SqlType::INTEGER:
                    sql += field->name == "id" ? "INTEGER PRIMARY KEY AUTOINCREMENT" : "INTEGER";
                    break;
                case SqlType::REAL:
                    sql += "REAL";
                    break;
                case SqlType::TEXT:
                    sql += "TEXT";
                    break;
                case SqlType::BLOB:
                    sql += "BLOB";
                    break;
                case SqlType::NULL_TYPE:
                    throw Exception("NULL_TYPE is not a valid SQL type");
            }
            if (field->isUnique) {
                sql += " UNIQUE";
            }
            if (!field->isOptional) {
                sql += " NOT NULL";
            }
        }
        for (const auto& field : _fields) {
            if (field->fkInfo) {
                sql += ", FOREIGN KEY(\"" + field->name + "\") REFERENCES \"" + field->fkInfo->refTable + "\"(\"" + field->fkInfo->refColumn + "\")";
                if (field->fkInfo->on_update == Action::ON_UPDATE_CASCADE) {
                    sql += " ON UPDATE CASCADE";
                }
                if (field->fkInfo->on_delete == Action::ON_DELETE_CASCADE) {
                    sql += " ON DELETE CASCADE";
                }
            }
        }
        sql += ");";
        return sql;
    }

private:
    std::string _tableName;
    std::vector<std::unique_ptr<BaseFieldInfo>> _fields;
};

class Registry
{
public:
    static Registry& instance()
    {
        static Registry registry;
        return registry;
    }

    template <typename T>
    static void registerTable(const std::string& table_name, std::vector<std::unique_ptr<BaseFieldInfo>> fields)
    {
        auto& self = instance();
        auto metadata = std::make_unique<TableMetadata<T>>(table_name, std::move(fields));
        self._tables.push_back(std::move(metadata));
        self._tablesMap[typeid(T).name()] = self._tables.size() - 1;
    }

    template <typename T>
    static const TableMetadata<T>* findMetadata()
    {
        auto& self = instance();
        auto it = self._tablesMap.find(typeid(T).name());
        if (it != self._tablesMap.end()) {
            return static_cast<const TableMetadata<T>*>(self._tables[it->second].get());
        }
        return nullptr;
    }

    static void syncTables(Database& db) { instance().doSyncTables(db); }

private:
    void doSyncTables(Database& db) const;
    struct SQLiteTableInfo
    {
        std::string name;
        std::string type;
        bool isOptional{};
        bool isUnique{};
        std::optional<ForeignKeyInfo> fkInfo;
        std::string defaultValue;
    };

    static std::string escapeIdent(const std::string& s);
    static void fetchForeignKeyInfo(Session& session, const std::string& tableName, std::unordered_map<std::string, ForeignKeyInfo>& foreignKeys);
    static std::vector<SQLiteTableInfo> getSQLiteTableSchema(Database& db, const std::string& tableName);
    static std::set<std::vector<std::string>> getSQLiteUniqueIndexes(Database& db, const std::string& tableName);
    static void analyzeTableDifferences(Database& db, const std::string& tableName, const std::vector<SQLiteTableInfo>& existing, const std::vector<std::unique_ptr<BaseFieldInfo>>& required);

    std::vector<std::unique_ptr<BaseTableMetadata>> _tables;
    std::unordered_map<std::string, size_t> _tablesMap;
};

template <typename ST, typename FT, typename T>
auto operator==(const FieldInfo<ST, FT>& f, T v);

template <typename ST, typename FT, typename T>
auto operator!=(const FieldInfo<ST, FT>& f, T v);

template <typename ST, typename FT, typename T>
auto operator<(const FieldInfo<ST, FT>& f, T v);

template <typename ST, typename FT, typename T>
auto operator<=(const FieldInfo<ST, FT>& f, T v);

template <typename ST, typename FT, typename T>
auto operator>(const FieldInfo<ST, FT>& f, T v);

template <typename ST, typename FT, typename T>
auto operator>=(const FieldInfo<ST, FT>& f, T v);

template <class L, class R>
    requires SameTable<L, R>
auto operator&&(L l, R r);

template <class L, class R>
    requires SameTable<L, R>
auto operator||(L l, R r);

template <class E>
auto operator!(E e);

template <class ST, class FT>
auto like(const FieldInfo<ST, FT>& f, std::string_view pattern);

template <OrmRecord StructType>
class Table
{
public:
    explicit Table(std::string table_name);

    template <typename FieldType>
    Table& column(const std::string& name, FieldType StructType::* member, bool isUnique);

    template <typename FieldType>
    Table& column(const std::string& name, FieldType StructType::* member, const std::string& refTable = std::string(), const std::string& refColumn = std::string(), Action updateAction = Action::NO_ACTION, Action deleteAction = Action::NO_ACTION);

    void registerTable();

private:
    template <typename FieldType>
    void addField(std::vector<std::unique_ptr<BaseFieldInfo>>& fields,
                  const std::string& name,
                  FieldType StructType::* member,
                  bool isUnique,
                  const std::string& refTable = std::string(),
                  const std::string& refColumn = std::string(),
                  Action updateAction = Action::NO_ACTION,
                  Action deleteAction = Action::NO_ACTION);

    std::string _tableName;
    std::vector<std::unique_ptr<BaseFieldInfo>> _fields;
};

//-----------------------------------------------------------------------------
// IMPLEMENTATION
//-----------------------------------------------------------------------------

namespace detail {
template <std::ranges::input_range R>
std::string join(R&& r, std::string_view delimiter)
{
    std::ostringstream out;

    auto it = std::ranges::begin(r);
    auto end = std::ranges::end(r);

    if (it == end)
        return {};

    out << *it;
    auto delimiterStr = std::string(delimiter); // TODO: to support incomplete libstdc++ variants
    for (++it; it != end; ++it) {
        out << delimiterStr;
        out << *it;
    }

    return out.str();
}
}  // namespace detail

// Exception ctor
inline Exception::Exception(const std::string& msg)
    : std::runtime_error(msg)
{
}

// QueryResult<T> inline/template implementations
template <class T>
QueryResult<T>::QueryResult() = default;

template <class T>
QueryResult<T>::QueryResult(vector_type v)
    : _data(std::move(v))
{
}

template <class T>
std::size_t QueryResult<T>::size() const noexcept
{
    return _data.size();
}

template <class T>
bool QueryResult<T>::empty() const noexcept
{
    return _data.empty();
}

template <class T>
QueryResult<T>::iterator QueryResult<T>::begin() noexcept
{
    return _data.begin();
}

template <class T>
QueryResult<T>::iterator QueryResult<T>::end() noexcept
{
    return _data.end();
}

template <class T>
QueryResult<T>::const_iterator QueryResult<T>::begin() const noexcept
{
    return _data.begin();
}

template <class T>
QueryResult<T>::const_iterator QueryResult<T>::end() const noexcept
{
    return _data.end();
}

template <class T>
T& QueryResult<T>::operator[](std::size_t i) noexcept
{
    return _data[i];
}

template <class T>
const T& QueryResult<T>::operator[](std::size_t i) const noexcept
{
    return _data[i];
}

template <class T>
template <class... Args>
T& QueryResult<T>::emplace_back(Args&&... args)
{
    return _data.emplace_back(std::forward<Args>(args)...);
}

template <class T>
void QueryResult<T>::push_back(T v)
{
    _data.push_back(std::move(v));
}

template <class T>
const QueryResult<T>::vector_type& QueryResult<T>::asVector() const& noexcept
{
    return _data;
}

template <class T>
QueryResult<T>::vector_type& QueryResult<T>::asVector() & noexcept
{
    return _data;
}

template <class T>
QueryResult<T>::vector_type QueryResult<T>::intoVector() && noexcept
{
    return std::move(_data);
}

template <class T>
QueryResult<T>::operator const vector_type&() const& noexcept
{
    return _data;
}

template <class T>
QueryResult<T>::operator vector_type() && noexcept
{
    return std::move(_data);
}

template <class T>
template <class FT>
    requires std::convertible_to<FT, T>
T QueryResult<T>::valueOrDefault(FT&& defaultValue) const
{
    if (_data.size() == 1)
        return _data.front();
    return static_cast<T>(std::forward<FT>(defaultValue));
}

template <class T>
std::optional<T> QueryResult<T>::one() const
{
    if (_data.size() == 1)
        return _data.front();
    return std::nullopt;
}

template <class T>
const T& QueryResult<T>::expectOne() const
{
    if (_data.size() != 1)
        throw Exception("expected exactly one row");
    return _data.front();
}

template <class T>
const T& QueryResult<T>::first() const
{
    if (_data.empty())
        throw Exception("expected at least one row");
    return _data.front();
}

// Expression toSql helpers
inline void emitComparison(std::string& out, std::vector<Parameter>& ps, std::string_view col, const Parameter& p, const char* op)
{
    out += col;
    out += ' ';
    out += op;
    out += " ?";
    ps.push_back(p);
}

template <class Col, class Val, class OpTag>
void toSql(const Cmp<Col, Val, OpTag>& e, std::string& out, std::vector<Parameter>& ps)
{
    // Map Val -> Parameter
    using FT = typename Col::field_type;
    (void)sizeof(FT);
    Parameter p;
    if constexpr (std::is_same_v<typename Col::field_type, std::string>)
        p = std::string(e.val);
    else if constexpr (std::is_integral_v<typename Col::field_type> && !std::is_same_v<typename Col::field_type, bool>)
        p = static_cast<int64_t>(e.val);
    else if constexpr (std::is_same_v<typename Col::field_type, bool>)
        p = static_cast<int64_t>(e.val ? 1 : 0);
    else if constexpr (std::is_floating_point_v<typename Col::field_type>)
        p = static_cast<double>(e.val);
    else
        static_assert(sizeof(Col) == 0, "Add binder for this cpp_type.");

    if constexpr (std::is_same_v<OpTag, OpEq>)
        emitComparison(out, ps, e.col.name, p, "=");
    if constexpr (std::is_same_v<OpTag, OpNe>)
        emitComparison(out, ps, e.col.name, p, "!=");
    if constexpr (std::is_same_v<OpTag, OpLt>)
        emitComparison(out, ps, e.col.name, p, "<");
    if constexpr (std::is_same_v<OpTag, OpLe>)
        emitComparison(out, ps, e.col.name, p, "<=");
    if constexpr (std::is_same_v<OpTag, OpGt>)
        emitComparison(out, ps, e.col.name, p, ">");
    if constexpr (std::is_same_v<OpTag, OpGe>)
        emitComparison(out, ps, e.col.name, p, ">=");
    if constexpr (std::is_same_v<OpTag, OpLike>) {
        out += e.col.name + " LIKE ?";
        ps.push_back(p);
    }
}

template <class L, class R, class Tag>
void toSql(const Bin<L, R, Tag>& e, std::string& out, std::vector<Parameter>& ps)
{
    out += '(';
    toSql(e.l, out, ps);
    out += std::is_same_v<Tag, OpAnd> ? " AND " : " OR ";
    toSql(e.r, out, ps);
    out += ')';
}

template <class E>
void toSql(const Not<E>& e, std::string& out, std::vector<Parameter>& ps)
{
    out += "(NOT ";
    toSql(e.e, out, ps);
    out += ')';
}

// ValueRef optional conversion
template <StdOptional T>
ValueRef::operator T() const
{
    using ValueType = typename T::value_type;
    if (isNull()) {
        return std::nullopt;
    }
    return static_cast<ValueType>(*this);
}

// getSqlType
template <typename T>
constexpr SqlType getSqlType()
{
    if constexpr (std::is_integral_v<T>) {
        return SqlType::INTEGER;
    }
    else if constexpr (is_std_optional_v<T> && std::is_integral_v<typename T::value_type>) {
        return SqlType::INTEGER;
    }
    else if constexpr (std::is_same_v<T, float>) {
        return SqlType::REAL;
    }
    else if constexpr (std::is_same_v<T, double>) {
        return SqlType::REAL;
    }
    else if constexpr (std::is_same_v<T, std::string>) {
        return SqlType::TEXT;
    }
    else if constexpr (std::is_same_v<T, std::vector<int8_t>>) {
        return SqlType::BLOB;
    }
    else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
        return SqlType::BLOB;
    }
    else {
        static_assert(sizeof(T) == 0, "Unsupported type for ORM");
    }
    return SqlType::NULL_TYPE;
}

// BaseFieldInfo ctor
inline BaseFieldInfo::BaseFieldInfo(std::string tableType, std::string n, SqlType type, size_t off, bool isOptional_, bool isUnique_, const std::string& refTable, const std::string& refColumn, Action updateAction, Action deleteAction)
    : tableTypeName(std::move(tableType))
    , name(std::move(n))
    , sqlType(type)
    , offset(off)
    , isOptional(isOptional_)
    , isUnique(isUnique_)
{
    if (!refTable.empty() && !refColumn.empty()) {
        fkInfo = ForeignKeyInfo{refTable, refColumn, updateAction, deleteAction};
    }
}

// FieldInfo
template <typename StructType, typename FieldType>
FieldInfo<StructType, FieldType>::FieldInfo(std::string name, size_t offset, bool isUnique_)
    : BaseFieldInfo(typeid(StructType).name(), std::move(name), getSqlType<FieldType>(), offset, is_std_optional_v<FieldType>, isUnique_)
{
}

template <typename StructType, typename FieldType>
FieldInfo<StructType, FieldType>::FieldInfo(std::string name, size_t offset, const std::string& refTable, const std::string& refColumn, Action updateAction, Action deleteAction)
    : BaseFieldInfo(typeid(StructType).name(), std::move(name), getSqlType<FieldType>(), offset, is_std_optional_v<FieldType>, false, refTable, refColumn, updateAction, deleteAction)
{
}

template <typename StructType, typename FieldType>
FieldType& FieldInfo<StructType, FieldType>::get(StructType& obj) const
{
    return *reinterpret_cast<FieldType*>(reinterpret_cast<std::byte*>(&obj) + offset);
}

template <typename StructType, typename FieldType>
const FieldType& FieldInfo<StructType, FieldType>::get(const StructType& obj) const
{
    return *reinterpret_cast<const FieldType*>(reinterpret_cast<const std::byte*>(&obj) + offset);
}

template <typename StructType, typename FieldType>
void FieldInfo<StructType, FieldType>::bind(Statement& stmt, const void* obj)
{
    auto* field = reinterpret_cast<const FieldType*>(static_cast<const char*>(obj) + offset);
    if constexpr (is_std_optional_v<FieldType>) {
        if (field->has_value()) {
            stmt << field->value();
        }
        else {
            stmt << nullptr;
        }
    }
    else {
        stmt << *field;
    }
}

template <typename StructType, typename FieldType>
void FieldInfo<StructType, FieldType>::fetch(Statement& stmt, void* obj, int columnIndex)
{
    (void)columnIndex;  // unused, kept for signature compatibility
    auto* field = reinterpret_cast<FieldType*>(static_cast<char*>(obj) + offset);
    stmt >> *field;
}

// SelectBuilder fluent setters
template <typename T>
SelectBuilder<T>& SelectBuilder<T>::orderBy(const char* col, bool desc)
{
    orderName = col;
    orderDesc = desc;
    return *this;
}

template <typename T>
SelectBuilder<T>& SelectBuilder<T>::limit(int n)
{
    lim = n;
    return *this;
}

template <typename T>
SelectBuilder<T>& SelectBuilder<T>::one()
{
    lim = 1;
    return *this;
}

template <typename T>
SelectBuilder<T>& SelectBuilder<T>::offset(int n)
{
    off = n;
    return *this;
}

template <typename T, typename FT>
SelectFieldBuilder<T, FT>& SelectFieldBuilder<T, FT>::orderBy(const char* col, bool desc)
{
    orderName = col;
    orderDesc = desc;
    return *this;
}

template <typename T, typename FT>
SelectFieldBuilder<T, FT>& SelectFieldBuilder<T, FT>::limit(int n)
{
    lim = n;
    return *this;
}

template <typename T, typename FT>
SelectFieldBuilder<T, FT>& SelectFieldBuilder<T, FT>::one()
{
    lim = 1;
    return *this;
}

template <typename T, typename FT>
SelectFieldBuilder<T, FT>& SelectFieldBuilder<T, FT>::offset(int n)
{
    off = n;
    return *this;
}

// Operators
template <typename ST, typename FT, typename T>
auto operator==(const FieldInfo<ST, FT>& f, T v)
{
    return Cmp<FieldInfo<ST, FT>, T, OpEq>{{}, f, std::move(v)};
}

template <typename ST, typename FT, typename T>
auto operator!=(const FieldInfo<ST, FT>& f, T v)
{
    return Cmp<FieldInfo<ST, FT>, T, OpNe>{{}, f, std::move(v)};
}

template <typename ST, typename FT, typename T>
auto operator<(const FieldInfo<ST, FT>& f, T v)
{
    return Cmp<FieldInfo<ST, FT>, T, OpLt>{{}, f, std::move(v)};
}

template <typename ST, typename FT, typename T>
auto operator<=(const FieldInfo<ST, FT>& f, T v)
{
    return Cmp<FieldInfo<ST, FT>, T, OpLe>{{}, f, std::move(v)};
}

template <typename ST, typename FT, typename T>
auto operator>(const FieldInfo<ST, FT>& f, T v)
{
    return Cmp<FieldInfo<ST, FT>, T, OpGt>{{}, f, std::move(v)};
}

template <typename ST, typename FT, typename T>
auto operator>=(const FieldInfo<ST, FT>& f, T v)
{
    return Cmp<FieldInfo<ST, FT>, T, OpGe>{{}, f, std::move(v)};
}

template <class L, class R>
    requires SameTable<L, R>
auto operator&&(L l, R r)
{
    return Bin<L, R, OpAnd>{std::move(l), std::move(r)};
}

template <class L, class R>
    requires SameTable<L, R>
auto operator||(L l, R r)
{
    return Bin<L, R, OpOr>{std::move(l), std::move(r)};
}

template <class E>
auto operator!(E e)
{
    return Not<E>{std::move(e)};
}

template <class ST, class FT>
auto like(const FieldInfo<ST, FT>& f, std::string_view pattern)
{
    return Cmp<FieldInfo<ST, FT>, std::string, OpLike>{{}, f, std::string(pattern)};
}

// Table<>
template <OrmRecord StructType>
Table<StructType>::Table(std::string table_name)
    : _tableName(std::move(table_name))
{
}

template <OrmRecord StructType>
template <typename FieldType>
Table<StructType>& Table<StructType>::column(const std::string& name, FieldType StructType::* member, bool isUnique)
{
    addField(_fields, name, member, isUnique);
    return *this;
}

template <OrmRecord StructType>
template <typename FieldType>
Table<StructType>& Table<StructType>::column(const std::string& name, FieldType StructType::* member, const std::string& refTable, const std::string& refColumn, Action updateAction, Action deleteAction)
{
    addField(_fields, name, member, false, refTable, refColumn, updateAction, deleteAction);
    return *this;
}

template <OrmRecord StructType>
void Table<StructType>::registerTable()
{
    Registry::instance().registerTable<StructType>(_tableName, std::move(_fields));
}

template <OrmRecord StructType>
template <typename FieldType>
void Table<StructType>::addField(std::vector<std::unique_ptr<BaseFieldInfo>>& fields,
                                 const std::string& name,
                                 FieldType StructType::* member,
                                 bool isUnique,
                                 const std::string& refTable,
                                 const std::string& refColumn,
                                 Action updateAction,
                                 Action deleteAction)
{
    auto offset = reinterpret_cast<size_t>(&(static_cast<StructType*>(nullptr)->*member));
    if (isUnique)
        fields.push_back(std::make_unique<FieldInfo<StructType, FieldType>>(name, offset, true));
    else
        fields.push_back(std::make_unique<FieldInfo<StructType, FieldType>>(name, offset, refTable, refColumn, updateAction, deleteAction));
}

// Session template methods
template <OrmRecord T>
int64_t Session::insertOrUpdate(T& obj)
{
    if (obj.id) {
        update(obj);
        return 0;
    }
    return insert(obj);
}

template <OrmRecord T>
int64_t Session::insert(T& obj)
{
    auto id = insert(std::as_const(obj));
    obj.id = id;
    return id;
}

template <OrmRecord T>
int64_t Session::insert(const T& obj)
{
    auto metadata = Registry::findMetadata<T>();
    if (!metadata) {
        throw Exception("No metadata found for type");
    }

    Statement stmt(*this, metadata->insertSql());
    int paramIdx = 1;
    for (size_t i = 1; i < metadata->fields().size(); ++i) {  // Skip id field
        const auto& field = metadata->fields()[i];
        field->bind(stmt, &obj);
    }
    auto id = stmt.execute();
    return id;
}

template <OrmRecord T>
int64_t Session::update(const T& obj)
{
    auto metadata = Registry::findMetadata<T>();
    if (!metadata) {
        throw Exception("No metadata found for type");
    }
    Statement stmt(*this, metadata->updateSql());
    int paramIdx = 1;
    for (size_t i = 1; i < metadata->fields().size(); ++i) {
        const auto& field = metadata->fields()[i];
        field->bind(stmt, &obj);
    }
    const auto& idField = metadata->fields()[0];
    idField->bind(stmt, &obj);
    return stmt.execute();
}

template <OrmRecord T>
T Session::fetch(const int64_t id)
{
    auto metadata = Registry::findMetadata<T>();
    if (!metadata) {
        throw Exception("No metadata found for type");
    }
    std::string sql = "SELECT * FROM \"" + metadata->tableName() + "\" WHERE id = ?";
    Statement stmt(*this, sql);
    stmt << id;
    if (!stmt.step()) {
        throw Exception("Record not found");
    }
    T result;
    for (size_t i = 0; i < metadata->fields().size(); ++i) {
        metadata->fields()[i]->fetch(stmt, &result, static_cast<int>(i));
    }
    return result;
}

template <OrmRecord T>
SelectBuilder<T> Session::select()
{
    auto metadata = Registry::findMetadata<T>();
    if (!metadata) {
        throw Exception("No metadata found for type");
    }
    return SelectBuilder<T>{*this, metadata->tableName()};
}

template <OrmRecord T, typename FT>
SelectFieldBuilder<T, FT> Session::select(const FieldInfo<T, FT>& field)
{
    auto metadata = Registry::findMetadata<T>();
    if (!metadata) {
        throw Exception("No metadata found for type");
    }
    return SelectFieldBuilder<T, FT>{*this, metadata->tableName(), field.name};
}

template <OrmRecord T>
std::vector<T> Session::fetchAll()
{
    auto metadata = Registry::findMetadata<T>();
    if (!metadata) {
        throw Exception("No metadata found for type");
    }
    std::string sql = "SELECT * FROM \"" + metadata->tableName() + "\"";
    Statement stmt(*this, sql);
    std::vector<T> results;
    while (stmt.step()) {
        T obj;
        for (size_t i = 0; i < metadata->fields().size(); ++i) {
            metadata->fields()[i]->fetch(stmt, &obj, static_cast<int>(i));
        }
        results.push_back(std::move(obj));
    }
    return results;
}

// Compile and fetch helpers
template <class E>
SqlAndParams compileWhere(const BaseTableMetadata& metadata, const std::string& output, const char* tableName, const E& expr, std::optional<int> limit, std::optional<int> offset, std::optional<std::pair<const char*, bool>> orderBy)
{
    SqlAndParams r;
    std::string whereSql;
    toSql(expr, whereSql, r.params);
    std::string sql = "SELECT " + output + " FROM ";
    sql += tableName;
    sql += " WHERE ";
    sql += whereSql;

    if (orderBy) {
        if (metadata.hasField(orderBy->first)) {
            sql += " ORDER BY ";
            sql += orderBy->first;
            sql += orderBy->second ? " DESC" : " ASC";
        }
        else {
            throw Exception("Invalid ORDER BY column");
        }
    }
    if (limit) {
        sql += " LIMIT ?";
        r.params.emplace_back(int64_t(*limit));
    }
    if (offset) {
        sql += " OFFSET ?";
        r.params.emplace_back(int64_t(*offset));
    }
    r.sql = sql;
    return r;
}

template <typename T, class WhereExpr>
QueryResult<T> fetchWhere(Session& session, const char* tableName, const WhereExpr& where, std::optional<int> limit, std::optional<int> offset, std::optional<std::pair<const char*, bool>> orderBy)
{
    static_assert(std::is_same_v<typename WhereExpr::table_type, T>, "WHERE must target the same table");
    auto metadata = Registry::findMetadata<T>();
    if (!metadata) {
        throw Exception("No metadata found for type");
    }

    auto [sql, params] = compileWhere(*metadata, "*", tableName, where, limit, offset, orderBy);
    Statement stmt(session, sql);

    int idx = 1;
    for (auto& p : params)
        stmt.bind(idx++, p);

    QueryResult<T> results;
    while (stmt.step()) {
        T obj;
        for (size_t i = 0; i < metadata->fields().size(); ++i) {
            metadata->fields()[i]->fetch(stmt, &obj, static_cast<int>(i));
        }
        results.push_back(std::move(obj));
    }
    return results;
}

template <typename T, typename FT, class WhereExpr>
QueryResult<FT> fetchFieldWhere(Session& session, const char* tableName, const std::string& column, const WhereExpr& where, std::optional<int> limit, std::optional<int> offset, std::optional<std::pair<const char*, bool>> orderBy)
{
    static_assert(std::is_same_v<typename WhereExpr::table_type, T>, "WHERE must target the same table");
    auto metadata = Registry::findMetadata<T>();
    if (!metadata) {
        throw Exception("No metadata found for type");
    }

    auto [sql, params] = compileWhere(*metadata, column, tableName, where, limit, offset, orderBy);
    Statement stmt(session, sql);

    int idx = 1;
    for (auto& p : params)
        stmt.bind(idx++, p);

    QueryResult<FT> results;
    while (stmt.step()) {
        results.push_back(stmt.column(0));
    }
    return results;
}

template <typename StructType, typename FieldType>
const FieldInfo<StructType, FieldType>& col(FieldType StructType::* memberPtr)
{
    auto metadata = Registry::findMetadata<StructType>();
    if (!metadata) {
        throw Exception("No metadata found for type");
    }
    return *metadata->template findTypedFieldByMember<StructType, FieldType>(memberPtr);
}

template <typename T>
template <class E>
QueryResult<T> SelectBuilder<T>::where(const E& e)
{
    return fetchWhere<T>(db, table.c_str(), e, lim, off, orderName.empty() ? std::nullopt : std::optional{std::pair{orderName.c_str(), orderDesc}});
}

template <typename T, typename FT>
template <class E>
QueryResult<FT> SelectFieldBuilder<T, FT>::where(const E& e)
{
    return fetchFieldWhere<T, FT>(db, table.c_str(), columnToFetch, e, lim, off, orderName.empty() ? std::nullopt : std::optional{std::pair{orderName.c_str(), orderDesc}});
}

template <typename T>
void Statement::bind(const std::string& name, const T& value)
{
    bind(parameterIndexForName(name), value);
}

template <typename T>
    requires ValidColumnType<T>
Statement& Statement::operator<<(const T& value)
{
    if (_bindColumnIdx > parameterCount()) {
        throw Exception("Too many parameters bound");
    }
    bind(_bindColumnIdx++, value);
    return *this;
}

template <size_t N>
Statement& Statement::operator<<(const char (&lit)[N])
{
    return (*this) << static_cast<const char*>(lit);
}

template <typename T>
Statement& Statement::operator>>(T& value)
{
    if (_resultColumnIdx < 0 || _resultColumnIdx >= _resultColumns) {
        throw Exception("No result collumns left");
    }
    int idx = _resultColumnIdx;
    using U = std::remove_cv_t<std::remove_reference_t<T>>;
    auto tmp = static_cast<U>(column(idx));
    value = std::move(tmp);
    _resultColumnIdx = idx + 1;
    return *this;
}

}  // namespace ghc::sqlite
