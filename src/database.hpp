//---------------------------------------------------------------------------------------
// src/database.hpp
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
#pragma once

#include <string>
#include <vector>

#include <configuration.hpp>
#include <emulation/coreregistry.hpp>
#include <threadpool.hpp>

#include <chiplet/sha1.hpp>
#include <raylib.h>

class Database
{
public:
    struct FileInfo
    {
        std::string filePath;
        Sha1::Digest digest;
    };
    struct Program
    {
        std::string name;
        emu::Properties properties;
        std::vector<uint8_t> data; // REVIEW: was a span onto a temporary vector built in render(), so it dangled immediately
    };
    struct BadgeInfo
    {
        enum Type { SMART_TAG, PRESET, GENERIC, UNDEFINED, NEW_TAG, USER_TAG};
        Type type;
        std::string text;
        Color textCol{};
        Color badgeCol{};
    };
    explicit Database(const emu::CoreRegistry& registry, CadmiumConfiguration& configuration, ThreadPool& threadPool, const std::string& path);
    ~Database();
    int scanLibrary();
    static ghc::expected<FileInfo,LoadError> scanFile(const std::string& filePath, std::vector<uint8_t>* outData = nullptr);
    std::optional<Program> getSelectedProgram() const;
    enum class Action { NONE, LOAD, RUN };
    Action render(Font& font); // LOAD or RUN if getSelectedProgram() should be loaded (and run)
    bool fetchC8PDB();
    bool containsBinary(const Sha1::Digest& sha1) const;
    std::string storeSettings(const Sha1::Digest& sha1, const emu::Properties& properties);
    std::string addFiles(const std::vector<std::string>& paths);
    // Compares the library with programs.json of the chip-8-database (https://github.com/chip-8/chip-8-database,
    // platforms.json is expected next to it), reports where they disagree on the variants and settings of ROMs,
    // and writes programs_<timestamp>.json with the library ROMs missing in it added, used by --update-program-database.
    static int updateProgramDatabase(const std::string& dataPath, const std::string& programsJsonPath);
    // Writes a copy of the library with only its metadata (no ROMs, file names, ignored ROMs or "new" tags),
    // marked as starter metadata, used by --export-metadata-db to create the starter library of the resources.
    static int exportMetadata(const std::string& dataPath, const std::string& outputFile);
    // Prints the opcode statistics of ROM files as JSON, analyzed like the library does with the first config the
    // library (if there is one) or the compiled in metadata has for them, used by --opcode-stats to put the
    // statistics into the starter library.
    static int printOpcodeStatistics(const std::string& dataPath, const std::vector<std::string>& files);
private:
    void refreshBadges();
    void fetchProgramInfo();
    void updatePreview();
    static Vector2 drawBadge(Font& font, std::string_view text, Vector2 pos, Color textCol, Color badgeCol);
    const std::string& getBadge(std::string badgeText) const;
    const emu::CoreRegistry& _registry;
    ThreadPool& _threadPool;
    CadmiumConfiguration& _configuration;
    std::unordered_map<std::string, BadgeInfo> _badges;
    std::future<int> _scanResult;
    std::set<Sha1::Digest> _digests;
    std::chrono::steady_clock::duration durationOfLastJob{};
    struct Private;
    std::unique_ptr<Private> _pimpl;
    std::optional<Program> _selectedProgram;
};

