//---------------------------------------------------------------------------------------
// src/emulation/iemulationcore.hpp
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

#include <emulation/config.hpp>
#include <emulation/hardware/genericcpu.hpp>
#include <emulation/videoscreen.hpp>

#include <optional>
#include <span>

namespace emu {

/// An abstract emulation core class
/// An IEmulationCore is a unit that is combining one or multiple execution units (like a CPU)
//// with peripherals like a display and/or audio to to a full system that can be run by the
/// Cadmium host system.
class IEmulationCore {
public:
    enum CoreState { ECS_NORMAL, ECS_WAITING, ECS_ERROR };
    struct PixelRatio { int x{1}, y{1}; };
    static constexpr int SUPPORTED_SCREEN_WIDTH = 256;
    static constexpr int SUPPORTED_SCREEN_HEIGHT = 192;
    using VideoType = VideoScreen<uint8_t, SUPPORTED_SCREEN_WIDTH, SUPPORTED_SCREEN_HEIGHT>;
    using VideoRGBAType = VideoScreen<uint32_t, SUPPORTED_SCREEN_WIDTH, SUPPORTED_SCREEN_HEIGHT>;

    virtual ~IEmulationCore() = default;

    virtual void reset() = 0;

    virtual std::string name() const = 0;
    virtual CoreState coreState() const { return ECS_NORMAL; }
    virtual const std::string& errorMessage() const { static std::string none; return none; }

    virtual bool isGenericEmulation() const = 0;
    virtual size_t numberOfExecutionUnits() const { return 1; }
    virtual GenericCpu* executionUnit(size_t index) = 0;
    virtual void setFocussedExecutionUnit(GenericCpu* unit) = 0;
    virtual GenericCpu* focussedExecutionUnit() = 0;

    virtual GenericCpu::ExecMode execMode() const = 0;
    virtual void setExecMode(GenericCpu::ExecMode mode) = 0;

    virtual void executeFrame() = 0;
    virtual int64_t cycles() const = 0;
    virtual int64_t machineCycles() const = 0;
    virtual int64_t frames() const = 0;
    virtual int frameRate() const = 0;
    virtual bool supportsFrameBoost() const { return true; }

    virtual uint8_t* memory() = 0;
    virtual int memSize() const = 0;

    virtual bool loadData(std::span<const uint8_t> data, std::optional<uint32_t> loadAddress) = 0;

    virtual bool needsScreenUpdate() { return true; }
    virtual uint16_t getCurrentScreenWidth() const { return 0; }
    virtual uint16_t getCurrentScreenHeight() const { return 0; }
    virtual uint16_t getMaxScreenWidth() const { return 0; }
    virtual uint16_t getMaxScreenHeight() const { return 0; }
    virtual PixelRatio getPixelRatio() const { return {}; }
    virtual bool isDoublePixel() const { return false; }
    virtual const VideoType* getScreen() const { return nullptr; }
    virtual const VideoRGBAType* getScreenRGBA() const { return nullptr; }
    virtual const VideoRGBAType* getWorkRGBA() const { return nullptr; }
    virtual uint8_t getScreenAlpha() const { return 255; }
    virtual void setPalette(std::span<uint32_t> palette) {}
    virtual void renderAudio(int16_t* samples, size_t frames, int sampleFrequency) { while (frames--) *samples++ = 0; }
};

} // emu

