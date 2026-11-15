//---------------------------------------------------------------------------------------
// src/headlessrun.hpp
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

#include <emulation/properties.hpp>

#include <cstdint>
#include <string>
#include <vector>

struct KeyEvent
{
    int64_t frame{0};     // first frame (counted from 0) the keys are down
    int64_t hold{0};      // number of frames they stay down
    uint16_t keys{0};     // bit n set: CHIP-8 key n
};

class KeyScript
{
public:
    static constexpr int64_t DEFAULT_HOLD_FRAMES = 4;
    // Parses a list of "[+]<frame>:<hex keys>[+<frames held>]" separated by spaces or commas: "60:1 +30:16+300"
    // holds key 1 from frame 60 for 4 frames, then keys 1 and 6 together for 300 frames, starting 30 frames after
    // the release of the previous keys; throws std::invalid_argument on a syntax error.
    static KeyScript parse(const std::string& script);
    bool empty() const { return _events.empty(); }
    const std::string& text() const { return _text; }
    uint16_t keysDownAt(int64_t frame) const;
    int64_t endFrame() const;
    const std::vector<KeyEvent>& events() const { return _events; }

private:
    std::string _text;
    std::vector<KeyEvent> _events;
};

struct HeadlessRunOptions
{
    static constexpr int64_t DEFAULT_MAX_INSTRUCTIONS = 3'000'000;
    static constexpr int64_t DEFAULT_MAX_FRAMES = 3600;
    int64_t untilPresentations{-1};
    int64_t maxInstructions{DEFAULT_MAX_INSTRUCTIONS};
    bool maxInstructionsGiven{false};
    int64_t maxFrames{-1};
    int64_t dumpScale{0};
    std::string keys;
    std::string dumpScreen;
    std::string dumpState;
};

int runHeadless(const std::string& romPath, const emu::Properties& properties, const HeadlessRunOptions& options);
