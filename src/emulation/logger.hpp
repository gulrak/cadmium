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
#include <fmt/format.h>
#include <fstream>
#include <string>
#include <cstdint>

extern "C" {
extern void TraceLog(int logLevel, const char *text, ...);
}

namespace emu {

enum LogLevel {
    LLV_ALL = 0,        // Display all logs
    LLV_TRACE,          // Trace logging, intended for internal use only
    LLV_DEBUG,          // Debug logging, used for internal debugging, it should be disabled on release builds
    LLV_INFO,           // Info logging, used for program execution info
    LLV_WARNING,        // Warning logging, used on recoverable failures
    LLV_ERROR,          // Error logging, used on unrecoverable failures
    LLV_FATAL,          // Fatal logging, used to abort program: exit(EXIT_FAILURE)
    LLV_NONE            // Disable logging
};

class Logger
{
public:
    enum Source { eHOST, eCHIP8, eBACKEND_EMU };
    enum TraceFormat { eCADMIUM1, eCADMIUM2 };
    struct FrameTime {
        FrameTime(int f, int c) : frame(f & 0xff), cycle(c) {}
        uint32_t frame:16;
        uint32_t cycle:16;
    };
    explicit Logger(const std::string& dataPath)
    {
#ifndef PLATFORM_WEB
        if (!dataPath.empty() && !_logFile.is_open()) {
            _logFile.open((dataPath + "/logfile.txt").c_str());
        }
#endif
    }
    virtual ~Logger() = default;
    static void setLogger(Logger* logger) { _logger = logger; }
    static void setTraceFile(const std::string& traceFile)
    {
        if (_traceFile.is_open()) {
            _traceFile.close();
        }
        if (!traceFile.empty()) {
            _traceFile.open(traceFile.c_str());
        }
    }
    static void setTraceFormat(TraceFormat format)
    {
        _traceFormat = format;
    }
    static TraceFormat getTraceFormat()
    {
        return _traceFormat;
    }
    static void log(Source source, uint64_t cycle, FrameTime frameTime, const char* msg)
    {
        if (_logFile.is_open()) {
            if (source == eHOST) {
                _logFile << fmt::format("[      ] {}", msg) << std::endl;
            }
            else {
                if (_traceFile.is_open()) {
                    _traceFile << fmt::format("[{:08x}] {}", cycle, msg) << std::endl;
                }
            }
        }
        if(_logger) {
            _logger->doLog(source, cycle, frameTime, msg);
        }
    }
    static void log(LogLevel lvl, const char* msg)
    {
        if (_logFile.is_open()) {
            _logFile << fmt::format("[{}] {}", getLogLevelName(lvl), msg) << std::endl;
        }
        if(_logger) {
            _logger->doLog(lvl, msg);
        }
    }
    static const char* getLogLevelName(LogLevel lvl)
    {
        switch(lvl) {
        case LLV_TRACE: return "TRACE";
        case LLV_DEBUG: return "DEBUG";
        case LLV_INFO: return "INFO";
        case LLV_WARNING: return "WARNING";
        case LLV_ERROR: return "ERROR";
        case LLV_FATAL: return "FATAL";
        default: return "       ";
        }
    }

    virtual void doLog(Source source, uint64_t cycle, FrameTime frameTime, const char* msg) = 0;
    virtual void doLog(LogLevel lvl, const char* msg) = 0;

private:
    static inline Logger* _logger{nullptr};
#ifndef PLATFORM_WEB
    static inline std::ofstream _logFile;
    static inline std::ofstream _traceFile;
    static inline TraceFormat _traceFormat{eCADMIUM2};
#endif
};

}



#ifdef ENABLE_CONSOLE_LOGGER
namespace emu {

class ConsoleLogger : public Logger
{
public:
    ConsoleLogger(std::ostream& outStream)
    : Logger("")
    , _outStream(outStream)
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
    void doLog(LogLevel lvl, const char* msg) override
    {
        auto content = fmt::format("[{}] {}", Logger::getLogLevelName(lvl), msg);
        _outStream << content << std::endl;
    }

private:
    std::ostream& _outStream;
};

}

#endif

#define INFO_LOG(fmt_str, ...) do { emu::Logger::log(emu::LLV_INFO, fmt::format(fmt_str, ##__VA_ARGS__).c_str()); } while (0)
#define ERROR_LOG(fmt_str, ...) do { emu::Logger::log(emu::LLV_ERROR, fmt::format(fmt_str, ##__VA_ARGS__).c_str()); } while (0)
#define WARNING_LOG(fmt_str, ...) do { emu::Logger::log(emu::LLV_WARNING, fmt::format(fmt_str, ##__VA_ARGS__).c_str()); } while (0)
#ifndef NDEBUG
#define DEBUG_LOG(fmt_str, ...) do { emu::Logger::log(emu::LLV_DEBUG, fmt::format(fmt_str, ##__VA_ARGS__).c_str()); } while (0)
#else
#define DEBUG_LOG(fmt_str, ...) do { } while (0)
#endif
