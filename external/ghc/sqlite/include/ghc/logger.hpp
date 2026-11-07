//---------------------------------------------------------------------------------------
// ghc/logger.hpp
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

#ifndef GHC_CUSTOM_LOGGER_PROVIDED
#include <fstream>
#include <sstream>
#include <thread>
#include <string>
#ifdef USE_FMTLIB_POLYFILL
#include <fmt/core.h>
namespace std {
using fmt::format;
using fmt::formatter;
}
// Formatter for std::thread::id with fmt library
template <> struct fmt::formatter<std::thread::id> : fmt::formatter<std::string> {
    auto format(const std::thread::id& id, fmt::format_context& ctx) const {
        std::ostringstream oss;
        oss << id;
        return fmt::formatter<std::string>::format(oss.str(), ctx);
    }
};
#else
#include <format>
// Formatter for std::thread::id with std::format
template <> struct std::formatter<std::thread::id> : std::formatter<std::string> {
    auto format(const std::thread::id& id, std::format_context& ctx) const {
        std::ostringstream oss;
        oss << id;
        return std::formatter<std::string>::format(oss.str(), ctx);
    }
};
#endif


namespace ghc {

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
    static void log(LogLevel lvl, const char* msg)
    {
        if (_logFile.is_open()) {
            _logFile << "[" << getLogLevelName(lvl) << "]" << msg << std::endl;
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

protected:
    virtual void doLog(LogLevel lvl, const char* msg) {}

private:
    static inline Logger* _logger{nullptr};
#ifndef PLATFORM_WEB
    static inline std::ofstream _logFile;
#endif
};

}

#define INFO_LOG(fmt_str, ...) do { ghc::Logger::log(ghc::LLV_INFO, std::format(fmt_str, ##__VA_ARGS__).c_str()); } while (0)
#define ERROR_LOG(fmt_str, ...) do { ghc::Logger::log(ghc::LLV_ERROR, std::format(fmt_str, ##__VA_ARGS__).c_str()); } while (0)
#define WARNING_LOG(fmt_str, ...) do { ghc::Logger::log(ghc::LLV_WARNING, std::format(fmt_str, ##__VA_ARGS__).c_str()); } while (0)
#ifndef NDEBUG
#define DEBUG_LOG(fmt_str, ...) do { ghc::Logger::log(ghc::LLV_DEBUG, std::format(fmt_str, ##__VA_ARGS__).c_str()); } while (0)
#else
#define DEBUG_LOG(fmt_str, ...) do { } while (0)
#endif

#else
#include GHC_CUSTOM_LOGGER_PROVIDED
#endif
