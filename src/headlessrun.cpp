//---------------------------------------------------------------------------------------
// src/headlessrun.cpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2026, Steffen Schümann <s.schuemann@pobox.com>
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
#include "headlessrun.hpp"

#include <emuhostex.hpp>
#include <emulation/ichip8.hpp>
#include <nlohmann/json.hpp>
#include <raylib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

KeyScript KeyScript::parse(const std::string& script)
{
    KeyScript result;
    result._text = script;
    int64_t previousEnd = 0;
    std::string token;
    std::istringstream input(script);
    auto fail = [&token](const char* reason) {
        return std::invalid_argument("invalid key event '" + token + "': " + reason + ", expected [+]<frame>:<hex keys>[+<frames held>]");
    };
    auto parseNumber = [&](std::string_view text) {
        int64_t value = 0;
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (text.empty() || error != std::errc() || end != text.data() + text.size() || value < 0)
            throw fail("bad frame count");
        return value;
    };
    while (input >> token) {
        // commas separate like spaces, "60:1,+30:2" is two events
        std::string_view rest = token;
        for (auto comma = rest.find(','); !rest.empty(); comma = rest.find(',')) {
            const auto part = rest.substr(0, comma);
            rest = comma == std::string_view::npos ? std::string_view{} : rest.substr(comma + 1);
            if (part.empty())
                continue;
            const auto colon = part.find(':');
            if (colon == std::string_view::npos)
                throw fail("missing ':'");
            auto start = part.substr(0, colon);
            const bool relative = start.starts_with('+');
            if (relative)
                start.remove_prefix(1);
            auto keys = part.substr(colon + 1);
            int64_t hold = DEFAULT_HOLD_FRAMES;
            if (const auto plus = keys.find('+'); plus != std::string_view::npos) {
                hold = parseNumber(keys.substr(plus + 1));
                keys = keys.substr(0, plus);
                if (hold < 1)
                    throw fail("keys must be held for at least one frame");
            }
            if (keys.empty())
                throw fail("no keys");
            KeyEvent event{.frame = parseNumber(start) + (relative ? previousEnd : 0), .hold = hold};
            for (const char key : keys) {
                const int value = std::isxdigit(static_cast<unsigned char>(key)) ? std::stoi(std::string(1, key), nullptr, 16) : -1;
                if (value < 0)
                    throw fail("keys are the hex digits 0-F");
                event.keys |= static_cast<uint16_t>(1u << value);
            }
            previousEnd = event.frame + event.hold;
            result._events.push_back(event);
        }
    }
    return result;
}

uint16_t KeyScript::keysDownAt(int64_t frame) const
{
    uint16_t keys = 0;
    for (const auto& event : _events) {
        if (frame >= event.frame && frame < event.frame + event.hold)
            keys |= event.keys;
    }
    return keys;
}

int64_t KeyScript::endFrame() const
{
    int64_t end = 0;
    for (const auto& event : _events)
        end = std::max(end, event.frame + event.hold);
    return end;
}

namespace
{

class RunHost : public emu::HeadlessHost
{
public:
    explicit RunHost(KeyScript keyScript) : _keyScript(std::move(keyScript)) {}
    void startCapture() { _capturing = true; }

    void setFrame(int64_t frame)
    {
        _frameIndex = frame;
        const auto keys = _keyScript.keysDownAt(frame);
        for (int key = 0; key < 16; ++key)
            _keyStates[key] = keys & (1u << key);
    }
    const KeyScript& keyScript() const { return _keyScript; }
    bool keyScriptDone() const { return _frameIndex + 1 >= _keyScript.endFrame(); }

    // The keys go to the cores through the host like in the GUI: isKeyDown() (VIP, ETI, HP-48 and the generic
    // core's Ex9E/ExA1), getKeyStates() (DREAM) and getKeyPressed() for the Fx0A of the generic and strict cores.
    bool isKeyDown(uint8_t key) override { return key < 16 && _keyStates[key]; }
    const std::array<bool, 16>& getKeyStates() const override { return _keyStates; }
    int getKeyPressed() override
    {
        // Like the VIP's Fx0A: a held key is taken, the result comes with its release, until then < 0 (the
        // generic core beeps meanwhile like the VIP)
        if (_waitKey >= 0) {
            if (_keyStates[_waitKey])
                return -1;
            return std::exchange(_waitKey, -1) + 1;
        }
        for (int key = 0; key < 16; ++key) {
            if (_keyStates[key]) {
                _waitKey = key;
                return -1;
            }
        }
        return 0;
    }
    int64_t presentations() const { return _presentations; }
    int width() const { return _width; }
    int height() const { return _height; }

    void updateScreen() override
    {
        if (_capturing) {
            captureScreen();
            ++_presentations;
        }
    }

    void captureScreen()
    {
        _frame.resize(static_cast<size_t>(emu::SUPPORTED_SCREEN_WIDTH) * emu::SUPPORTED_SCREEN_HEIGHT);
        const auto rendered = emu::renderScreen(emuCore(), {_frame.data(), emu::SUPPORTED_SCREEN_WIDTH, emu::SUPPORTED_SCREEN_HEIGHT});
        if (rendered.empty())
            throw std::runtime_error("Selected core has no capturable screen");
        _width = rendered.width;
        _height = rendered.height;
        _ratio = std::max(1, rendered.ratio);
        _rotation = emuCore().screenRotation();
        _pixels.resize(static_cast<size_t>(_width) * _height);
        for (int y = 0; y < _height; ++y)
            std::copy_n(_frame.data() + y * emu::SUPPORTED_SCREEN_WIDTH, _width, _pixels.data() + y * _width);
    }

    void exportScreen(const std::string& path, int scale)
    {
        if (_pixels.empty())
            captureScreen();
        auto width = _width;
        auto height = _height;
        const auto* pixels = _pixels.data();
        std::vector<uint32_t> shown;
        if (scale > 0) {
            shown = shownScreen(scale, width, height);
            pixels = shown.data();
        }
        Image image{const_cast<uint32_t*>(pixels), width, height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        if (!ExportImage(image, path.c_str()))
            throw std::runtime_error("Could not export screen to " + path);
    }

private:
    std::vector<uint32_t> shownScreen(int scale, int& width, int& height) const
    {
        const int scaledWidth = _width * scale;
        const int scaledHeight = std::max(1, _height * scale / _ratio);
        std::vector<uint32_t> scaled(static_cast<size_t>(scaledWidth) * scaledHeight);
        for (int y = 0; y < scaledHeight; ++y) {
            const int sourceY = std::min(_height - 1, y * _ratio / scale);
            for (int x = 0; x < scaledWidth; ++x)
                scaled[static_cast<size_t>(y) * scaledWidth + x] = _pixels[static_cast<size_t>(sourceY) * _width + x / scale];
        }
        const bool sideways = _rotation == 90 || _rotation == 270;
        width = sideways ? scaledHeight : scaledWidth;
        height = sideways ? scaledWidth : scaledHeight;
        if (_rotation != 90 && _rotation != 180 && _rotation != 270)
            return scaled;
        std::vector<uint32_t> rotated(scaled.size());
        for (int y = 0; y < scaledHeight; ++y) {
            for (int x = 0; x < scaledWidth; ++x) {
                // clockwise, like the rotation of DrawTexturePro() with the y axis pointing down
                int targetX = x, targetY = y;
                if (_rotation == 90) {
                    targetX = scaledHeight - 1 - y;
                    targetY = x;
                }
                else if (_rotation == 180) {
                    targetX = scaledWidth - 1 - x;
                    targetY = scaledHeight - 1 - y;
                }
                else {
                    targetX = y;
                    targetY = scaledWidth - 1 - x;
                }
                rotated[static_cast<size_t>(targetY) * width + targetX] = scaled[static_cast<size_t>(y) * scaledWidth + x];
            }
        }
        return rotated;
    }

    KeyScript _keyScript;
    std::array<bool, 16> _keyStates{};
    int _waitKey{-1};
    int64_t _frameIndex{0};
    bool _capturing{false};
    int64_t _presentations{0};
    int _width{0};
    int _height{0};
    int _ratio{1};
    int _rotation{0};
    std::vector<uint32_t> _frame;
    std::vector<uint32_t> _pixels;
};

struct RunResult
{
    std::string stopReason;
    int64_t instructions{0};
    int64_t frames{0};
    bool reachedTarget{false};
    bool failed{false};
    std::string errorMessage;
    std::optional<uint32_t> lastPc;
    std::optional<uint16_t> lastOpcode;
};

bool isExitOpcode(uint16_t opcode)
{
    return opcode == 0x00FD || opcode == 0x00ED;
}

// The generic core alone: single instructions with an own frame schedule from the instructions per frame,
// stops exactly at the instruction that presents the wanted frame.
RunResult runInstructions(RunHost& host, emu::IChip8Emulator& chip8, int64_t instructionsPerFrame, const HeadlessRunOptions& options)
{
    // Reset may mark the initial blank display dirty. Do not count that as a
    // program presentation. Start timers just as executeFrame() would.
    int64_t frame = 0;
    host.setFrame(frame);
    chip8.handleTimer();
    host.startCapture();

    RunResult result{.stopReason = "instruction-limit"};
    int64_t nextFrameCycle = instructionsPerFrame;
    uint32_t lastPc = chip8.getPC();
    uint16_t lastOpcode = 0;
    while (result.instructions < options.maxInstructions) {
        if (chip8.cycles() >= nextFrameCycle) {
            host.setFrame(++frame);
            chip8.handleTimer();
            nextFrameCycle = (chip8.cycles() / instructionsPerFrame + 1) * instructionsPerFrame;
            if (options.untilPresentations > 0 && host.presentations() >= options.untilPresentations) {
                result.stopReason = "presentations";
                break;
            }
        }
        lastPc = chip8.getPC();
        lastOpcode = chip8.opcode();
        chip8.executeInstruction();
        ++result.instructions;
        if (chip8.inErrorState()) {
            result.stopReason = "emulator-error";
            break;
        }
        if (options.untilPresentations > 0 && host.presentations() >= options.untilPresentations) {
            result.stopReason = "presentations";
            break;
        }
        if (chip8.execMode() != emu::GenericCpu::eRUNNING) {
            result.stopReason = isExitOpcode(lastOpcode) ? "program-exit" : "paused";
            break;
        }
    }
    result.lastPc = lastPc;
    result.lastOpcode = lastOpcode;
    result.failed = chip8.inErrorState();
    result.errorMessage = chip8.errorMessage();
    result.reachedTarget = options.untilPresentations < 0 || host.presentations() >= options.untilPresentations;
    if (result.stopReason == "paused")
        result.reachedTarget = false;
    return result;
}

RunResult runFrames(RunHost& host, emu::IEmulationCore& core, const HeadlessRunOptions& options)
{
    // Drop a pending update of the blank screen from the reset, like the instruction mode does.
    core.needsScreenUpdate();
    host.startCapture();

    RunResult result{.stopReason = "frame-limit"};
    const int64_t maxFrames = options.maxFrames > 0 ? options.maxFrames : HeadlessRunOptions::DEFAULT_MAX_FRAMES;
    const int64_t startInstructions = core.executedInstructions();
    auto* unit = core.focussedExecutionUnit();
    auto* chip8 = core.chip8Core();
    while (result.frames < maxFrames) {
        host.setFrame(result.frames);
        core.executeFrame();
        ++result.frames;
        if (core.coreState() == emu::IEmulationCore::ECS_ERROR || (unit && unit->inErrorState())) {
            result.stopReason = "emulator-error";
            result.failed = true;
            break;
        }
        if (options.untilPresentations > 0 && host.presentations() >= options.untilPresentations) {
            result.stopReason = "presentations";
            break;
        }
        if (core.execMode() != emu::GenericCpu::eRUNNING) {
            result.stopReason = "paused";
            if (chip8) {
                // The generic core stops on the exit instruction, real CHIP-8 interpreter cores stop
                // (after a few frames) on a jump to itself, both mean the program is done.
                const auto opcode = chip8->opcode();
                if (isExitOpcode(opcode))
                    result.stopReason = "program-exit";
                else if ((opcode & 0xF000) == 0x1000 && (opcode & 0xFFF) == chip8->getPC())
                    result.stopReason = "endless-loop";
            }
            break;
        }
    }
    result.instructions = core.executedInstructions() - startInstructions;
    // A core that stopped within a frame (exit, endless loop) did not present its last drawing, the GUI still shows
    // it, so the dump of a frame-driven run is the final screen.
    host.captureScreen();
    if (result.failed) {
        result.errorMessage = core.errorMessage();
        if (result.errorMessage.empty() && unit)
            result.errorMessage = unit->errorMessage();
    }
    result.reachedTarget = options.untilPresentations < 0 || host.presentations() >= options.untilPresentations;
    if (result.stopReason == "paused")
        result.reachedTarget = false;
    return result;
}

nlohmann::json registerState(const emu::GenericCpu& cpu)
{
    nlohmann::json registers = nlohmann::json::object();
    const auto& names = cpu.registerNames();
    for (size_t index = 0; index < cpu.numRegisters() && index < names.size(); ++index)
        registers[names[index]] = cpu.registerbyIndex(index).value;
    return {{"name", cpu.name()}, {"pc", cpu.getPC()}, {"sp", cpu.getSP()}, {"cycles", cpu.cycles()},
            {"execMode", cpu.execMode()}, {"cpuState", cpu.cpuState()}, {"registers", registers}};
}

void writeState(RunHost& host, const std::string& path, const RunResult& result, int exitCode, const char* mode, int64_t instructionsPerFrame, int frameRate,
                uint32_t loadAddress)
{
    auto& core = host.emuCore();
    nlohmann::json state = {
        {"schemaVersion", 2}, {"mode", mode}, {"core", core.name()}, {"stopReason", result.stopReason}, {"exitCode", exitCode},
        {"instructions", result.instructions}, {"cycles", core.cycles()}, {"machineCycles", core.machineCycles()},
        {"frames", core.frames()}, {"executedFrames", result.frames}, {"presentations", host.presentations()},
        {"execMode", core.execMode()}, {"error", result.errorMessage}, {"frameRate", frameRate},
        {"memorySize", core.memSize()}, {"loadAddress", loadAddress},
        {"screenWidth", host.width() ? host.width() : core.getCurrentScreenWidth()},
        {"screenHeight", host.height() ? host.height() : core.getCurrentScreenHeight()}
    };
    if (instructionsPerFrame > 0)
        state["instructionsPerFrame"] = instructionsPerFrame;
    if (result.lastPc)
        state["lastInstructionPc"] = *result.lastPc;
    if (result.lastOpcode)
        state["lastOpcode"] = *result.lastOpcode;
    if (auto* chip8 = core.chip8Core()) {
        nlohmann::json registers = nlohmann::json::array();
        for (int index = 0; index < 16; ++index)
            registers.push_back(chip8->getV(index));
        nlohmann::json stack = nlohmann::json::array();
        for (unsigned index = 0; index < chip8->getSP() && index < chip8->stackSize(); ++index)
            stack.push_back(chip8->stackElement(index));
        state.update({{"pc", chip8->getPC()}, {"i", chip8->getI()}, {"sp", chip8->getSP()}, {"v", registers}, {"stack", stack},
                      {"delayTimer", chip8->delayTimer()}, {"soundTimer", chip8->soundTimer()}, {"cpuState", chip8->cpuState()}});
    }
    nlohmann::json units = nlohmann::json::array();
    for (size_t index = 0; index < core.numberOfExecutionUnits(); ++index) {
        if (const auto* unit = core.executionUnit(index))
            units.push_back(registerState(*unit));
    }
    state["executionUnits"] = units;
    if (!host.keyScript().empty()) {
        state["keys"] = host.keyScript().text();
        state["keysDone"] = host.keyScriptDone();
    }
    std::ofstream output(path);
    output << state.dump(2) << '\n';
    output.close();
    if (!output)
        throw std::runtime_error("Could not export state to " + path);
}

}

int runHeadless(const std::string& romPath, const emu::Properties& properties, const HeadlessRunOptions& options)
{
    if (options.dumpScale > 0 && options.dumpScreen.empty())
        throw std::runtime_error("--dump-scale requires --dump-screen");
    RunHost host(KeyScript::parse(options.keys));
    host.updateEmulatorOptions(properties);
    auto loadOptions = emu::EmuHostEx::DontChangeOptions;
    if (options.applyCh8Attributes)
        loadOptions |= emu::EmuHostEx::ApplyCh8Attributes;
    const auto loadsBefore = host.lifecycleCounters().programLoads;
    if (!host.loadRom(romPath, loadOptions) || host.lifecycleCounters().programLoads == loadsBefore)
        throw std::runtime_error("Could not load ROM: " + romPath);
    auto& core = host.emuCore();
    auto* chip8 = dynamic_cast<emu::IChip8Emulator*>(core.executionUnit(0));
    int64_t instructionsPerFrame = 0;
    if (const auto* ipfProperty = host.getProperties().find(emu::propid::INSTRUCTIONS_PER_FRAME)) {
        if (ipfProperty->getInt() <= 0)
            throw std::runtime_error("--headless requires a positive --instructions-per-frame");
        instructionsPerFrame = ipfProperty->getInt();
    }
    const bool frameMode = options.maxFrames > 0 || !instructionsPerFrame || !chip8;
    if (frameMode && options.maxInstructionsGiven)
        throw std::runtime_error("--max-instructions only applies to the generic CHIP-8 core without --max-frames");
    const auto* frameRateProperty = host.getProperties().find(emu::propid::FRAME_RATE);
    const int frameRate = frameRateProperty ? frameRateProperty->getInt() : static_cast<int>(std::lround(core.frameRate()));
    const auto loadAddress = core.defaultLoadAddress();
    core.setExecMode(emu::GenericCpu::eRUNNING);

    const auto result = frameMode ? runFrames(host, core, options) : runInstructions(host, *chip8, instructionsPerFrame, options);
    const int exitCode = result.failed ? 1 : result.reachedTarget ? 0 : 2;
    if (!options.dumpScreen.empty())
        host.exportScreen(options.dumpScreen, static_cast<int>(options.dumpScale));
    if (!options.dumpState.empty())
        writeState(host, options.dumpState, result, exitCode, frameMode ? "frames" : "instructions", instructionsPerFrame, frameRate, loadAddress);
    std::cout << "Stopped: " << result.stopReason << ", instructions: " << result.instructions;
    if (frameMode)
        std::cout << ", frames: " << result.frames;
    std::cout << ", presentations: " << host.presentations() << '\n';
    if (!host.keyScript().empty() && !host.keyScriptDone())
        std::cerr << "WARNING: the run stopped before the end of the key script (frame " << host.keyScript().endFrame() << ")\n";
    if (exitCode != 0)
        std::cerr << "ERROR: " << (result.failed ? result.errorMessage : "Run stopped before reaching its target") << '\n';
    return exitCode;
}
