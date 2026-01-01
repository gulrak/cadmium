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
#include "stylemanager.hpp"
#include <c8db/database.hpp>

#include <raylib.h>
#include <ghc/sqlite.hpp>
#include <rlguipp/rlguipp.hpp>


#ifndef _WIN32
#include <httplib.h>
#endif

#include "fmt/os.h"

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

    Rectangle rect{};
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
    db::Blob data;

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

#if 0
inline auto initStorage(const std::string &path) {
    // clang-format off
    return make_storage(
        path + "/cadmium_library.cd48db",
        make_table("version",
            make_column("id", &DBVersion::id, primary_key().autoincrement()),
            make_column("schema_version", &DBVersion::schema_version, unique())
        ),
        make_table("programs",
            make_column("id", &DBProgram::id, primary_key().autoincrement()),
            make_column("name", &DBProgram::name),
            make_column("origin", &DBProgram::origin),
            make_column("description", &DBProgram::description),
            make_column("release", &DBProgram::release)
        ),
        make_table("binaries",
            make_column("id", &DBBinary::id, primary_key().autoincrement()),
            make_column("program_id", &DBBinary::program_id),
            make_column("sha1", &DBBinary::sha1, unique()),
            make_column("release", &DBBinary::release),
            make_column("description", &DBBinary::description),
            make_column("data", &DBBinary::data),
            foreign_key(&DBBinary::program_id)
                .references(&DBProgram::id)
                .on_update.cascade()
                .on_delete.cascade()
        ),
        make_table("binary_configs",
            make_column("id", &DBBinaryConfig::id, primary_key().autoincrement()),
            make_column("binary_id", &DBBinaryConfig::binary_id),
            make_column("preset", &DBBinaryConfig::preset),
            make_column("properties", &DBBinaryConfig::properties),
            foreign_key(&DBBinaryConfig::binary_id)
                .references(&DBBinary::id)
                .on_update.cascade()
                .on_delete.cascade()
        ),
        make_table("filenames",
            make_column("id", &DBFilename::id, primary_key().autoincrement()),
            make_column("binary_id", &DBFilename::binary_id),
            make_column("name", &DBFilename::name, unique()),
            foreign_key(&DBFilename::binary_id)
                .references(&DBBinary::id)
                .on_update.cascade()
                .on_delete.cascade()
        ),
        make_table("tags",
            make_column("id", &DBTags::id, primary_key().autoincrement()),
            make_column("name", &DBTags::name, unique()),
            make_column("color", &DBTags::color)
        ),
        make_table("programs_tags",
            make_column("id", &DBProgramTag::id, primary_key().autoincrement()),
            make_column("program_id", &DBProgramTag::program_id),
            make_column("tag_id", &DBProgramTag::tag_id),
            foreign_key(&DBProgramTag::program_id)
                .references(&DBProgram::id)
                .on_update.cascade()
                .on_delete.cascade(),
            foreign_key(&DBProgramTag::tag_id)
                .references(&DBTags::id)
                .on_update.cascade()
                .on_delete.cascade()
        ),
        make_table("binaries_tags",
            make_column("id", &DBBinaryTag::id, primary_key().autoincrement()),
            make_column("binary_id", &DBBinaryTag::binary_id),
            make_column("tag_id", &DBBinaryTag::tag_id),
            foreign_key(&DBBinaryTag::binary_id)
                .references(&DBBinary::id)
                .on_update.cascade()
                .on_delete.cascade(),
            foreign_key(&DBBinaryTag::tag_id)
                .references(&DBTags::id)
                .on_update.cascade()
                .on_delete.cascade()
        ),
        make_table("binarie_config_tags",
            make_column("id", &DBBinaryConfigTag::id, primary_key().autoincrement()),
            make_column("binary_id", &DBBinaryConfigTag::binary_config_id),
            make_column("tag_id", &DBBinaryConfigTag::tag_id),
            foreign_key(&DBBinaryConfigTag::binary_config_id)
                .references(&DBBinaryConfig::id)
                .on_update.cascade()
                .on_delete.cascade(),
            foreign_key(&DBBinaryConfigTag::tag_id)
                .references(&DBTags::id)
                .on_update.cascade()
                .on_delete.cascade()
        )
    );
    // clang-format on
}

using Storage = decltype(initStorage(""));
#endif

struct Database::Private
{
    std::mutex mutex;
    db::Database database;
    //db::Session session{database};
    int newTagId{1};
    int unclassifiedTagId{2};
    std::unordered_map<int, DBProgram> programs;
    std::unordered_map<int, DBBinary> binaries;
    std::unordered_map<int, DBTags> tags;
    std::vector<size_t> shownIndices;
    std::vector<std::string> sortedTags;
    std::string queryLine;
    std::string presetFilter;
    std::string textFilter;
    Vector2 tagsScrollPos{};
    float listContentHeight{0};
    emu::ThreadedBackgroundHost backgroundHost;


    explicit Private(CadmiumConfiguration& cfg, const std::string& path)
        : database(path + "/cadmium_library.cd48db", 4)
        , backgroundHost(cfg)
    {
        registerTables();
        db::Registry::syncTables(database);
    }

    void updateFilter()
    {
        shownIndices.clear();
        shownIndices.reserve(programs.size());
        for (auto& program : programs | std::views::values) {
            auto show = true;
            if (!presetFilter.empty()) {
                bool matchesPreset = false;
                for (const auto& binid : program.binaries) {
                    const auto& binary = binaries[binid];
                    for (const auto& bincfg : binary.configs) {
                        if (presetFilter.find(bincfg.preset) != std::string::npos) {
                            matchesPreset = true;
                            break;
                        }
                    }
                }
                if (!matchesPreset) {
                    show = false;
                }
            }
            if (show) {
                if (!textFilter.empty()) {
                    if (program.name.find(textFilter) == std::string::npos
                        && program.description.find(textFilter) == std::string::npos) {
                        show = false;
                    }
                }
            }
            if (show) {
                shownIndices.emplace_back(program.id);
            }
        }
        std::ranges::sort(shownIndices, std::less<>{}, [this](const int key) {
            return toLower(programs.at(key).name);
        });
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


#define IGNORE(INSERT_EXPR)                                                 \
    do {                                                                    \
        try {                                                               \
            (INSERT_EXPR);                                                  \
        } catch (const std::system_error& ex) {                             \
            std::cerr << ex.what() << std::endl;                            \
            /* sqlite_orm wraps every engine error in std::system_error*/   \
            /* whose `.code()` is the underlying SQLite result code */      \
            const int ec = ex.code().value();                               \
            if (ec == SQLITE_CONSTRAINT                                     \
                || ec == SQLITE_CONSTRAINT_UNIQUE                           \
                || ((ex.what() &&                                           \
                std::strstr(ex.what(), "UNIQUE constraint") != nullptr))) { \
                    /* duplicate → ignore */                                \
            } else {                                                        \
                throw;  /* something else went wrong → bubble up  */        \
            }                                                               \
        }                                                                   \
    } while (false)


Database::Database(const emu::CoreRegistry& registry, CadmiumConfiguration& configuration, ThreadPool& threadPool, const std::string& path)
    : _registry(registry)
    , _threadPool(threadPool)
    , _configuration(configuration)
    , _pimpl(std::make_unique<Private>(configuration, path))
{
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

Database::~Database() = default;

void Database::refreshBadges()
{
    _badges.clear();
    _pimpl->sortedTags.clear();
    _badges.emplace("generic-chip-8", BadgeInfo{BadgeInfo::GENERIC, "generic-chip-8", DARKGRAY, {0xE0, 0xC0, 0x00, 0xFF}});
    _pimpl->sortedTags.emplace_back("generic-chip-8");
    for(const auto& [name, info] : _registry) {
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
        }
    }
    for (const auto& [id, tag] : _pimpl->tags) {
        if (!_badges.contains(tag.name)) {
            emu::Palette::Color col(tag.color);
            if (tag.name == "???") {
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
            auto programs = session.fetchAll<DBProgram>();
            for (const auto& program : programs) {
                auto [iter, added] = _pimpl->programs.emplace(program.id, program);
                auto binaries = session.select(db::col(&DBBinary::id)).where(db::col(&DBBinary::program_id) == program.id);
                iter->second.binaries.clear();
                for (const auto& bin : binaries) {
                    iter->second.binaries.push_back(bin);
                }
            }
        }
        {
            // TODO: Don't load all binaries data into memory ;-)
            _pimpl->binaries.clear();
            _digests.clear();
            auto binaries = session.fetchAll<DBBinary>();
            for (const auto& binary : binaries) {
                auto [iter, added] = _pimpl->binaries.emplace(binary.id, binary);
                _digests.insert(Sha1::Digest(binary.sha1));
                auto configs = session.select<DBBinaryConfig>().where(db::col(&DBBinaryConfig::binary_id) == binary.id);
                for (const auto& config : configs) {
                    iter->second.configs.push_back(config);
                }
                auto filenames = session.select(db::col(&DBFilename::name)).where(db::col(&DBFilename::binary_id) == binary.id);
                for (const auto& filename : filenames) {
                    iter->second.filenames.push_back(filename);
                }
            }
        }
        refreshBadges();
        _pimpl->updateFilter();
    }
}

int Database::scanLibrary()
{
    std::vector<DBProgram> programs;
    std::vector<DBBinary> binarys;
    auto start = std::chrono::steady_clock::now();
    auto extensions = _registry.getSupportedExtensions();
    std::vector<const KnownRomInfo*> foundRoms;
    int numFiles = 0;
    auto session = db::Session(_pimpl->database);
    for (const auto& folder : _configuration.libraryPath) {
        try {
            for (const auto& de : fs::recursive_directory_iterator(folder, fs::directory_options::skip_permission_denied)) {
                if (de.is_regular_file() && extensions.contains(de.path().extension().string())) {
                    std::vector<uint8_t> data;
                    auto info = scanFile(de.path().string(), &data);
                    if (info) {
                        bool digested = false;
                        {
                            std:std::lock_guard lock(_pimpl->mutex);
                            digested = _digests.contains(info->digest);
                            if (!digested) {
                                _digests.emplace(info->digest);
                            }
                        }
                        if (!digested) {
                            DBProgram program;
                            DBBinary binary;
                            std::string name;
                            std::string preset = "???";
                            if (Librarian::findKnownRoms(info->digest, foundRoms)) {
                                name = foundRoms.front()->name ? fmt::format(" {} -", foundRoms.front()->name) : "";
                                preset = foundRoms.front()->preset;
                                try {
                                    db::Transaction transaction{session};
                                    program = DBProgram{.name = std::string(foundRoms.front()->name) };
                                    session.insert(program);
                                    binary = DBBinary{.program_id = program.id, .sha1 = info->digest.to_hex(), .data = db::Blob(data.begin(), data.end())};
                                    session.insert(binary);
                                    for (const auto* romInfo : foundRoms) {
                                        auto config = DBBinaryConfig{.binary_id = binary.id, .preset = std::string(romInfo->preset), .properties = std::string(romInfo->options ? romInfo->options : "")};
                                        binary.configs.push_back(config);
                                        session.insert( config);
                                        //std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                                        auto filename = DBFilename{.binary_id = binary.id, .name = de.path().string()};
                                        std::cout << "insert DBFilename: " << filename.binary_id << ", " << filename.name << std::endl;
                                        session.insert(filename);
                                        DBProgramTag progTag{.program_id = program.id, .tag_id = _pimpl->newTagId};
                                        session.insert(progTag);
                                    }
                                    transaction.commit();
                                }
                                catch (const std::system_error& ex) {
                                    std::cerr << "SQLExecutionError: " << ex.what() << std::endl;
                                }
                            }
                            else {
                                std::cout << "already digested " << info->digest.to_hex() << std::endl;
                                try {
                                    db::Transaction transaction{session};
                                    auto extension = de.path().extension().string();
                                    std::string preset;
                                    if (extension != ".ch8") {
                                        preset = toLower(emu::CoreRegistry::presetForExtension(extension));
                                    }
                                    program = DBProgram{.name = de.path().filename().stem().string() };
                                    session.insert(program);
                                    binary = DBBinary{.program_id = program.id, .sha1 = info->digest.to_hex(), .data = db::Blob(data.begin(), data.end())};
                                    session.insert( binary);
                                    if (!preset.empty()) {
                                        auto config = DBBinaryConfig{.binary_id = binary.id, .preset = preset};
                                        session.insert(config);
                                        binary.configs.push_back(config);
                                    }
                                    session.insert(DBFilename{.binary_id = binary.id, .name = de.path().string()});
                                    session.insert(DBProgramTag{.program_id = program.id, .tag_id = _pimpl->newTagId});
                                    session.insert(DBBinaryTag{.binary_id =binary.id, .tag_id = _pimpl->unclassifiedTagId});
                                    transaction.commit();
                                }
                                catch (const std::system_error& ex) {
                                    std::cerr << "SQLExecutionError: " << ex.what() << std::endl;
                                }
                            }
                            TraceLog(LOG_INFO, fmt::format("found {}: {:14}{} '{}'", info->digest.to_hex(), preset, name, de.path().string()).c_str());
                            {
                                std::lock_guard lock(_pimpl->mutex);
                                const auto iter = _pimpl->programs.emplace(program.id, program).first;
                                iter->second.binaries.push_back(binary.id);
                                _pimpl->binaries.emplace(binary.id, binary);
                                if ((numFiles & 63) == 0) {
                                    _pimpl->updateFilter();
                                }
                            }
                        }
                        else {
                            const std::string digest = info->digest.to_hex();
                            auto bid = session.select(db::col(&DBBinary::id)).where(db::col(&DBBinary::sha1) == digest);
                            if (!bid.empty()) {
                                session.insert(DBFilename{.binary_id = bid.first(), .name = de.path().string()});
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
        catch (const std::system_error& e) {
            std::cerr << "SQLConstraintError: " << e.what() << std::endl;
        }
    }
    {
        std::lock_guard lock(_pimpl->mutex);
        _pimpl->updateFilter();
    }
    durationOfLastJob = std::chrono::steady_clock::now() - start;
    return numFiles;
}

ghc::expected<Database::FileInfo,LoadError> Database::scanFile(const std::string& filePath, std::vector<uint8_t>* outData)
{
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

bool Database::render(Font& font)
{
    using namespace gui;
    static bool first = true;
    static bool second = false;
    bool binarySelected = false;
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
        SetSpacing(4);
        auto area = GetContentAvailable();
        _pimpl->relayoutList(area.width);
        TextBox(_pimpl->queryLine, 4096);
        BeginColumns();
        {
            SetSpacing(4);
            auto tagsWidth = area.width / 4 - 5;
            SetNextWidth(tagsWidth);
            auto offset = GetCurrentPos();
            BeginTableView(GetContentAvailable().height - 135, 2, &_pimpl->tagsScrollPos);
            for (const auto& tagText : _pimpl->sortedTags) {
                const auto& badge = _badges[tagText];
                TableNextRow(10);
                TableNextColumn(tagsWidth - 8, [&](Rectangle rect) {
                    drawBadge(font, badge.text.c_str(), {rect.x + 2, rect.y + 2}, badge.textCol, badge.badgeCol);
                });
            }
            EndTableView();
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
            for (const int pid : _pimpl->shownIndices) {
                const auto& program = _pimpl->programs.at(pid);
                Rectangle itemRect = {program.rect.x + px + cx + scrollPos.x, program.rect.y + py + cy + scrollPos.y, program.rect.width, program.rect.height - 2};
                if (CheckCollisionRecs(listRect, itemRect)) {
                    ++disp;
                    if (disp > maxDisp) {
                        maxDisp = disp;
                    }
                    if (CheckCollisionPointRec(GetMousePosition(), itemRect)) {
                        DrawRectangleClipped(itemRect.x - 2, itemRect.y - 2, itemRect.width, itemRect.height, StyleManager::getStyleColor(gui::Style::BASE_COLOR_NORMAL));
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                            if (program.binaries.size() == 1) {
                                const auto& binary = _pimpl->binaries.at(program.binaries.front());
                                if (binary.configs.size() == 1) {
                                    const auto& config = binary.configs.front();
                                    auto preset = config.preset;
                                    if (preset == "generic-chip-8")
                                        preset = "chip-8";
                                    emu::Properties props;
                                    if (!fuzzyCompare(preset, "generic-chip-8")) {
                                        props = emu::CoreRegistry::propertiesForPreset(preset);
                                        const auto& propString = _pimpl->binaries[program.binaries.front()].configs.front().properties;
                                        if (!propString.empty()) {
                                            props.applyDiff(nlohmann::json::parse(propString));
                                        }
                                    }
                                    const auto& data = _pimpl->binaries[program.binaries.front()].data;
                                    _selectedProgram = {.name = program.name, .properties = props, .data = std::vector<uint8_t>(data.begin(), data.end())};
                                    _pimpl->backgroundHost.killEmulation();
                                    _pimpl->backgroundHost.loadBinary(_selectedProgram->name, _selectedProgram->data, _selectedProgram->properties, true);
                                    binarySelected = true;
                                }
                            }
                            else {
                                _selectedProgram = {};//{.name = program.name, .properties = {}, .data = {}};
                            }
                        }
                    }
                    DrawTextClipped(font, fmt::format("{}", program.name.c_str()).c_str(), {itemRect.x, itemRect.y}, lightgrayCol);
                    for (size_t i = 0; i < program.binaries.size(); ++i) {
                        const auto binary = _pimpl->binaries.at(program.binaries[i]);
                        DrawTextClipped(font, fmt::format("{}", _pimpl->binaries[program.binaries[i]].sha1.substr(0,8)).c_str(), {itemRect.x, itemRect.y + (i+1) * 9}, grayCol);
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
            //DrawRectangleLinesEx(listRect, 1, MAGENTA);
        }
        EndColumns();
        //auto innerHeight = _pimpl->programs
        //BeginScrollPanel(listRect.height, {0,0,area.width-6, (float)(_core->memSize()/8 + 1) * lineSpacing}, &memScroll);
        auto pos = GetCurrentPos();
        DrawPanelClipped({pos.x + area.width - 131.0f, pos.y, 130.0f, 98.0f}, 1, StyleManager::instance().getStyleColor(gui::Style::BORDER_COLOR_NORMAL), {0,0,0,0});
        auto [texture, rect] = _pimpl->backgroundHost.updateTexture();
        _pimpl->backgroundHost.drawScreen({pos.x + area.width - 130, pos.y + 1, 128, 96});
        DrawTextEx(font, fmt::format("FPS: {:02.1f} ({} frames)", 1000000.0/_pimpl->backgroundHost.getFrameTimeAvg()+0.005, _pimpl->backgroundHost.getFrames()).c_str(), {pos.x + area.width - 130 + 4, pos.y + 100}, 8, 1, WHITE);
        //DrawTexturePro(*texture, {0,0,128,64}, {pos.x + area.width - 130, pos.y + 1, 128, 64}, {0,0}, 0, WHITE);
        //DrawText(fmt::format("{}x{}", mx, my).c_str(), area.x + 2, area.y + 2, 8, RED);
    }
    return false;
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
