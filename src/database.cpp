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
#include "librarian.hpp"
#include "stylemanager.hpp"
#include <c8db/database.hpp>

#include <raylib.h>
#include <sqlite3/sqlite3.h>
#include <rlguipp/rlguipp.hpp>
#include <zxorm/zxorm.hpp>

#include <httplib.h>

#include "fmt/os.h"

using namespace zxorm;

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
    std::vector<DBTags> tags;
    std::vector<int> binaries;
};

struct DBBinary
{
    int id{0};
    int program_id{0};
    std::string preset;
    std::string properties;
    std::string sha1;
    std::vector<uint8_t> data;
    std::vector<std::string> filenames;
    std::vector<DBTags> tags;
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

using VersionTable = Table<"version",
                                  DBVersion,
                                  Column<"id", &DBVersion::id, PrimaryKey<>>,
                                  Column<"schema_version", &DBVersion::schema_version, Unique<conflict_t::ignore>>>;
using ProgramsTable = Table<"programs",
                                  DBProgram,
                                  Column<"id", &DBProgram::id, PrimaryKey<>>,
                                  Column<"name", &DBProgram::name>,
                                  Column<"origin", &DBProgram::origin>,
                                  Column<"description", &DBProgram::description>,
                                  Column<"release", &DBProgram::release>>;
using BinariesTable = Table<"binaries",
                                 DBBinary,
                                 Column<"id", &DBBinary::id, PrimaryKey<>>,
                                 Column<"program_id", &DBBinary::program_id, ForeignKey<"programs", "id", action_t::cascade, action_t::cascade>>,
                                 Column<"preset", &DBBinary::preset>,
                                 Column<"properties", &DBBinary::properties>,
                                 Column<"sha1", &DBBinary::sha1, Unique<conflict_t::ignore>>,
                                 Column<"data", &DBBinary::data>>;
using FilenamesTable = Table<"filenames",
                                    DBFilename,
                                    Column<"id", &DBFilename::id, PrimaryKey<>>,
                                    Column<"binary_id", &DBFilename::binary_id, ForeignKey<"binaries", "id", action_t::cascade, action_t::cascade>>,
                                    Column<"name", &DBFilename::name, Unique<conflict_t::ignore>>>;
using TagsTable = Table<"tags",
                               DBTags,
                               Column<"id", &DBTags::id, PrimaryKey<>>,
                               Column<"name", &DBTags::name, Unique<conflict_t::ignore>>,
                               Column<"color", &DBTags::color>>;
using ProgramsTagsTable = Table<"programs_tags",
                                       DBProgramTag,
                                       Column<"id", &DBProgramTag::id, PrimaryKey<>>,
                                       Column<"program_id", &DBProgramTag::program_id, ForeignKey<"programs", "id", action_t::cascade, action_t::cascade>>,
                                       Column<"tag_id", &DBProgramTag::tag_id, ForeignKey<"tags", "id", action_t::cascade, action_t::cascade>>>;
using BinariesTagsTable = Table<"binaries_tags",
                                       DBBinaryTag,
                                       Column<"id", &DBBinaryTag::id, PrimaryKey<>>,
                                       Column<"binary_id", &DBBinaryTag::binary_id, ForeignKey<"binaries", "id", action_t::cascade, action_t::cascade>>,
                                       Column<"tag_id", &DBBinaryTag::tag_id, ForeignKey<"tags", "id", action_t::cascade, action_t::cascade>>>;

using DBConnection = Connection<VersionTable, ProgramsTable, BinariesTable, FilenamesTable, TagsTable, ProgramsTagsTable, BinariesTagsTable>;


struct Database::Private
{
    std::mutex mutex;
    std::unique_ptr<DBConnection> connection;
    int newTagId{1};
    int unclassifiedTagId{2};
    std::unordered_map<int, DBProgram> programs;
    std::unordered_map<int, DBBinary> binaries;
    std::vector<size_t> shownIndices;
    std::string queryLine;
    std::string presetFilter;
    std::string textFilter;
    Vector2 tagsScrollPos{};
    float listContentHeight{0};

    void updateFilter()
    {
        shownIndices.clear();
        shownIndices.reserve(programs.size());
        for (auto& program : programs | std::views::values) {
            auto show = true;
            if (!presetFilter.empty()) {
                bool matchesPreset = false;
                for (const auto& binid : program.binaries) {
                    if (const auto& binary = binaries[binid]; presetFilter.find(binary.preset) != std::string::npos) {
                        matchesPreset = true;
                        break;
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

Database::Database(const emu::CoreRegistry& registry, CadmiumConfiguration& configuration, ThreadPool& threadPool, const std::string& path, const std::unordered_map<std::string, std::string>& badges)
    : _registry(registry)
    , _threadPool(threadPool)
    , _configuration(configuration)
    , _badges(badges)
    , _pimpl(std::make_unique<Private>())
{
    _pimpl->connection = std::make_unique<DBConnection>((path + "/cadmium_library.sqlite").c_str(), 0, nullptr, [](auto level, const auto& msg) {
    if (zxorm::log_level::Error == level)
        std::cerr << "Ooops: " << msg << std::endl;
    else
        std::cout << msg << std::endl;
});
    _pimpl->connection->create_tables();
    _pimpl->connection->insert_record(DBVersion{});
    _pimpl->connection->insert_record(DBTags{0,"new", "#8080FF"});
    _pimpl->connection->insert_record(DBTags{0,"???", "#FFFF00"});
    //_pimpl->newTagId = _pimpl->connection->select_query<Select<TagsTable::field_t<"id">>>().where_one(TagsTable::field_t<"name">().like("new")).exec().value_or(0);
    //_pimpl->newTagId = _pimpl->connection->select_query<Select<TagsTable::field_t<"id">>>().where_one(TagsTable::field_t<"name">().like("???")).exec().value_or(0);
    fetchProgramInfo();
}

Database::~Database() = default;

void Database::fetchProgramInfo()
{
    {
        std::lock_guard lock(_pimpl->mutex);
        {
            _pimpl->programs.clear();
            auto programs = _pimpl->connection->select_query<DBProgram>().many().exec();
            for (const auto& program : programs) {
                auto [iter, added] = _pimpl->programs.emplace(program.id, program);
                auto binaries = _pimpl->connection->select_query<Select<BinariesTable::field_t<"id">>>().where_many(BinariesTable::field_t<"program_id">() == program.id).exec();
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
            auto binaries = _pimpl->connection->select_query<DBBinary>().many().exec();
            for (const auto& binary : binaries) {
                auto [iter, added] = _pimpl->binaries.emplace(binary.id, binary);
                _digests.insert(Sha1::Digest(binary.sha1));
                auto filenames = _pimpl->connection->select_query<Select<FilenamesTable::field_t<"name">>>().where_many(FilenamesTable::field_t<"binary_id">() == binary.id).exec();
                for (const auto& filename : filenames) {
                    iter->second.filenames.push_back(filename);
                }
            }
        }
        _pimpl->updateFilter();
    }
}

int Database::scanLibrary()
{
    std::vector<DBProgram> programs;
    std::vector<DBBinary> binarys;
    auto start = std::chrono::steady_clock::now();
    auto extensions = _registry.getSupportedExtensions();
    int numFiles = 0;
    for (auto folders = split(_configuration.libraryPath, ';'); const auto& folder : folders) {
        try {
            for (const auto& de : fs::recursive_directory_iterator(folder, fs::directory_options::skip_permission_denied)) {
                if (de.is_regular_file() && extensions.contains(de.path().extension())) {
                    std::vector<uint8_t> data;
                    auto info = scanFile(de.path(), &data);
                    bool digested = false;
                    {
                        std:std::lock_guard lock(_pimpl->mutex);
                        digested = _digests.contains(info.digest);
                        if (!digested) {
                            _digests.emplace(info.digest);
                        }
                    }
                    if (!digested) {
                        const char* preset = "unknown";
                        std::string name = "";
                        const auto* romInfo = Librarian::findKnownRom(info.digest);
                        DBProgram program;
                        DBBinary binary;
                        if (romInfo) {
                            preset = romInfo->preset;
                            name = romInfo->name ? fmt::format(" {} -", romInfo->name) : "";
                            try {
                                _pimpl->connection->transaction([this,romInfo, &info, &data, &de, &program, &binary]() {
                                    program = DBProgram{.name = std::string(romInfo->name) };
                                    _pimpl->connection->insert_record(program);
                                    binary = DBBinary{.program_id = program.id, .preset = std::string(romInfo->preset), .sha1 = info.digest.to_hex(), .data = data};
                                    _pimpl->connection->insert_record(binary);
                                    //std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                                    auto filename = DBFilename{.binary_id = binary.id, .name = de.path().string()};
                                    //std::cout << "insert DBFilename: " << filename.binary_id << ", " << filename.name << std::endl;
                                    _pimpl->connection->insert_record(filename);
                                    _pimpl->connection->insert_record(DBProgramTag{.program_id = program.id, .tag_id = _pimpl->newTagId});
                                });
                            }
                            catch (const SQLExecutionError& ex) {
                                std::cerr << "SQLExecutionError: " << ex.what() << std::endl;
                            }
                        }
                        else {
                            try {
                                _pimpl->connection->transaction([this, &info, &data, &de, &program, &binary]() {
                                    auto extension = de.path().extension().string();
                                    std::string preset;
                                    if (extension != ".ch8") {
                                        preset = toLower(emu::CoreRegistry::presetForExtension(extension));
                                    }
                                    program = DBProgram{.name = de.path().filename().stem().string() };
                                    _pimpl->connection->insert_record(program);
                                    binary = DBBinary{.program_id = program.id, .preset = preset, .sha1 = info.digest.to_hex(), .data = data};
                                    _pimpl->connection->insert_record(binary);
                                    _pimpl->connection->insert_record(DBFilename{.binary_id = binary.id, .name = de.path().string()});
                                    _pimpl->connection->insert_record(DBProgramTag{.program_id = program.id, .tag_id = _pimpl->newTagId});
                                    _pimpl->connection->insert_record(DBBinaryTag{.binary_id =binary.id, .tag_id = _pimpl->unclassifiedTagId});
                                });
                            }
                            catch (const SQLExecutionError& ex) {
                                std::cerr << "SQLExecutionError: " << ex.what() << std::endl;
                            }
                        }
                        ++numFiles;
                        TraceLog(LOG_INFO, fmt::format("found {}: {:14}{} '{}'", info.digest.to_hex(), preset, name, de.path().string()).c_str());
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
                        const std::string digest = info.digest.to_hex();
                        auto bid = _pimpl->connection->select_query<Select<BinariesTable::field_t<"id">>>().where_one(BinariesTable::field_t<"sha1">().like(digest)).exec();
                        if (bid) {
                            _pimpl->connection->insert_record(DBFilename{.binary_id = *bid, .name = de.path().string()});
                        }
                    }
                }
            }
        }
        catch (const fs::filesystem_error& e) {
            // ...
        }
        catch (const SQLConstraintError& e) {
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
Database::FileInfo Database::scanFile(const std::string& filePath, std::vector<uint8_t>* outData)
{
    auto data = loadFile(filePath);
    auto result = FileInfo{filePath,calculateSha1(data.data(), data.size())};
    if (outData) {
        std::swap(*outData, data);
    }
    return result;
}

const std::string& Database::getBadge(std::string badgeText) const
{
    static const std::string none;
    const auto iter = _badges.find(badgeText);
    return iter != _badges.end() ? iter->second : none;
}

void Database::render(Font& font)
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
        auto area = GetContentAvailable();
        _pimpl->relayoutList(area.width);
        TextBox(_pimpl->queryLine, 4096);
        BeginColumns();
        {
            auto tagsWidth = area.width / 3 - 5;
            auto listWidth = area.width - tagsWidth - 5;
            SetNextWidth(tagsWidth);
            BeginTableView(area.height - 135, 2, &_pimpl->tagsScrollPos);
            TableNextRow(22);
            TableNextColumn(64);
            Label("Huhu");
            EndTableView();
            auto listRect = Rectangle{area.x + tagsWidth + 5, area.y, area.width * 3 / 2, area.height - 135};
            static Vector2 scrollPos{0,0};
            auto [px, py] = GetCurrentPos();
            SetNextWidth(listWidth);
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
                Rectangle itemRect = {program.rect.x + px + cx + scrollPos.x, program.rect.y + py + cy + scrollPos.y, program.rect.width, program.rect.height};
                if (CheckCollisionRecs(listRect, itemRect)) {
                    ++disp;
                    if (disp > maxDisp) {
                        maxDisp = disp;
                    }
                    DrawTextEx(font, fmt::format("{}", program.name.c_str()).c_str(), {itemRect.x, itemRect.y}, 8, 0, lightgrayCol);
                    for (size_t i = 0; i < program.binaries.size(); ++i) {
                        const auto binary = _pimpl->binaries.at(program.binaries[i]);
                        std::string badges = getBadge( binary.preset);
                        if (badges.empty()) {
                            badges = getBadge("???");
                        }
                        DrawTextEx(font, fmt::format("{} {}", _pimpl->binaries[program.binaries[i]].sha1, badges).c_str(), {itemRect.x, itemRect.y + (i+1) * 9}, 8, 0, grayCol);
                    }
                }
                ypos += program.rect.height;
            }
            //DrawRectangleLines(-scrollPos.x, py-scrollPos.y, listRect.width-2, listRect.height-2, BLACK);
            EndScrollPanel();
        }
        EndColumns();
        //auto innerHeight = _pimpl->programs
        //BeginScrollPanel(listRect.height, {0,0,area.width-6, (float)(_core->memSize()/8 + 1) * lineSpacing}, &memScroll);
        auto pos = GetCurrentPos();
        DrawRectangleLines(pos.x + area.width - 130, pos.y, 130, 66, StyleManager::instance().getStyleColor(Style::BORDER_COLOR_NORMAL));
        //DrawText(fmt::format("{}x{}", mx, my).c_str(), area.x + 2, area.y + 2, 8, RED);
    }
}

bool Database::fetchC8PDB()
{
    httplib::Client cli("https://raw.githubusercontent.com");
    cli.enable_server_certificate_verification(false);
    cli.enable_server_hostname_verification(false);
    auto res = cli.Get("/chip-8/chip-8-database/refs/heads/master/database/programs.json");
    res->status;
    res->body;
    return false;
}
