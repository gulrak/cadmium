//---------------------------------------------------------------------------------------
// src/emuhostex.hpp
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

#include <configuration.hpp>
//#include <emulation/chip8options.hpp>
#include <emulation/emulatorhost.hpp>
#include <emulation/iemulationcore.hpp>
#include <emulation/palette.hpp>
#include <emulation/coreregistry.hpp>
#include <chiplet/octocompiler.hpp>
#include <chiplet/hexfile.hpp>
#include <ghc/bitenum.hpp>
#include <ghc/span.hpp>
#include <librarian.hpp>
#ifndef PLATFORM_WEB
#include <threadpool.hpp>
#ifdef CADMIUM_WITH_DATABASE
#include <database.hpp>
#endif
#endif

#ifdef CADMIUM_WITH_BACKGROUND_EMULATION
#include <thread>
#include <raylib.h>
#endif

#include <array>
#include <string>
#include <vector>
#include <optional>
#include <limits>
#include <cmath>

namespace emu {

static constexpr int SUPPORTED_SCREEN_WIDTH = 540;
static constexpr int SUPPORTED_SCREEN_HEIGHT = 296;

class IChip8Emulator;

/*
class Chip8HeadlessHost : public Chip8EmulatorHost
{
public:
    explicit Chip8HeadlessHost(const Chip8EmulatorOptions& options_) : options(options_) {}
    ~Chip8HeadlessHost() override = default;
    bool isHeadless() const override { return true; }
    uint8_t getKeyPressed() override { return 0; }
    bool isKeyDown(uint8_t key) override { return false; }
    const std::array<bool,16>& getKeyStates() const override { static const std::array<bool,16> keys{}; return keys; }
    void updateScreen() override {}
    void vblank() override {}
    void updatePalette(const std::array<uint8_t,16>& palette) override {}
    void updatePalette(const std::vector<uint32_t>& palette, size_t offset) override {}
    Chip8EmulatorOptions options;
};
*/

class EmuHostEx : public EmulatorHost
{
public:
    enum LoadOptions { None = 0, DontChangeOptions = 1, SetToRun = 2 };
    explicit EmuHostEx(CadmiumConfiguration& cfg);
    ~EmuHostEx() override = default;
    //virtual bool isHeadless() const = 0;
    //virtual uint8_t getKeyPressed() = 0;
    //virtual bool isKeyDown(uint8_t key) = 0;
    //virtual bool isKeyUp(uint8_t key) { return !isKeyDown(key); }
    //virtual const std::array<bool,16>& getKeyStates() const = 0;
    //virtual void updateScreen() = 0;
    //virtual void updatePalette(const std::array<uint8_t, 16>& palette) = 0;
    //virtual void updatePalette(const std::vector<uint32_t>& palette, size_t offset) = 0;
    virtual bool loadRom(std::string_view filename, LoadOptions loadOpt);
    virtual bool loadBinary(std::string_view filename, ghc::span<const uint8_t> binary, LoadOptions loadOpt);
    virtual bool loadBinary(std::string_view filename, ghc::span<const uint8_t> binary, const Properties& props, bool isKnown);
    virtual void updateEmulatorOptions(const Properties& properties);
    void reloadProgram();
    void setPalette(const std::vector<uint32_t>& colors, size_t offset = 0, bool setAsDefault = false);
    void setPalette(const Palette& palette);

    struct LifecycleCounters
    {
        int coreCreations{0};
        int resets{0};
        int programLoads{0};
    };
    const LifecycleCounters& lifecycleCounters() const { return _lifecycleCounters; }

    struct DisassemblyKey
    {
        Sha1::Digest sha1{};
        chip8::VariantSet variants{};
        uint32_t address{0};
        bool operator==(const DisassemblyKey& other) const { return sha1 == other.sha1 && variants == other.variants && address == other.address; }
    };

    struct DisassemblyRequest
    {
        DisassemblyKey key;
        std::string name;
        std::vector<uint8_t> image;
    };

    std::optional<DisassemblyKey> disassemblyKey() const;
    std::optional<DisassemblyRequest> disassemblyRequest() const;
    static std::string disassemble(const DisassemblyRequest& request);

protected:
    enum class ProgramOrigin { FILE, EDITOR, DATABASE };
    struct Program
    {
        std::string name;
        std::vector<uint8_t> image;
        std::optional<HexFile> hexFile;  // HEX files keep their ranges, image is only a canonical copy then
        Sha1::Digest sha1;
        std::optional<uint32_t> loadAddress;  // default load address of the core if not set
        bool isKnown{false};
        ProgramOrigin origin{ProgramOrigin::FILE};
        bool autoRun{false};
        std::string source;  // set if the program was compiled from source, otherwise a disassembly is generated
        std::shared_ptr<emu::OctoCompiler> compiler;
    };

    class RequestScope
    {
    public:
        explicit RequestScope(EmuHostEx& host) : _host(host) { ++_host._requestDepth; }
        ~RequestScope();
        RequestScope(const RequestScope&) = delete;
        RequestScope& operator=(const RequestScope&) = delete;
        bool finish();
    private:
        EmuHostEx& _host;
        bool _finished{false};
    };
    void requestConfig(const Properties& properties);
    void requestProgram(Program program);
    void requestReload();
    void applyPendingConfig();
    virtual bool applyRequests();
    bool replaceProgramFromEditor(std::vector<uint8_t> image, const Sha1::Digest& sha1);
    // The palette as the current core uses it: exactly as many colors as it supports, missing ones from the default palette.
    Palette completedPalette(Palette palette) const;

    bool loadHexFile(std::string_view filename, LoadOptions loadOpt, std::string_view sourcePath = {});
    bool loadRomImage();
    std::unique_ptr<IEmulationCore> create(Properties& properties, IEmulationCore* iother = nullptr);
    void resetAllBreakpoints();
    virtual void whenRomLoaded(const std::string& filename, bool autoRun, emu::OctoCompiler* compiler, const std::string& source) {}
    virtual void whenEmuChanged(emu::IEmulationCore& emu) {}
    virtual void whenMachineReset() {}
    static int saturatedCast(double value) {
        if (value > static_cast<double>(std::numeric_limits<int>::max()))
            return std::numeric_limits<int>::max();
        if (value < static_cast<double>(std::numeric_limits<int>::min()))
            return std::numeric_limits<int>::min();
        return static_cast<int>(std::round(value));
    }
    static inline std::atomic_int _instanceCounter{0};
    int _instanceNum{_instanceCounter++};
    CadmiumConfiguration& _cfg;
    CoreRegistry _cores;
    std::string _cfgPath;
    std::string _databaseDirectory;
    std::string _currentDirectory;
    std::string _currentFileName;
    Librarian _librarian;
    std::unordered_map<std::string, std::string> _badges;
#ifndef PLATFORM_WEB
    ThreadPool _threadPool;
#ifdef CADMIUM_WITH_DATABASE
    static std::unique_ptr<Database> _database;
#endif
#endif
    std::unique_ptr<IEmulationCore> _chipEmu;
    std::string _romName;
    std::vector<uint8_t> _romImage;
    std::optional<HexFile> _romHexFile;
    std::optional<uint32_t> _romLoadAddress;
    Sha1::Digest _romSha1;
    bool _romIsWellKnown{false};
    ProgramOrigin _romOrigin{ProgramOrigin::FILE};
    bool _romHasSource{false};
    bool _customPalette{false};
    Palette _colorPalette;
    Palette _defaultPalette;
    emu::Properties* _properties{nullptr};
    std::map<std::string, Properties> _propertiesByClass;
    std::string _variantName;
    emu::Properties _romWellKnownProperties;
    emu::Properties _previousProperties;
    LifecycleCounters _lifecycleCounters;

private:
    void syncPalette();
    bool programFits(const Program& program) const;
    void adoptProgram(Program& program);
    int _requestDepth{0};
    std::optional<Properties> _pendingConfig;
    std::optional<Program> _pendingProgram;
    bool _pendingReload{false};
    bool _coreChanged{false};
};

GHC_ENUM_ENABLE_BIT_OPERATIONS(EmuHostEx::LoadOptions)

class HeadlessHostConfiguration
{
protected:
    CadmiumConfiguration _headlessConfig;
};

class HeadlessHost : private HeadlessHostConfiguration, public EmuHostEx
{
public:
    HeadlessHost() : EmuHostEx(_headlessConfig) {}
    explicit HeadlessHost(const Properties& options) : EmuHostEx(_headlessConfig) { updateEmulatorOptions(options); }
    ~HeadlessHost() override = default;
    Properties& getProperties() { return *_properties; }
    IEmulationCore& emuCore() { return *_chipEmu; }
    bool isHeadless() const override { return true; }
    int getKeyPressed() override { return 0; }
    bool isKeyDown(uint8_t key) override { return false; }
    const std::array<bool,16>& getKeyStates() const override { static const std::array<bool,16> keys{}; return keys; }
    void updateScreen() override {}
    void vblank() override {}
    void updatePalette(const std::array<uint8_t,16>& palette) override {}
    void updatePalette(const std::vector<uint32_t>& palette, size_t offset) override {}
};

#ifdef CADMIUM_WITH_BACKGROUND_EMULATION
class ThreadedBackgroundHost : public EmuHostEx
{
public:
    explicit ThreadedBackgroundHost(CadmiumConfiguration& cfg, double initialFrameRate = 60.0);
    explicit ThreadedBackgroundHost(CadmiumConfiguration& cfg, const Properties& options, double initialFrameRate = 60.0);
    ~ThreadedBackgroundHost() override;
    void setFrameRate(double frequency);
    Properties& getProperties() { return *_properties; }
    IEmulationCore& emuCore() { return *_chipEmu; }
    bool isHeadless() const override { return true; }
    int getKeyPressed() override { return 0; }
    bool isKeyDown(uint8_t key) override { return false; }
    const std::array<bool,16>& getKeyStates() const override { static const std::array<bool,16> keys{}; return keys; }
    void killEmulation();
    void updateEmulatorOptions(const Properties& properties) override;
    void updateScreen() override;
    std::pair<Texture2D*, Rectangle> updateTexture();
    void vblank() override {}
    void updatePalette(const std::array<uint8_t,16>& palette) override {}
    void updatePalette(const std::vector<uint32_t>& palette, size_t offset) override {}
    void drawScreen(Rectangle dest) const;
    int getFrameTimeAvg() const { return saturatedCast(_smaFrameTime_us.get()); }
    int getFrames() const { return _chipEmu ? _chipEmu->frames() : 0; }
    bool loadBinary(std::string_view filename, ghc::span<const uint8_t> binary, const Properties& props, bool isKnown) override;

private:
    bool applyRequests() override;
    void worker();
    void tick();
    Image _screen{};
    Texture2D _screenTexture{};
    std::atomic_bool _shutdown{false};
    std::atomic<std::chrono::steady_clock::duration> _frameDuration{std::chrono::steady_clock::duration::zero()};
    std::recursive_mutex _mutex;
    SMA<120> _smaFrameTime_us;
    std::thread _workerThread;
};
#endif

}
