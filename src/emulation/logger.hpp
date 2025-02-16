//---------------------------------------------------------------------------------------
// src/logger.hpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2022, Steffen Schümann <s.schuemann@pobox.com>
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
#define FULL_CONSOLE_TRACE
//#include <emulation/config.hpp>

#include <cstdint>


namespace emu {

class Logger
{
public:
    enum Source { eHOST, eCHIP8, eBACKEND_EMU };
    struct FrameTime {
        FrameTime(int f, int c) : frame(f & 0xff), cycle(c) {}
        uint32_t frame:16;
        uint32_t cycle:16;
    };
    virtual ~Logger() = default;
    static void setLogger(Logger* logger) { _logger = logger; }
    static void log(Source source, uint64_t cycle, FrameTime frameTime, const char* msg)
    {
        if(_logger) {
            _logger->doLog(source, cycle, frameTime, msg);
        }
    }

    virtual void doLog(Source source, uint64_t cycle, FrameTime frameTime, const char* msg) = 0;

private:
    static inline Logger* _logger{nullptr};
};

}

#ifdef ENABLE_CONSOLE_LOGGER
#include <iostream>
#include <fmt/format.h>

namespace emu {

class ConsoleLogger : public Logger
{
public:
    ConsoleLogger(std::ostream& outStream)
    : _outStream(outStream)
    {
        setLogger(this);
    }

    ~ConsoleLogger() override
    {
        setLogger(nullptr);
    }

    void doLog(Source source, uint64_t cycle, FrameTime frameTime, const char* msg) override
    {
        auto content = source != eHOST ? fmt::format("[{:02x}:{:04x}] {}", (int)frameTime.frame, (int)frameTime.cycle, msg) : fmt::format("[      ] {}", msg);
        _outStream << content << std::endl;
    }

private:
    std::ostream& _outStream;
};

}

#endif
