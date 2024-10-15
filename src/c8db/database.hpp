//---------------------------------------------------------------------------------------
// src/c8db/database.hpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2023, Steffen Schümann <s.schuemann@pobox.com>
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

#include <cstdint>
#include <system_error>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <sstream>
#include <unordered_map>
#include <vector>

#include <iostream>

#include <nlohmann/json.hpp>

namespace c8db {
using json = nlohmann::json;

enum class DatabaseError {
    OK,
    FileError,
    ParsingError
};

struct DatabaseErrorCategory : std::error_category
{
    const char* name() const noexcept override
    {
        return "database error";
    }
    std::string message(int ev) const override
    {
        using namespace std::string_literals;
        switch (static_cast<DatabaseError>(ev)) {
            case DatabaseError::OK: return "Ok"s;
            case DatabaseError::FileError: return "File error"s;
            case DatabaseError::ParsingError: return "Parsing error"s;
        }
        std::abort();
    }
};

}

template<>
struct std::is_error_code_enum<c8db::DatabaseError> : std::true_type{};

namespace c8db {

inline std::error_code make_error_code(DatabaseError e)
{
    return {static_cast<int>(e), c8db::DatabaseErrorCategory()};
}

enum class OriginType {
    UNKNOWN, GAMEJAM, EVENT, MAGAZINE, MANUAL
};

NLOHMANN_JSON_SERIALIZE_ENUM(OriginType,
{
    { OriginType::UNKNOWN, nullptr },
    { OriginType::GAMEJAM, "gamejam" },
    { OriginType::EVENT, "event" },
    { OriginType::MAGAZINE, "magazine" },
    { OriginType::MANUAL, "manual" }
})

struct Origin {
    OriginType type{OriginType::UNKNOWN};
    std::string reference;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Origin, type, reference)

enum class ScreenRotation {
    NONE=0, CW0=NONE, CW90, CW180, CW270
};

inline void to_json(json& val, const ScreenRotation& rot)
{
    val = static_cast<int>(rot) * 90;
}

inline void to_json(nlohmann::ordered_json& val, const ScreenRotation& rot)
{
    val = static_cast<int>(rot) * 90;
}

inline void from_json(const json& val, ScreenRotation& rot)
{
    if(val.is_number_integer()) {
        const auto r = val.get<int>();
        rot = r == 90 ? ScreenRotation::CW90 : r == 180 ? ScreenRotation::CW180 : r == 270 ? ScreenRotation::CW270 : ScreenRotation::CW0;
    }
}

enum class TouchInputMode {
    UNKNOWN, NONE, SWIPE, SEG16, SEG16FILL, GAMEPAD, VIP
};

NLOHMANN_JSON_SERIALIZE_ENUM(TouchInputMode,
{
    { TouchInputMode::UNKNOWN, nullptr},
    { TouchInputMode::NONE, "none" },
    { TouchInputMode::SWIPE, "swipe" },
    { TouchInputMode::SEG16, "seg16" },
    { TouchInputMode::SEG16FILL, "seg16fill" },
    { TouchInputMode::GAMEPAD, "gamepad" },
    { TouchInputMode::VIP, "vip" }
})

struct Color {
    uint8_t r{}, g{}, b{};
    explicit operator std::string() const
    {
        std::ostringstream os;
        os << "#" << std::setfill('0') << std::setw(2) << std::hex << (unsigned)r
           << std::setfill('0') << std::setw(2) << std::hex << (unsigned)g
           << std::setfill('0') << std::setw(2) << std::hex << (unsigned)b;
        return os.str();
    }
};

inline void to_json(json& val, const Color& col)
{
    val = static_cast<std::string>(col);
}

inline void from_json(const json& val, Color& col)
{
    if(val.is_string()) {
        auto str = val.get<std::string>();
        if(str.length()>1 && str[0] == '#') {
            auto c = std::strtoul(str.data() + 1, nullptr, 16);
            col.r = c >> 16;
            col.g = c >> 8;
            col.b = c;
            return;
        }
    }
    col = {0, 0, 0};
}

inline void to_json(json& val, const std::optional<Color>& col)
{
    if(col)
        to_json(val, *col);
    val = nullptr;
}

inline void from_json(const json& val, std::optional<Color>& col)
{
    Color c;
    from_json(val, c);
    col = c;
}


struct Resolution {
    unsigned width;
    unsigned height;
};

inline void to_json(json& val, const Resolution& res)
{
    val = std::to_string(res.width) + "x" + std::to_string(res.height);
}

inline void from_json(const json& val, Resolution& res)
{
    std::string str = val;
    auto p = str.find('x');
    if(p != std::string::npos) {
        res.width = std::stoul(str.substr(0, p));
        res.height = std::stoul(str.substr(p+1));
    }
    else {
        res = {0,0};
    }
}

using QuirkMap = std::map<std::string,bool>;

struct Platform {
    std::string id;
    std::string name;
    std::string description;
    std::string release;
    std::vector<std::string> authors;
    std::vector<std::string> urls;
    std::string copyright;
    std::string license;
    std::vector<Resolution> displayResolutions;
    uint32_t defaultTickrate{};
    QuirkMap quirks;

    bool quirkEnabled(const std::string& name) const { auto iter = quirks.find(name); return iter != quirks.end() ? iter->second : false; }
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Platform, id, name, description, release, authors, urls, copyright, license, displayResolutions, defaultTickrate, quirks)

struct ColorDef {
    std::vector<Color> pixels;
    std::optional<Color> buzzer;
    std::optional<Color> silence;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ColorDef, pixels, buzzer, silence)

struct Program {
    struct Rom {
        std::string file;
        std::string embeddedTitle;
        std::string description;
        std::string release;
        std::vector<std::string> platforms;
        using QuirkMap = std::map<std::string,bool>;
        std::map<std::string, QuirkMap> quirkyPlatforms;
        std::vector<std::string> authors;
        std::vector<std::string> images;
        std::vector<std::string> urls;
        uint32_t tickrate{};
        uint32_t startAddress{0x200};
        ScreenRotation screenRotation{ScreenRotation::NONE};
        std::map<std::string,uint8_t> keys;
        TouchInputMode touchInputMode{TouchInputMode::UNKNOWN};
        std::string fontStyle;
        ColorDef colors;
    };
    std::string title;
    Origin origin{};
    std::string description;
    std::string release;
    std::string copyright;
    std::string license;
    std::vector<std::string> images;
    std::vector<std::string> urls;
    std::vector<std::string> authors;
    std::map<std::string,Rom> roms;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Program::Rom, file, embeddedTitle, description, release, platforms, quirkyPlatforms, authors, images, urls, tickrate, startAddress, screenRotation, keys, touchInputMode, fontStyle, colors)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Program, title, origin, description, release, copyright, license, images, urls, authors, roms)

class Database
{
public:
    struct RomInfo
    {
        const Platform& platform;
        const Program& program;
        const Program::Rom& rom;
        QuirkMap effectiveQuirks;
        RomInfo(const Platform& plat, const Program& prog, const Program::Rom& rom, QuirkMap quirks) : platform(plat), program(prog), rom(rom), effectiveQuirks(quirks) {}
        bool quirkEnabled(const std::string& name) const { auto iter = effectiveQuirks.find(name); return iter != effectiveQuirks.end() ? iter->second : false; }
    };

    explicit Database(std::string directory, std::string platformsFile = "platforms.json", std::string programsFile = "programs.json")
    : dbDir(std::move(directory))
    , platformsFile(platformsFile)
    , programsFile(programsFile)
    {
        platformList = readPlatforms(dbDir + "/" + platformsFile);
        programList= readPrograms(dbDir + "/" + programsFile);
        std::for_each(programList.begin(), programList.end(), [this](c8db::Program& p) {
            for(auto&[sha,r] : p.roms)
                romLookupTable[sha] = &p;
        });
    }

    size_t numRoms() const { return romLookupTable.size(); }
    const std::unordered_map<std::string,Program*>& romTable() const { return romLookupTable; }
    const auto& platforms() const { return platformList; }
    const auto& programs() const { return programList; }
    const Platform* findPlatform(const std::string& name) const
    {
        auto iter = std::find_if(platformList.begin(), platformList.end(), [&](const auto& p) { return p.id == name; });
        return iter != platformList.end() ? &*iter : nullptr;
    }

    std::vector<RomInfo> findProgram(const std::string& sha1sum) const
    {
        auto iter = romLookupTable.find(sha1sum);
        std::vector<RomInfo> info;
        if(iter != romLookupTable.end()) {
            const auto& program = *iter->second;
            for(const auto& [sha,rom] : program.roms) {
                if(sha1sum == sha) {
                    for(const auto& platform : rom.platforms) {
                        const auto* plat = findPlatform(platform);
                        if(plat) {
                            info.emplace_back(RomInfo(*plat, program, rom, {plat->quirks}));
                        }
                    }
                    for(const auto& [platform,quirks] : rom.quirkyPlatforms) {
                        const auto* plat = findPlatform(platform);
                        if(plat) {
                            auto effectiveQuirks = quirks;
                            effectiveQuirks.merge(QuirkMap(plat->quirks));
                            info.emplace_back(RomInfo(*plat, program, rom, effectiveQuirks));
                        }
                    }
                }
            }
        }
        return info;
    }

    bool exportPrograms(const std::string& outputFilePath)
    {
        return writePrograms(outputFilePath, programList);
    }

private:
    std::vector<Platform> readPlatforms(const std::string& filepath)
    {
        static const char* platformsFallback = R"(
            [
              {"id": "originalChip8", "name": "Cosmac VIP CHIP-8", "defaultTickrate": 15, "quirks": {"shift": false, "memoryIncrementByX": false, "memoryLeaveIUnchanged": false, "wrap": false, "jump": false, "vblank": true, "logic": true}},
              {"id": "hybridVIP", "name": "CHIP-8 with Cosmac VIP instructions", "defaultTickrate": 15, "quirks": {"shift": false, "memoryIncrementByX": false, "memoryLeaveIUnchanged": false, "wrap": false, "jump": false, "vblank": true, "logic": true}},
              {"id": "modernChip8", "name": "Modern CHIP-8", "defaultTickrate": 12, "quirks": {"shift": false, "memoryIncrementByX": false, "memoryLeaveIUnchanged": false, "wrap": false, "jump": false, "vblank": false, "logic": false}},
              {"id": "chip8x", "name": "CHIP-8X", "defaultTickrate": 15, "quirks": {"shift": false, "memoryIncrementByX": false, "memoryLeaveIUnchanged": false, "wrap": false, "jump": false, "vblank": true, "logic": true}},
              {"id": "chip48", "name": "CHIP48 for the HP48", "defaultTickrate": 30, "quirks": {"shift": true, "memoryIncrementByX": true, "memoryLeaveIUnchanged": false, "wrap": false, "jump": true, "vblank": false, "logic": false}},
              {"id": "superchip1", "name": "Superchip 1.0", "defaultTickrate": 30, "quirks": {"shift": true, "memoryIncrementByX": true, "memoryLeaveIUnchanged": false, "wrap": false, "jump": true, "vblank": false, "logic": false}},
              {"id": "superchip", "name": "Superchip 1.1", "defaultTickrate": 30, "quirks": {"shift": true, "memoryLeaveIUnchanged": true, "wrap": false, "jump": true, "vblank": false, "logic": false}},
              {"id": "megachip8", "name": "MEGA-CHIP", "defaultTickrate": 1000, "quirks": {"shift": true, "memoryLeaveIUnchanged": true, "wrap": false, "jump": true, "vblank": false, "logic": false}},
              {"id": "xochip", "name": "XO-CHIP", "defaultTickrate": 100, "quirks": {"shift": false, "memoryIncrementByX": false, "memoryLeaveIUnchanged": false, "wrap": true, "jump": false, "vblank": false, "logic": false}}
            ])";
        std::vector<Platform> result;
        std::ifstream ifs(filepath);
        try {
            auto j = ifs.fail() ? json::parse(std::istringstream(platformsFallback)) : json::parse(ifs);
            if(j.is_array()) {
                for(const auto& pObj : j) {
                    Platform p;
                    from_json(pObj, p);
                    result.push_back(p);
                }
            }
        }
        catch(...) {
            return result;
        }
        return result;
    }

    bool readRom(const json& obj, Program::Rom& rom)
    {
        try {
            from_json(obj, rom);
        }
        catch (...) {
            return false;
        }
        return true;
    }

    void to_json_ordered(nlohmann::ordered_json& j, const Program::Rom& rom)
    {
        if(!rom.file.empty()) j["file"] = rom.file;
        if(!rom.embeddedTitle.empty()) j["embeddedTitle"] = rom.embeddedTitle;
        if(!rom.description.empty()) j["description"] = rom.description;
        if(!rom.release.empty()) j["release"] = rom.release;
        j["platforms"] = rom.platforms;
        if(!rom.quirkyPlatforms.empty()) j["quirkyPlatforms"] = rom.quirkyPlatforms;
        if(!rom.authors.empty()) j["authors"] = rom.authors;
        if(!rom.images.empty()) j["images"] = rom.images;
        if(!rom.urls.empty()) j["urls"] = rom.urls;
        if(rom.tickrate) j["tickrate"] = rom.tickrate;
        if(rom.startAddress != 512) j["startAddress"] = rom.startAddress;
        if(rom.screenRotation != ScreenRotation::NONE) j["screenRotation"] = rom.screenRotation;
        if(!rom.keys.empty()) j["keys"] = rom.keys;
        if(rom.touchInputMode != TouchInputMode::UNKNOWN) j["touchInputMode"] = rom.touchInputMode;
        if(!rom.fontStyle.empty()) j["fontStyle"] = rom.fontStyle;
        if(!rom.colors.pixels.empty()) j["colors"]["pixels"] = json(rom.colors.pixels);
        if(rom.colors.buzzer) j["colors"]["buzzer"] = json(*rom.colors.buzzer);
        if(rom.colors.silence) j["colors"]["silence"] = json(*rom.colors.silence);
    }

    void to_json_ordered(nlohmann::ordered_json& j, const Program& prg)
    {
        j["title"] = prg.title;
        if(prg.origin.type != OriginType::UNKNOWN) j["origin"] = json(prg.origin);
        if(!prg.description.empty()) j["description"] = prg.description;
        if(!prg.release.empty()) j["release"] = prg.release;
        if(!prg.copyright.empty()) j["copyright"] = prg.copyright;
        if(!prg.license.empty()) j["license"] = prg.license;
        if(!prg.authors.empty()) j["authors"] = prg.authors;
        if(!prg.images.empty()) j["images"] = prg.images;
        if(!prg.urls.empty()) j["urls"] = prg.urls;
        j["roms"] = {};
        for(const auto& [sha,r] : prg.roms) {
            nlohmann::ordered_json robj;
            to_json_ordered(robj, r);
            j["roms"][sha] = robj;
        }
    }

    std::vector<Program> readPrograms(const std::string& filepath)
    {
        std::vector<Program> result;
        std::ifstream ifs(filepath);
        if(ifs.fail())
            return result;
        try {
            auto j = json::parse(ifs);
            if(j.is_array()) {
                for(const auto& pObj : j) {
                    Program p;
                    from_json(pObj, p);
                    result.push_back(p);
                }
            }
        }
        catch (const json::parse_error& ex) {
            std::cerr << "EXCEPTION: " << ex.what() << std::endl;
        }
        catch (std::exception& ex) {
            std::cerr << "EXCEPTION: " << ex.what() << std::endl;
            return result;
        }
        return result;
    }

    bool writePrograms(const std::string& filepath, const std::vector<Program>& programs)
    {
        std::ofstream ofs(filepath);
        if(ofs.fail())
            return false;

        auto j = nlohmann::ordered_json::array();
        for(const auto& prog : programs) {
            nlohmann::ordered_json obj;
            to_json_ordered(obj, prog);
            j.push_back(obj);
        }
        ofs << j.dump(2);
        return true;
    }

private:
    std::string dbDir;
    std::string platformsFile;
    std::string programsFile;
    std::vector<Platform> platformList;
    std::vector<Program> programList;
    std::unordered_map<std::string,Program*> romLookupTable;
};

}
