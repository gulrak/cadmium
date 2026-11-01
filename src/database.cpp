//---------------------------------------------------------------------------------------
// src/database.cpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2024, Steffen Schümann <s.schuemann@pobox.com>
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

#include "database.hpp"
#include "emuhostex.hpp"
#include "librarian.hpp"
#include "libraryquery.hpp"
#include <chiplet/chip8decompiler.hpp>
#include "resourcemanager.hpp"
#include "stylemanager.hpp"
#include <c8db/database.hpp>

#include <raylib.h>
#include <ghc/sqlite.hpp>
#include <rlguipp/rlguipp.hpp>


#ifndef _WIN32
#include <httplib.h>
#endif

#include "fmt/os.h"

#include <atomic>
#include <charconv>

namespace db = ghc::sqlite;

struct DBVersion
{
    int id{0};
    int schema_version{1};
    std::optional<int> starter_version;  // the version of the starter metadata the library was created from
};

struct DBTags
{
    int id{0};
    std::string name;
    std::string color;
    std::optional<std::string> query;  // a smart tag: the saved query it selects, it is never given to programs
};

struct DBProgram
{
    int id{0};
    std::string name;
    std::string origin;            // "gamejam", "event", "magazine", "manual" or empty
    std::string origin_reference;  // the game jam, event, magazine issue or manual the program is from
    std::string description;
    std::string release;  // a date like 1978, 1978-05 or 1978-05-12, unknown digits of the year as x or ?
    std::string url;
    std::string license;    // an SPDX license expression, like "MIT" or "MIT OR Apache-2.0"
    std::string copyright;  // free text, for programs without a known license
    std::optional<int> starter_version;  // the starter metadata version the row came from, none if added here
    int user_modified{0};                // changed by the user, so newer starter metadata doesn't overwrite it

    Rectangle rect{};
    std::vector<int> tags;
    std::vector<int> authors;  // in the order of their position
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
    std::optional<db::Blob> data;  // none for a ROM only known from metadata, until a scan finds its file
    int size{0};                   // known without the data, too
    std::optional<int> starter_version;  // the starter metadata version the row came from, none if added here
    int user_modified{0};                // its program or configs were changed by the user
    std::optional<int> analysis_version;  // the version of the opcode analysis its binary_opcodes come from

    std::vector<std::string> filenames;
    std::vector<uint16_t> opcodes;  // the distinct opcodes of its binary_opcodes, sorted
    std::vector<int> tags;
    std::vector<DBBinaryConfig> configs;
};

// How often an opcode is used on the execution paths of a ROM, as the decompiler finds them.
struct DBOpcode
{
    int id{0};
    int binary_id{0};
    int opcode{0};
    int count{0};
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

struct DBAuthor
{
    int id{0};
    std::string name;
};

// The authors of a program, position keeps their order as given (the main author first).
struct DBProgramAuthor
{
    int id{0};
    int program_id{0};
    int author_id{0};
    int position{0};
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

struct DBIgnoredBinary
{
    int id{0};
    std::string sha1;
    std::string name;
};

void registerTables()
{
    static bool registered = false;
    if (registered) return;
    registered = true;
    db::Table<DBVersion>("version")
        .column("id", &DBVersion::id)
        .column("schema_version", &DBVersion::schema_version)
        .column("starter_version", &DBVersion::starter_version)
        .registerTable();

    db::Table<DBProgram>("programs")
        .column("id", &DBProgram::id)
        .column("name", &DBProgram::name)
        .column("origin", &DBProgram::origin)
        .column("origin_reference", &DBProgram::origin_reference)
        .column("description", &DBProgram::description)
        .column("release", &DBProgram::release)
        .column("url", &DBProgram::url)
        .column("license", &DBProgram::license)
        .column("copyright", &DBProgram::copyright)
        .column("starter_version", &DBProgram::starter_version)
        .column("user_modified", &DBProgram::user_modified)
        .registerTable();

    db::Table<DBBinary>("binaries")
        .column("id", &DBBinary::id)
        .column("program_id", &DBBinary::program_id, "programs", "id", db::Action::ON_UPDATE_CASCADE, db::Action::ON_DELETE_CASCADE)
        .column("sha1", &DBBinary::sha1, true)
        .column("release", &DBBinary::release)
        .column("description", &DBBinary::description)
        .column("data", &DBBinary::data)
        .column("size", &DBBinary::size)
        .column("starter_version", &DBBinary::starter_version)
        .column("user_modified", &DBBinary::user_modified)
        .column("analysis_version", &DBBinary::analysis_version)
        .registerTable();

    db::Table<DBBinaryConfig>("binary_configs")
        .column("id", &DBBinaryConfig::id)
        .column("binary_id", &DBBinaryConfig::binary_id, "binaries", "id", db::Action::ON_UPDATE_CASCADE, db::Action::ON_DELETE_CASCADE)
        .column("preset", &DBBinaryConfig::preset)
        .column("properties", &DBBinaryConfig::properties)
        .registerTable();

    db::Table<DBOpcode>("binary_opcodes")
        .column("id", &DBOpcode::id)
        .column("binary_id", &DBOpcode::binary_id, "binaries", "id", db::Action::ON_UPDATE_CASCADE, db::Action::ON_DELETE_CASCADE)
        .column("opcode", &DBOpcode::opcode)
        .column("count", &DBOpcode::count)
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
        .column("query", &DBTags::query)
        .registerTable();

    db::Table<DBProgramTag>("program_tags")
        .column("id", &DBProgramTag::id)
        .column("program_id", &DBProgramTag::program_id, "programs", "id", db::Action::ON_UPDATE_CASCADE, db::Action::ON_DELETE_CASCADE)
        .column("tag_id", &DBProgramTag::tag_id, "tags", "id", db::Action::ON_UPDATE_CASCADE, db::Action::ON_DELETE_CASCADE)
        .registerTable();

    db::Table<DBAuthor>("authors")
        .column("id", &DBAuthor::id)
        .column("name", &DBAuthor::name, true)
        .registerTable();

    db::Table<DBProgramAuthor>("program_authors")
        .column("id", &DBProgramAuthor::id)
        .column("program_id", &DBProgramAuthor::program_id, "programs", "id", db::Action::ON_UPDATE_CASCADE, db::Action::ON_DELETE_CASCADE)
        .column("author_id", &DBProgramAuthor::author_id, "authors", "id", db::Action::ON_UPDATE_CASCADE, db::Action::ON_DELETE_CASCADE)
        .column("position", &DBProgramAuthor::position)
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

    db::Table<DBIgnoredBinary>("ignored_binaries")
        .column("id", &DBIgnoredBinary::id)
        .column("sha1", &DBIgnoredBinary::sha1, true)
        .column("name", &DBIgnoredBinary::name)
        .registerTable();
}

void migrateData(db::Database& database)
{
    auto session = db::Session(database);
    db::Statement(session, "CREATE INDEX IF NOT EXISTS binary_opcodes_binary_id ON binary_opcodes (binary_id);").execute();
    auto versions = session.fetchAll<DBVersion>();
    auto version = versions.empty() ? DBVersion{} : versions.front();
    if (version.schema_version < 2) {
        // Earlier library scans added a filenames row for every known file on every start,
        // and a program_tags row per known ROM entry, so drop all but the first of each.
        db::Transaction transaction{session};
        db::Statement(session, "DELETE FROM filenames WHERE id NOT IN (SELECT MIN(id) FROM filenames GROUP BY binary_id, name);").execute();
        db::Statement(session, "DELETE FROM program_tags WHERE id NOT IN (SELECT MIN(id) FROM program_tags GROUP BY program_id, tag_id);").execute();
        version.schema_version = 2;
        session.insertOrUpdate(version);
        transaction.commit();
    }
    if (version.schema_version < 3) {
        // The origin was free text, now it is the type of origin, other text moves to the new reference.
        db::Transaction transaction{session};
        db::Statement(session, "UPDATE programs SET origin = lower(trim(origin)) WHERE lower(trim(origin)) IN ('gamejam', 'event', 'magazine', 'manual');").execute();
        db::Statement(session, "UPDATE programs SET origin_reference = trim(origin), origin = '' WHERE origin <> '' AND origin NOT IN ('gamejam', 'event', 'magazine', 'manual') AND origin_reference = '';").execute();
        db::Statement(session, "UPDATE programs SET origin = '' WHERE origin NOT IN ('gamejam', 'event', 'magazine', 'manual');").execute();
        version.schema_version = 3;
        session.insertOrUpdate(version);
        transaction.commit();
    }
    if (version.schema_version < 4) {
        // The release of a program is a date like in the chip-8-database, the year column it replaces moves into
        // an empty release. Libraries created after the change never had the column.
        db::Transaction transaction{session};
        bool hasYear = false;
        {
            db::Statement query(session, "SELECT COUNT(*) FROM pragma_table_info('programs') WHERE name = 'year';");
            hasYear = query.step() && static_cast<int>(query.column(0)) > 0;
        }
        if (hasYear) {
            db::Statement(session, "UPDATE programs SET release = CAST(year AS TEXT) WHERE release = '' AND year > 0;").execute();
            db::Statement(session, "ALTER TABLE programs DROP COLUMN year;").execute();
        }
        version.schema_version = 4;
        session.insertOrUpdate(version);
        transaction.commit();
    }
    if (version.schema_version < 5) {
        // ROM data became optional, so the size is kept separately
        db::Transaction transaction{session};
        db::Statement(session, "UPDATE binaries SET size = length(data) WHERE data IS NOT NULL;").execute();
        version.schema_version = 5;
        session.insertOrUpdate(version);
        transaction.commit();
    }
}


// Shorten UTF-8 text to fit into width, keeping the end (for paths) or the start,
// whole code points are removed, the gap is marked with an ellipsis (U+2026).
static std::string fitText(const std::string& text, float width, bool keepEnd = false)
{
    static const std::string ellipsis = "\xE2\x80\xA6";
    if (gui::GetTextSize(text.c_str()).x <= width) {
        return text;
    }
    auto isContinuation = [](char c) { return (static_cast<unsigned char>(c) & 0xC0) == 0x80; };
    auto result = text;
    while (!result.empty() && gui::GetTextSize((keepEnd ? ellipsis + result : result + ellipsis).c_str()).x > width) {
        if (keepEnd) {
            do {
                result.erase(result.begin());
            } while (!result.empty() && isContinuation(result.front()));
        }
        else {
            while (!result.empty() && isContinuation(result.back())) {
                result.pop_back();
            }
            if (!result.empty()) {
                result.pop_back();
            }
        }
    }
    return keepEnd ? ellipsis + result : result + ellipsis;
}

// Colors for tags created in the editor, picked round robin
static constexpr std::array<const char*, 6> NEW_TAG_COLORS = {"#E0A000", "#A060E0", "#40C060", "#E06080", "#4090E0", "#C0C040"};
// The licenses offered by the editor, "Other..." allows any SPDX license expression.
static constexpr std::array<const char*, 14> COMMON_LICENSES = {"CC0-1.0", "Unlicense", "MIT", "BSD-2-Clause", "BSD-3-Clause", "Apache-2.0", "MPL-2.0", "GPL-2.0-or-later", "GPL-3.0-or-later", "LGPL-3.0-or-later", "CC-BY-4.0", "CC-BY-SA-4.0", "CC-BY-NC-4.0", "CC-BY-NC-SA-4.0"};
static constexpr int OTHER_LICENSE_INDEX = static_cast<int>(COMMON_LICENSES.size()) + 1;
// The origin types of the chip-8-database, and how the editor shows them.
static constexpr std::array<const char*, 4> ORIGIN_TYPES = {"gamejam", "event", "magazine", "manual"};
static constexpr std::array<const char*, 4> ORIGIN_LABELS = {"Game jam", "Event", "Magazine", "Manual"};

// A light check of an SPDX license expression (https://spdx.github.io/spdx-spec/v2.3/SPDX-license-expressions/):
// license ids made of letters, digits, '.', '-', '+' and ':', joined by AND, OR and WITH, grouped by parentheses.
// It doesn't check the ids against the SPDX license list.
static bool isSpdxExpression(std::string_view expression)
{
    std::vector<std::string> tokens;
    std::string token;
    for (const char c : expression) {
        if (c == ' ' || c == '(' || c == ')') {
            if (!token.empty()) {
                tokens.push_back(std::move(token));
                token.clear();
            }
            if (c != ' ') {
                tokens.emplace_back(1, c);
            }
        }
        else if (std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-' || c == '+' || c == ':') {
            token.push_back(c);
        }
        else {
            return false;
        }
    }
    if (!token.empty()) {
        tokens.push_back(std::move(token));
    }
    int depth = 0;
    bool expectOperand = true;
    for (const auto& t : tokens) {
        const bool isOperator = t == "AND" || t == "OR" || t == "WITH";
        if (expectOperand) {
            if (t == "(") {
                ++depth;
            }
            else if (t == ")" || isOperator) {
                return false;
            }
            else {
                expectOperand = false;
            }
        }
        else if (t == ")") {
            if (--depth < 0) {
                return false;
            }
        }
        else if (isOperator) {
            expectOperand = true;
        }
        else {
            return false;
        }
    }
    return !tokens.empty() && !expectOperand && depth == 0;
}

// A release date as in the chip-8-database: a year, optionally with month and day (1978, 1978-05, 1978-5-12),
// digits of the year that are not known can be x or ? (197x).
static bool isReleaseDate(std::string_view release)
{
    if (release.size() < 4 || !std::ranges::all_of(release.substr(0, 4), [](char c) { return std::isdigit(static_cast<unsigned char>(c)) || c == 'x' || c == '?'; })) {
        return false;
    }
    std::array<int, 2> limits = {12, 31};
    auto rest = release.substr(4);
    for (const auto limit : limits) {
        if (rest.empty()) {
            return true;
        }
        size_t digits = 1;
        while (digits < rest.size() && std::isdigit(static_cast<unsigned char>(rest[digits]))) {
            ++digits;
        }
        int value = 0;
        if (rest.front() != '-' || digits < 2 || digits > 3 || std::from_chars(rest.data() + 1, rest.data() + digits, value).ec != std::errc{} || value < 1 || value > limit) {
            return false;
        }
        rest = rest.substr(digits);
    }
    return rest.empty();
}

static const char* const STARTER_LIBRARY = "library/cadmium_library_starter.cd48db";
static constexpr int OPCODE_ANALYSIS_VERSION = 1;

static bool isAnalyzable(const std::string& preset)
{
    return !preset.ends_with("-none");
}

static int analysisStartAddress(const std::string& preset, const std::string& properties)
{
    try {
        auto props = emu::CoreRegistry::propertiesForPreset(preset == "generic-chip-8" ? std::string("xo-chip") : preset);
        if (!trim(properties).empty()) {
            props.applyDiff(nlohmann::json::parse(properties));
        }
        if (const auto* prop = props.find("startAddress")) {
            return prop->getInt();
        }
    }
    catch (const std::exception&) {
    }
    return preset.starts_with("eti") ? 0x600 : 0x200;
}

static std::map<uint16_t, int> opcodeStatistics(std::span<const uint8_t> data, int startAddress)
{
    std::scoped_lock lock(Librarian::decompilerMutex());
    emu::Chip8Decompiler decompiler{data, static_cast<uint32_t>(startAddress)};
    decompiler.decompile("", static_cast<uint32_t>(startAddress), nullptr, true, true);
    return {decompiler.fullStats().begin(), decompiler.fullStats().end()};
}

// A new library starts from the metadata compiled into Cadmium, if there is a starter database.
static std::string libraryFile(const std::string& path)
{
    const auto file = path + "/cadmium_library.cd48db";
    std::error_code ec;
    if (fs::exists(file, ec) || !ResourceManager::instance().resourceAvailable(STARTER_LIBRARY)) {
        return file;
    }
    const auto resource = ResourceManager::instance().resourceForName(STARTER_LIBRARY);
    {
        std::ofstream os(file, std::ios::binary);
        os.write(reinterpret_cast<const char*>(resource.data()), static_cast<std::streamsize>(resource.size()));
        if (os) {
            TraceLog(LOG_INFO, fmt::format("New library created from the starter metadata ({} bytes)", resource.size()).c_str());
            return file;
        }
    }
    // a partly written file would be taken for a library, better start empty
    TraceLog(LOG_WARNING, fmt::format("Could not create the library from the starter metadata at '{}'", file).c_str());
    fs::remove(file, ec);
    return file;
}

// The library answers the Librarian's questions about known ROMs, its configs come before the compiled in list.
class LibraryMetadataSource : public Librarian::MetadataSource
{
public:
    explicit LibraryMetadataSource(db::Database& database)
        : _database(database)
    {
    }

    std::optional<RomMetadata> romMetadata(const Sha1::Digest& sha1) const override
    {
        try {
            auto session = db::Session(_database);
            // the first config of a ROM is its default, as in the library's preview
            db::Statement query(session, "SELECT binary_configs.preset, binary_configs.properties FROM binary_configs JOIN binaries ON binaries.id = binary_configs.binary_id "
                                         "WHERE binaries.sha1 = ? ORDER BY binary_configs.id LIMIT 1");
            query << sha1.to_hex();
            if (query.step()) {
                return RomMetadata{std::string(query.column(0)), std::string(query.column(1))};
            }
        }
        catch (const db::Exception& ex) {
            TraceLog(LOG_WARNING, fmt::format("Looking up {} in the library failed: {}", sha1.to_hex(), ex.what()).c_str());
        }
        return std::nullopt;
    }

private:
    db::Database& _database;
};

struct Database::Private
{
    // The edit buffer of the editor dialog, written to the database only on save.
    struct ProgramEdit
    {
        DBProgram program;
        std::string authors;  // comma separated, in order
        bool customLicense{false};  // "Other..." is chosen, the license is edited as text
        int licenseIndex{0};
        int originIndex{0};
        std::set<int> tagIds;
        std::vector<std::string> newTags;
        std::string newTag;
        std::string error;
        int binaryId{-1};   // the ROM shown in the Configs tab
        std::map<int, std::vector<DBBinaryConfig>> configs;
        std::vector<std::pair<int, int>> variantSelections;
    };
    enum class Request { NONE, EDIT, LOAD, RUN, DETACH, MOVE, MERGE, REMOVE };
    struct NewEntry
    {
        std::optional<DBProgram> program;
        DBBinary binary;
        std::string preset;
    };
    static constexpr gui::WindowBoxFlags DIALOG_FLAGS = gui::WindowBoxFlags(gui::WBF_CLOSABLE | gui::WBF_MOVABLE | gui::WBF_MODAL);

    std::mutex mutex;
    db::Database database;
    //db::Session session{database};
    int newTagId{1};
    int unclassifiedTagId{2};
    std::unordered_map<int, DBProgram> programs;
    std::unordered_map<int, DBBinary> binaries;
    std::unordered_map<int, DBTags> tags;
    std::unordered_map<int, DBAuthor> authors;
    std::set<std::string> ignoredSha1s;
    std::vector<std::string> removedSha1s;
    Rectangle visibleListRect{};
    std::vector<std::pair<Rectangle, int>> visibleRows;
    std::vector<size_t> shownIndices;
    std::vector<std::string> sortedTags;
    struct VariantGroup
    {
        std::string core;
        std::vector<std::string> presets;  // the names as used in configs
        std::string variantsCombo;
    };
    std::vector<VariantGroup> variantGroups;
    std::string coresCombo;
    std::string queryLine;
    bool showMissing{false};  // also list programs none of whose ROMs were found in the library folders
    std::set<std::string> selectedTags;
    LibraryQuery query;     // the last valid query of the search field
    std::string queryText;   // the text of that query
    std::string queryError;  // why the current text of the search field is no valid query
    size_t queryErrorPosition{0};
    size_t shownRomCount{0};  // the ROMs matching a query with ROM fields, of the shown programs
    LibraryQuery filter;     // the query of the search field together with the queries of the selected smart tags
    std::string smartTagsQuery;  // the queries of the selected smart tags as one
    std::unordered_map<int, std::set<int>> matchingBinaries;  // of the shown programs, for a query with ROM fields
    std::set<std::string> queryProperties;  // the names of all properties of the cores, lowercase, with aliases
    mutable std::unordered_map<std::string, ConfigFacts> configFactsCache;  // by preset and properties text
    Vector2 tagsScrollPos{};
    float listContentHeight{0};
    int selectedProgramId{-1};
    int selectedBinaryId{-1};  // a ROM of the selected program, or -1 if the program itself is selected
    std::set<int> markedProgramIds;  // the selected program and the ones added with Cmd/Ctrl click
    std::pair<int, int> lastClick{-1, -1};
    double lastClickTime{0};
    Request request{Request::NONE};
    Action requestedAction{Action::NONE};
    std::optional<std::pair<DBBinaryConfig, Action>> configAction;
    bool editorOpen{false};
    int editorTab{0};
    ProgramEdit edit;
    bool moveOpen{false};
    bool mergeOpen{false};
    bool returnToEditor{false};
    int moveBinaryId{-1};
    std::string moveSearch;
    std::vector<int> moveCandidates;
    int moveScroll{0};
    int moveActive{-1};
    std::vector<int> mergeIds;
    int mergeScroll{0};
    int mergeActive{-1};
    std::string dialogError;
    bool removeOpen{false};
    std::vector<int> removeIds;
    std::string removeQuestion;
    bool smartTagOpen{false};
    int smartTagId{-1};  // the edited smart tag, -1 for a new one
    std::string smartTagName;
    std::string smartTagQuery;
    // dragging a ROM line onto another program
    int dragBinaryId{-1};
    Vector2 dragStart{};
    bool dragging{false};
    std::atomic<bool> cancelScan{false};
    emu::ThreadedBackgroundHost backgroundHost;
    LibraryMetadataSource metadataSource{database};


    explicit Private(CadmiumConfiguration& cfg, const std::string& path)
        : database(libraryFile(path), 4)
        , backgroundHost(cfg)
    {
        registerTables();
        db::Registry::syncTables(database);
        migrateData(database);
    }

    bool hasVariant(const DBProgram& program, const std::string& preset) const
    {
        return std::ranges::any_of(program.binaries, [&](int binaryId) {
            auto iter = binaries.find(binaryId);
            return iter != binaries.end() && std::ranges::any_of(iter->second.configs, [&](const DBBinaryConfig& config) { return config.preset == preset; });
        });
    }

    bool isPresent(const DBProgram& program) const
    {
        return std::ranges::any_of(program.binaries, [&](int binaryId) {
            auto iter = binaries.find(binaryId);
            return iter != binaries.end() && iter->second.data;
        });
    }

    // A ROM known only from metadata gets its data when a scan or a drop finds its file, its program is
    // tagged as new like the program of a new ROM. Only the database is written, see applyBinaryData().
    bool storeBinaryData(db::Session& session, int binaryId, int programId, bool tagNew, const std::vector<uint8_t>& data)
    {
        try {
            db::Transaction transaction{session};
            db::Statement fill(session, "UPDATE binaries SET data = ?, size = ? WHERE id = ?");
            fill << db::Blob(data.begin(), data.end()) << static_cast<int>(data.size()) << binaryId;
            fill.execute();
            if (tagNew) {
                session.insert(DBProgramTag{.program_id = programId, .tag_id = newTagId});
            }
            transaction.commit();
        }
        catch (const db::Exception& ex) {
            TraceLog(LOG_WARNING, fmt::format("Storing the data of ROM {} failed: {}", binaryId, ex.what()).c_str());
            return false;
        }
        return true;
    }

    // the cached side of storeBinaryData(), called with the mutex held where the scan runs concurrently
    void applyBinaryData(int binaryId, bool tagNew, const std::vector<uint8_t>& data)
    {
        auto& binary = binaries.at(binaryId);
        binary.data = db::Blob(data.begin(), data.end());
        binary.size = static_cast<int>(data.size());
        if (tagNew) {
            programs.at(binary.program_id).tags.push_back(newTagId);
        }
    }

    // Analyzes the ROMs with data that have no opcode statistics of the current analysis yet, with the mutex
    // released while decompiling, as this runs after scans. Returns the number of analyzed ROMs.
    int analyzeBinaries(db::Session& session)
    {
        struct Job
        {
            int binaryId;
            db::Blob data;
            int startAddress;
            bool analyzable;
        };
        std::vector<Job> jobs;
        {
            std::lock_guard lock(mutex);
            for (const auto& [binaryId, binary] : binaries) {
                if (binary.data && binary.analysis_version != OPCODE_ANALYSIS_VERSION) {
                    const auto preset = binary.configs.empty() ? std::string() : binary.configs.front().preset;
                    const auto properties = binary.configs.empty() ? std::string() : binary.configs.front().properties;
                    jobs.push_back({binaryId, *binary.data, analysisStartAddress(preset, properties), isAnalyzable(preset)});
                }
            }
        }
        int analyzed = 0;
        for (const auto& job : jobs) {
            if (cancelScan) {
                break;
            }
            const auto statistics = job.analyzable ? opcodeStatistics(job.data, job.startAddress) : std::map<uint16_t, int>{};
            try {
                db::Transaction transaction{session};
                db::Statement remove(session, "DELETE FROM binary_opcodes WHERE binary_id = ?");
                remove << job.binaryId;
                remove.execute();
                for (const auto& [opcode, count] : statistics) {
                    session.insert(DBOpcode{.binary_id = job.binaryId, .opcode = opcode, .count = count});
                }
                db::Statement version(session, "UPDATE binaries SET analysis_version = ? WHERE id = ?");
                version << OPCODE_ANALYSIS_VERSION << job.binaryId;
                version.execute();
                transaction.commit();
            }
            catch (const db::Exception& ex) {
                TraceLog(LOG_WARNING, fmt::format("Storing the opcodes of ROM {} failed: {}", job.binaryId, ex.what()).c_str());
                continue;
            }
            std::lock_guard lock(mutex);
            if (auto iter = binaries.find(job.binaryId); iter != binaries.end()) {
                iter->second.opcodes.clear();
                for (const auto& [opcode, count] : statistics) {
                    iter->second.opcodes.push_back(opcode);
                }
                iter->second.analysis_version = OPCODE_ANALYSIS_VERSION;
            }
            if ((++analyzed & 63) == 0) {
                updateFilter();  // "uses" finds more as the analysis goes on
            }
        }
        if (analyzed) {
            std::lock_guard lock(mutex);
            updateFilter();
            TraceLog(LOG_INFO, fmt::format("analyzed the opcodes of {} ROMs", analyzed).c_str());
        }
        return analyzed;
    }

    // whether filling in the data of the ROM should also tag its program as new
    bool needsNewTag(int programId) const
    {
        auto iter = programs.find(programId);
        return newTagId && iter != programs.end() && std::ranges::find(iter->second.tags, newTagId) == iter->second.tags.end();
    }

    ProgramFacts programFacts(const DBProgram& program) const
    {
        ProgramFacts facts{.name = program.name, .description = program.description, .release = program.release, .origin = program.origin,
                           .originReference = program.origin_reference, .license = program.license, .copyright = program.copyright, .url = program.url,
                           .present = isPresent(program)};
        for (const auto authorId : program.authors) {
            if (auto iter = authors.find(authorId); iter != authors.end()) {
                facts.authors.push_back(iter->second.name);
            }
        }
        for (const auto& [tagId, tag] : tags) {
            if (hasTag(program, tagId)) {
                facts.tags.push_back(tag.name);
            }
        }
        
        if (filter.usesRomFields()) {
            for (const auto binaryId : program.binaries) {
                const auto& binary = binaries.at(binaryId);
                RomFacts rom{.sha1 = binary.sha1, .size = binary.size, .files = binary.filenames, .present = binary.data.has_value(),
                             .analyzed = binary.analysis_version == OPCODE_ANALYSIS_VERSION, .opcodes = binary.opcodes};
                for (const auto& config : binary.configs) {
                    rom.configs.push_back(configFacts(config));
                }
                facts.roms.push_back(std::move(rom));
            }
        }
        return facts;
    }

    const ConfigFacts& configFacts(const DBBinaryConfig& config) const
    {
        const auto key = config.preset + '\n' + config.properties;
        if (auto iter = configFactsCache.find(key); iter != configFactsCache.end()) {
            return iter->second;
        }
        ConfigFacts facts{.preset = config.preset};
        try {
            auto props = emu::CoreRegistry::propertiesForPreset(config.preset == "generic-chip-8" ? std::string("xo-chip") : config.preset);
            if (!trim(config.properties).empty()) {
                props.applyDiff(nlohmann::json::parse(config.properties));
            }
            for (size_t i = 0; i < props.numProperties(); ++i) {
                const auto& prop = props[i];
                std::string value;
                std::visit([&value](const auto& v) {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, bool>) {
                        value = v ? "true" : "false";
                    }
                    else if constexpr (std::is_same_v<T, emu::Property::Integer>) {
                        value = std::to_string(v.intValue);
                    }
                    else if constexpr (std::is_same_v<T, std::string>) {
                        value = v;
                    }
                    else if constexpr (std::is_same_v<T, emu::Property::Combo>) {
                        value = v.selectedText();
                    }
                }, prop.getValue());
                if (prop.isValid()) {
                    facts.properties.emplace(toLower(prop.getId()), value);
                }
            }
        }
        catch (const std::exception& ex) {
            TraceLog(LOG_WARNING, fmt::format("No properties of config {} for the search: {}", config.id, ex.what()).c_str());
        }
        auto& p = facts.properties;
        auto flag = [&p](const char* name) { auto iter = p.find(name); return iter != p.end() && iter->second == "true"; };
        if (p.contains("instructionsperframe")) {
            p["ipf"] = p["instructionsperframe"];
        }
        if (p.contains("justshiftvx")) {
            auto text = [](bool value) { return std::string(value ? "true" : "false"); };
            p["shift"] = text(flag("justshiftvx"));
            p["memoryincrementbyx"] = text(flag("loadstoreincibyx"));
            p["memoryleaveiunchanged"] = text(!flag("loadstoreincibyxplus1") && !flag("loadstoreincibyx"));
            p["wrap"] = text(flag("wrapsprites"));
            p["jump"] = text(flag("jump0bxnn"));
            p["vblank"] = text(!flag("instantdxyn"));
            p["logic"] = text(!flag("dontresetvf"));
        }
        return configFactsCache.emplace(key, std::move(facts)).first->second;
    }

    const std::set<std::string>& searchProperties()
    {
        if (queryProperties.empty()) {
            for (const auto& group : variantGroups) {
                for (const auto& preset : group.presets) {
                    for (const auto& [name, value] : configFacts(DBBinaryConfig{.preset = preset}).properties) {
                        queryProperties.insert(name);
                    }
                }
            }
        }
        return queryProperties;
    }

    void setQuery(const std::string& text)
    {
        try {
            query = LibraryQuery::parse(text, searchProperties());
            queryText = text;
            queryError.clear();
        }
        catch (const LibraryQuery::Error& error) {
            queryError = error.what();
            queryErrorPosition = error.position();
        }
        updateFilter();
    }

    // The line under the search field: why its text is no valid query, or what the list shows.
    std::string queryFeedback() const
    {
        if (!queryError.empty()) {
            return fmt::format("Column {}: {} - the list shows the last valid query", queryErrorPosition + 1, queryError);
        }
        auto text = fmt::format("{} of {} programs", shownIndices.size(), programs.size());
        if (filter.usesRomFields()) {
            text += fmt::format(", {} matching ROM{}", shownRomCount, shownRomCount == 1 ? "" : "s");
        }
        return text;
    }

    std::unordered_map<int, DBTags>::const_iterator findTag(const std::string& name) const
    {
        return std::ranges::find_if(tags, [&](const auto& entry) { return entry.second.name == name; });
    }

    int editableSmartTag() const
    {
        if (selectedTags.size() != 1 || !trim(queryLine).empty()) {
            return -1;
        }
        auto iter = findTag(*selectedTags.begin());
        return iter != tags.end() && iter->second.query ? iter->first : -1;
    }

    std::string filterAsQuery() const
    {
        std::vector<std::string> parts;
        std::vector<std::string> variants;
        for (const auto& name : selectedTags) {
            auto iter = findTag(name);
            if (iter == tags.end()) {
                variants.push_back("variant = " + LibraryQuery::quoted(name));
            }
            else if (iter->second.query) {
                parts.push_back(*iter->second.query);
            }
            else {
                parts.push_back("tag = " + LibraryQuery::quoted(name));
            }
        }
        if (!variants.empty()) {
            parts.insert(parts.begin(), join(variants.begin(), variants.end(), " or "));
        }
        parts.push_back(queryLine);
        return LibraryQuery::combine(parts);
    }

    bool hasTag(const DBProgram& program, int tagId) const
    {
        return std::ranges::find(program.tags, tagId) != program.tags.end() || std::ranges::any_of(program.binaries, [&](int binaryId) {
                   auto iter = binaries.find(binaryId);
                   return iter != binaries.end() && std::ranges::find(iter->second.tags, tagId) != iter->second.tags.end();
               });
    }

    // The ROM shown in the info form and the preview, the selected one or the first of the selected program.
    int currentBinaryId() const
    {
        if (selectedBinaryId >= 0 && binaries.contains(selectedBinaryId)) {
            return selectedBinaryId;
        }
        if (auto iter = programs.find(selectedProgramId); iter != programs.end() && !iter->second.binaries.empty()) {
            // a program shows its first ROM that is in the library folders, ROMs only known from metadata can't run
            const auto& ids = iter->second.binaries;
            auto present = std::ranges::find_if(ids, [this](int binaryId) { return binaries.contains(binaryId) && binaries.at(binaryId).data; });
            return present != ids.end() ? *present : ids.front();
        }
        return -1;
    }

    bool clickEntry(int programId, int binaryId, bool addToMarked)
    {
        const auto entry = std::pair{programId, binaryId};
        const bool isDoubleClick = !addToMarked && lastClick == entry && GetTime() - lastClickTime < 0.4;
        lastClick = entry;
        lastClickTime = GetTime();
        if (addToMarked && binaryId < 0) {
            if (markedProgramIds.erase(programId)) {
                if (selectedProgramId == programId) {
                    selectedProgramId = markedProgramIds.empty() ? -1 : *markedProgramIds.begin();
                    selectedBinaryId = -1;
                }
                return false;
            }
            markedProgramIds.insert(programId);
        }
        else {
            markedProgramIds = {programId};
        }
        selectedProgramId = programId;
        selectedBinaryId = binaryId;
        return isDoubleClick;
    }

    static constexpr int PROGRAM_TAB = 0;
    static constexpr int ROMS_TAB = 1;
    static constexpr int CONFIGS_TAB = 2;

    static void setProgramOfBinary(db::Session& session, int binaryId, int programId)
    {
        // an update of just the column, session.update() would also write the ROM data
        db::Statement relink(session, "UPDATE binaries SET program_id = ?, user_modified = 1 WHERE id = ?");
        relink << programId << binaryId;
        relink.execute();
    }

    void relinkBinary(int binaryId, int programId)
    {
        auto& binary = binaries.at(binaryId);
        if (auto iter = programs.find(binary.program_id); iter != programs.end()) {
            std::erase(iter->second.binaries, binaryId);
        }
        binary.program_id = programId;
        binary.user_modified = 1;  // in sync with setProgramOfBinary() and the relink of mergePrograms()
        programs.at(programId).binaries.push_back(binaryId);
    }

    std::string authorNames(const DBProgram& program) const
    {
        std::vector<std::string> names;
        for (const auto authorId : program.authors) {
            if (auto iter = authors.find(authorId); iter != authors.end()) {
                names.push_back(iter->second.name);
            }
        }
        return join(names.begin(), names.end(), ", ");
    }

    static std::vector<std::string> parseAuthors(const std::string& text)
    {
        std::vector<std::string> names;
        for (const auto part : std::views::split(text, ',')) {
            auto name = trim(std::string(part.begin(), part.end()));
            if (!name.empty() && std::ranges::find(names, name) == names.end()) {
                names.push_back(name);
            }
        }
        return names;
    }

    // Links the given authors to the program in order, creating the unknown ones, and returns their ids. Authors are
    // looked up in the database, not the cache, as the cache may still hold authors that were removed as unused.
    std::vector<int> storeAuthors(db::Session& session, int programId, const std::vector<std::string>& names, std::vector<DBAuthor>& createdAuthors)
    {
        db::Statement removeAuthors(session, "DELETE FROM program_authors WHERE program_id = ?");
        removeAuthors << programId;
        removeAuthors.execute();
        std::vector<int> authorIds;
        for (const auto& name : names) {
            int authorId = session.select(db::col(&DBAuthor::id)).where(db::col(&DBAuthor::name) == name).valueOrDefault(0);
            if (!authorId) {
                DBAuthor author{0, name};
                session.insert(author);
                createdAuthors.push_back(author);
                authorId = author.id;
            }
            session.insert(DBProgramAuthor{.program_id = programId, .author_id = authorId, .position = static_cast<int>(authorIds.size())});
            authorIds.push_back(authorId);
        }
        return authorIds;
    }

    void forgetProgram(int programId)
    {
        programs.erase(programId);
        markedProgramIds.erase(programId);
        if (selectedProgramId == programId) {
            selectedProgramId = -1;
            selectedBinaryId = -1;
        }
    }

    std::string detachBinary(int binaryId)
    {
        auto binIter = binaries.find(binaryId);
        if (binIter == binaries.end()) {
            return "Unknown ROM.";
        }
        const auto& source = programs.at(binIter->second.program_id);
        if (source.binaries.size() < 2) {
            return "The only ROM of a program can't be detached.";
        }
        DBProgram program{.name = source.name, .origin = source.origin, .origin_reference = source.origin_reference, .description = source.description, .release = source.release, .url = source.url, .license = source.license, .copyright = source.copyright};
        try {
            auto session = db::Session(database);
            db::Transaction transaction{session};
            session.insert(program);
            for (const auto tagId : source.tags) {
                session.insert(DBProgramTag{.program_id = program.id, .tag_id = tagId});
            }
            for (size_t i = 0; i < source.authors.size(); ++i) {
                session.insert(DBProgramAuthor{.program_id = program.id, .author_id = source.authors[i], .position = static_cast<int>(i)});
            }
            setProgramOfBinary(session, binaryId, program.id);
            transaction.commit();
        }
        catch (const db::Exception& ex) {
            return ex.what();
        }
        program.tags = source.tags;
        program.authors = source.authors;
        programs.emplace(program.id, program);
        relinkBinary(binaryId, program.id);
        if (selectedBinaryId == binaryId) {
            selectedProgramId = program.id;
            markedProgramIds = {program.id};
        }
        updateFilter();
        return {};
    }

    std::string moveBinary(int binaryId, int targetId)
    {
        auto binIter = binaries.find(binaryId);
        if (binIter == binaries.end() || !programs.contains(targetId)) {
            return "Unknown ROM or program.";
        }
        const int sourceId = binIter->second.program_id;
        if (sourceId == targetId) {
            return {};
        }
        const bool removeSource = programs.at(sourceId).binaries.size() == 1;
        try {
            auto session = db::Session(database);
            db::Transaction transaction{session};
            setProgramOfBinary(session, binaryId, targetId);
            if (removeSource) {
                session.remove<DBProgram>(sourceId);
            }
            transaction.commit();
        }
        catch (const db::Exception& ex) {
            return ex.what();
        }
        relinkBinary(binaryId, targetId);
        if (removeSource) {
            forgetProgram(sourceId);
        }
        if (selectedBinaryId == binaryId || selectedProgramId < 0) {
            selectedProgramId = targetId;
            selectedBinaryId = binaryId;
            markedProgramIds = {targetId};
        }
        updateFilter();
        return {};
    }

    std::string mergePrograms(int survivorId, const std::vector<int>& programIds)
    {
        if (!programs.contains(survivorId)) {
            return "Unknown program.";
        }
        std::set<int> tagIds(programs.at(survivorId).tags.begin(), programs.at(survivorId).tags.end());
        auto authorIds = programs.at(survivorId).authors;
        try {
            auto session = db::Session(database);
            db::Transaction transaction{session};
            for (const auto programId : programIds) {
                if (programId == survivorId || !programs.contains(programId)) {
                    continue;
                }
                db::Statement relink(session, "UPDATE binaries SET program_id = ?, user_modified = 1 WHERE program_id = ?");
                relink << survivorId << programId;
                relink.execute();
                for (const auto tagId : programs.at(programId).tags) {
                    if (tagIds.insert(tagId).second) {
                        session.insert(DBProgramTag{.program_id = survivorId, .tag_id = tagId});
                    }
                }
                // the authors of the merged programs follow the ones of the survivor
                for (const auto authorId : programs.at(programId).authors) {
                    if (std::ranges::find(authorIds, authorId) == authorIds.end()) {
                        session.insert(DBProgramAuthor{.program_id = survivorId, .author_id = authorId, .position = static_cast<int>(authorIds.size())});
                        authorIds.push_back(authorId);
                    }
                }
                session.remove<DBProgram>(programId);
            }
            db::Statement modified(session, "UPDATE programs SET user_modified = 1 WHERE id = ?");
            modified << survivorId;
            modified.execute();
            transaction.commit();
        }
        catch (const db::Exception& ex) {
            return ex.what();
        }
        programs.at(survivorId).user_modified = 1;
        for (const auto programId : programIds) {
            if (programId == survivorId || !programs.contains(programId)) {
                continue;
            }
            for (const auto binaryId : std::vector<int>(programs.at(programId).binaries)) {
                relinkBinary(binaryId, survivorId);
            }
            forgetProgram(programId);
        }
        programs.at(survivorId).tags.assign(tagIds.begin(), tagIds.end());
        programs.at(survivorId).authors = authorIds;
        selectedProgramId = survivorId;
        selectedBinaryId = -1;
        markedProgramIds = {survivorId};
        updateFilter();
        return {};
    }

    std::vector<int> sortedByName(std::vector<int> programIds) const
    {
        std::ranges::sort(programIds, std::less<>{}, [this](int programId) { return toLower(programs.at(programId).name); });
        return programIds;
    }

    std::string programLabel(int programId) const
    {
        const auto& program = programs.at(programId);
        return fmt::format("{} ({} ROM{})", program.name, program.binaries.size(), program.binaries.size() == 1 ? "" : "s");
    }

    void updateMoveCandidates()
    {
        moveCandidates.clear();
        const auto search = toLower(trim(moveSearch));
        const int sourceId = binaries.at(moveBinaryId).program_id;
        for (const auto& [programId, program] : programs) {
            if (programId != sourceId && (search.empty() || toLower(program.name).find(search) != std::string::npos)) {
                moveCandidates.push_back(programId);
            }
        }
        moveCandidates = sortedByName(std::move(moveCandidates));
        moveActive = -1;
    }

    void openMove(int binaryId, bool fromEditor, int targetId = -1)
    {
        if (!binaries.contains(binaryId)) {
            return;
        }
        moveBinaryId = binaryId;
        moveSearch.clear();
        moveScroll = 0;
        dialogError.clear();
        updateMoveCandidates();
        if (auto iter = std::ranges::find(moveCandidates, targetId); iter != moveCandidates.end()) {
            moveActive = static_cast<int>(iter - moveCandidates.begin());
            moveScroll = std::max(0, moveActive - 3);
        }
        returnToEditor = fromEditor && editorOpen;
        editorOpen = false;
        moveOpen = true;
    }

    void openMerge()
    {
        if (markedProgramIds.size() < 2) {
            return;
        }
        mergeIds = sortedByName({markedProgramIds.begin(), markedProgramIds.end()});
        auto iter = std::ranges::find(mergeIds, selectedProgramId);
        mergeActive = iter != mergeIds.end() ? static_cast<int>(iter - mergeIds.begin()) : 0;
        mergeScroll = 0;
        dialogError.clear();
        returnToEditor = false;
        mergeOpen = true;
    }

    std::optional<NewEntry> insertBinary(db::Session& session, const std::string& path, const std::vector<uint8_t>& data, const Sha1::Digest& digest, int targetProgramId)
    {
        std::vector<const KnownRomInfo*> foundRoms;
        const bool isKnown = Librarian::findKnownRoms(digest, foundRoms);
        NewEntry entry;
        try {
            db::Transaction transaction{session};
            int programId = targetProgramId;
            if (programId < 0) {
                DBProgram program{.name = isKnown ? std::string(foundRoms.front()->name) : fs::path(path).filename().stem().string()};
                session.insert(program);
                session.insert(DBProgramTag{.program_id = program.id, .tag_id = newTagId});
                program.tags.push_back(newTagId);
                programId = program.id;
                entry.program = program;
            }
            entry.binary = DBBinary{.program_id = programId, .sha1 = digest.to_hex(), .data = db::Blob(data.begin(), data.end()), .size = static_cast<int>(data.size())};
            session.insert(entry.binary);
            if (isKnown) {
                for (const auto* romInfo : foundRoms) {
                    auto config = DBBinaryConfig{.binary_id = entry.binary.id, .preset = std::string(romInfo->preset), .properties = std::string(romInfo->options ? romInfo->options : "")};
                    session.insert(config);
                    entry.binary.configs.push_back(config);
                }
                entry.preset = foundRoms.front()->preset;
            }
            else {
                // unknown ROMs get a variant guessed from the extension and stay unclassified
                auto extension = fs::path(path).extension().string();
                if (extension != ".ch8") {
                    if (auto preset = toLower(emu::CoreRegistry::presetForExtension(extension)); !preset.empty()) {
                        auto config = DBBinaryConfig{.binary_id = entry.binary.id, .preset = preset};
                        session.insert(config);
                        entry.binary.configs.push_back(config);
                    }
                }
                session.insert(DBBinaryTag{.binary_id = entry.binary.id, .tag_id = unclassifiedTagId});
                entry.binary.tags.push_back(unclassifiedTagId);
                entry.preset = "???";
            }
            session.insert(DBFilename{.binary_id = entry.binary.id, .name = path});
            entry.binary.filenames.push_back(path);
            transaction.commit();
        }
        catch (const db::Exception& ex) {
            TraceLog(LOG_WARNING, fmt::format("Adding '{}' to the library failed: {}", path, ex.what()).c_str());
            return std::nullopt;
        }
        return entry;
    }

    void addEntry(const NewEntry& entry)
    {
        if (entry.program) {
            programs.emplace(entry.program->id, *entry.program);
        }
        binaries.emplace(entry.binary.id, entry.binary);
        programs.at(entry.binary.program_id).binaries.push_back(entry.binary.id);
    }

    std::string removeBinaries(const std::vector<int>& binaryIds, bool ignore)
    {
        std::map<int, size_t> removedPerProgram;
        try {
            auto session = db::Session(database);
            db::Transaction transaction{session};
            for (const auto binaryId : binaryIds) {
                const auto& binary = binaries.at(binaryId);
                if (ignore && !ignoredSha1s.contains(binary.sha1)) {
                    session.insert(DBIgnoredBinary{.sha1 = binary.sha1, .name = binary.filenames.empty() ? std::string() : binary.filenames.front()});
                }
                session.remove<DBBinary>(binaryId);
                ++removedPerProgram[binary.program_id];
            }
            for (const auto& [programId, count] : removedPerProgram) {
                if (programs.at(programId).binaries.size() == count) {
                    session.remove<DBProgram>(programId);
                }
            }
            transaction.commit();
        }
        catch (const db::Exception& ex) {
            return ex.what();
        }
        for (const auto binaryId : binaryIds) {
            const auto& binary = binaries.at(binaryId);
            if (ignore) {
                ignoredSha1s.insert(binary.sha1);
            }
            removedSha1s.push_back(binary.sha1);
            std::erase(programs.at(binary.program_id).binaries, binaryId);
            if (selectedBinaryId == binaryId) {
                selectedBinaryId = -1;
            }
            binaries.erase(binaryId);
        }
        for (const auto& [programId, count] : removedPerProgram) {
            if (programs.at(programId).binaries.empty()) {
                forgetProgram(programId);
            }
        }
        updateFilter();
        return {};
    }

    void openRemove(int programId, int binaryId, bool fromEditor)
    {
        auto iter = programs.find(programId);
        if (iter == programs.end() || (binaryId >= 0 && !binaries.contains(binaryId))) {
            return;
        }
        const auto& program = iter->second;
        if (binaryId >= 0) {
            removeIds = {binaryId};
            removeQuestion = fmt::format("Remove the ROM {} of '{}'?", binaries.at(binaryId).sha1.substr(0, 8), program.name);
        }
        else {
            removeIds = program.binaries;
            removeQuestion = fmt::format("Remove '{}' with its {} ROM{}?", program.name, program.binaries.size(), program.binaries.size() == 1 ? "" : "s");
        }
        dialogError.clear();
        returnToEditor = fromEditor && editorOpen;
        editorOpen = false;
        removeOpen = true;
    }

    void renderRemoveDialog()
    {
        using namespace gui;
        if (!BeginWindowBox({-1, -1, 520, 180}, "Remove###libraryRemove", &removeOpen, DIALOG_FLAGS)) {
            if (!removeOpen) {
                closeSubDialog();
            }
            return;
        }
        SetSpacing(2);
        Label(fitText(removeQuestion, GetContentAvailable().width - 16).c_str());
        Space(4);
        Label("\"Remove entry\" removes it from the library, the next scan adds it");
        Label("again while the file is in a library folder. \"Remove and ignore\"");
        Label("keeps future scans from adding it, dropping the file here undoes that.");
        Space(GetContentAvailable().height - GetStyle(DEFAULT, TEXT_SIZE) - 16);
        BeginColumns();
        SetNextWidth(GetContentAvailable().width - 80 - 2 * 140 - 24);
        Label(dialogError.c_str());
        SetNextWidth(80);
        if (Button("Cancel")) {
            closeSubDialog();
        }
        std::optional<bool> ignore;
        SetNextWidth(140);
        if (Button("Remove entry")) {
            ignore = false;
        }
        if (Button("Remove and ignore")) {
            ignore = true;
        }
        EndColumns();
        if (ignore) {
            if (auto error = removeBinaries(removeIds, *ignore); error.empty()) {
                closeSubDialog();
            }
            else {
                dialogError = error;
            }
        }
        EndWindowBox();
    }

    void openSmartTag()
    {
        dialogError.clear();
        smartTagId = editableSmartTag();
        if (smartTagId >= 0) {
            smartTagName = tags.at(smartTagId).name;
            smartTagQuery = tags.at(smartTagId).query.value_or("");
        }
        else {
            smartTagName.clear();
            try {
                smartTagQuery = filterAsQuery();
            }
            catch (const LibraryQuery::Error&) {
                smartTagQuery = queryLine;
                dialogError = "A selected tag can't be part of a query, it has a quote in its name.";
            }
        }
        smartTagOpen = true;
    }

    // Stores the smart tag of the dialog, a new one becomes the selection instead of what it was saved from.
    std::string saveSmartTag(const std::set<std::string>& reservedNames)
    {
        const auto name = trim(smartTagName);
        const auto text = trim(smartTagQuery);
        if (name.empty()) {
            return "The smart tag needs a name.";
        }
        if (auto iter = findTag(name); (iter != tags.end() && iter->first != smartTagId) || reservedNames.contains(name)) {
            return fmt::format("There is a tag or variant named '{}' already.", name);
        }
        if (text.empty()) {
            return "The smart tag needs a query.";
        }
        try {
            LibraryQuery::parse(text, searchProperties());
        }
        catch (const LibraryQuery::Error& error) {
            return error.what();
        }
        DBTags tag = smartTagId >= 0 ? tags.at(smartTagId) : DBTags{0, name, NEW_TAG_COLORS[tags.size() % NEW_TAG_COLORS.size()]};
        tag.name = name;
        tag.query = text;
        try {
            auto session = db::Session(database);
            if (tag.id) {
                session.update(tag);
            }
            else {
                session.insert(tag);
            }
        }
        catch (const db::Exception& ex) {
            return ex.what();
        }
        if (smartTagId >= 0) {
            selectedTags.erase(tags.at(smartTagId).name);
        }
        else {
            selectedTags.clear();
            queryLine.clear();
            setQuery(queryLine);
        }
        tags.insert_or_assign(tag.id, tag);
        selectedTags.insert(name);
        updateFilter();
        return {};
    }

    std::string deleteSmartTag()
    {
        try {
            auto session = db::Session(database);
            db::Statement remove(session, "DELETE FROM tags WHERE id = ?");
            remove << smartTagId;
            remove.execute();
        }
        catch (const db::Exception& ex) {
            return ex.what();
        }
        selectedTags.erase(tags.at(smartTagId).name);
        tags.erase(smartTagId);
        updateFilter();
        return {};
    }

    // Returns true when the smart tags changed.
    bool renderSmartTagDialog(const std::set<std::string>& reservedNames)
    {
        using namespace gui;
        constexpr float labelWidth = 50;
        if (!BeginWindowBox({-1, -1, 520, 150}, smartTagId >= 0 ? "Edit smart tag###librarySmartTag" : "Save as smart tag###librarySmartTag", &smartTagOpen, DIALOG_FLAGS)) {
            return false;
        }
        SetSpacing(2);
        Label("A smart tag lists the programs its query finds, combined with the other selections.");
        Space(4);
        BeginColumns();
        SetNextWidth(labelWidth);
        Label("Name:");
        SetNextWidth(160);
        TextBox("##smartTagName", smartTagName, 32);
        EndColumns();
        BeginColumns();
        SetNextWidth(labelWidth);
        Label("Query:");
        TextBox("##smartTagQuery", smartTagQuery, 4096);
        EndColumns();
        Space(GetContentAvailable().height - GetStyle(DEFAULT, TEXT_SIZE) - 16);
        bool changed = false;
        BeginColumns();
        SetSpacing(4);
        const auto errorWidth = GetContentAvailable().width - 3 * 80 - 3 * 4;
        SetNextWidth(errorWidth);
        Label(fitText(dialogError, errorWidth).c_str());
        SetNextWidth(80);
        if (smartTagId < 0) {
            GuiDisable();
        }
        if (Button("Delete")) {
            dialogError = deleteSmartTag();
            changed = dialogError.empty();
        }
        GuiEnable();
        SetNextWidth(80);
        if (Button("Cancel")) {
            smartTagOpen = false;
        }
        SetNextWidth(80);
        if (Button("Save")) {
            dialogError = saveSmartTag(reservedNames);
            changed = dialogError.empty();
        }
        EndColumns();
        if (changed) {
            smartTagOpen = false;
        }
        EndWindowBox();
        return changed;
    }

    void closeSubDialog()
    {
        moveOpen = mergeOpen = removeOpen = false;
        editorOpen = returnToEditor && programs.contains(edit.program.id);
        returnToEditor = false;
    }

    bool renderDialogButtons(const char* action, bool enabled)
    {
        using namespace gui;
        bool clicked = false;
        BeginColumns();
        SetNextWidth(GetContentAvailable().width - 2 * 80 - 8);
        Label(dialogError.c_str());
        SetNextWidth(80);
        if (Button("Cancel")) {
            closeSubDialog();
        }
        if (!enabled) {
            GuiDisable();
        }
        clicked = Button(action);
        GuiEnable();
        EndColumns();
        return clicked;
    }

    void renderMoveDialog()
    {
        using namespace gui;
        if (!BeginWindowBox({-1, -1, 460, 330}, "Move ROM###libraryMove", &moveOpen, DIALOG_FLAGS)) {
            if (!moveOpen) {
                closeSubDialog();
            }
            return;
        }
        SetSpacing(2);
        const auto& binary = binaries.at(moveBinaryId);
        const auto& source = programs.at(binary.program_id);
        const bool removesSource = source.binaries.size() == 1;
        Label(fitText(fmt::format("Move the ROM {} of '{}' to:", binary.sha1.substr(0, 8), source.name), GetContentAvailable().width - 16).c_str());
        if (TextBox("##moveSearch", moveSearch, 255)) {
            updateMoveCandidates();
        }
        std::vector<std::string> labels;
        std::vector<const char*> items;
        for (const auto programId : moveCandidates) {
            labels.push_back(programLabel(programId));
        }
        for (const auto& label : labels) {
            items.push_back(label.c_str());
        }
        ListViewEx(180, items.data(), static_cast<int>(items.size()), &moveScroll, &moveActive);
        Label(removesSource ? "The program has no other ROM, it will be removed." : "");
        if (renderDialogButtons("Move", moveActive >= 0 && moveActive < static_cast<int>(moveCandidates.size()))) {
            if (auto error = moveBinary(moveBinaryId, moveCandidates[moveActive]); error.empty()) {
                closeSubDialog();
            }
            else {
                dialogError = error;
            }
        }
        EndWindowBox();
    }

    void renderMergeDialog()
    {
        using namespace gui;
        if (!BeginWindowBox({-1, -1, 460, 300}, "Merge programs###libraryMerge", &mergeOpen, DIALOG_FLAGS)) {
            if (!mergeOpen) {
                closeSubDialog();
            }
            return;
        }
        SetSpacing(2);
        Label("Merge these programs into one, keeping the metadata of:");
        std::vector<std::string> labels;
        std::vector<const char*> items;
        for (const auto programId : mergeIds) {
            labels.push_back(programLabel(programId));
        }
        for (const auto& label : labels) {
            items.push_back(label.c_str());
        }
        ListViewEx(160, items.data(), static_cast<int>(items.size()), &mergeScroll, &mergeActive);
        Label("The ROMs and tags of the others move to it, then they are removed.");
        if (renderDialogButtons("Merge", mergeActive >= 0 && mergeActive < static_cast<int>(mergeIds.size()))) {
            if (auto error = mergePrograms(mergeIds[mergeActive], mergeIds); error.empty()) {
                closeSubDialog();
            }
            else {
                dialogError = error;
            }
        }
        EndWindowBox();
    }

    static int originIndex(const std::string& origin)
    {
        auto iter = std::ranges::find(ORIGIN_TYPES, origin);
        return iter == ORIGIN_TYPES.end() ? 0 : static_cast<int>(iter - ORIGIN_TYPES.begin()) + 1;
    }

    // the entry of the license dropdown for the edited program
    int licenseIndex() const
    {
        if (edit.customLicense) {
            return OTHER_LICENSE_INDEX;
        }
        auto iter = std::ranges::find(COMMON_LICENSES, edit.program.license);
        return iter == COMMON_LICENSES.end() ? 0 : static_cast<int>(iter - COMMON_LICENSES.begin()) + 1;
    }

    void openEditor(int tab = PROGRAM_TAB)
    {
        auto iter = programs.find(selectedProgramId);
        if (iter == programs.end()) {
            return;
        }
        const auto& program = iter->second;
        edit = ProgramEdit{.program = program, .authors = authorNames(program), .tagIds = {program.tags.begin(), program.tags.end()}};
        edit.customLicense = !program.license.empty() && std::ranges::find(COMMON_LICENSES, program.license) == COMMON_LICENSES.end();
        edit.licenseIndex = licenseIndex();
        edit.originIndex = originIndex(program.origin);
        edit.binaryId = currentBinaryId();
        editorTab = tab;
        editorOpen = true;
    }

    // Keeps the "???" tag of a ROM in sync with it having configs or not.
    void updateUnclassifiedTag(db::Session& session, DBBinary& binary)
    {
        const bool isTagged = std::ranges::find(binary.tags, unclassifiedTagId) != binary.tags.end();
        if (binary.configs.empty() && !isTagged && unclassifiedTagId) {
            session.insert(DBBinaryTag{.binary_id = binary.id, .tag_id = unclassifiedTagId});
            binary.tags.push_back(unclassifiedTagId);
        }
        else if (!binary.configs.empty() && isTagged) {
            db::Statement removeTag(session, "DELETE FROM binary_tags WHERE binary_id = ? AND tag_id = ?");
            removeTag << binary.id << unclassifiedTagId;
            removeTag.execute();
            std::erase(binary.tags, unclassifiedTagId);
        }
    }

    void storeConfigs(db::Session& session, DBBinary& binary, std::vector<DBBinaryConfig> configs)
    {
        db::Statement removeConfigs(session, "DELETE FROM binary_configs WHERE binary_id = ?");
        removeConfigs << binary.id;
        removeConfigs.execute();
        for (auto& config : configs) {
            config.id = 0;
            config.binary_id = binary.id;
            session.insert(config);
        }
        db::Statement modified(session, "UPDATE binaries SET user_modified = 1 WHERE id = ?");
        modified << binary.id;
        modified.execute();
        binary.configs = std::move(configs);
        binary.user_modified = 1;
        updateUnclassifiedTag(session, binary);
    }

    std::vector<DBBinaryConfig>* editedConfigs()
    {
        auto iter = binaries.find(edit.binaryId);
        if (iter == binaries.end()) {
            return nullptr;
        }
        return &edit.configs.try_emplace(edit.binaryId, iter->second.configs).first->second;
    }

    bool saveEditor()
    {
        auto program = edit.program;
        program.name = trim(program.name);
        if (program.name.empty()) {
            edit.error = "The name must not be empty.";
            return false;
        }
        program.release = trim(program.release);
        if (!program.release.empty() && !isReleaseDate(program.release)) {
            edit.error = "The release must be a date like 1978, 1978-05 or 1978-05-12.";
            return false;
        }
        program.origin_reference = trim(program.origin_reference);
        program.license = trim(program.license);
        if (!program.license.empty() && !isSpdxExpression(program.license)) {
            edit.error = "The license must be an SPDX expression like MIT OR Apache-2.0.";
            return false;
        }
        program.copyright = trim(program.copyright);
        for (const auto& [binaryId, configs] : edit.configs) {
            for (const auto& config : configs) {
                if (config.preset.empty()) {
                    edit.error = "Every config needs a variant.";
                    return false;
                }
                if (!trim(config.properties).empty() && !nlohmann::json::accept(config.properties)) {
                    edit.error = fmt::format("The properties of {} are not valid JSON.", config.preset);
                    return false;
                }
            }
        }
        auto tagIds = edit.tagIds;
        std::vector<DBTags> createdTags;
        std::vector<int> authorIds;
        std::vector<DBAuthor> createdAuthors;
        // configs are replaced on copies, so a failed transaction leaves the cached ROMs unchanged
        std::map<int, DBBinary> changedBinaries;
        try {
            auto session = db::Session(database);
            db::Transaction transaction{session};
            for (const auto& [binaryId, configs] : edit.configs) {
                if (auto iter = binaries.find(binaryId); iter != binaries.end() && iter->second.configs.size() + configs.size() > 0) {
                    auto& binary = changedBinaries.emplace(binaryId, iter->second).first->second;
                    auto trimmed = configs;
                    for (auto& config : trimmed) {
                        config.properties = trim(config.properties);
                    }
                    storeConfigs(session, binary, trimmed);
                }
            }
            for (const auto& name : edit.newTags) {
                if (auto iter = findTag(name); iter != tags.end() && iter->second.query) {
                    edit.error = fmt::format("'{}' is a smart tag, it can't be given to programs.", name);
                    return false;
                }
                else if (iter != tags.end()) {
                    tagIds.insert(iter->first);
                }
                else {
                    DBTags tag{0, name, NEW_TAG_COLORS[(tags.size() + createdTags.size()) % NEW_TAG_COLORS.size()]};
                    session.insert(tag);
                    createdTags.push_back(tag);
                    tagIds.insert(tag.id);
                }
            }
            program.user_modified = 1;
            session.update(program);
            db::Statement removeTags(session, "DELETE FROM program_tags WHERE program_id = ?");
            removeTags << program.id;
            removeTags.execute();
            for (const auto tagId : tagIds) {
                session.insert(DBProgramTag{.program_id = program.id, .tag_id = tagId});
            }
            authorIds = storeAuthors(session, program.id, parseAuthors(edit.authors), createdAuthors);
            db::Statement(session, "DELETE FROM authors WHERE id NOT IN (SELECT author_id FROM program_authors);").execute();
            transaction.commit();
        }
        catch (const db::Exception& ex) {
            edit.error = ex.what();
            return false;
        }
        for (const auto& tag : createdTags) {
            tags.emplace(tag.id, tag);
        }
        for (const auto& author : createdAuthors) {
            authors.emplace(author.id, author);
        }
        for (auto& [binaryId, binary] : changedBinaries) {
            binaries.at(binaryId) = std::move(binary);
        }
        auto& stored = programs.at(program.id);
        stored.name = program.name;
        stored.origin = program.origin;
        stored.origin_reference = program.origin_reference;
        stored.description = program.description;
        stored.release = program.release;
        stored.url = program.url;
        stored.license = program.license;
        stored.copyright = program.copyright;
        stored.user_modified = program.user_modified;
        stored.tags.assign(tagIds.begin(), tagIds.end());
        stored.authors = authorIds;
        updateFilter();
        return true;
    }

    void renderTagToggles(float labelWidth)
    {
        using namespace gui;
        std::vector<const DBTags*> sorted;
        for (const auto& tag : tags | std::views::values) {
            if (!tag.query) {
                sorted.push_back(&tag);
            }
        }
        std::ranges::sort(sorted, std::less<>{}, [](const DBTags* tag) { return toLower(tag->name); });
        // the tags as toggles, wrapped into rows, followed by the tags that will be created on save
        std::vector<std::pair<std::string, bool>> items;
        for (const auto* tag : sorted) {
            items.emplace_back(tag->name, edit.tagIds.contains(tag->id));
        }
        for (const auto& name : edit.newTags) {
            items.emplace_back(name, true);
        }
        const float width = GetContentAvailable().width - labelWidth - 4;
        size_t index = 0;
        do {
            BeginColumns();
            SetNextWidth(labelWidth);
            Label(index ? "" : "Tags:");
            float used = 0;
            while (index < items.size()) {
                auto& [name, active] = items[index];
                const auto itemWidth = GetTextSize(name.c_str()).x + 12;
                if (used > 0 && used + itemWidth > width) {
                    break;
                }
                SetNextWidth(itemWidth);
                if (Toggle(name.c_str(), &active)) {
                    if (index >= sorted.size()) {
                        std::erase(edit.newTags, name);
                    }
                    else if (active) {
                        edit.tagIds.insert(sorted[index]->id);
                    }
                    else {
                        edit.tagIds.erase(sorted[index]->id);
                    }
                }
                used += itemWidth + 4;
                ++index;
            }
            EndColumns();
        } while (index < items.size());
        BeginColumns();
        SetNextWidth(labelWidth);
        Label("");
        SetNextWidth(120);
        TextBox("##newTag", edit.newTag, 32);
        SetNextWidth(60);
        if (Button("+ tag")) {
            auto name = trim(edit.newTag);
            if (!name.empty()) {
                if (auto iter = findTag(name); iter != tags.end() && iter->second.query) {
                    edit.error = fmt::format("'{}' is a smart tag, it can't be given to programs.", name);
                }
                else if (iter != tags.end()) {
                    edit.tagIds.insert(iter->first);
                }
                else if (std::ranges::find(edit.newTags, name) == edit.newTags.end()) {
                    edit.newTags.push_back(name);
                }
            }
            edit.newTag.clear();
        }
        EndColumns();
    }

    void renderProgramTab()
    {
        using namespace gui;
        constexpr float labelWidth = 80;
        auto textRow = [&](const char* label, const char* id, std::string& value, int maxLength) {
            BeginColumns();
            SetNextWidth(labelWidth);
            Label(label);
            TextBox(id, value, maxLength);
            EndColumns();
        };
        textRow("Name:", "##name", edit.program.name, 255);
        textRow("Authors:", "##authors", edit.authors, 1024);
        BeginColumns();
        SetNextWidth(labelWidth);
        Label("Release:");
        SetNextWidth(100);
        TextBox("##release", edit.program.release, 10);
        SetNextWidth(70);
        Label("  Origin:");
        static const auto originItems = "-;" + join(ORIGIN_LABELS.begin(), ORIGIN_LABELS.end(), ";") + "##origin";
        SetNextWidth(100);
        if (DropdownBox(originItems.c_str(), &edit.originIndex)) {
            edit.program.origin = edit.originIndex > 0 ? ORIGIN_TYPES[edit.originIndex - 1] : "";
        }
        edit.originIndex = originIndex(edit.program.origin);
        TextBox("##originReference", edit.program.origin_reference, 255);
        EndColumns();
        BeginColumns();
        SetNextWidth(labelWidth);
        Label("License:");
        static const auto licenseItems = "-;" + join(COMMON_LICENSES.begin(), COMMON_LICENSES.end(), ";") + ";Other...##license";
        SetNextWidth(140);
        if (DropdownBox(licenseItems.c_str(), &edit.licenseIndex)) {
            // a license chosen from the list stays as text to start from, when switching to "Other..."
            edit.customLicense = edit.licenseIndex == OTHER_LICENSE_INDEX;
            if (!edit.customLicense) {
                edit.program.license = edit.licenseIndex > 0 ? COMMON_LICENSES[edit.licenseIndex - 1] : "";
            }
        }
        // the selection is made when the list is drawn at the end of the frame, so the index is synced after the call
        edit.licenseIndex = licenseIndex();
        if (edit.customLicense) {
            TextBox("##licenseExpression", edit.program.license, 255);
        }
        EndColumns();
        textRow("Copyright:", "##copyright", edit.program.copyright, 255);
        textRow("URL:", "##url", edit.program.url, 1024);
        textRow("Description:", "##description", edit.program.description, 4096);
        renderTagToggles(labelWidth);
    }

    void renderRomsTab()
    {
        using namespace gui;
        const auto& program = programs.at(edit.program.id);
        Label("The actions in this tab are applied immediately, not on Save.");
        std::optional<int> showConfigs, detach, move, remove;
        const auto infoWidth = GetContentAvailable().width - 4 * 100;
        for (const auto binaryId : program.binaries) {
            const auto& binary = binaries.at(binaryId);
            std::vector<std::string> variants;
            for (const auto& config : binary.configs) {
                variants.push_back(config.preset);
            }
            PushId(&binary);
            BeginColumns();
            SetNextWidth(infoWidth);
            Label(fitText(fmt::format("{}  {} bytes  {} file{}  {}", binary.sha1.substr(0, 8), binary.size, binary.filenames.size(), binary.filenames.size() == 1 ? "" : "s", variants.empty() ? "???" : join(variants.begin(), variants.end(), ", ")), infoWidth - 16).c_str());
            SetNextWidth(80);
            if (Button("Configs")) {
                showConfigs = binaryId;
            }
            if (program.binaries.size() < 2) {
                GuiDisable();
            }
            SetNextWidth(80);
            if (Button("Detach")) {
                detach = binaryId;
            }
            GuiEnable();
            SetNextWidth(80);
            if (Button("Move...")) {
                move = binaryId;
            }
            if (Button("Remove...")) {
                remove = binaryId;
            }
            EndColumns();
            PopId();
        }
        if (showConfigs) {
            edit.binaryId = *showConfigs;
            editorTab = CONFIGS_TAB;
        }
        if (detach) {
            edit.error = detachBinary(*detach);
        }
        if (move) {
            openMove(*move, true);
        }
        else if (remove) {
            openRemove(edit.program.id, *remove, true);
        }
    }

    std::pair<int, int> findVariant(const std::string& preset) const
    {
        for (size_t group = 0; group < variantGroups.size(); ++group) {
            if (auto iter = std::ranges::find(variantGroups[group].presets, preset); iter != variantGroups[group].presets.end()) {
                return {static_cast<int>(group), static_cast<int>(iter - variantGroups[group].presets.begin())};
            }
        }
        return {-1, -1};
    }

    void renderConfigsTab()
    {
        using namespace gui;
        const auto& program = programs.at(edit.program.id);
        if (program.binaries.empty()) {
            Label("This program has no ROMs.");
            return;
        }
        if (std::ranges::find(program.binaries, edit.binaryId) == program.binaries.end()) {
            edit.binaryId = program.binaries.front();
        }
        // the ROM to edit, a combo box if there are several
        BeginColumns();
        SetNextWidth(80);
        Label("ROM:");
        std::vector<std::string> romNames;
        for (const auto binaryId : program.binaries) {
            const auto& binary = binaries.at(binaryId);
            romNames.push_back(fmt::format("{} ({} bytes)", binary.sha1.substr(0, 8), binary.size));
        }
        if (romNames.size() == 1) {
            Label(romNames.front().c_str());
        }
        else {
            int romIndex = static_cast<int>(std::ranges::find(program.binaries, edit.binaryId) - program.binaries.begin());
            SetNextWidth(200);
            if (ComboBox(join(romNames.begin(), romNames.end(), ";").c_str(), &romIndex)) {
                edit.binaryId = program.binaries[romIndex];
            }
        }
        EndColumns();
        auto* configs = editedConfigs();
        if (!configs) {
            return;
        }
        Label("The variants this ROM runs on, the first one is used for the preview.");
        Label("Properties are a JSON diff to the variant, \"Save settings to library\"");
        Label("in the main menu captures them from the running emulator.");
        std::optional<size_t> moveUp, remove;
        std::optional<std::pair<size_t, Action>> load;
        edit.variantSelections.resize(configs->size());
        for (size_t i = 0; i < configs->size(); ++i) {
            auto& config = (*configs)[i];
            auto& [coreSelection, variantSelection] = edit.variantSelections[i];
            PushId(&config);
            BeginColumns();
            // lower rows open their lists upwards, so they stay inside the window
            const bool openUp = GetCurrentPos().y > 210;
            auto [groupIndex, variantIndex] = findVariant(config.preset);
            // a variant name that is not known stays as first core item, until another core is chosen
            const int offset = groupIndex < 0 ? 1 : 0;
            const auto coreItems = (groupIndex < 0 ? config.preset + ";" : std::string()) + coresCombo;
            SetNextWidth(140);
            if (DropdownBox((coreItems + "##core").c_str(), &coreSelection, openUp) && coreSelection >= offset && coreSelection - offset < static_cast<int>(variantGroups.size())) {
                config.preset = variantGroups[coreSelection - offset].presets.front();
            }
            // the items are part of the id, rlguipp keeps the items of a dropdown with the same id
            const auto variantItems = groupIndex < 0 ? config.preset : variantGroups[groupIndex].variantsCombo;
            if (groupIndex < 0) {
                GuiDisable();
            }
            SetNextWidth(110);
            if (DropdownBox((variantItems + "##variant").c_str(), &variantSelection, openUp) && groupIndex >= 0 && variantSelection < static_cast<int>(variantGroups[groupIndex].presets.size())) {
                config.preset = variantGroups[groupIndex].presets[variantSelection];
            }
            GuiEnable();
            // in sync with the (maybe just changed) variant, as shown when the dropdowns are drawn
            std::tie(groupIndex, variantIndex) = findVariant(config.preset);
            coreSelection = groupIndex < 0 ? 0 : groupIndex;
            variantSelection = std::max(0, variantIndex);
            if (i == 0) {
                GuiDisable();
            }
            SetNextWidth(40);
            if (Button("Up")) {
                moveUp = i;
            }
            GuiEnable();
            SetNextWidth(40);
            if (Button("Del")) {
                remove = i;
            }
            // a ROM only known from metadata has nothing to load
            if (!binaries.at(edit.binaryId).data) {
                GuiDisable();
            }
            SetNextWidth(20);
            if (Button(IconText(ICON_ROM, ""))) {
                load = {i, Action::LOAD};
            }
            SetTooltip("LOAD");
            SetNextWidth(20);
            if (Button(IconText(ICON_PLAYER_PLAY, ""))) {
                load = {i, Action::RUN};
            }
            SetTooltip("RUN");
            GuiEnable();
            TextBox("##properties", config.properties, 4096);
            EndColumns();
            PopId();
        }
        if (moveUp) {
            std::swap((*configs)[*moveUp], (*configs)[*moveUp - 1]);
        }
        if (remove) {
            configs->erase(configs->begin() + static_cast<long>(*remove));
        }
        if (load) {
            configAction = {(*configs)[load->first], load->second};
            configAction->first.binary_id = edit.binaryId;
        }
        SetNextWidth(100);
        if (Button("+ variant")) {
            configs->push_back(DBBinaryConfig{.binary_id = edit.binaryId, .preset = "chip-8"});
        }
    }

    bool renderEditorFooter()
    {
        using namespace gui;
        bool saved = false;
        Space(GetContentAvailable().height - GetStyle(DEFAULT, TEXT_SIZE) - 16);
        BeginColumns();
        SetNextWidth(GetContentAvailable().width - 2 * 80 - 8);
        Label(edit.error.c_str());
        SetNextWidth(80);
        if (Button("Cancel")) {
            editorOpen = false;
        }
        if (Button("Save") && saveEditor()) {
            editorOpen = false;
            saved = true;
        }
        EndColumns();
        return saved;
    }

    bool renderEditor()
    {
        using namespace gui;
        bool saved = false;
        const auto title = fmt::format("Edit: {}###libraryEditor", edit.program.name);
        if (!BeginWindowBox({-1, -1, 620, 440}, title.c_str(), &editorOpen, DIALOG_FLAGS)) {
            return false;
        }
        SetSpacing(2);
        BeginTabView(&editorTab);
        if (BeginTab("Program")) {
            SetSpacing(2);
            renderProgramTab();
            saved = renderEditorFooter();
            EndTab();
        }
        if (BeginTab("ROMs")) {
            SetSpacing(2);
            renderRomsTab();
            saved = renderEditorFooter();
            EndTab();
        }
        if (BeginTab("Configs")) {
            SetSpacing(2);
            renderConfigsTab();
            saved = renderEditorFooter();
            EndTab();
        }
        EndTabView();
        EndWindowBox();
        return saved;
    }

    Program programForConfig(const DBBinary& binary, const DBBinaryConfig& config) const
    {
        auto preset = config.preset == "generic-chip-8" ? std::string("xo-chip") : config.preset;
        auto props = emu::CoreRegistry::propertiesForPreset(preset);
        if (!config.properties.empty()) {
            try {
                props.applyDiff(nlohmann::json::parse(config.properties));
            }
            catch (const nlohmann::json::exception& ex) {
                TraceLog(LOG_WARNING, fmt::format("Ignoring invalid properties of config {}: {}", config.id, ex.what()).c_str());
            }
        }
        auto programIter = programs.find(binary.program_id);
        return {.name = programIter != programs.end() ? programIter->second.name : binary.sha1, .properties = props, .data = binary.data ? std::vector<uint8_t>(binary.data->begin(), binary.data->end()) : std::vector<uint8_t>{}};
    }

    void selectTag(const std::string& tag, bool addToSelection)
    {
        if (addToSelection) {
            if (!selectedTags.erase(tag)) {
                selectedTags.insert(tag);
            }
        }
        else if (selectedTags.size() == 1 && selectedTags.contains(tag)) {
            selectedTags.clear();
        }
        else {
            selectedTags = {tag};
        }
        updateFilter();
    }

    void updateFilter()
    {
        std::vector<std::string> variants;
        std::vector<int> tagIds;
        std::vector<std::string> smartQueries;
        for (const auto& name : selectedTags) {
            auto iter = findTag(name);
            if (iter != tags.end() && iter->second.query) {
                smartQueries.push_back(*iter->second.query);
            }
            else if (iter != tags.end()) {
                tagIds.push_back(iter->first);
            }
            else {
                variants.push_back(name);
            }
        }
        filter = query;
        smartTagsQuery = LibraryQuery::combine(smartQueries);
        if (!smartQueries.empty()) {
            smartQueries.push_back(queryText);
            try {
                filter = LibraryQuery::parse(LibraryQuery::combine(smartQueries), searchProperties());
            }
            catch (const LibraryQuery::Error& error) {
                TraceLog(LOG_WARNING, fmt::format("A selected smart tag has no valid query: {}", error.what()).c_str());
            }
        }
        shownIndices.clear();
        matchingBinaries.clear();
        shownRomCount = 0;
        shownIndices.reserve(programs.size());
        for (auto& program : programs | std::views::values) {
            auto show = true;
            if (!variants.empty() && std::ranges::none_of(variants, [&](const std::string& preset) { return hasVariant(program, preset); })) {
                show = false;
            }
            if (show && !std::ranges::all_of(tagIds, [&](int tagId) { return hasTag(program, tagId); })) {
                show = false;
            }
            // a query asking for present or missing programs decides that itself, else the eye toggle does
            if (show && !filter.usesPresence() && !showMissing && !isPresent(program)) {
                show = false;
            }
            if (show && !filter.empty()) {
                const auto facts = programFacts(program);
                if (!filter.matches(facts)) {
                    show = false;
                }
                else if (filter.usesRomFields()) {
                    auto& matching = matchingBinaries[program.id];
                    for (const auto index : filter.matchingRoms(facts)) {
                        matching.insert(program.binaries[index]);
                    }
                }
            }
            if (show) {
                shownIndices.emplace_back(program.id);
                if (auto matching = matchingBinaries.find(program.id); matching != matchingBinaries.end()) {
                    shownRomCount += matching->second.size();
                }
            }
        }
        std::ranges::sort(shownIndices, std::less<>{}, [this](const int key) {
            return toLower(programs.at(key).name);
        });
    }

    void renderProgramInfo()
    {
        using namespace gui;
        auto iter = programs.find(selectedProgramId);
        if (iter == programs.end()) {
            Label("No program selected, drop ROM files here to add them.");
            return;
        }
        const auto& program = iter->second;
        const DBBinary* binary = nullptr;
        if (auto binIter = binaries.find(currentBinaryId()); binIter != binaries.end()) {
            binary = &binIter->second;
        }
        std::vector<std::string> variants;
        std::vector<std::string> tagNames;
        auto addTag = [&](int tagId) {
            if (auto tagIter = tags.find(tagId); tagIter != tags.end() && std::ranges::find(tagNames, tagIter->second.name) == tagNames.end()) {
                tagNames.push_back(tagIter->second.name);
            }
        };
        std::ranges::for_each(program.tags, addTag);
        if (binary) {
            for (const auto& config : binary->configs) {
                if (std::ranges::find(variants, config.preset) == variants.end()) {
                    variants.push_back(config.preset);
                }
            }
            std::ranges::for_each(binary->tags, addTag);
        }
        std::string release = program.release;
        std::string origin = program.origin_reference;
        if (auto originIter = std::ranges::find(ORIGIN_TYPES, program.origin); originIter != ORIGIN_TYPES.end()) {
            const auto* label = ORIGIN_LABELS[originIter - ORIGIN_TYPES.begin()];
            origin = origin.empty() ? std::string(label) : fmt::format("{}: {}", label, origin);
        }
        if (!origin.empty()) {
            release = release.empty() ? origin : fmt::format("{}, {}", release, origin);
        }
        std::string sha1 = binary ? binary->sha1 : "";
        if (binary && program.binaries.size() > 1) {
            auto index = std::ranges::find(program.binaries, binary->id) - program.binaries.begin();
            sha1 += fmt::format(" ({} of {} binaries)", index + 1, program.binaries.size());
        }
        std::string file;
        if (binary && !binary->filenames.empty()) {
            file = binary->filenames.front();
            if (binary->filenames.size() > 1) {
                file += fmt::format(" (+{})", binary->filenames.size() - 1);
            }
        }
        std::string description = !program.description.empty() ? program.description : (binary ? binary->description : "");
        // descriptions from the chip-8-database can have several lines, the row has one
        std::ranges::replace_if(description, [](char c) { return c == '\n' || c == '\r' || c == '\t'; }, ' ');
        description.erase(std::unique(description.begin(), description.end(), [](char a, char b) { return a == ' ' && b == ' '; }), description.end());

        constexpr float labelWidth = 80;
        SetRowHeight(12);
        SetSpacing(0);
        const auto valueWidth = GetContentAvailable().width - labelWidth - 16;  // minus the label text padding
        auto row = [&](const char* label, const std::string& value, bool keepEnd = false) {
            BeginColumns();
            SetNextWidth(labelWidth);
            Label(label);
            Label(value.empty() ? "-" : fitText(value, valueWidth, keepEnd).c_str());
            EndColumns();
        };
        row("Name:", program.name);
        row("Authors:", authorNames(program));
        row("Release:", release);
        row("License:", !program.license.empty() ? program.license : program.copyright);
        row("Variant:", join(variants.begin(), variants.end(), ", "));
        row("Tags:", join(tagNames.begin(), tagNames.end(), ", "));
        row("SHA1:", sha1);
        row("Size:", binary ? fmt::format("{} bytes{}", binary->size, binary->data ? "" : " (missing, not found in the library folders)") : "");
        row("File:", file, true);
        row("Description:", description);
    }

    void relayoutList(float width)
    {
        float ypos = 0;
        for (auto pid : shownIndices) {
            auto& program = programs[pid];
            program.rect = Rectangle{0, ypos, width, static_cast<float>(program.binaries.size() + 1) * 9 + 4};
            ypos += program.rect.height;
        }
        listContentHeight = ypos;
    }
};


Database::Database(const emu::CoreRegistry& registry, CadmiumConfiguration& configuration, ThreadPool& threadPool, const std::string& path)
    : _registry(registry)
    , _threadPool(threadPool)
    , _configuration(configuration)
    , _pimpl(std::make_unique<Private>(configuration, path))
{
    Librarian::setMetadataSource(&_pimpl->metadataSource);
    /*_pimpl->connection = std::make_unique<DBConnection>((path + "/cadmium_library.sqlite").c_str(), 0, nullptr, [](auto level, const auto& msg) {
        if (zxorm::log_level::Error == level)
            std::cerr << "Ooops: " << msg << std::endl;
        else
            std::cout << msg << std::endl;
    });*/
    ////_pimpl->connection->create_tables();
    //_pimpl->session.insert(or_ignore(), into<DBVersion>(), columns(&DBVersion::schema_version), values(std::make_tuple(DBVersion{}.schema_version)));
    {
        _pimpl->tags.clear();
        {
            auto session = db::Session(_pimpl->database);
            auto tags = session.fetchAll<DBTags>();
            bool hasNewTag = false;
            bool hasUnclassifiedTag = false;
            for (const auto& tag : tags) {
                _pimpl->tags.emplace(tag.id, tag);
                if (tag.name == "new") {
                    hasNewTag = true;
                }
                if (tag.name == "???") {
                    hasUnclassifiedTag = true;
                }
            }
            if (!hasNewTag) {
                DBTags newTag{0,"new", "#00C0E0"};
                session.insert(newTag);
                _pimpl->tags.emplace(newTag.id, newTag);
            }
            if (!hasUnclassifiedTag) {
                DBTags unclassifiedTag{0,"???", "#E04040"};
                session.insert(unclassifiedTag);
                _pimpl->tags.emplace(unclassifiedTag.id, unclassifiedTag);
            }
        }
    }
    fetchProgramInfo();
}

Database::~Database()
{
    Librarian::setMetadataSource(nullptr);
    // The library scan runs on the thread pool and uses _pimpl, so stop it before that goes away.
    _pimpl->cancelScan = true;
    if (_scanResult.valid()) {
        _scanResult.wait();
    }
}

void Database::refreshBadges()
{
    _badges.clear();
    _pimpl->sortedTags.clear();
    _pimpl->variantGroups.clear();
    _badges.emplace("generic-chip-8", BadgeInfo{BadgeInfo::GENERIC, "generic-chip-8", DARKGRAY, {0xE0, 0xC0, 0x00, 0xFF}});
    _pimpl->sortedTags.emplace_back("generic-chip-8");
    _pimpl->variantGroups.push_back({"GENERIC", {"generic-chip-8"}, "ANY CHIP-8"});
    for(const auto& [name, info] : _registry) {
        auto& group = _pimpl->variantGroups.emplace_back(Private::VariantGroup{std::string(name), {}, info->variantsCombo});
        //std::cout << toOptionName(name) << std::endl;
        //coresAvailable += fmt::format("        {} - {}\n", toOptionName(name), info->description);
        //presetsDescription += fmt::format("        {}:\n", info->description);
        for(size_t i = 0; i < info->numberOfVariants(); ++i) {
            std::string presetName;
            if(info->prefix().empty())
                presetName = toOptionName(info->variantName(i));
            else
                presetName = toOptionName(info->prefix() + '-' + info->variantName(i));

            _badges.emplace(presetName, BadgeInfo{BadgeInfo::PRESET , presetName, DARKGRAY, {0x00, 0xE0, 0x00, 0xFF}});
            _pimpl->sortedTags.emplace_back(presetName);
            group.presets.emplace_back(presetName);
        }
    }
    _pimpl->coresCombo.clear();
    for (const auto& group : _pimpl->variantGroups) {
        _pimpl->coresCombo += (_pimpl->coresCombo.empty() ? "" : ";") + group.core;
    }
    for (const auto& [id, tag] : _pimpl->tags) {
        if (!_badges.contains(tag.name)) {
            emu::Palette::Color col(tag.color);
            if (tag.query) {
                // a smart tag shows its color as text on a dark badge, unlike the tags given to programs
                const Color color{col.r, col.g, col.b, 0xFF};
                _badges.emplace(tag.name, BadgeInfo{BadgeInfo::SMART_TAG, tag.name, color, ColorBrightness(color, -0.7f)});
            }
            else if (tag.name == "???") {
                _badges.emplace(tag.name, BadgeInfo{BadgeInfo::UNDEFINED, tag.name, LIGHTGRAY, {col.r, col.g, col.b, 0xFF}});
            }
            else if (fuzzyCompare(tag.name, "new")) {
                _badges.emplace(tag.name, BadgeInfo{BadgeInfo::NEW_TAG, tag.name, DARKGRAY, {col.r, col.g, col.b, 0xFF}});
            }
            else {
                _badges.emplace(tag.name, BadgeInfo{BadgeInfo::USER_TAG, tag.name, DARKGRAY, {col.r, col.g, col.b, 0xFF}});
            }
            _pimpl->sortedTags.emplace_back(tag.name);
        }
    }
    std::ranges::sort(_pimpl->sortedTags, [this](const std::string& s1, const std::string& s2) {
        const auto& b1 = _badges.at(s1);
        const auto& b2 = _badges.at(s2);
        return std::tie(b1.type, b1.text) < std::tie(b2.type, b2.text);
    });
}

void Database::fetchProgramInfo()
{
    {
        std::lock_guard lock(_pimpl->mutex);
        auto session = db::Session(_pimpl->database);
        {
            _pimpl->tags.clear();
            auto tags = session.fetchAll<DBTags>();
            for (const auto& tag : tags) {
                _pimpl->tags.emplace(tag.id, tag);
            }
            _pimpl->newTagId = session.select(db::col(&DBTags::id)).where(db::like(db::col(&DBTags::name), "new")).valueOrDefault(0);
            _pimpl->unclassifiedTagId = session.select(db::col(&DBTags::id)).where(db::like(db::col(&DBTags::name), "???")).valueOrDefault(0);
        }

        {
            _pimpl->programs.clear();
            for (const auto& program : session.fetchAll<DBProgram>()) {
                _pimpl->programs.emplace(program.id, program);
            }
            for (const auto& programTag : session.select<DBProgramTag>().orderBy("id").all()) {
                if (auto iter = _pimpl->programs.find(programTag.program_id); iter != _pimpl->programs.end()) {
                    iter->second.tags.push_back(programTag.tag_id);
                }
            }
        }
        {
            // removing programs or ROMs can leave authors without a program
            db::Statement(session, "DELETE FROM authors WHERE id NOT IN (SELECT author_id FROM program_authors);").execute();
            _pimpl->authors.clear();
            for (const auto& author : session.fetchAll<DBAuthor>()) {
                _pimpl->authors.emplace(author.id, author);
            }
            for (const auto& programAuthor : session.select<DBProgramAuthor>().orderBy("position").all()) {
                if (auto iter = _pimpl->programs.find(programAuthor.program_id); iter != _pimpl->programs.end()) {
                    iter->second.authors.push_back(programAuthor.author_id);
                }
            }
        }
        {
            // TODO: Don't load all binaries data into memory ;-)
            _pimpl->binaries.clear();
            _digests.clear();
            for (const auto& binary : session.fetchAll<DBBinary>()) {
                _pimpl->binaries.emplace(binary.id, binary);
                _digests.insert(Sha1::Digest(binary.sha1));
                if (auto iter = _pimpl->programs.find(binary.program_id); iter != _pimpl->programs.end()) {
                    iter->second.binaries.push_back(binary.id);
                }
            }
            for (const auto& config : session.select<DBBinaryConfig>().orderBy("id").all()) {
                if (auto iter = _pimpl->binaries.find(config.binary_id); iter != _pimpl->binaries.end()) {
                    iter->second.configs.push_back(config);
                }
            }
            for (const auto& filename : session.select<DBFilename>().orderBy("id").all()) {
                if (auto iter = _pimpl->binaries.find(filename.binary_id); iter != _pimpl->binaries.end()) {
                    iter->second.filenames.push_back(filename.name);
                }
            }
            for (const auto& opcode : session.select<DBOpcode>().orderBy("opcode").all()) {
                if (auto iter = _pimpl->binaries.find(opcode.binary_id); iter != _pimpl->binaries.end()) {
                    iter->second.opcodes.push_back(static_cast<uint16_t>(opcode.opcode));
                }
            }
            for (const auto& binaryTag : session.select<DBBinaryTag>().orderBy("id").all()) {
                if (auto iter = _pimpl->binaries.find(binaryTag.binary_id); iter != _pimpl->binaries.end()) {
                    iter->second.tags.push_back(binaryTag.tag_id);
                }
            }
            _pimpl->ignoredSha1s.clear();
            for (const auto& ignored : session.fetchAll<DBIgnoredBinary>()) {
                _pimpl->ignoredSha1s.insert(ignored.sha1);
            }
        }
        refreshBadges();
        _pimpl->updateFilter();
    }
}

int Database::scanLibrary()
{
    auto start = std::chrono::steady_clock::now();
    auto extensions = _registry.getSupportedExtensions();
    int numFiles = 0;
    auto session = db::Session(_pimpl->database);
    for (const auto& folder : _configuration.libraryPath) {
        try {
            for (const auto& de : fs::recursive_directory_iterator(folder, fs::directory_options::skip_permission_denied)) {
                if (_pimpl->cancelScan) {
                    return numFiles;
                }
                if (de.is_regular_file() && extensions.contains(de.path().extension().string())) {
                    std::vector<uint8_t> data;
                    auto info = scanFile(de.path().string(), &data);
                    if (info) {
                        bool digested = false;
                        bool ignored = false;
                        {
                            std:std::lock_guard lock(_pimpl->mutex);
                            ignored = _pimpl->ignoredSha1s.contains(info->digest.to_hex());
                            digested = _digests.contains(info->digest);
                            if (!digested && !ignored) {
                                _digests.emplace(info->digest);
                            }
                        }
                        if (ignored) {
                            // removed from the library with "Remove and ignore"
                        }
                        else if (!digested) {
                            // only what was committed is cached, ids of a rolled back transaction don't exist
                            // in the database (and are reused because of AUTOINCREMENT rolling back too)
                            if (auto entry = _pimpl->insertBinary(session, de.path().string(), data, info->digest, -1)) {
                                TraceLog(LOG_INFO, fmt::format("found {}: {:14} '{}'", info->digest.to_hex(), entry->preset, de.path().string()).c_str());
                                std::lock_guard lock(_pimpl->mutex);
                                _pimpl->addEntry(*entry);
                                if ((numFiles & 63) == 0) {
                                    _pimpl->updateFilter();
                                }
                            }
                        }
                        else {
                            const std::string digest = info->digest.to_hex();
                            auto bid = session.select(db::col(&DBBinary::id)).where(db::col(&DBBinary::sha1) == digest);
                            if (!bid.empty()) {
                                const auto path = de.path().string();
                                int programId = -1;
                                bool tagNew = false;
                                {
                                    std::lock_guard lock(_pimpl->mutex);
                                    if (auto iter = _pimpl->binaries.find(bid.first()); iter != _pimpl->binaries.end() && !iter->second.data) {
                                        programId = iter->second.program_id;
                                        tagNew = _pimpl->needsNewTag(programId);
                                    }
                                }
                                if (programId >= 0 && _pimpl->storeBinaryData(session, bid.first(), programId, tagNew, data)) {
                                    TraceLog(LOG_INFO, fmt::format("found {}: known from metadata '{}'", digest, path).c_str());
                                    std::lock_guard lock(_pimpl->mutex);
                                    _pimpl->applyBinaryData(bid.first(), tagNew, data);
                                    if ((numFiles & 63) == 0) {
                                        _pimpl->updateFilter();
                                    }
                                }
                                auto known = session.select(db::col(&DBFilename::id)).where(db::col(&DBFilename::binary_id) == bid.first() && db::col(&DBFilename::name) == path);
                                if (known.empty()) {
                                    session.insert(DBFilename{.binary_id = bid.first(), .name = path});
                                    std::lock_guard lock(_pimpl->mutex);
                                    if (auto iter = _pimpl->binaries.find(bid.first()); iter != _pimpl->binaries.end()) {
                                        iter->second.filenames.push_back(path);
                                    }
                                }
                            }
                        }
                        ++numFiles;
                    }
                }
            }
        }
        catch (const fs::filesystem_error& e) {
            // ...
        }
        catch (const db::Exception& e) {
            std::cerr << "SQLConstraintError: " << e.what() << std::endl;
        }
    }
    {
        std::lock_guard lock(_pimpl->mutex);
        _pimpl->updateFilter();
    }
    durationOfLastJob = std::chrono::steady_clock::now() - start;
    // the opcodes of new ROMs and of ROMs now found, the list is usable before
    _pimpl->analyzeBinaries(session);
    return numFiles;
}

ghc::expected<Database::FileInfo,LoadError> Database::scanFile(const std::string& filePath, std::vector<uint8_t>* outData)
{
    if (toLower(fs::path(filePath).extension().string()) == ".hex") {
        // HEX files are known by the SHA1 of the data they load, like EmuHostEx::loadHexFile() identifies them,
        // so the library keeps that data (from the first address on, gaps filled with zeros), not the HEX text.
        try {
            emu::HexFile hex(filePath, emu::HexFile::Mode::READ);
            if (hex.ranges().empty()) {
                return ghc::unexpected(LoadError{std::make_error_code(std::errc::invalid_argument), "HEX file without data", filePath});
            }
            const auto firstAddress = hex.ranges().front().first;
            const auto& [lastAddress, lastData] = hex.ranges().back();
            std::vector<uint8_t> image(lastAddress + lastData.size() - firstAddress, 0);
            for (const auto& [address, data] : hex.ranges()) {
                std::copy(data.begin(), data.end(), image.begin() + (address - firstAddress));
            }
            auto result = FileInfo{filePath, hex.sha1Digest()};
            if (outData) {
                *outData = std::move(image);
            }
            return result;
        }
        catch (const std::exception& ex) {
            return ghc::unexpected(LoadError{std::make_error_code(std::errc::invalid_argument), ex.what(), filePath});
        }
    }
    auto data = loadFile(filePath);
    if (data) {
        auto result = FileInfo{filePath, calculateSha1(*data)};
        if (outData) {
            std::swap(*outData, *data);
        }
        return result;
    }
    return ghc::unexpected(data.error());
}

std::optional<Database::Program> Database::getSelectedProgram() const
{
    return _selectedProgram;
}

bool Database::containsBinary(const Sha1::Digest& sha1) const
{
    std::lock_guard lock(_pimpl->mutex);
    return _digests.contains(sha1);
}

std::string Database::storeSettings(const Sha1::Digest& sha1, const emu::Properties& properties)
{
    std::lock_guard lock(_pimpl->mutex);
    const auto sha1Hex = sha1.to_hex();
    auto binIter = std::ranges::find_if(_pimpl->binaries, [&](const auto& entry) { return entry.second.sha1 == sha1Hex; });
    if (binIter == _pimpl->binaries.end()) {
        return "The current ROM is not part of the library.";
    }
    // the variant the properties are closest to, named like the variant badges
    std::string preset;
    emu::Properties variantProperties;
    for (const auto& [name, info] : _registry) {
        if (fuzzyCompare(std::string(name), properties.propertyClass())) {
            auto index = info->variantIndex(properties).index;
            preset = toOptionName(info->prefix().empty() ? info->variantName(index) : info->prefix() + '-' + info->variantName(index));
            variantProperties = info->variantProperties(index);
            break;
        }
    }
    if (preset.empty()) {
        return "The current settings don't belong to a known variant.";
    }
    auto diff = variantProperties.createDiff(properties);
    const auto propertiesText = diff.is_null() || diff.empty() ? std::string() : diff.dump();
    // replaces a config of the same variant, or adds a new one
    auto applyTo = [&](std::vector<DBBinaryConfig>& configs) {
        if (auto iter = std::ranges::find_if(configs, [&](const DBBinaryConfig& config) { return fuzzyCompare(config.preset, preset); }); iter != configs.end()) {
            iter->properties = propertiesText;
        }
        else {
            configs.push_back(DBBinaryConfig{.binary_id = binIter->first, .preset = preset, .properties = propertiesText});
        }
    };
    auto binary = binIter->second;
    auto configs = binary.configs;
    applyTo(configs);
    try {
        auto session = db::Session(_pimpl->database);
        db::Transaction transaction{session};
        _pimpl->storeConfigs(session, binary, configs);
        transaction.commit();
    }
    catch (const db::Exception& ex) {
        return fmt::format("Storing the settings failed: {}", ex.what());
    }
    binIter->second = std::move(binary);
    // an open editor showing this ROM gets the change too
    if (auto iter = _pimpl->edit.configs.find(binIter->first); _pimpl->editorOpen && iter != _pimpl->edit.configs.end()) {
        applyTo(iter->second);
    }
    _pimpl->updateFilter();
    auto programIter = _pimpl->programs.find(binIter->second.program_id);
    return fmt::format("Stored the settings as {} config of '{}'.", preset, programIter != _pimpl->programs.end() ? programIter->second.name : sha1Hex);
}

std::string Database::addFiles(const std::vector<std::string>& paths)
{
    std::lock_guard lock(_pimpl->mutex);
    int targetId = -1;
    if (const auto mouse = GetMousePosition(); CheckCollisionPointRec(mouse, _pimpl->visibleListRect)) {
        if (auto iter = std::ranges::find_if(_pimpl->visibleRows, [&](const auto& row) { return CheckCollisionPointRec(mouse, row.first); }); iter != _pimpl->visibleRows.end() && _pimpl->programs.contains(iter->second)) {
            targetId = iter->second;
        }
    }
    auto session = db::Session(_pimpl->database);
    int added = 0, known = 0;
    for (const auto& path : paths) {
        std::vector<uint8_t> data;
        auto info = scanFile(path, &data);
        if (!info || !_registry.getSupportedExtensions().contains(fs::path(path).extension().string())) {
            continue;
        }
        const auto sha1 = info->digest.to_hex();
        try {
            if (_pimpl->ignoredSha1s.contains(sha1)) {
                db::Statement unignore(session, "DELETE FROM ignored_binaries WHERE sha1 = ?");
                unignore << sha1;
                unignore.execute();
                _pimpl->ignoredSha1s.erase(sha1);
            }
            int binaryId = -1;
            if (auto iter = std::ranges::find_if(_pimpl->binaries, [&](const auto& entry) { return entry.second.sha1 == sha1; }); iter != _pimpl->binaries.end()) {
                // a known ROM gets the file name (and its data if it was only known from metadata) and moves
                // to the program it was dropped on
                binaryId = iter->first;
                if (!iter->second.data) {
                    const bool tagNew = _pimpl->needsNewTag(iter->second.program_id);
                    if (_pimpl->storeBinaryData(session, binaryId, iter->second.program_id, tagNew, data)) {
                        _pimpl->applyBinaryData(binaryId, tagNew, data);
                    }
                }
                if (std::ranges::find(iter->second.filenames, path) == iter->second.filenames.end()) {
                    session.insert(DBFilename{.binary_id = binaryId, .name = path});
                    iter->second.filenames.push_back(path);
                }
                if (targetId >= 0 && iter->second.program_id != targetId) {
                    if (auto error = _pimpl->moveBinary(binaryId, targetId); !error.empty()) {
                        TraceLog(LOG_WARNING, "%s", error.c_str());
                    }
                }
                ++known;
            }
            else if (auto entry = _pimpl->insertBinary(session, path, data, info->digest, targetId)) {
                _digests.insert(info->digest);
                _pimpl->addEntry(*entry);
                binaryId = entry->binary.id;
                ++added;
            }
            if (binaryId >= 0) {
                _pimpl->selectedProgramId = _pimpl->binaries.at(binaryId).program_id;
                _pimpl->selectedBinaryId = binaryId;
                _pimpl->markedProgramIds = {_pimpl->selectedProgramId};
            }
        }
        catch (const db::Exception& ex) {
            TraceLog(LOG_WARNING, fmt::format("Adding '{}' to the library failed: {}", path, ex.what()).c_str());
        }
    }
    _pimpl->analyzeBinaries(session);
    _pimpl->updateFilter();
    updatePreview();
    return fmt::format("Added {} new and {} known ROM{} to the library.", added, known, added + known == 1 ? "" : "s");
}

void Database::updatePreview()
{
    _selectedProgram.reset();
    _pimpl->backgroundHost.killEmulation();
    auto iter = _pimpl->binaries.find(_pimpl->currentBinaryId());
    if (iter == _pimpl->binaries.end() || iter->second.configs.empty() || !iter->second.data) {
        return;  // no preview and nothing to load for a ROM only known from metadata
    }
    _selectedProgram = _pimpl->programForConfig(iter->second, iter->second.configs.front());
    _pimpl->backgroundHost.loadBinary(_selectedProgram->name, _selectedProgram->data, _selectedProgram->properties, true);
}

Vector2 Database::drawBadge(Font& font, std::string_view text, Vector2 pos, Color textCol, Color badgeCol)
{
    Vector2 size{text.length() * 6.0f + 5, 7.0f};
    DrawRectangleClipped(pos.x, pos.y + 1, text.length() * 6 + 5, 5, badgeCol);
    DrawRectangleClipped(pos.x + 1, pos.y, text.length() * 6 + 3, 7, badgeCol);
    pos.x += 3;
    pos.y -= 1;
    for (int cp : text) {
        cp |= 0xE000;
        DrawTextCodepointClipped(font, cp, pos, 8.0f, textCol);
        pos.x += 6;
    }
    return size;
}

Database::Action Database::render(Font& font)
{
    using namespace gui;
    static bool first = true;
    static bool second = false;
    if (first) {
        first = false;
        fetchProgramInfo();
        _scanResult = _threadPool.enqueue([this]() {
            return scanLibrary();
        });
    }
    if (!second && _scanResult.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        TraceLog(LOG_INFO, fmt::format("scan result: {} ({} unique) files ({}ms)", _scanResult.get(), _digests.size(), std::chrono::duration_cast<std::chrono::milliseconds>(durationOfLastJob).count()).c_str());
        second = true;
    }
    {
        std:std::unique_lock lock(_pimpl->mutex);
        switch (std::exchange(_pimpl->request, Private::Request::NONE)) {
            case Private::Request::EDIT:
                _pimpl->openEditor();
                break;
            case Private::Request::LOAD:
                _pimpl->requestedAction = Action::LOAD;
                break;
            case Private::Request::RUN:
                _pimpl->requestedAction = Action::RUN;
                break;
            case Private::Request::DETACH:
                if (auto error = _pimpl->detachBinary(_pimpl->selectedBinaryId); !error.empty()) {
                    TraceLog(LOG_WARNING, "%s", error.c_str());
                }
                break;
            case Private::Request::MOVE:
                _pimpl->openMove(_pimpl->selectedBinaryId, false);
                break;
            case Private::Request::MERGE:
                _pimpl->openMerge();
                break;
            case Private::Request::REMOVE:
                _pimpl->openRemove(_pimpl->selectedProgramId, _pimpl->selectedBinaryId, false);
                break;
            case Private::Request::NONE:
                break;
        }
        SetSpacing(4);
        auto area = GetContentAvailable();
        _pimpl->relayoutList(area.width);
        BeginColumns();
        SetSpacing(4);

        SetNextWidth(area.width - 2 * 60 - 4 * 20 - 6 * 4);
        if (TextBox("query", _pimpl->queryLine, 4096)) {
            _pimpl->setQuery(_pimpl->queryLine);
        }
        if (_pimpl->queryLine.empty() && !_pimpl->smartTagsQuery.empty()) {
            // the queries of the selected smart tags show in the empty field, as what the list is searched for
            const auto box = GetLastWidgetRect();
            const auto text = fitText(_pimpl->smartTagsQuery, box.width - 12);
            const auto size = MeasureTextEx(font, text.c_str(), 8, 1);
            DrawTextEx(font, text.c_str(), {box.x + 6, std::floor(box.y + (box.height - size.y) / 2)}, 8, 1, StyleManager::mappedColor(GRAY));
        }
        SetTooltip("WORDS, OR FIELDS LIKE: release < 1980, author ~ winter, tag = new, variant = xo-chip, ipf > 100, wrap = true, size < 1k, uses F?75, missing - COMBINE WITH and, or, not/-, ( )");
        SetNextWidth(20);
        const bool editSmartTag = _pimpl->editableSmartTag() >= 0;
        if (!editSmartTag && _pimpl->selectedTags.empty() && trim(_pimpl->queryLine).empty()) {
            GuiDisable();
        }
        if (Button(IconText(ICON_STAR, ""))) {
            _pimpl->openSmartTag();
        }
        GuiEnable();
        SetTooltip(editSmartTag ? "EDIT SMART TAG" : "SAVE SELECTION AND SEARCH AS SMART TAG");
        SetNextWidth(20);
        if (Button(IconText(_pimpl->showMissing ? ICON_EYE_ON : ICON_EYE_OFF, ""))) {
            _pimpl->showMissing = !_pimpl->showMissing;
            _pimpl->updateFilter();
        }
        SetTooltip(_pimpl->showMissing ? "HIDE MISSING ROMS" : "SHOW MISSING ROMS");
        if (!_pimpl->programs.contains(_pimpl->selectedProgramId)) {
            GuiDisable();
        }
        SetNextWidth(60);
        if (Button("Edit...")) {
            _pimpl->openEditor();
        }
        GuiEnable();
        if (_pimpl->markedProgramIds.size() < 2) {
            GuiDisable();
        }
        SetNextWidth(60);
        if (Button("Merge...")) {
            _pimpl->openMerge();
        }
        GuiEnable();
        if (!_selectedProgram) {
            GuiDisable();
        }
        SetNextWidth(20);
        if (Button(IconText(ICON_ROM, ""))) {
            _pimpl->requestedAction = Action::LOAD;
        }
        SetTooltip("LOAD");
        if (Button(IconText(ICON_PLAYER_PLAY, ""))) {
            _pimpl->requestedAction = Action::RUN;
        }
        SetTooltip("RUN");
        GuiEnable();
        EndColumns();
        {
            // what the query found or why it is no valid query, on its own line to have room for the text
            const auto labelColor = GetStyle(LABEL, TEXT_COLOR_NORMAL);
            if (!_pimpl->queryError.empty()) {
                SetStyle(LABEL, TEXT_COLOR_NORMAL, ColorToInt(StyleManager::mappedColor(RED)));
            }
            SetNextWidth(area.width);
            Label(fitText(_pimpl->queryFeedback(), area.width - 8).c_str());
            SetStyle(LABEL, TEXT_COLOR_NORMAL, labelColor);
        }
        BeginColumns();
        {
            SetSpacing(4);
            auto tagsWidth = area.width / 4 - 5;
            SetNextWidth(tagsWidth);
            auto offset = GetCurrentPos();
            auto tagsRect = Rectangle{offset.x, offset.y, tagsWidth, GetContentAvailable().height - 135};
            const std::string* clickedTag = nullptr;
            BeginTableView(tagsRect.height, "tags", {TableColumn::Stretch()}, &_pimpl->tagsScrollPos);
            for (const auto& tagText : _pimpl->sortedTags) {
                const auto& badge = _badges[tagText];
                const bool isSelected = _pimpl->selectedTags.contains(tagText);
                TableNextRow(10, [&](Rectangle rect) {
                    const bool isHovered = !GuiIsLocked() && CheckCollisionPointRec(GetMousePosition(), tagsRect) && CheckCollisionPointRec(GetMousePosition(), rect);
                    if (isSelected) {
                        DrawRectangleClipped(rect.x, rect.y, rect.width, rect.height, StyleManager::getStyleColor(gui::Style::BASE_COLOR_PRESSED));
                    }
                    else if (isHovered) {
                        DrawRectangleClipped(rect.x, rect.y, rect.width, rect.height, StyleManager::getStyleColor(gui::Style::BASE_COLOR_NORMAL));
                    }
                    if (isHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        clickedTag = &tagText;
                    }
                });
                TableNextColumn([&](Rectangle rect) {
                    drawBadge(font, badge.text.c_str(), {rect.x + 2, rect.y + 2}, badge.textCol, badge.badgeCol);
                });
            }
            EndTableView();
            if (clickedTag) {
                _pimpl->selectTag(*clickedTag, IsSysKeyDown());
            }
            auto tableArea = GetContentAvailable();
            auto listWidth = tableArea.width;
            auto listRect = Rectangle{tableArea.x, tableArea.y, listWidth, tableArea.height - 135};
            static Vector2 scrollPos{0,0};
            auto [px, py] = GetCurrentPos();
            BeginScrollPanel(listRect.height, Rectangle{0.0f,0.0f, listRect.width - 8, _pimpl->listContentHeight < listRect.height ? listRect.height : _pimpl->listContentHeight}, &scrollPos);
            auto [cx, cy] = GetCurrentPos();
            auto [mx, my] = GetMousePosition();
            float ypos = 8;
            int disp = 0;
            static int maxDisp = 0;
            //for (size_t i = 0; i < 100; ++i) {
            //    DrawText(fmt::format("{}. {}", i, i*10).c_str(), px + cx + scrollPos.x, py + cy + i * 10 + scrollPos.y, 8, GREEN);
            //}
            auto grayCol = StyleManager::mappedColor(GRAY);
            auto lightgrayCol = StyleManager::mappedColor(LIGHTGRAY);
            auto selectedCol = StyleManager::getStyleColor(gui::Style::TEXT_COLOR_PRESSED);
            auto missingCol = StyleManager::mappedColor(DARKGRAY);  // ROMs only known from metadata
            const auto mouse = GetMousePosition();
            const bool mouseInList = !GuiIsLocked() && CheckCollisionPointRec(mouse, listRect);
            std::optional<std::pair<int, int>> clicked;  // program id and binary id (-1 for the program line)
            std::optional<std::pair<int, int>> rightClicked;
            int dropTargetId = -1;
            _pimpl->visibleListRect = listRect;
            _pimpl->visibleRows.clear();
            if (_pimpl->dragBinaryId >= 0 && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && std::hypot(mouse.x - _pimpl->dragStart.x, mouse.y - _pimpl->dragStart.y) > 5) {
                _pimpl->dragging = true;
            }
            for (const int pid : _pimpl->shownIndices) {
                const auto& program = _pimpl->programs.at(pid);
                Rectangle itemRect = {program.rect.x + px + cx + scrollPos.x, program.rect.y + py + cy + scrollPos.y, program.rect.width, program.rect.height - 2};
                if (CheckCollisionRecs(listRect, itemRect)) {
                    _pimpl->visibleRows.emplace_back(itemRect, pid);
                    ++disp;
                    if (disp > maxDisp) {
                        maxDisp = disp;
                    }
                    const bool isSelected = _pimpl->markedProgramIds.contains(pid);
                    if (isSelected) {
                        DrawRectangleClipped(itemRect.x - 2, itemRect.y - 2, itemRect.width, itemRect.height, StyleManager::getStyleColor(gui::Style::BASE_COLOR_PRESSED));
                    }
                    if (mouseInList && CheckCollisionPointRec(mouse, itemRect)) {
                        if (!isSelected) {
                            DrawRectangleClipped(itemRect.x - 2, itemRect.y - 2, itemRect.width, itemRect.height, StyleManager::getStyleColor(gui::Style::BASE_COLOR_NORMAL));
                        }
                        if (_pimpl->dragging) {
                            dropTargetId = pid;
                            DrawPanelClipped({itemRect.x - 2, itemRect.y - 2, itemRect.width, itemRect.height}, 1, StyleManager::getStyleColor(gui::Style::BORDER_COLOR_FOCUSED), {0, 0, 0, 0});
                        }
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                            // the first text line is the program, every following line one of its ROMs
                            auto line = static_cast<size_t>((mouse.y - itemRect.y) / 9);
                            auto entry = std::pair{pid, line > 0 && line <= program.binaries.size() ? program.binaries[line - 1] : -1};
                            (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ? clicked : rightClicked) = entry;
                            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && entry.second >= 0) {
                                // a ROM line can be dragged onto another program
                                _pimpl->dragBinaryId = entry.second;
                                _pimpl->dragStart = mouse;
                                _pimpl->dragging = false;
                            }
                        }
                    }
                    DrawTextClipped(font, fmt::format("{}", program.name.c_str()).c_str(), {itemRect.x, itemRect.y}, isSelected ? selectedCol : (_pimpl->isPresent(program) ? lightgrayCol : grayCol));
                    for (size_t i = 0; i < program.binaries.size(); ++i) {
                        const auto& binary = _pimpl->binaries.at(program.binaries[i]);
                        if (pid == _pimpl->selectedProgramId && binary.id == _pimpl->selectedBinaryId) {
                            DrawPanelClipped({itemRect.x - 2, itemRect.y + (i + 1) * 9 - 1, itemRect.width, 9}, 1, StyleManager::getStyleColor(gui::Style::BORDER_COLOR_PRESSED), {0, 0, 0, 0});
                        }
                        // the ROMs a query with ROM fields found
                        if (auto matching = _pimpl->matchingBinaries.find(pid); matching != _pimpl->matchingBinaries.end() && matching->second.contains(binary.id)) {
                            DrawRectangleClipped(itemRect.x - 2, itemRect.y + (i + 1) * 9 - 1, 9 * 6 - 4, 9, Fade(StyleManager::getStyleColor(gui::Style::BASE_COLOR_FOCUSED), 0.35f));
                        }
                        DrawTextClipped(font, fmt::format("{}", _pimpl->binaries[program.binaries[i]].sha1.substr(0,8)).c_str(), {itemRect.x, itemRect.y + (i+1) * 9}, isSelected ? selectedCol : (binary.data ? grayCol : missingCol));
                        //DrawTextEx(font, fmt::format("{} ", badges).c_str(), {itemRect.x + 9*6, itemRect.y + (i+1) * 9}, 8, 0, WHITE);
                        Vector2 pos = {itemRect.x + 9*6, itemRect.y + (i+1) * 9};
                        for (const auto& bincfg : binary.configs) {
                            auto size = drawBadge(font, bincfg.preset, pos, DARKGRAY, {0x00, 0xE0, 0x00, 0xFF});
                            pos.x += size.x + 1;
                        }
                    }
                }
                ypos += program.rect.height;
            }
            //DrawRectangleLines(-scrollPos.x, py-scrollPos.y, listRect.width-2, listRect.height-2, BLACK);
            EndScrollPanel();
            if (clicked) {
                const bool isDoubleClick = _pimpl->clickEntry(clicked->first, clicked->second, IsSysKeyDown());
                updatePreview();
                if (isDoubleClick) {
                    _pimpl->openEditor(clicked->second >= 0 ? Private::CONFIGS_TAB : Private::PROGRAM_TAB);
                }
            }
            if (_pimpl->dragBinaryId >= 0 && !IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                const auto binaryId = std::exchange(_pimpl->dragBinaryId, -1);
                if (std::exchange(_pimpl->dragging, false) && dropTargetId >= 0 && _pimpl->binaries.contains(binaryId) && _pimpl->binaries.at(binaryId).program_id != dropTargetId) {
                    if (_pimpl->programs.at(_pimpl->binaries.at(binaryId).program_id).binaries.size() == 1) {
                        // the source program would be removed, so the move dialog asks first
                        _pimpl->openMove(binaryId, false, dropTargetId);
                    }
                    else if (auto error = _pimpl->moveBinary(binaryId, dropTargetId); !error.empty()) {
                        TraceLog(LOG_WARNING, "%s", error.c_str());
                    }
                    updatePreview();
                }
            }
            if (_pimpl->dragging && _pimpl->binaries.contains(_pimpl->dragBinaryId)) {
                DrawTextEx(font, _pimpl->binaries.at(_pimpl->dragBinaryId).sha1.substr(0, 8).c_str(), {mouse.x + 12, mouse.y + 4}, 8, 1, selectedCol);
            }
            if (rightClicked && !PopupMenu::IsOpen()) {
                // a right click on a marked program keeps the marking, so the marked programs can be merged
                if (rightClicked->second < 0 && _pimpl->markedProgramIds.contains(rightClicked->first)) {
                    _pimpl->selectedProgramId = rightClicked->first;
                    _pimpl->selectedBinaryId = -1;
                }
                else {
                    _pimpl->clickEntry(rightClicked->first, rightClicked->second, false);
                }
                updatePreview();
                auto loadItem = PopupMenu::Action("Load", [this]() { _pimpl->request = Private::Request::LOAD; });
                auto runItem = PopupMenu::Action("Run", [this]() { _pimpl->request = Private::Request::RUN; });
                PopupMenu::Menu menu{
                    PopupMenu::Action("Edit...", [this]() { _pimpl->request = Private::Request::EDIT; }),
                    _selectedProgram ? loadItem : PopupMenu::Disabled(loadItem),
                    _selectedProgram ? runItem : PopupMenu::Disabled(runItem),
                };
                if (rightClicked->second >= 0) {
                    auto detachItem = PopupMenu::Action("Detach as new program", [this]() { _pimpl->request = Private::Request::DETACH; });
                    menu.push_back(PopupMenu::Separator());
                    menu.push_back(_pimpl->programs.at(rightClicked->first).binaries.size() > 1 ? detachItem : PopupMenu::Disabled(detachItem));
                    menu.push_back(PopupMenu::Action("Move to program...", [this]() { _pimpl->request = Private::Request::MOVE; }));
                    menu.push_back(PopupMenu::Action("Remove ROM...", [this]() { _pimpl->request = Private::Request::REMOVE; }));
                }
                else {
                    menu.push_back(PopupMenu::Separator());
                    menu.push_back(PopupMenu::Action("Remove program...", [this]() { _pimpl->request = Private::Request::REMOVE; }));
                }
                if (_pimpl->markedProgramIds.size() > 1) {
                    menu.push_back(PopupMenu::Separator());
                    menu.push_back(PopupMenu::Action(fmt::format("Merge {} programs...", _pimpl->markedProgramIds.size()), [this]() { _pimpl->request = Private::Request::MERGE; }));
                }
                PopupMenu::Open(GetMousePosition(), menu);
            }
            //DrawRectangleLinesEx(listRect, 1, MAGENTA);
        }
        EndColumns();
        //auto innerHeight = _pimpl->programs
        //BeginScrollPanel(listRect.height, {0,0,area.width-6, (float)(_core->memSize()/8 + 1) * lineSpacing}, &memScroll);
        auto pos = GetCurrentPos();
        SetNextWidth(area.width - 135);
        Begin();
        _pimpl->renderProgramInfo();
        End();
        auto previewRect = Rectangle{pos.x + area.width - 131.0f, pos.y, 130.0f, 98.0f};
        DrawPanelClipped(previewRect, 1, StyleManager::instance().getStyleColor(gui::Style::BORDER_COLOR_NORMAL), {0,0,0,0});
        auto [texture, rect] = _pimpl->backgroundHost.updateTexture();
        if (_selectedProgram) {
            _pimpl->backgroundHost.drawScreen({pos.x + area.width - 130, pos.y + 1, 128, 96});
            //DrawTextEx(font, fmt::format("FPS: {:02.1f} ({} frames)", 1000000.0/_pimpl->backgroundHost.getFrameTimeAvg()+0.005, _pimpl->backgroundHost.getFrames()).c_str(), {pos.x + area.width - 130 + 4, pos.y + 100}, 8, 1, WHITE);
        }
        else {
            // Nothing selected, or no single binary with a known variant to run
            const char* noPreview = "NO PREVIEW";
            auto textSize = MeasureTextEx(font, noPreview, 8, 1);
            DrawTextEx(font, noPreview, {std::floor(previewRect.x + (previewRect.width - textSize.x) / 2), std::floor(previewRect.y + (previewRect.height - textSize.y) / 2)}, 8, 1, StyleManager::mappedColor(LIGHTGRAY));
        }
        //DrawTexturePro(*texture, {0,0,128,64}, {pos.x + area.width - 130, pos.y + 1, 128, 64}, {0,0}, 0, WHITE);
        //DrawText(fmt::format("{}x{}", mx, my).c_str(), area.x + 2, area.y + 2, 8, RED);
        if (_pimpl->editorOpen && _pimpl->renderEditor()) {
            refreshBadges();
            updatePreview();
        }
        if (_pimpl->smartTagOpen) {
            std::set<std::string> variantNames;
            for (const auto& [name, badge] : _badges) {
                if (badge.type == BadgeInfo::PRESET || badge.type == BadgeInfo::GENERIC) {
                    variantNames.insert(name);
                }
            }
            if (_pimpl->renderSmartTagDialog(variantNames)) {
                refreshBadges();
            }
        }
        if (_pimpl->moveOpen || _pimpl->mergeOpen || _pimpl->removeOpen) {
            if (_pimpl->moveOpen) {
                _pimpl->renderMoveDialog();
            }
            else if (_pimpl->mergeOpen) {
                _pimpl->renderMergeDialog();
            }
            else {
                _pimpl->renderRemoveDialog();
            }
            if (!_pimpl->moveOpen && !_pimpl->mergeOpen && !_pimpl->removeOpen) {
                updatePreview();
            }
        }
        for (const auto& sha1 : std::exchange(_pimpl->removedSha1s, {})) {
            _digests.erase(Sha1::Digest(sha1));
        }
        if (auto configAction = std::exchange(_pimpl->configAction, std::nullopt)) {
            const auto& [config, action] = *configAction;
            if (auto iter = _pimpl->binaries.find(config.binary_id); iter != _pimpl->binaries.end()) {
                _selectedProgram = _pimpl->programForConfig(iter->second, config);
                _pimpl->requestedAction = action;
            }
        }
        const auto action = std::exchange(_pimpl->requestedAction, Action::NONE);
        return _selectedProgram ? action : Action::NONE;
    }
}

bool Database::fetchC8PDB()
{
#ifndef _WIN32
    httplib::Client cli("https://raw.githubusercontent.com");
    //cli.enable_server_certificate_verification(false);
    //cli.enable_server_hostname_verification(false);
    auto res = cli.Get("/chip-8/chip-8-database/refs/heads/master/database/programs.json");
    res->status;
    res->body;
#endif
    return false;
}

//---------------------------------------------------------------------------------------
// Comparing the library with programs.json of the chip-8-database (--update-program-database)
//---------------------------------------------------------------------------------------

namespace {

using OrderedJson = nlohmann::ordered_json;

// The chip-8-database platforms and the Cadmium presets they correspond to, modernChip8 is
// chip-8 without the vblank and logic (vF reset) quirks.
const std::vector<std::pair<std::string, std::string>> C8DB_PLATFORM_PRESETS = {
    {"originalChip8", "chip-8"}, {"hybridVIP", "vip-chip-8"}, {"modernChip8", "chip-8"}, {"chip8x", "chip-8-x"}, {"chip48", "chip-48"},
    {"superchip1", "schip-1-0"}, {"superchip", "schip-1-1"}, {"megachip8", "megachip"}, {"xochip", "xo-chip"},
};
const std::vector<std::string> C8DB_QUIRKS = {"shift", "memoryIncrementByX", "memoryLeaveIUnchanged", "wrap", "jump", "vblank", "logic"};

// Preset names are spelled differently in the library (schip-1.1, schip-1-1, chip-8x, chip-8-x...)
std::string presetKey(std::string_view preset)
{
    std::string key;
    for (const char c : preset) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            key.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
    }
    return key;
}

struct C8dbSettings
{
    std::map<std::string, bool> quirks;  // empty for cores without the generic CHIP-8 quirks
    int tickrate{0};
    int startAddress{0x200};
    int screenRotation{0};  // clockwise in degrees
};

// The settings of Cadmium properties in chip-8-database terms
C8dbSettings c8dbSettings(const emu::Properties& props)
{
    C8dbSettings settings;
    auto flag = [&](std::string_view id) { const auto* prop = props.find(id); return prop && prop->getBool(); };
    if (props.find("justShiftVx")) {
        settings.quirks = {
            {"shift", flag("justShiftVx")},
            {"memoryIncrementByX", flag("loadStoreIncIByx")},
            {"memoryLeaveIUnchanged", !flag("loadStoreIncIByXPlus1") && !flag("loadStoreIncIByx")},
            {"wrap", flag("wrapSprites")},
            {"jump", flag("jump0Bxnn")},
            {"vblank", !flag("instantDxyn")},
            {"logic", !flag("dontResetVf")},
        };
    }
    if (const auto* prop = props.find("instructionsPerFrame")) {
        settings.tickrate = prop->getInt();
    }
    if (const auto* prop = props.find("startAddress")) {
        settings.startAddress = prop->getInt();
    }
    if (const auto* prop = props.find("screenRotation")) {
        settings.screenRotation = prop->getSelectedIndex() * 90;
    }
    return settings;
}

// A library config in chip-8-database terms, the platform is empty if there is none for its preset.
struct C8dbConfig
{
    std::string preset;
    std::string platform;
    C8dbSettings settings;
    C8dbSettings presetSettings;  // what the config inherits, to tell which settings it sets explicitly
};

C8dbConfig c8dbConfig(const DBBinaryConfig& config)
{
    C8dbConfig result{config.preset};
    auto key = presetKey(config.preset);
    auto iter = std::ranges::find_if(C8DB_PLATFORM_PRESETS, [&](const auto& entry) { return presetKey(entry.second) == key; });
    if (iter == C8DB_PLATFORM_PRESETS.end()) {
        return result;
    }
    auto presetProps = emu::CoreRegistry::propertiesForPreset(config.preset);
    auto props = presetProps;
    if (!trim(config.properties).empty()) {
        try {
            props.applyDiff(nlohmann::json::parse(config.properties));
        }
        catch (const nlohmann::json::exception&) {
        }
    }
    result.settings = c8dbSettings(props);
    result.presetSettings = c8dbSettings(presetProps);
    result.platform = iter->first;
    if (result.platform == "originalChip8" && !result.settings.quirks.empty() && !result.settings.quirks["vblank"] && !result.settings.quirks["logic"]) {
        result.platform = "modernChip8";
    }
    return result;
}

// The settings of a ROM on a platform as the chip-8-database defines them, the explicit set
// tells which of them the ROM sets itself (quirkyPlatforms, tickrate, startAddress).
C8dbSettings c8dbRomSettings(const OrderedJson& platform, const OrderedJson& rom, const std::string& platformId, std::set<std::string>& explicitSettings)
{
    C8dbSettings settings;
    for (const auto& quirk : C8DB_QUIRKS) {
        settings.quirks[quirk] = platform.contains("quirks") && platform["quirks"].value(quirk, false);
    }
    if (rom.contains("quirkyPlatforms") && rom["quirkyPlatforms"].contains(platformId)) {
        for (const auto& [quirk, value] : rom["quirkyPlatforms"][platformId].items()) {
            settings.quirks[quirk] = value.get<bool>();
            explicitSettings.insert(quirk);
        }
    }
    settings.tickrate = rom.value("tickrate", platform.value("defaultTickrate", 0));
    if (rom.contains("tickrate")) {
        explicitSettings.insert("tickrate");
    }
    settings.startAddress = rom.value("startAddress", 0x200);
    if (rom.contains("startAddress")) {
        explicitSettings.insert("startAddress");
    }
    settings.screenRotation = rom.value("screenRotation", 0);
    if (rom.contains("screenRotation")) {
        explicitSettings.insert("screenRotation");
    }
    return settings;
}

// The settings a library config sets itself, those that differ from its preset
std::set<std::string> explicitSettings(const C8dbConfig& config)
{
    std::set<std::string> result;
    for (const auto& [quirk, value] : config.settings.quirks) {
        if (config.presetSettings.quirks.at(quirk) != value) {
            result.insert(quirk);
        }
    }
    if (config.settings.tickrate != config.presetSettings.tickrate) {
        result.insert("tickrate");
    }
    if (config.settings.startAddress != config.presetSettings.startAddress) {
        result.insert("startAddress");
    }
    if (config.settings.screenRotation != config.presetSettings.screenRotation) {
        result.insert("screenRotation");
    }
    return result;
}

size_t utf8Length(const std::string& text)
{
    return static_cast<size_t>(std::ranges::count_if(text, [](char c) { return (static_cast<unsigned char>(c) & 0xC0) != 0x80; }));
}

// Formats JSON like the files of the chip-8-database (prettier style): two space indentation and
// arrays of plain values on one line as long as that line fits into 80 characters.
std::string formatC8dbJson(const OrderedJson& value, size_t indent = 0, size_t prefixLength = 0)
{
    const std::string pad(indent, ' ');
    if (value.is_object()) {
        if (value.empty()) {
            return "{}";
        }
        std::string result = "{\n";
        bool first = true;
        for (const auto& [key, item] : value.items()) {
            const auto prefix = pad + "  " + OrderedJson(key).dump() + ": ";
            result += (first ? "" : ",\n") + prefix + formatC8dbJson(item, indent + 2, utf8Length(prefix));
            first = false;
        }
        return result + "\n" + pad + "}";
    }
    if (value.is_array()) {
        if (std::none_of(value.begin(), value.end(), [](const OrderedJson& item) { return item.is_structured(); })) {
            std::string inlined = "[";
            for (size_t i = 0; i < value.size(); ++i) {
                inlined += (i ? ", " : "") + value[i].dump();
            }
            inlined += "]";
            if (prefixLength + utf8Length(inlined) + 1 <= 80) {
                return inlined;
            }
        }
        std::string result = "[\n";
        for (size_t i = 0; i < value.size(); ++i) {
            result += (i ? ",\n" : "") + pad + "  " + formatC8dbJson(value[i], indent + 2, indent + 2);
        }
        return result + "\n" + pad + "]";
    }
    return value.dump();
}

std::optional<OrderedJson> loadOrderedJson(const fs::path& path)
{
    std::ifstream is(path);
    if (!is) {
        std::cerr << "ERROR: Could not open '" << path.string() << "'." << std::endl;
        return std::nullopt;
    }
    try {
        return OrderedJson::parse(is);
    }
    catch (const nlohmann::json::exception& ex) {
        std::cerr << "ERROR: Could not parse '" << path.string() << "': " << ex.what() << std::endl;
        return std::nullopt;
    }
}

std::string settingText(const C8dbSettings& settings, const std::string& name)
{
    if (name == "tickrate") {
        // 0 instructions per frame is unlimited in Cadmium, there is no such tickrate in the chip-8-database
        return settings.tickrate ? std::to_string(settings.tickrate) : "0 (unlimited)";
    }
    if (name == "startAddress") {
        return std::to_string(settings.startAddress);
    }
    if (name == "screenRotation") {
        return std::to_string(settings.screenRotation);
    }
    return settings.quirks.at(name) ? "true" : "false";
}

// A ROM entry for a library ROM that is not in the chip-8-database, nothing if none of its
// configs has a chip-8-database platform.
std::optional<OrderedJson> c8dbRomEntry(const DBBinary& binary, const std::map<std::string, OrderedJson>& platforms)
{
    auto platformList = OrderedJson::array();
    auto quirkyPlatforms = OrderedJson::object();
    std::optional<int> tickrate, startAddress, screenRotation;
    for (const auto& dbConfig : binary.configs) {
        auto config = c8dbConfig(dbConfig);
        if (config.platform.empty() || std::find(platformList.begin(), platformList.end(), config.platform) != platformList.end() || quirkyPlatforms.contains(config.platform)) {
            continue;
        }
        std::set<std::string> ignored;
        const auto defaults = c8dbRomSettings(platforms.at(config.platform), OrderedJson::object(), config.platform, ignored);
        const auto explicitOnes = explicitSettings(config);
        // only what the library sets explicitly, the presets of both differ on some defaults
        auto overrides = OrderedJson::object();
        for (const auto& quirk : C8DB_QUIRKS) {
            if (explicitOnes.contains(quirk) && config.settings.quirks.at(quirk) != defaults.quirks.at(quirk)) {
                overrides[quirk] = config.settings.quirks.at(quirk);
            }
        }
        if (overrides.empty()) {
            platformList.push_back(config.platform);
        }
        else {
            quirkyPlatforms[config.platform] = overrides;
        }
        if (!tickrate && explicitOnes.contains("tickrate") && config.settings.tickrate > 0 && config.settings.tickrate != defaults.tickrate) {
            tickrate = config.settings.tickrate;
        }
        if (!startAddress && explicitOnes.contains("startAddress") && config.settings.startAddress != 0x200) {
            startAddress = config.settings.startAddress;
        }
        if (!screenRotation && explicitOnes.contains("screenRotation") && config.settings.screenRotation != 0) {
            screenRotation = config.settings.screenRotation;
        }
    }
    if (platformList.empty() && quirkyPlatforms.empty()) {
        return std::nullopt;
    }
    OrderedJson rom;
    rom["file"] = binary.filenames.empty() ? binary.sha1 : fs::path(binary.filenames.front()).filename().string();
    rom["platforms"] = platformList;
    if (!quirkyPlatforms.empty()) {
        rom["quirkyPlatforms"] = quirkyPlatforms;
    }
    if (tickrate) {
        rom["tickrate"] = *tickrate;
    }
    if (startAddress) {
        rom["startAddress"] = *startAddress;
    }
    if (screenRotation) {
        rom["screenRotation"] = *screenRotation;
    }
    return rom;
}

}  // namespace

static bool upgradeLibrary(db::Database& database)
{
    std::ostringstream schemaReport;
    auto* coutBuffer = std::cout.rdbuf(schemaReport.rdbuf());
    try {
        db::Registry::syncTables(database);
        migrateData(database);
    }
    catch (const db::Exception& ex) {
        std::cout.rdbuf(coutBuffer);
        std::cerr << "ERROR: Could not update the library schema: " << ex.what() << std::endl;
        return false;
    }
    std::cout.rdbuf(coutBuffer);
    return true;
}

int Database::printOpcodeStatistics(const std::string& dataPath, const std::vector<std::string>& files)
{
    // the configs of the library come first, like when it analyzes its ROMs itself
    const auto library = fs::path(dataPath) / "cadmium_library.cd48db";
    std::unique_ptr<db::Database> database;
    std::unique_ptr<LibraryMetadataSource> metadataSource;
    if (fs::exists(library)) {
        registerTables();
        database = std::make_unique<db::Database>(library.string(), 1);
        metadataSource = std::make_unique<LibraryMetadataSource>(*database);
        Librarian::setMetadataSource(metadataSource.get());
    }
    nlohmann::ordered_json roms = nlohmann::ordered_json::object();
    int failed = 0;
    for (const auto& file : files) {
        std::vector<uint8_t> data;
        auto info = scanFile(file, &data);
        if (!info) {
            std::cerr << "ERROR: Could not read '" << file << "': " << info.error().message << std::endl;
            ++failed;
            continue;
        }
        const auto metadata = Librarian::findRomMetadata(info->digest).value_or(RomMetadata{});
        const auto startAddress = analysisStartAddress(metadata.preset, metadata.properties);
        auto opcodes = nlohmann::ordered_json::object();
        if (isAnalyzable(metadata.preset)) {
            for (const auto& [opcode, count] : opcodeStatistics(data, startAddress)) {
                opcodes[fmt::format("{:04X}", opcode)] = count;
            }
        }
        roms[info->digest.to_hex()] = {{"file", file}, {"preset", metadata.preset}, {"startAddress", startAddress}, {"opcodes", opcodes}};
    }
    Librarian::setMetadataSource(nullptr);
    nlohmann::ordered_json result = {{"analysisVersion", OPCODE_ANALYSIS_VERSION}, {"roms", roms}};
    std::cout << result.dump(1) << std::endl;
    return failed ? 1 : 0;
}

int Database::exportMetadata(const std::string& dataPath, const std::string& outputFile)
{
    const auto library = fs::path(dataPath) / "cadmium_library.cd48db";
    if (!fs::exists(library)) {
        std::cerr << "ERROR: No library found at '" << library.string() << "'." << std::endl;
        return 1;
    }
    if (fs::exists(outputFile)) {
        std::cerr << "ERROR: '" << outputFile << "' exists already, it is not overwritten." << std::endl;
        return 1;
    }
    registerTables();
    {
        db::Database database(library.string(), 1);
        if (!upgradeLibrary(database)) {
            return 1;
        }
        try {
            // a consistent copy, the library could be in use by a running Cadmium
            auto session = database.session();
            db::Statement copy(session, "VACUUM INTO ?");
            copy << outputFile;
            copy.execute();
        }
        catch (const db::Exception& ex) {
            std::cerr << "ERROR: Could not copy the library to '" << outputFile << "': " << ex.what() << std::endl;
            return 1;
        }
    }
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &now);
#else
    gmtime_r(&now, &tm);
#endif
    const int starterVersion = (tm.tm_year + 1900) * 10000 + (tm.tm_mon + 1) * 100 + tm.tm_mday;
    int programs = 0, binaries = 0;
    try {
        db::Database database(outputFile, 1);
        auto session = database.session();
        db::Transaction transaction{session};
        // only metadata: no ROMs, file names or what this installation ignores or found newly
        db::Statement(session, "UPDATE binaries SET data = NULL;").execute();
        db::Statement(session, "DELETE FROM filenames;").execute();
        db::Statement(session, "DELETE FROM ignored_binaries;").execute();
        db::Statement(session, "DELETE FROM program_tags WHERE tag_id IN (SELECT id FROM tags WHERE name = 'new');").execute();
        db::Statement(session, "DELETE FROM binary_tags WHERE tag_id IN (SELECT id FROM tags WHERE name = 'new');").execute();
        for (const auto* table : {"programs", "binaries"}) {
            db::Statement mark(session, fmt::format("UPDATE {} SET starter_version = ?, user_modified = 0;", table));
            mark << starterVersion;
            mark.execute();
        }
        db::Statement version(session, "UPDATE version SET starter_version = ?;");
        version << starterVersion;
        version.execute();
        transaction.commit();
        programs = static_cast<int>(database.executeForValue("SELECT COUNT(*) FROM programs;"));
        binaries = static_cast<int>(database.executeForValue("SELECT COUNT(*) FROM binaries;"));
        // a single file without WAL, as it is copied out of the resources
        database.execute("PRAGMA journal_mode = DELETE;");
        database.execute("VACUUM;");
    }
    catch (const db::Exception& ex) {
        std::cerr << "ERROR: Could not strip '" << outputFile << "' to metadata: " << ex.what() << std::endl;
        return 1;
    }
    std::cout << fmt::format("Exported {} programs with {} ROMs as starter metadata version {} to '{}' ({} bytes).", programs, binaries, starterVersion, outputFile, fs::file_size(outputFile)) << std::endl;
    return 0;
}

int Database::updateProgramDatabase(const std::string& dataPath, const std::string& programsJsonPath)
{
    const fs::path programsPath = programsJsonPath;
    const auto libraryFile = fs::path(dataPath) / "cadmium_library.cd48db";
    if (!fs::exists(libraryFile)) {
        std::cerr << "ERROR: No library found at '" << libraryFile.string() << "'." << std::endl;
        return 1;
    }
    auto programsJson = loadOrderedJson(programsPath);
    auto platformsJson = loadOrderedJson(programsPath.parent_path() / "platforms.json");
    if (!programsJson || !platformsJson || !programsJson->is_array() || !platformsJson->is_array()) {
        std::cerr << "ERROR: Expected programs.json and platforms.json of the chip-8-database database folder." << std::endl;
        return 1;
    }
    std::map<std::string, OrderedJson> platforms;
    for (const auto& platform : *platformsJson) {
        platforms.emplace(platform.value("id", ""), platform);
    }
    std::map<std::string, size_t> programOfSha1;
    for (size_t i = 0; i < programsJson->size(); ++i) {
        for (const auto& [sha1, rom] : (*programsJson)[i]["roms"].items()) {
            programOfSha1.emplace(sha1, i);
        }
    }

    // the library, read without the UI and the scan
    registerTables();
    db::Database database(libraryFile.string(), 1);
    if (!upgradeLibrary(database)) {
        return 1;
    }
    auto session = db::Session(database);
    std::map<int, DBProgram> programs;
    for (const auto& program : session.fetchAll<DBProgram>()) {
        programs.emplace(program.id, program);
    }
    std::map<int, std::string> authorNames;
    for (const auto& author : session.fetchAll<DBAuthor>()) {
        authorNames.emplace(author.id, author.name);
    }
    for (const auto& programAuthor : session.select<DBProgramAuthor>().orderBy("position").all()) {
        if (auto iter = programs.find(programAuthor.program_id); iter != programs.end()) {
            iter->second.authors.push_back(programAuthor.author_id);
        }
    }
    std::map<int, DBBinary> binaries;
    for (const auto& binary : session.fetchAll<DBBinary>()) {
        binaries.emplace(binary.id, binary);
        if (auto iter = programs.find(binary.program_id); iter != programs.end()) {
            iter->second.binaries.push_back(binary.id);
        }
    }
    for (const auto& config : session.select<DBBinaryConfig>().orderBy("id").all()) {
        if (auto iter = binaries.find(config.binary_id); iter != binaries.end()) {
            iter->second.configs.push_back(config);
        }
    }
    for (const auto& filename : session.select<DBFilename>().orderBy("id").all()) {
        if (auto iter = binaries.find(filename.binary_id); iter != binaries.end()) {
            iter->second.filenames.push_back(filename.name);
        }
    }

    // ROMs in both: report where the variants and their settings disagree, sorted by title
    int known = 0, disagreeing = 0;
    std::vector<std::pair<std::string, std::string>> reports;
    for (const auto& binary : binaries | std::views::values) {
        auto programIter = programOfSha1.find(binary.sha1);
        if (programIter == programOfSha1.end()) {
            continue;
        }
        ++known;
        const auto& c8dbProgram = (*programsJson)[programIter->second];
        const auto& rom = c8dbProgram["roms"][binary.sha1];
        std::vector<std::string> romPlatforms;
        for (const auto& platform : rom.value("platforms", OrderedJson::array())) {
            romPlatforms.push_back(platform.get<std::string>());
        }
        const auto quirkyPlatforms = rom.value("quirkyPlatforms", OrderedJson::object());  // items() of a temporary would dangle
        for (const auto& [platform, quirks] : quirkyPlatforms.items()) {
            if (std::ranges::find(romPlatforms, platform) == romPlatforms.end()) {
                romPlatforms.push_back(platform);
            }
        }
        std::vector<C8dbConfig> configs;
        std::vector<std::string> withoutPlatform;
        for (const auto& dbConfig : binary.configs) {
            if (auto config = c8dbConfig(dbConfig); config.platform.empty()) {
                withoutPlatform.push_back(dbConfig.preset);
            }
            else {
                configs.push_back(std::move(config));
            }
        }
        std::vector<std::string> lines;
        std::vector<std::string> onlyInJson, onlyInLibrary;
        for (const auto& platform : romPlatforms) {
            auto config = std::ranges::find(configs, platform, &C8dbConfig::platform);
            if (config == configs.end()) {
                onlyInJson.push_back(platform);
                continue;
            }
            if (!platforms.contains(platform)) {
                continue;
            }
            std::set<std::string> explicitInJson;
            const auto romSettings = c8dbRomSettings(platforms.at(platform), rom, platform, explicitInJson);
            const auto explicitInLibrary = explicitSettings(*config);
            std::vector<std::string> names = config->settings.quirks.empty() ? std::vector<std::string>{} : C8DB_QUIRKS;
            if (!config->settings.quirks.empty()) {
                names.insert(names.end(), {"tickrate", "startAddress", "screenRotation"});
            }
            // settings are only compared where at least one side sets them, the defaults differ
            for (const auto& name : names) {
                if ((explicitInJson.contains(name) || explicitInLibrary.contains(name)) && settingText(romSettings, name) != settingText(config->settings, name)) {
                    lines.push_back(fmt::format("    {}: {} is {} in programs.json, {} in the library", platform, name, settingText(romSettings, name), settingText(config->settings, name)));
                }
            }
        }
        for (const auto& config : configs) {
            if (std::ranges::find(romPlatforms, config.platform) == romPlatforms.end() && std::ranges::find(onlyInLibrary, config.platform) == onlyInLibrary.end()) {
                onlyInLibrary.push_back(config.platform);
            }
        }
        if (!onlyInJson.empty()) {
            lines.insert(lines.begin(), "    only in programs.json: " + join(onlyInJson.begin(), onlyInJson.end(), ", "));
        }
        if (!onlyInLibrary.empty()) {
            lines.insert(lines.begin(), "    only in the library: " + join(onlyInLibrary.begin(), onlyInLibrary.end(), ", "));
        }
        if (!lines.empty()) {
            ++disagreeing;
            const auto title = c8dbProgram.value("title", "?");
            auto text = fmt::format("{} [{}] {}\n", title, binary.sha1.substr(0, 8), rom.value("file", ""));
            for (const auto& line : lines) {
                text += line + "\n";
            }
            if (!withoutPlatform.empty()) {
                text += "    (library variants without chip-8-database platform: " + join(withoutPlatform.begin(), withoutPlatform.end(), ", ") + ")\n";
            }
            reports.emplace_back(toLower(title), text);
        }
    }
    std::ranges::sort(reports);
    for (const auto& text : reports | std::views::values) {
        std::cout << text;
    }

    // ROMs only in the library: added to the program of another of their ROMs, or as new program
    int addedRoms = 0, newPrograms = 0, skipped = 0;
    for (const auto& program : programs | std::views::values) {
        std::optional<size_t> target;
        for (const auto binaryId : program.binaries) {
            if (auto iter = programOfSha1.find(binaries.at(binaryId).sha1); iter != programOfSha1.end()) {
                target = iter->second;
                break;
            }
        }
        auto roms = OrderedJson::object();
        for (const auto binaryId : program.binaries) {
            const auto& binary = binaries.at(binaryId);
            if (programOfSha1.contains(binary.sha1)) {
                continue;
            }
            if (auto rom = c8dbRomEntry(binary, platforms)) {
                roms[binary.sha1] = *rom;
            }
            else {
                ++skipped;
            }
        }
        if (roms.empty()) {
            continue;
        }
        addedRoms += static_cast<int>(roms.size());
        if (target) {
            for (const auto& [sha1, rom] : roms.items()) {
                (*programsJson)[*target]["roms"][sha1] = rom;
            }
        }
        else {
            // in the order of the chip-8-database schema
            OrderedJson entry;
            entry["title"] = program.name;
            if (!program.origin.empty() || !program.origin_reference.empty()) {
                auto origin = OrderedJson::object();
                if (!program.origin.empty()) {
                    origin["type"] = program.origin;
                }
                if (!program.origin_reference.empty()) {
                    origin["reference"] = program.origin_reference;
                }
                entry["origin"] = origin;
            }
            if (!program.description.empty()) {
                entry["description"] = program.description;
            }
            if (isReleaseDate(program.release)) {
                entry["release"] = program.release;
            }
            if (!program.copyright.empty()) {
                entry["copyright"] = program.copyright;
            }
            if (!program.license.empty()) {
                entry["license"] = program.license;
            }
            if (!program.url.empty()) {
                entry["urls"] = OrderedJson::array({program.url});
            }
            if (!program.authors.empty()) {
                auto authors = OrderedJson::array();
                for (const auto authorId : program.authors) {
                    if (auto iter = authorNames.find(authorId); iter != authorNames.end()) {
                        authors.push_back(iter->second);
                    }
                }
                entry["authors"] = authors;
            }
            entry["roms"] = roms;
            programsJson->push_back(entry);
            ++newPrograms;
        }
    }

    // programs_<ISO 8601 basic format timestamp>.json, without colons to be a valid file name everywhere
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &now);
#else
    gmtime_r(&now, &tm);
#endif
    char timestamp[32];
    std::strftime(timestamp, sizeof(timestamp), "%Y%m%dT%H%M%SZ", &tm);
    const auto outputFile = fs::path(fmt::format("programs_{}.json", timestamp));
    {
        std::ofstream os(outputFile, std::ios::binary);
        os << formatC8dbJson(*programsJson) << "\n";
        if (!os) {
            std::cerr << "ERROR: Could not write '" << outputFile.string() << "'." << std::endl;
            return 1;
        }
    }
    std::cout << std::endl
              << fmt::format("{} library ROMs, {} of them in programs.json, {} with disagreements.", binaries.size(), known, disagreeing) << std::endl
              << fmt::format("Added {} ROMs ({} new programs), {} ROMs not added as they have no chip-8-database platform.", addedRoms, newPrograms, skipped) << std::endl
              << "Written: " << fs::absolute(outputFile).string() << std::endl;
    return 0;
}
