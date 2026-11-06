//---------------------------------------------------------------------------------------
// src/disassemblyview.cpp
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

#include <disassemblyview.hpp>

#include <algorithm>

DisassemblyView::DisassemblyView(const Config& config)
    : _config(config)
{
}

void DisassemblyView::setConfig(const Config& config)
{
    if (!(config == _config)) {
        _config = config;
        reset();
    }
}

void DisassemblyView::reset()
{
    _lines.clear();
    _anchor = 0;
}

void DisassemblyView::follow(uint32_t address)
{
    address = std::min(address, _config.addressEnd - 1);
    auto iter = std::lower_bound(_lines.begin(), _lines.end(), address, [](const Line& line, uint32_t addr) { return line.address < addr; });
    if (iter != _lines.end() && iter->address == address) {
        _anchor = static_cast<int>(iter - _lines.begin());
        return;
    }
    _lines.clear();
    _lines.push_back({address, sizeAt(address)});
    _anchor = 0;
}

void DisassemblyView::scroll(int lines)
{
    if (_lines.empty())
        return;
    if (lines < 0) {
        extendAbove(-lines);
        _anchor = std::max(0, _anchor + lines);
    }
    else if (lines > 0) {
        extendBelow(lines);
        _anchor = std::min(static_cast<int>(_lines.size()) - 1, _anchor + lines);
    }
}

void DisassemblyView::update(int linesAbove, int linesBelow)
{
    if (_lines.empty())
        return;
    validate(linesAbove, linesBelow);
    extendAbove(linesAbove);
    extendBelow(linesBelow);
    trim();
}

std::vector<DisassemblyView::Line> DisassemblyView::linesAbove(int count) const
{
    const int first = std::max(0, _anchor - count);
    return {_lines.begin() + first, _lines.begin() + _anchor};
}

std::vector<DisassemblyView::Line> DisassemblyView::resolveBefore(uint32_t address, int count) const
{
    std::vector<Line> result;
    if (!address || count <= 0)
        return result;

    const uint32_t window = std::min(address, static_cast<uint32_t>(count) * _config.unitsPerLine + _config.leadIn);
    const uint32_t base = address - window;
    std::vector<uint32_t> next(window);
    std::vector<uint8_t> lands(window);
    std::vector<uint32_t> support(window);
    for (uint32_t i = window; i-- > 0;) {
        const auto size = sizeAt(base + i);
        next[i] = i + size;
        lands[i] = (next[i] == window || (next[i] < window && lands[next[i]])) && !coversKnownStart(base + i, size);
    }
    for (uint32_t i = 0; i < window; ++i) {
        if (lands[i]) {
            support[i] += isKnownStart(base + i) ? window + 1 : 1;
            if (next[i] < window)
                support[next[i]] += support[i];
        }
    }
    uint32_t target = window;
    while (static_cast<int>(result.size()) < count && target > 0) {
        uint32_t best = target;
        const uint32_t first = target > _config.maxInstructionSize ? target - _config.maxInstructionSize : 0;
        for (uint32_t i = target; i-- > first;) {
            if (next[i] == target && lands[i] && (best == target || support[i] > support[best]))
                best = i;
        }
        if (best == target) {
            result.push_back({base + target - 1, 1, true});
            break;
        }
        result.push_back({base + best, target - best});
        target = best;
    }
    std::reverse(result.begin(), result.end());
    return result;
}

uint32_t DisassemblyView::sizeAt(uint32_t address) const
{
    return std::max(1u, _sizeFunction ? _sizeFunction(address) : 1u);
}

bool DisassemblyView::coversKnownStart(uint32_t address, uint32_t size) const
{
    if (!_knownStartFunction)
        return false;
    for (uint32_t offset = 1; offset < size; ++offset) {
        if (_knownStartFunction(address + offset))
            return true;
    }
    return false;
}

void DisassemblyView::validate(int linesAbove, int linesBelow)
{
    // A line that changed its size (the memory under it changed) breaks all lines
    // further away from the anchor, they are decoded or resolved again. Lines above
    // the anchor are also resolved again when they decode over a known start.
    // Truncated lines are kept, their size never matched the decoding.
    const int last = std::min(static_cast<int>(_lines.size()) - 1, _anchor + linesBelow);
    for (int i = _anchor; i <= last; ++i) {
        const auto size = sizeAt(_lines[i].address);
        if (!_lines[i].truncated && size != _lines[i].size) {
            _lines[i].size = size;
            _lines.erase(_lines.begin() + i + 1, _lines.end());
            break;
        }
    }
    const int first = std::max(0, _anchor - linesAbove);
    for (int i = _anchor - 1; i >= first; --i) {
        if ((!_lines[i].truncated && sizeAt(_lines[i].address) != _lines[i].size) || coversKnownStart(_lines[i].address, _lines[i].size)) {
            _lines.erase(_lines.begin(), _lines.begin() + i + 1);
            _anchor -= i + 1;
            break;
        }
    }
}

void DisassemblyView::extendAbove(int linesAbove)
{
    while (_anchor < linesAbove) {
        const auto lines = resolveBefore(_lines.front().address, std::max(linesAbove - _anchor, RESOLVE_BATCH_LINES));
        if (lines.empty())
            break;
        _lines.insert(_lines.begin(), lines.begin(), lines.end());
        _anchor += static_cast<int>(lines.size());
    }
}

void DisassemblyView::extendBelow(int linesBelow)
{
    while (static_cast<int>(_lines.size()) - 1 - _anchor < linesBelow) {
        const auto& last = _lines.back();
        const uint32_t next = last.address + last.size;
        if (next >= _config.addressEnd)
            break;
        _lines.push_back({next, sizeAt(next)});
    }
}

void DisassemblyView::trim()
{
    if (_anchor > MAX_CACHED_LINES) {
        _lines.erase(_lines.begin(), _lines.begin() + (_anchor - MAX_CACHED_LINES));
        _anchor = MAX_CACHED_LINES;
    }
    if (static_cast<int>(_lines.size()) - 1 - _anchor > MAX_CACHED_LINES) {
        _lines.erase(_lines.begin() + _anchor + 1 + MAX_CACHED_LINES, _lines.end());
    }
}
