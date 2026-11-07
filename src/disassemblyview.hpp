//---------------------------------------------------------------------------------------
// src/disassemblyview.hpp
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
#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <vector>

/// Keeps the instruction boundaries of a live disassembly view stable.
///
/// The view is a cached list of lines around an anchor line. The anchor is always
/// an instruction start (the PC or a line reached by scrolling), lines below it are
/// decoded forward, lines above it are resolved backwards so that they end exactly
/// on the first known line. Scrolling moves through the cached lines, so lines
/// already shown don't change unless the memory under them does.
///
/// Addresses and sizes are in the address units of the CPU (bytes, or nibbles for
/// the Saturn). The class doesn't know the CPU, it only needs the instruction size.
class DisassemblyView
{
public:
    using SizeFunction = std::function<uint32_t(uint32_t address)>;
    using KnownStartFunction = std::function<bool(uint32_t address)>;
    struct Line
    {
        uint32_t address{};
        uint32_t size{};
        bool truncated{false};  ///< shorter than its decoding, only happens in data
    };
    struct Config
    {
        uint32_t addressEnd{0x10000};      ///< first address after the address space
        uint32_t maxInstructionSize{4};    ///< longest possible instruction
        uint32_t unitsPerLine{4};          ///< backwards window per line to resolve
        uint32_t leadIn{32};               ///< extra window to let the decoding converge
        bool operator==(const Config&) const = default;
    };
    static constexpr int MAX_CACHED_LINES = 512;
    static constexpr int RESOLVE_BATCH_LINES = 32;

    DisassemblyView() = default;
    explicit DisassemblyView(const Config& config);

    void setConfig(const Config& config);
    void setSizeFunction(SizeFunction sizeFunction) { _sizeFunction = std::move(sizeFunction); }
    /// Optional hint for addresses known to be instruction starts, like executed ones
    void setKnownStartFunction(KnownStartFunction knownStartFunction) { _knownStartFunction = std::move(knownStartFunction); }
    void reset();
    bool empty() const { return _lines.empty(); }

    /// Make address the anchor, keeping the cached lines if it is one of their starts
    void follow(uint32_t address);
    /// Move the anchor by the given number of lines, positive is towards higher addresses
    void scroll(int lines);
    /// Revalidate and complete the lines around the anchor, call once per frame before reading them
    void update(int linesAbove, int linesBelow);

    const Line& anchor() const { return _lines[_anchor]; }
    /// Up to count lines directly above the anchor, in ascending order
    std::vector<Line> linesAbove(int count) const;

    /// Resolve up to count lines that end exactly at address, in ascending order
    std::vector<Line> resolveBefore(uint32_t address, int count) const;

private:
    uint32_t sizeAt(uint32_t address) const;
    bool isKnownStart(uint32_t address) const { return _knownStartFunction && _knownStartFunction(address); }
    bool coversKnownStart(uint32_t address, uint32_t size) const;
    void validate(int linesAbove, int linesBelow);
    void extendAbove(int linesAbove);
    void extendBelow(int linesBelow);
    void trim();
    Config _config;
    SizeFunction _sizeFunction;
    KnownStartFunction _knownStartFunction;
    std::deque<Line> _lines;
    int _anchor{0};
};
