#include <doctest/doctest.h>
#include <emuhostex.hpp>
#include <configuration.hpp>
#include <librarian.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace {

// Records what the host hands to the editor
class SourceTestHost : public emu::HeadlessHost
{
public:
    using HeadlessHost::HeadlessHost;
    using EmuHostEx::nativeSourceLanguage;
    using EmuHostEx::isProgramTarget;
    void whenRomLoaded(const std::string& filename, bool, emu::OctoCompiler* compiler, const std::string& source) override
    {
        loadedName = filename;
        hadCompiler = compiler != nullptr;
        loadedSource = source;
        ++loads;
    }
    std::string loadedName;
    std::string loadedSource;
    bool hadCompiler{false};
    int loads{0};
};

std::vector<uint8_t> bytesOf(std::string_view text)
{
    return {text.begin(), text.end()};
}

const char* const BLINK = R"(.. blink Q
        ORG #0010
START:  SEQ
        LDI #20
        PLO R7
WAIT:   DEC R7
        GLO R7
        BNZ WAIT
        REQ
        BR START
)";

}  // namespace

TEST_CASE("A CDP1802 source is assembled and loaded on a machine without CHIP-8 interpreter")
{
    SourceTestHost host(emu::CoreRegistry::propertiesForPreset("chip-8"));
    CHECK(host.nativeSourceLanguage() == emu::SourceLanguage::OCTO);
    const auto data = bytesOf(BLINK);
    REQUIRE(host.loadBinary("/tmp/blink.asm", data, emu::EmuHostEx::LoadOptions::None));
    // the raw VIP was chosen, the program is at its ORG
    CHECK(host.nativeSourceLanguage() == emu::SourceLanguage::CDP1802);
    auto* memory = host.emuCore().memory();
    REQUIRE(memory);
    CHECK(memory[0x10] == 0x7B);  // SEQ
    CHECK(memory[0x11] == 0xF8);  // LDI #20
    CHECK(memory[0x12] == 0x20);
    // the editor gets the source without an Octo compiler
    CHECK(host.loads == 1);
    CHECK(host.loadedName == "/tmp/blink.asm");
    CHECK_FALSE(host.hadCompiler);
    CHECK(host.loadedSource == BLINK);

    // on a CDP1802 machine it stays
    SourceTestHost studio(emu::CoreRegistry::propertiesForPreset("vip-none"));
    REQUIRE(studio.nativeSourceLanguage() == emu::SourceLanguage::CDP1802);
    REQUIRE(studio.loadBinary("blink.asm", data, emu::EmuHostEx::LoadOptions::None));
    CHECK(studio.emuCore().memory()[0x13] == 0xA7);  // PLO R7
}

TEST_CASE("A source the host can't assemble only goes to the editor")
{
    SourceTestHost host(emu::CoreRegistry::propertiesForPreset("chip-8"));
    // a CDP1802 source with an error
    const auto broken = bytesOf("        ORG #0010\n        SEQ\n        FOO R7\n        GHI R1\n");
    REQUIRE(host.loadBinary("broken.asm", broken, emu::EmuHostEx::LoadOptions::None));
    CHECK(host.loads == 1);
    CHECK_FALSE(host.hadCompiler);
    CHECK(host.nativeSourceLanguage() == emu::SourceLanguage::OCTO);
    // a 6800 source
    const auto m6800 = bytesOf("START   LDAA    #$10\n        STAA    $20\n        JSR     $C000\n        RTS\n");
    REQUIRE(host.loadBinary("count.asm", m6800, emu::EmuHostEx::LoadOptions::None));
    CHECK(host.loads == 2);
    CHECK(host.loadedName == "count.asm");
    CHECK(host.nativeSourceLanguage() == emu::SourceLanguage::OCTO);
}

TEST_CASE("Programs with a program extension are no sources")
{
    SourceTestHost host(emu::CoreRegistry::propertiesForPreset("chip-8"));
    // a CHIP-8 program without zero bytes that happens to be text
    const auto data = bytesOf("GHI R1\nPLO R2\nSEX R2\nLDI 1\n");
    REQUIRE(host.loadBinary("text.ch8", data, emu::EmuHostEx::LoadOptions::DontChangeOptions));
    CHECK(host.loadedSource.empty());
    CHECK(host.nativeSourceLanguage() == emu::SourceLanguage::OCTO);
}

TEST_CASE("A CDP1802 source loaded on the raw ETI-660 is called at its ORG")
{
    SourceTestHost host(emu::CoreRegistry::propertiesForPreset("eti-none"));
    REQUIRE(host.nativeSourceLanguage() == emu::SourceLanguage::CDP1802);
    const auto data = bytesOf("        ORG #0700\n        LDI #0E\n        PHI RF\n        LDI #80\n        PLO RF\n        LDI #2A\n        STR RF\nSTAY:   BR STAY\n");
    REQUIRE(host.loadBinary("store.asm", data, emu::EmuHostEx::LoadOptions::None));
    CHECK(host.getProperties()["codeStart"].getInt() == 0x700);
    CHECK(host.nativeSourceLanguage() == emu::SourceLanguage::CDP1802);
    for (int frame = 0; frame < 10; ++frame)
        host.emuCore().executeFrame();
    CHECK(host.emuCore().memory()[0xE80] == 0x2A);
}

#ifdef CADMIUM_WITH_HP48
namespace {

// A code object that returns to RPL at once, with the binary header like chip-48.asap writes it
const char* const RETURN_OBJECT = R"(; returns to RPL
        data.b 'H'
        data.b 'P'
        data.b 'H'
        data.b 'P'
        data.b '4'
        data.b '8'
        data.b '-'
        data.b 'A'
        data.a #2dcc
begin:  data.a end-begin
        move.a @d0,a
        add.a 5,d0
        jump.a @a
end:
)";

}  // namespace

TEST_CASE("A Saturn source goes as an HP object onto the raw HP-48SX")
{
    SourceTestHost host(emu::CoreRegistry::propertiesForPreset("chip-8"));
    const auto data = bytesOf(RETURN_OBJECT);
    REQUIRE(host.loadBinary("return.asap", data, emu::EmuHostEx::LoadOptions::None));
    CHECK(host.nativeSourceLanguage() == emu::SourceLanguage::SATURN);
    CHECK(host.emuCore().errorMessage().empty());
    CHECK(host.loads == 1);
    CHECK_FALSE(host.hadCompiler);
    CHECK(host.loadedSource == RETURN_OBJECT);

    // a Saturn source with an error only goes into the editor
    SourceTestHost other(emu::CoreRegistry::propertiesForPreset("chip-8"));
    const auto broken = bytesOf("        move.a  @d0,a\n        frobnicate.a c\n");
    REQUIRE(other.loadBinary("broken.asap", broken, emu::EmuHostEx::LoadOptions::None));
    CHECK(other.loads == 1);
    CHECK(other.nativeSourceLanguage() == emu::SourceLanguage::OCTO);
}
#endif

TEST_CASE("A machine with the CPU that can't load programs is no target")
{
    // the Studio II runs a CDP1802 without CHIP-8 interpreter, but can't load programs yet
    SourceTestHost host(emu::CoreRegistry::propertiesForPreset("studio-2-builtin"));
    CHECK(host.nativeSourceLanguage() == emu::SourceLanguage::CDP1802);
    CHECK_FALSE(host.isProgramTarget(emu::SourceLanguage::CDP1802));
    // a CDP1802 source goes to the raw VIP instead
    REQUIRE(host.loadBinary("blink.asm", bytesOf(BLINK), emu::EmuHostEx::LoadOptions::None));
    CHECK(host.isProgramTarget(emu::SourceLanguage::CDP1802));
    CHECK(host.emuCore().memory()[0x10] == 0x7B);  // SEQ at the ORG
    // the raw VIP is a target, a CHIP-8 machine isn't
    SourceTestHost chip8(emu::CoreRegistry::propertiesForPreset("chip-8"));
    CHECK_FALSE(chip8.isProgramTarget(emu::SourceLanguage::CDP1802));
}

TEST_CASE("The Load view lists assembly sources as sources")
{
    const auto stamp = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    std::filesystem::path folder;
    for (int attempt = 0;; ++attempt) {
        folder = std::filesystem::temp_directory_path() / ("cadmium-librarian-source-test-" + stamp + "-" + std::to_string(attempt));
        if (std::filesystem::create_directory(folder))
            break;
    }
    auto write = [&](const char* name, std::string_view text) { std::ofstream(folder / name, std::ios::binary) << text; };
    write("blink.asm", BLINK);
    write("count.asm", "START   LDAA    #$10\n        STAA    $20\n        JSR     $C000\n        RTS\n");
    write("prog.asap", "; anything, the extension tells\n");
    write("notes.txt", "just some notes\n");
    write("data.raw", std::string("\x00\x01\x02", 3));

    CadmiumConfiguration config;
    Librarian librarian(config);
    REQUIRE(librarian.fetchDir(folder.string()));
    const auto props = emu::CoreRegistry::propertiesForPreset("chip-8");
    while (librarian.update(props)) {}
    auto entry = [&](const std::string& name) {
        for (size_t index = 0; index < librarian.numEntries(); ++index) {
            if (librarian.getInfo(index).filePath == name)
                return librarian.getInfo(index);
        }
        FAIL("not listed: " << name);
        return Librarian::Info{};
    };
    CHECK(entry("blink.asm").type == Librarian::Info::eASSEMBLY_SOURCE);
    CHECK(entry("blink.asm").variant == "CDP1802");
    CHECK(entry("count.asm").type == Librarian::Info::eASSEMBLY_SOURCE);
    CHECK(entry("count.asm").variant == "M6800");
    CHECK(entry("prog.asap").type == Librarian::Info::eASSEMBLY_SOURCE);
    CHECK(entry("prog.asap").variant == "Saturn");
    CHECK(entry("notes.txt").type == Librarian::Info::eUNKNOWN_FILE);
    CHECK(entry("data.raw").type != Librarian::Info::eASSEMBLY_SOURCE);
    std::error_code ec;
    std::filesystem::remove_all(folder, ec);
}
