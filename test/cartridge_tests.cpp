//
// Saving Octo cartridges from the host and loading them back.
//
#include <doctest/doctest.h>
#include <emuhostex.hpp>
#include <configuration.hpp>
#include <chiplet/octocartridge.hpp>
#include <chiplet/utility.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

class CartTestHost : public emu::HeadlessHost
{
public:
    using HeadlessHost::HeadlessHost;
    const std::vector<uint8_t>& romImage() const { return _romImage; }
};

struct CartFixture
{
    CartFixture()
        : directory(std::filesystem::temp_directory_path() / ("cadmium-cart-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())))
    {
        std::filesystem::create_directory(directory);
    }
    ~CartFixture() { std::filesystem::remove_all(directory); }
    std::string path(const std::string& name) const { return (directory / name).string(); }
    std::filesystem::path directory;
};

const std::string PROGRAM = ": main\n  v0 := 1\n  loop again\n";

emu::OctoOptions allDifferentOptions()
{
    emu::OctoOptions options;
    options.tickrate = 1000;
    options.maxRom = 65024;
    options.rotation = 270;
    options.font = emu::OctoOptions::Font::Eti660;
    options.touchMode = emu::OctoOptions::Touch::GamePad;
    options.colors = {0x112233, 0x445566, 0x778899, 0xAABBCC, 0xDDEEFF, 0x010203};
    options.qShift = options.qJump0 = options.qLogic = options.qClip = options.qVBlank = true;
    options.qLoadStore = true;
    return options;
}

void checkSameOptions(const emu::OctoOptions& result, const emu::OctoOptions& expected)
{
    CHECK(result.tickrate == expected.tickrate);
    CHECK(result.maxRom == expected.maxRom);
    CHECK(result.rotation == expected.rotation);
    CHECK(result.font == expected.font);
    CHECK(result.touchMode == expected.touchMode);
    CHECK(result.colors == expected.colors);
    CHECK(result.qShift == expected.qShift);
    CHECK(result.qLoadStore == expected.qLoadStore);
    CHECK(result.qJump0 == expected.qJump0);
    CHECK(result.qLogic == expected.qLogic);
    CHECK(result.qClip == expected.qClip);
    CHECK(result.qVBlank == expected.qVBlank);
}

}  // namespace

TEST_SUITE("Cartridge")
{
    TEST_CASE("options survive the mapping to properties and back")
    {
        const auto options = allDifferentOptions();
        checkSameOptions(emu::CoreRegistry::octoOptionsFromProperties(emu::CoreRegistry::propertiesFromOctoOptions(options)), options);
        emu::OctoOptions defaults;
        defaults.maxRom = 65024;  // loading always uses a 64k XO-CHIP setup
        checkSameOptions(emu::CoreRegistry::octoOptionsFromProperties(emu::CoreRegistry::propertiesFromOctoOptions(defaults)), defaults);
    }

    TEST_CASE("each font and touch mode survives")
    {
        using Font = emu::OctoOptions::Font;
        using Touch = emu::OctoOptions::Touch;
        for (auto font : {Font::Octo, Font::Vip, Font::Dream6800, Font::Eti660, Font::SChip, Font::FishNChips}) {
            emu::OctoOptions options;
            options.font = font;
            CHECK(emu::CoreRegistry::octoOptionsFromProperties(emu::CoreRegistry::propertiesFromOctoOptions(options)).font == font);
        }
        for (auto touch : {Touch::None, Touch::Swipe, Touch::Seg16, Touch::Seg16Fill, Touch::GamePad, Touch::Vip}) {
            emu::OctoOptions options;
            options.touchMode = touch;
            CHECK(emu::CoreRegistry::octoOptionsFromProperties(emu::CoreRegistry::propertiesFromOctoOptions(options)).touchMode == touch);
        }
    }

    TEST_CASE("presets map to Octo quirks")
    {
        auto chip8 = emu::CoreRegistry::octoOptionsFromProperties(emu::CoreRegistry::propertiesForPreset("chip-8"));
        CHECK(chip8.maxRom == 3232);
        CHECK(chip8.qLogic);
        CHECK(chip8.qVBlank);
        CHECK_FALSE(chip8.qShift);
        CHECK(chip8.touchMode == emu::OctoOptions::Touch::None);
        CHECK(chip8.font == emu::OctoOptions::Font::Vip);
        auto xo = emu::CoreRegistry::octoOptionsFromProperties(emu::CoreRegistry::propertiesForPreset("xo-chip"));
        CHECK(xo.maxRom == 65024);
        CHECK_FALSE(xo.qClip);
        CHECK_FALSE(xo.qVBlank);
        CHECK(xo.touchMode == emu::OctoOptions::Touch::None);
        CHECK(xo.font == emu::OctoOptions::Font::Octo);
    }

    TEST_CASE("only variants a cartridge can represent are accepted")
    {
        for (const char* preset : {"chip-8", "chip-48", "schip-1.0", "schip-1.1", "schipc", "xo-chip"}) {
            CAPTURE(preset);
            CHECK(emu::CoreRegistry::supportsCartridge(emu::CoreRegistry::propertiesForPreset(preset)));
        }
        for (const char* preset : {"chip-8e", "chip-8x", "chip-10", "megachip", "vip-chip-8"}) {
            CAPTURE(preset);
            CHECK_FALSE(emu::CoreRegistry::supportsCartridge(emu::CoreRegistry::propertiesForPreset(preset)));
        }
    }

    TEST_CASE("a saved cartridge loads with same program and settings")
    {
        CartFixture fixture;
        const auto file = fixture.path("test.gif");
        auto props = emu::CoreRegistry::propertiesFromOctoOptions(allDifferentOptions());
        CartTestHost host(props);
        REQUIRE(host.saveCartridge(file, PROGRAM, "", {}, "TEST", false).empty());

        emu::OctoCartridge cart(file);
        REQUIRE(cart.loadCartridge());
        CHECK(cart.getSource() == PROGRAM);
        checkSameOptions(cart.getOptions(), allDifferentOptions());

        CartTestHost second(emu::CoreRegistry::propertiesForPreset("chip-8"));
        REQUIRE(second.loadRom(file, emu::EmuHostEx::LoadOptions::None));
        CHECK(second.getProperties().palette().borderColor->toRGBInt() == 0xDDEEFF);
        CHECK(second.getProperties().palette().signalColor->toRGBInt() == 0x010203);
        CHECK(second.getProperties().palette().colors[1].toRGBInt() == 0x445566);
        CHECK(second.getProperties()["screenRotation"].getSelectedIndex() == 3);
        CHECK(second.emuCore().screenRotation() == 270);
        CHECK(second.romImage() == std::vector<uint8_t>{0x60, 0x01, 0x12, 0x02});

        const auto again = fixture.path("again.gif");
        REQUIRE(second.saveCartridge(again, PROGRAM, "", second.romImage(), "AGAIN", true).empty());
        emu::OctoCartridge reloaded(again);
        REQUIRE(reloaded.loadCartridge());
        CHECK(reloaded.getSource() == PROGRAM);
        checkSameOptions(reloaded.getOptions(), allDifferentOptions());
    }

    TEST_CASE("a palette of 16 colors survives the mapping to properties and back")
    {
        auto options = allDifferentOptions();
        for (uint32_t i = 0; i < 16; ++i)
            options.palette.push_back(0x0F0E0D * i);
        const auto props = emu::CoreRegistry::propertiesFromOctoOptions(options);
        REQUIRE(props.palette().colors.size() == 16);
        CHECK(props.palette().colors[0].toRGBInt() == 0);
        CHECK(props.palette().colors[15].toRGBInt() == 0x0F0E0D * 15);
        CHECK(emu::CoreRegistry::octoOptionsFromProperties(props).palette == options.palette);
    }

    TEST_CASE("a saved cartridge keeps its own colors for four planes, but not the default ones")
    {
        CartFixture fixture;
        auto options = allDifferentOptions();
        for (uint32_t i = 0; i < 16; ++i)
            options.palette.push_back(0x100000 + 0x0F0E0D * i);
        CartTestHost host(emu::CoreRegistry::propertiesFromOctoOptions(options));
        const auto file = fixture.path("palette.gif");
        REQUIRE(host.saveCartridge(file, PROGRAM, "", {}, "TEST", false).empty());
        emu::OctoCartridge cart(file);
        REQUIRE(cart.loadCartridge());
        CHECK(cart.getOptions().palette == options.palette);
        CartTestHost second(emu::CoreRegistry::propertiesForPreset("chip-8"));
        REQUIRE(second.loadRom(file, emu::EmuHostEx::LoadOptions::None));
        REQUIRE(second.getProperties().palette().colors.size() == 16);
        CHECK(second.getProperties().palette().colors[12].toRGBInt() == 0x100000 + 0x0F0E0D * 12);

        CartTestHost plain(emu::CoreRegistry::propertiesForPreset("xo-chip"));
        const auto plainFile = fixture.path("plain.gif");
        REQUIRE(plain.saveCartridge(plainFile, PROGRAM, "", {}, "TEST", false).empty());
        emu::OctoCartridge plainCart(plainFile);
        REQUIRE(plainCart.loadCartridge());
        CHECK(plainCart.getOptions().palette.empty());
    }

    TEST_CASE("includes are resolved and no paths get stored")
    {
        CartFixture fixture;
        std::ofstream(fixture.path("part.8o")) << ": part\n  v1 := 2\n  return\n";
        const auto sourcePath = fixture.path("main.8o");
        const std::string source = ": main\n  part\n  loop again\n:include \"part.8o\"\n";
        CartTestHost host(emu::CoreRegistry::propertiesForPreset("xo-chip"));
        const auto file = fixture.path("include.gif");
        REQUIRE(host.saveCartridge(file, source, sourcePath, {}, "", false).empty());
        emu::OctoCartridge cart(file);
        REQUIRE(cart.loadCartridge());
        CHECK(cart.getSource().find(": part") != std::string::npos);
        CHECK(cart.getSource().find(":include") == std::string::npos);
        CHECK(cart.getSource().find(fixture.directory.string()) == std::string::npos);
    }

    TEST_CASE("a broken source gives an error and no file")
    {
        CartFixture fixture;
        const auto file = fixture.path("broken.gif");
        CartTestHost host(emu::CoreRegistry::propertiesForPreset("xo-chip"));
        const auto error = host.saveCartridge(file, ": main\n  this is no valid code\n", "", {}, "", false);
        CHECK_FALSE(error.empty());
        CHECK_FALSE(std::filesystem::exists(file));
    }

    TEST_CASE("the source has to match the expected program")
    {
        CartFixture fixture;
        const auto file = fixture.path("mismatch.gif");
        CartTestHost host(emu::CoreRegistry::propertiesForPreset("xo-chip"));
        CHECK_FALSE(host.saveCartridge(file, PROGRAM, "", {0x12, 0x00}, "", false).empty());
        CHECK_FALSE(std::filesystem::exists(file));
        CHECK(host.saveCartridge(file, PROGRAM, "", {0x60, 0x01, 0x12, 0x02}, "", false).empty());
    }

    TEST_CASE("an unsupported variant is refused")
    {
        CartFixture fixture;
        const auto file = fixture.path("megachip.gif");
        CartTestHost host(emu::CoreRegistry::propertiesForPreset("megachip"));
        CHECK_FALSE(host.saveCartridge(file, PROGRAM, "", {}, "", false).empty());
        CHECK_FALSE(std::filesystem::exists(file));
    }

    TEST_CASE("the limit grows with a big program")
    {
        CartFixture fixture;
        const auto file = fixture.path("big.gif");
        std::string source = ": main\n  loop again\n";
        for (int i = 0; i < 2000; ++i) {
            source += "  0x12 0x34\n";
        }
        CartTestHost host(emu::CoreRegistry::propertiesForPreset("schipc"));
        REQUIRE(host.saveCartridge(file, source, "", {}, "", false).empty());
        emu::OctoCartridge cart(file);
        REQUIRE(cart.loadCartridge());
        CHECK(cart.getOptions().maxRom == 65024);
    }

    TEST_CASE("binary only cartridge of a source")
    {
        CartFixture fixture;
        const auto file = fixture.path("binary.gif");
        const std::string source = "# SECRET\n: main\n  secret_label\n  loop again\n: secret_label\n  return\n";
        CartTestHost host(emu::CoreRegistry::propertiesForPreset("xo-chip"));
        REQUIRE(host.saveCartridge(file, source, "", {}, "", false, true).empty());
        emu::OctoCartridge cart(file);
        REQUIRE(cart.loadCartridge());
        CHECK(cart.getSource().starts_with(": main\n0x"));
        CHECK(cart.getSource().find("SECRET") == std::string::npos);
        CHECK(cart.getSource().find("secret_label") == std::string::npos);
        CartTestHost second;
        REQUIRE(second.loadRom(file, emu::EmuHostEx::LoadOptions::None));
        CHECK(second.romImage() == std::vector<uint8_t>{0x22, 0x04, 0x12, 0x02, 0x00, 0xEE});
    }

    TEST_CASE("binary only cartridge of a loaded program doesn't need its source")
    {
        CartFixture fixture;
        const auto file = fixture.path("binary.gif");
        const std::vector<uint8_t> image = {0x00, 0xE0, 0x12, 0x02, 0xFF, 0xFF, 0x80, 0x81};
        CartTestHost host(emu::CoreRegistry::propertiesForPreset("chip-8"));
        REQUIRE(host.saveCartridge(file, "this source isn't even valid", "", image, "", false, true).empty());
        CartTestHost second;
        REQUIRE(second.loadRom(file, emu::EmuHostEx::LoadOptions::None));
        CHECK(second.romImage() == image);
    }

    TEST_CASE("binary only cartridge with changed source still has to assemble")
    {
        CartFixture fixture;
        const auto file = fixture.path("binary.gif");
        CartTestHost host(emu::CoreRegistry::propertiesForPreset("xo-chip"));
        CHECK_FALSE(host.saveCartridge(file, ": main\n  jump nowhere\n", "", {}, "", false, true).empty());
        CHECK_FALSE(std::filesystem::exists(file));
    }

    TEST_CASE("binary only cartridge needs the default start address")
    {
        CartFixture fixture;
        const auto file = fixture.path("binary.gif");
        auto props = emu::CoreRegistry::propertiesForPreset("xo-chip");
        props[std::string(emu::propid::START_ADDRESS)].setInt(0x300);
        CartTestHost host(props);
        CHECK_FALSE(host.saveCartridge(file, "", "", {0x12, 0x00}, "", false, true).empty());
        CHECK_FALSE(std::filesystem::exists(file));
    }

    TEST_CASE("a program without source goes through its disassembly")
    {
        CartFixture fixture;
        const auto rom = fixture.path("rom.ch8");
        const std::vector<uint8_t> image = {0x00, 0xE0, 0x60, 0x05, 0xA2, 0x0A, 0xD0, 0x01, 0x12, 0x06, 0xF0, 0x00};
        std::ofstream(rom, std::ios::binary).write(reinterpret_cast<const char*>(image.data()), static_cast<std::streamsize>(image.size()));
        CartTestHost host(emu::CoreRegistry::propertiesForPreset("chip-8"));
        REQUIRE(host.loadRom(rom, emu::EmuHostEx::LoadOptions::None));
        auto request = host.disassemblyRequest();
        REQUIRE(request);
        const auto source = emu::EmuHostEx::disassemble(*request);
        const auto file = fixture.path("disassembled.gif");
        const auto error = host.saveCartridge(file, source, "", host.romImage(), "", false);
        CHECK(error.empty());
        CartTestHost second;
        REQUIRE(second.loadRom(file, emu::EmuHostEx::LoadOptions::None));
        CHECK(second.romImage() == image);
    }
}
