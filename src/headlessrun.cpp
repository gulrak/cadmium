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

#include <fstream>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{

class RunHost : public emu::HeadlessHost
{
public:
    void startCapture() { _capturing = true; }
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
        auto& core = emuCore();
        _width = core.getCurrentScreenWidth();
        _height = core.getCurrentScreenHeight();
        _pixels.resize(_width * _height);
        if (const auto* screen = core.getScreen())
            screen->convert(_pixels.data(), _width, 255, nullptr);
        else if (const auto* screen = core.getScreenRGBA())
            screen->convert(_pixels.data(), _width, core.getScreenAlpha(), core.getWorkRGBA());
        else
            throw std::runtime_error("Selected core has no capturable screen");
    }

    void exportScreen(const std::string& path)
    {
        if (_pixels.empty())
            captureScreen();
        Image image{_pixels.data(), _width, _height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        if (!ExportImage(image, path.c_str()))
            throw std::runtime_error("Could not export screen to " + path);
    }

private:
    bool _capturing{false};
    int64_t _presentations{0};
    int _width{0};
    int _height{0};
    std::vector<uint32_t> _pixels;
};

} // namespace

int runHeadless(const std::string& romPath, const emu::Properties& properties, const HeadlessRunOptions& options)
{
    RunHost host;
    host.updateEmulatorOptions(properties);
    auto& core = host.emuCore();
    auto* chip8 = dynamic_cast<emu::IChip8Emulator*>(core.executionUnit(0));
    if (!chip8 || !core.isGenericEmulation())
        throw std::runtime_error("--headless currently requires the generic CHIP-8 core");
    const auto* ipfProperty = host.getProperties().find("instructionsPerFrame");
    if (!ipfProperty || ipfProperty->getInt() <= 0)
        throw std::runtime_error("--headless requires a positive --instructions-per-frame");
    const int64_t instructionsPerFrame = ipfProperty->getInt();
    const auto* frameRateProperty = host.getProperties().find("frameRate");
    const int frameRate = frameRateProperty ? frameRateProperty->getInt() : core.frameRate();

    const auto loadAddress = core.defaultLoadAddress();
    if (std::filesystem::path(romPath).extension() == ".hex") {
        if (!host.loadRom(romPath, emu::EmuHostEx::DontChangeOptions))
            throw std::runtime_error("Could not load HEX ROM: " + romPath);
    }
    else {
        std::ifstream rom(romPath, std::ios::binary | std::ios::ate);
        if (!rom)
            throw std::runtime_error("Could not open ROM: " + romPath);
        const auto size = rom.tellg();
        if (size <= 0 || size > core.memSize() - loadAddress)
            throw std::runtime_error("ROM is empty or exceeds available memory: " + romPath);
        std::vector<uint8_t> bytes(static_cast<size_t>(size));
        rom.seekg(0);
        if (!rom.read(reinterpret_cast<char*>(bytes.data()), bytes.size()))
            throw std::runtime_error("Could not read ROM: " + romPath);
        core.reset();
        if (!core.loadData(bytes, loadAddress))
            throw std::runtime_error("Could not load ROM: " + romPath);
    }
    core.setExecMode(emu::GenericCpu::eRUNNING);
    // Reset may mark the initial blank display dirty. Do not count that as a
    // program presentation. Start timers just as executeFrame() would.
    chip8->handleTimer();
    host.startCapture();

    int64_t instructions = 0;
    int64_t nextFrameCycle = instructionsPerFrame;
    uint32_t lastPc = chip8->getPC();
    uint16_t lastOpcode = 0;
    std::string stopReason = "instruction-limit";
    while (instructions < options.maxInstructions) {
        if (chip8->cycles() >= nextFrameCycle) {
            chip8->handleTimer();
            nextFrameCycle = (chip8->cycles() / instructionsPerFrame + 1) * instructionsPerFrame;
            if (options.untilPresentations > 0 && host.presentations() >= options.untilPresentations) {
                stopReason = "presentations";
                break;
            }
        }
        lastPc = chip8->getPC();
        lastOpcode = chip8->opcode();
        chip8->executeInstruction();
        ++instructions;
        if (chip8->inErrorState()) {
            stopReason = "emulator-error";
            break;
        }
        if (options.untilPresentations > 0 && host.presentations() >= options.untilPresentations) {
            stopReason = "presentations";
            break;
        }
        if (chip8->execMode() != emu::GenericCpu::eRUNNING) {
            stopReason = lastOpcode == 0x00FD || lastOpcode == 0x00ED ? "program-exit" : "paused";
            break;
        }
    }
    const bool reachedTarget = options.untilPresentations < 0 || host.presentations() >= options.untilPresentations;
    const int exitCode = chip8->inErrorState() ? 1 : reachedTarget && stopReason != "paused" ? 0 : 2;
    if (!options.dumpScreen.empty())
        host.exportScreen(options.dumpScreen);
    if (!options.dumpState.empty()) {
        nlohmann::json registers = nlohmann::json::array();
        for (int index = 0; index < 16; ++index)
            registers.push_back(chip8->getV(index));
        nlohmann::json stack = nlohmann::json::array();
        for (unsigned index = 0; index < chip8->getSP() && index < chip8->stackSize(); ++index)
            stack.push_back(chip8->stackElement(index));
        nlohmann::json state = {
            {"schemaVersion", 1}, {"stopReason", stopReason}, {"exitCode", exitCode},
            {"instructions", instructions}, {"cycles", chip8->cycles()},
            {"frames", chip8->frames()}, {"presentations", host.presentations()},
            {"pc", chip8->getPC()}, {"i", chip8->getI()}, {"sp", chip8->getSP()},
            {"v", registers}, {"stack", stack},
            {"delayTimer", chip8->delayTimer()}, {"soundTimer", chip8->soundTimer()},
            {"execMode", chip8->execMode()}, {"cpuState", chip8->cpuState()},
            {"error", chip8->errorMessage()}, {"lastInstructionPc", lastPc}, {"lastOpcode", lastOpcode},
            {"instructionsPerFrame", instructionsPerFrame}, {"frameRate", frameRate},
            {"memorySize", core.memSize()}, {"loadAddress", loadAddress},
            {"screenWidth", host.width() ? host.width() : core.getCurrentScreenWidth()},
            {"screenHeight", host.height() ? host.height() : core.getCurrentScreenHeight()}
        };
        std::ofstream output(options.dumpState);
        output << state.dump(2) << '\n';
        output.close();
        if (!output)
            throw std::runtime_error("Could not export state to " + options.dumpState);
    }
    std::cout << "Stopped: " << stopReason << ", instructions: " << instructions
              << ", presentations: " << host.presentations() << '\n';
    if (exitCode != 0)
        std::cerr << "ERROR: " << (chip8->inErrorState() ? chip8->errorMessage() : "Run stopped before reaching its target") << '\n';
    return exitCode;
}
