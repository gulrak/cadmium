//---------------------------------------------------------------------------------------
// tools/c8db.hpp
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

#include <c8db/database.hpp>
#include <emulation/chip8options.hpp>
#include <chiplet/utility.hpp>
#include <librarian.hpp>

#include <algorithm>
#include <iostream>
#include <map>
#include <set>

#include <ghc/cli.hpp>
#include <ghc/filesystem.hpp>
#include <fmt/format.h>
#include <magic/magic_enum.hpp>

namespace fs = ghc::filesystem;
using Preset = emu::Chip8EmulatorOptions::SupportedPreset;

static std::map<std::string, Preset> platformPresetMapping {
    {"originalChip8", Preset::eCHIP8},
    {"hybridVIP", Preset::eCHIP8VIP},
    {"modernChip8", Preset::eSCHPC},
    {"chip8x", Preset::eCHIP8XVIP},
    {"chip48", Preset::eCHIP48},
    {"superchip", Preset::eSCHIP11},
    {"superchip1", Preset::eSCHIP10},
    {"megachip8", Preset::eMEGACHIP},
    {"xochip", Preset::eXOCHIP}
};

static bool platformMatchesOptions(const c8db::Platform& platform, const emu::Chip8EmulatorOptions& options)
{
    if (platform.quirkEnabled("shift") == options.optJustShiftVx &&
        platform.quirkEnabled("memoryIncrementByX") == options.optLoadStoreIncIByX &&
        platform.quirkEnabled("memoryLeaveIUnchanged") == options.optLoadStoreDontIncI &&
        platform.quirkEnabled("wrap") == options.optWrapSprites &&
        platform.quirkEnabled("jump") == options.optJump0Bxnn &&
        platform.quirkEnabled("vblank") == !options.optInstantDxyn &&
        platform.quirkEnabled("logic") == !options.optDontResetVf) {
        return true;
    }
    return false;
}

template<typename Iter>
std::string join(Iter first, Iter last, const std::string& delimiter)
{
    std::ostringstream result;
    for (Iter i = first; i != last; ++i) {
        if (i != first) {
            result << delimiter;
        }
        result << *i;
    }
    return result.str();
}


int main(int argc, char* argv[])
{
    fs::u8arguments u8guard(argc, argv);
    ghc::CLI cli(argc, argv);
    std::vector<std::string> files;
    std::map<std::string,std::string> dbRomMap;
    std::string scanDir;
    std::string infoSHA;
    std::string infoFile;
    cli.option({"--scan"}, scanDir, "Scan directory tree for roms, calc sha1 and report unknown ones");
    cli.option({"-s", "--sha1"}, infoSHA, "Lookup rom SHA1 checksum (all lower-case) and give info.");
    cli.option({"-i", "--info"}, infoFile, "Lookup rom file by looking at content and give info.");
    cli.positional(files, "files to convert");
    cli.parse();

    if(files.empty() || !fs::exists(files.front())) {
        std::cerr << "ERROR: No or unexisting directory given." << std::endl;
        exit(EXIT_FAILURE);
    }
    auto dir = fs::path(files.front());
    if(!fs::exists(dir / "platforms.json") || !fs::exists(dir / "programs.json")) {
        std::cerr << "ERROR: platforms.json and/or programs.json not found." << std::endl;
        exit(EXIT_FAILURE);
    }
    c8db::Database db{dir.string()};
    if(!db.numRoms()) {
        std::cerr << "ERROR: Coeldn't load any rom info." << std::endl;
        exit(EXIT_FAILURE);
    }
    if(!infoFile.empty()) {
        if(!fs::exists(infoFile)) {
            std::cerr << "ERROR: File doesn't exist." << std::endl;
        }
        auto data = loadFile(infoFile);
        infoSHA = calculateSha1Hex(data.data(), data.size());
        std::cout << "SHA1: " << infoSHA << std::endl;
    }
    if(!infoSHA.empty()) {
        auto info = db.findProgram(infoSHA);
        if(info.empty()) {
            std::cerr << "ROM could not be found." << std::endl;
            exit(EXIT_FAILURE);
        }
        size_t i = 0;
        const auto& program = info.front().program;
        const auto& rom = info.front().rom;
        std::cout << "Program: " << program.title << std::endl;
        if(program.authors.size()) {
            std::cout << "Authors: " << join(program.authors.begin(), program.authors.end(), ", ") << std::endl;
        }
        if(!program.release.empty()) {
            std::cout << "Release: " << program.release << std::endl;
        }
        if(!info.front().rom.platforms.empty()) {
            std::cout << "Vanilla platforms: " << join(rom.platforms.begin(), rom.platforms.end(), ", ") << std::endl;
        }
        if(!info.front().rom.quirkyPlatforms.empty()) {
            std::cout << "Quirky platforms: ";
            int t = 0;
            for(const auto& [name, quirks] : info.front().rom.quirkyPlatforms) std::cout << (t++?", ":"") << name;
            std::cout << std::endl;
        }
        if(rom.tickrate) {
            std::cout << "Tickrate: " << info.front().rom.tickrate << std::endl;
        }
        if(rom.startAddress != 512) {
            std::cout << "Start address: " << rom.startAddress << std::endl;
        }
        if(!rom.fontStyle.empty()) {
            std::cout << "Font style: " << rom.fontStyle << std::endl;
        }
        if(!rom.colors.pixels.empty()) {
            std::cout << "Pixel colors: ";
            int t = 0;
            for(const auto& col : rom.colors.pixels) std::cout << (t++?", ":"") << static_cast<std::string>(col);
            std::cout << std::endl;
        }
        if(rom.colors.buzzer) {
            std::cout << "Buzzer color: " << static_cast<std::string>(*rom.colors.buzzer) << std::endl;
        }
        if(rom.colors.silence) {
            std::cout << "Silence color: " << static_cast<std::string>(*rom.colors.silence) << std::endl;
        }
        std::cout << "Effective quirks:" << std::endl;
        for(const auto [quirk,val] : info.front().effectiveQuirks) {
            std::cout << "    " << quirk << ": " << (val ? "true" : "false") << std::endl;
        }
        std::cout << std::endl;
        exit(EXIT_SUCCESS);
    }
    std::cout << "Loaded information about " << db.numRoms() << " roms." << std::endl;
    auto platforms = db.platforms();
    std::cout << fmt::format("Found {} platforms.", platforms.size()) << std::endl;
    for(const auto& platform : platforms) {
        bool match = false;
        for(auto preset :  magic_enum::enum_values<emu::Chip8EmulatorOptions::SupportedPreset>()) {
            auto options = emu::Chip8EmulatorOptions::optionsOfPreset(preset);
            if(platformMatchesOptions(platform, options)) {
                std::cout << fmt::format("    {} matches Cadmium preset {}", platform.id, emu::Chip8EmulatorOptions::nameOfPreset(options.behaviorBase)) << std::endl;
                match = true;
            }
        }
        if(!match) {
            std::cout << fmt::format("    {} matches none of Cadmiums presets.", platform.id) << std::endl;
        }
    }
    auto programs = db.programs();
    size_t roms = 0;
    std::for_each(programs.begin(), programs.end(), [&roms, &dbRomMap](const c8db::Program& p){ roms += p.roms.size(); for(const auto&[sha,r] : p.roms) dbRomMap[sha] = r.file; });
    std::cout << fmt::format("Found {} programs with a total of {} roms.", programs.size(), roms) << std::endl;;
    Librarian lib({});
    std::cout << "\nLooking for programs in chip-8 database but not or different in Cadmium..." << std::endl;
    for(const auto& program : programs) {
        for(const auto& [sha1, rom] : program.roms) {
            if(!lib.isKnownFile(sha1)) {
                std::cout << fmt::format("    Unknown rom: {} - \"{}\" file: '{}'", sha1, program.title, rom.file) << std::endl;
            }
            else {
                auto options = lib.getOptionsForFile(sha1);
                if(!rom.quirkyPlatforms.empty()) {
                    const auto& quirks = rom.quirkyPlatforms.begin()->second;
                    auto presetIter = platformPresetMapping.find(rom.quirkyPlatforms.begin()->first);
                    if(presetIter == platformPresetMapping.end()) {
                        std::cerr << fmt::format("Found unknown platform: {}", rom.quirkyPlatforms.begin()->first) << std::endl;
                        break;
                    }
                    auto romOptions = emu::Chip8EmulatorOptions::optionsOfPreset(presetIter->second);
                    for(auto [name, val] : quirks) {
                        if(name == "shift") romOptions.optJustShiftVx = val;
                        else if(name == "memoryIncrementByX") romOptions.optLoadStoreIncIByX = val;
                        else if(name == "memoryLeaveIUnchanged") romOptions.optLoadStoreDontIncI = val;
                        else if(name == "wrap") romOptions.optWrapSprites = val;
                        else if(name == "jump") romOptions.optJump0Bxnn = val;
                        else if(name == "vblank") romOptions.optInstantDxyn = !val;
                        else if(name == "logic") romOptions.optDontResetVf = !val;
                    }
                    if(options.optJustShiftVx != romOptions.optJustShiftVx)
                        std::cerr << fmt::format("        {}: Quirk-Issue: shift quirk differs!", sha1) << std::endl;
                    if(options.optLoadStoreIncIByX != romOptions.optLoadStoreIncIByX)
                        std::cerr << fmt::format("        {}: Quirk-Issue: memoryIncrementByX quirk differs!", sha1) << std::endl;
                    if(options.optLoadStoreDontIncI != romOptions.optLoadStoreDontIncI)
                        std::cerr << fmt::format("        {}: Quirk-Issue: memoryLeaveIUnchanged quirk differs!", sha1) << std::endl;
                    if(options.optWrapSprites != romOptions.optWrapSprites)
                        std::cerr << fmt::format("        {}: Quirk-Issue: wrap quirk differs!", sha1) << std::endl;
                    if(options.optJump0Bxnn != romOptions.optJump0Bxnn)
                        std::cerr << fmt::format("        {}: Quirk-Issue: jump quirk differs!", sha1) << std::endl;
                    if(options.optInstantDxyn != romOptions.optInstantDxyn)
                        std::cerr << fmt::format("        {}: Quirk-Issue: vblank quirk differs!", sha1) << std::endl;
                    if(options.optDontResetVf != romOptions.optDontResetVf)
                        std::cerr << fmt::format("        {}: Quirk-Issue: logic quirk differs!", sha1) << std::endl;
                }
            }
        }
    }
    std::cout << "\nLooking for programs in Cadmium but not or different in chip-8 database..." << std::endl;
    for(size_t i = 0; i < Librarian::numKnownRoms(); ++i) {
        const auto& ri = Librarian::getKnownRoms()[i];
        auto info = db.findProgram(ri.sha1);
        if(info.empty()) {
            std::cout << fmt::format("    Database doesn't know {} - {}", ri.sha1, ri.name) << std::endl;
        }
        else {
            const auto& pi = info.front();
            if(ri.options) {
                auto options = nlohmann::json::parse(ri.options);
                auto opt = lib.getOptionsForFile(ri.sha1);
                if((!pi.rom.tickrate && options.contains("instructionsPerFrame"))) {
                    std::cout << "    " << ri.sha1 << ": database misses tickrate: " << options.at("instructionsPerFrame").get<int>() << " (" << pi.program.title << ")" << std::endl;
                }
                else if(pi.rom.tickrate && pi.rom.tickrate != opt.instructionsPerFrame) {
                    std::cout << "    " << ri.sha1 << ": database has tickrate: " << pi.rom.tickrate << " Cadmium uses " << opt.instructionsPerFrame << " (" << pi.program.title << ")" << std::endl;
                }
                if(pi.rom.colors.pixels.empty() && options.contains("advanced")) {
                    std::cout << "    " << ri.sha1 << ": database has no colors, Cadmium has advanced: " << ri.options << std::endl;
                }
            }
        }
    }
    if(!scanDir.empty()) {
        std::cout << "scanning for unknown programs..." << std::endl;
        std::set<std::string> unknowns;
        static std::set<std::string> validExtensions{ ".ch8", ".ch10", ".hc8", ".c8h", ".c8e", ".c8x", ".sc8", ".mc8", ".xo8" };
        for(auto& entry : fs::recursive_directory_iterator(scanDir, fs::directory_options::skip_permission_denied)) {
            if(entry.is_regular_file() && validExtensions.count(entry.path().extension().string())) {
                auto file = loadFile(entry.path().string());
                auto sha1sum = calculateSha1Hex(file.data(), file.size());
                if(!lib.isKnownFile(sha1sum)) {
                    std::cout << fmt::format("    found program unknown to Cadmium: {} - '{}'", sha1sum, entry.path().string()) << std::endl;
                    if(dbRomMap.count(sha1sum))
                        std::cout << fmt::format("        contained in programs.json as '{}'", dbRomMap[sha1sum]) << std::endl;
                    else
                        unknowns.insert(sha1sum);
                }
                else {
                    const auto* romInfo = Librarian::findKnownRom(sha1sum);
                    if(!romInfo->name || std::string(romInfo->name).empty()) {
                        std::cout << "    found program that is known to Cadmium but has no name: " << sha1sum << " - '" << entry.path().string() << "'" << std::endl;
                    }
                }
            }
        }
        std::cout << "found a total of " << unknowns.size() << " roms that are neither known by Cadmium nor the CHIP-8 program database." << std::endl;
    }
    db.exportPrograms("programs_out.json");
    return 0;
}
