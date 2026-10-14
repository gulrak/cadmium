#include <doctest/doctest.h>
#include <emuhostex.hpp>
#include <configuration.hpp>
#include <emulation/eti660.hpp>
#include <chiplet/utility.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>

namespace {
class HexTestHost : public emu::HeadlessHost
{
public:
    using HeadlessHost::HeadlessHost;
    const Sha1::Digest& romDigest() const { return _romSha1; }
    bool isKnown() const { return _romIsWellKnown; }
    bool reload() { return loadRomImage(); }
};

struct HexFixture
{
    HexFixture()
        : directory(std::filesystem::temp_directory_path() /
                    ("cadmium-hex-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())))
    {
        std::filesystem::create_directory(directory);
    }
    ~HexFixture() { std::filesystem::remove_all(directory); }
    std::filesystem::path directory;
};

Librarian::Info analyze(const std::filesystem::path& path)
{
    CadmiumConfiguration config;
    Librarian librarian(config);
    REQUIRE(librarian.fetchDir(path.parent_path().string()));
    auto props = emu::CoreRegistry::propertiesForPreset("chip-8");
    while (librarian.update(props)) {}
    for (size_t index = 0; index < librarian.numEntries(); ++index) {
        if (librarian.getInfo(index).filePath == path.filename().string())
            return librarian.getInfo(index);
    }
    FAIL("HEX file was not listed");
    return {};
}
}

TEST_CASE("HEX sparse ranges preserve addresses and gaps across loading and reload")
{
    HexFixture fixture;
    const auto path = fixture.directory / "sparse.hex";
    const std::array<uint8_t,4> code{0x60,0x2A,0x16,0x00};
    const std::array<uint8_t,2> data{0x12,0x34};
    {
        emu::HexFile hex(path.string(), emu::HexFile::Mode::WRITE);
        hex.write(0x800, data); // Also exercise out-of-order records.
        hex.write(0x600, code);
    }
    emu::HexFile parsed(path.string(), emu::HexFile::Mode::READ);
    REQUIRE(parsed.ranges().size() == 2);
    auto props = emu::CoreRegistry::propertiesForPreset("eti-chip-8");
    props["Clean RAM"].setBool(false);
    HexTestHost host(props);
    auto* memory = host.emuCore().memory();
    memory[0x700] = 0xA5;
    REQUIRE(host.loadRom(path.string(), emu::EmuHostEx::None));
    CHECK(host.emuCore().name() == "ETI660");
    CHECK_FALSE(host.isKnown());
    CHECK(host.romDigest() == parsed.sha1Digest());
    CHECK(memory[0x600] == code[0]);
    CHECK(memory[0x603] == code[3]);
    CHECK(memory[0x800] == data[0]);
    CHECK(memory[0x801] == data[1]);
    CHECK(memory[0x700] == 0xA5);
    host.emuCore().chip8Core()->executeInstruction();
    CHECK(host.emuCore().chip8Core()->getV(0) == 0x2A);
    memory[0x600] = memory[0x800] = 0;
    memory[0x700] = 0x5A;
    REQUIRE(host.reload());
    CHECK(memory[0x600] == code[0]);
    CHECK(memory[0x800] == data[0]);
    CHECK(memory[0x700] == 0x5A);
    const auto info = analyze(path);
    CHECK(info.type == Librarian::Info::eROM_FILE);
    CHECK_FALSE(info.isKnown);
    CHECK(info.sha1sum == parsed.sha1Digest());
    CHECK(info.variant != "eti");
    CHECK(emu::CoreRegistry::getSupportedExtensions().contains(".hex"));
    CHECK(emu::CoreRegistry::presetForExtension(".hex").empty());
    // Replacing the HEX ROM with a binary must discard retained HEX ranges.
    REQUIRE(host.loadBinary("replacement.ch8", code, emu::EmuHostEx::DontChangeOptions));
    memory[0x800] = 0xAA;
    REQUIRE(host.reload());
    CHECK(memory[0x800] == 0xAA);
}

TEST_CASE("HEX malformed empty overlapping and oversized files are rejected")
{
    HexFixture fixture;
    HexTestHost host(emu::CoreRegistry::propertiesForPreset("chip-8"));
    for (const auto text : {":00000001FF\n", ":020200006000100\n:00000001FF\n",
                            ":0202000060009C\n", ":0202000060009C\n:0202000060009C\n:00000001FF\n"}) {
        const auto path = fixture.directory / "invalid.hex";
        std::ofstream(path) << text;
        CHECK_FALSE(host.loadRom(path.string(), emu::EmuHostEx::None));
        CHECK(analyze(path).type == Librarian::Info::eUNKNOWN_FILE);
    }
    const auto path = fixture.directory / "oversized.hex";
    const std::array<uint8_t,2> data{0x12,0x34};
    {
        emu::HexFile hex(path.string(), emu::HexFile::Mode::WRITE);
        hex.write(0xFFF, data);
    }
    host.emuCore().memory()[0xFFF] = 0xA5;
    CHECK_FALSE(host.loadRom(path.string(), emu::EmuHostEx::None));
    CHECK(host.emuCore().memory()[0xFFF] == 0xA5);
}

#ifdef ETI660_EXAMPLES
TEST_CASE("HEX known Wipeout selects ETI by parsed content rather than encoding")
{
    HexFixture fixture;
    const auto binary = loadFile(std::string(ETI660_EXAMPLES) +
        "/Chip-8 ETI660 Hybrids/Wipeout (ETI660 hybrid) [W.F. Kreykes, 1982].ch8");
    REQUIRE(binary);
    for (const auto recordSize : {8, 31}) {
        INFO(recordSize);
        const auto path = fixture.directory / (std::to_string(recordSize) + ".hex");
        {
            emu::HexFile hex(path.string(), emu::HexFile::Mode::WRITE);
            hex.setDataBytesPerRecord(recordSize);
            hex.write(0x600, *binary);
        }
        emu::HexFile hex(path.string(), emu::HexFile::Mode::READ);
        const auto digest = hex.sha1Digest();
        CHECK(digest.to_hex() == "2fb4c14104b58b0df28368393b3fc0aaabe9e8ae");
        CHECK(digest == calculateSha1(*binary));
        const auto encoded = loadFile(path.string());
        REQUIRE(encoded);
        CHECK(digest != calculateSha1(*encoded));
        const auto info = analyze(path);
        CHECK(info.type == Librarian::Info::eROM_FILE);
        CHECK(info.isKnown);
        CHECK(info.sha1sum == digest);
        CHECK(info.variant == "eti-chip-8");
        HexTestHost host(emu::CoreRegistry::propertiesForPreset("chip-8"));
        REQUIRE(host.loadRom(path.string(), emu::EmuHostEx::None));
        CHECK(host.emuCore().name() == "ETI660");
        CHECK(host.isKnown());
        CHECK(host.romDigest() == digest);
        CHECK(std::equal(binary->begin(), binary->end(), host.emuCore().memory() + 0x600));
        host.emuCore().setExecMode(emu::GenericCpu::eSTEPOVER);
        for (int frame = 0; frame < 150; ++frame)
            host.emuCore().executeFrame();
        REQUIRE(host.emuCore().chip8Core()->getPC() == 0x602);
        host.emuCore().setExecMode(emu::GenericCpu::eRUNNING);
        for (int frame = 0; frame < 100; ++frame)
            host.emuCore().executeFrame();
        const auto* screen = host.emuCore().getScreenRGBA();
        REQUIRE(screen);
        std::set<uint32_t> colors;
        for (int y = 0; y < 192; ++y)
            for (int x = 0; x < 64; ++x)
                colors.insert(screen->getPixel(x, y));
        CHECK(colors.size() >= 4);
        // Browser-style in-memory uploads use the same parser and known hash.
        HexTestHost upload(emu::CoreRegistry::propertiesForPreset("chip-8"));
        REQUIRE(upload.loadBinary("upload.hex", *encoded, emu::EmuHostEx::None));
        CHECK(upload.emuCore().name() == "ETI660");
        CHECK(upload.romDigest() == digest);
        HexTestHost configured(emu::CoreRegistry::propertiesForPreset("chip-8"));
        REQUIRE(configured.loadBinary("configured.hex", *encoded,
                emu::CoreRegistry::propertiesForPreset("eti-chip-8"), true));
        CHECK(configured.emuCore().name() == "ETI660");
        CHECK(configured.romDigest() == digest);
        // Explicitly keeping the selected preset still loads at HEX addresses.
        HexTestHost keep(emu::CoreRegistry::propertiesForPreset("chip-8"));
        REQUIRE(keep.loadRom(path.string(), emu::EmuHostEx::DontChangeOptions));
        CHECK(keep.emuCore().isGenericEmulation());
        CHECK(keep.emuCore().memory()[0x600] == 0x28);
    }
}
#endif
