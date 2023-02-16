//---------------------------------------------------------------------------------------
// src/systemtools.hpp
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

#include <systemtools.hpp>
#include <ghc/fs_impl.hpp>
#include <filesystem.hpp>

#include <ghc/utf8.hpp>

#include <chrono>
#include <ctime>
#include <regex>
#include <stdexcept>

#ifdef __linux__
#define HOST_OS_LINUX
#include <sys/utsname.h>
#include <fcntl.h>
#include <unistd.h>
#elif defined(__APPLE__) && defined(__MACH__)
#define HOST_OS_MACOS
#include <sys/param.h>
#include <sys/sysctl.h>
#include <fcntl.h>
#include <unistd.h>
#elif defined(_WIN32) || defined(_WIN64)
#define HOST_OS_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__EMSCRIPTEN__)
#define HOST_OS_EMSCRIPTEN
#elif defined(__unix__)
#define HOST_OS_UNIX
#include <fcntl.h>
#include <unistd.h>
#endif


namespace {

static std::string g_appName = "net.gulrak.cadmium";
static std::string g_dataPath;

static std::string getSysEnv(const std::string& name)
{
#ifdef _WIN32
    std::wstring result;
    result.resize(65535);
    auto size = ::GetEnvironmentVariableW(ghc::utf8::toWString(name).c_str(), &result[0], 65535);
    result.resize(size);
    return ghc::utf8::toUtf8(result);
#elif !defined(HOST_OS_EMSCRIPTEN)
    auto env = ::getenv(name.c_str());
    return env ? std::string(env) : "";
#else
    return "";
#endif
}

static std::string getOS()
{
#ifdef HOST_OS_LINUX
    struct utsname unameData;
    int LinuxInfo = uname(&unameData);
    return std::string(unameData.sysname) + " " + unameData.release;
#endif

#ifdef HOST_OS_WINDOWS
    OSVERSIONINFOEX info;
    ZeroMemory(&info, sizeof(OSVERSIONINFOEX));
    info.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    GetVersionEx((LPOSVERSIONINFO)&info);
    return std::string("Windows ") + std::to_string(info.dwMajorVersion) + "." + std::to_string(info.dwMinorVersion);
#endif

#ifdef HOST_OS_MACOS
    const std::regex version(R"((\d+).(\d+).(\d+))");
    std::smatch match;
    int mib[] = {CTL_KERN, KERN_OSRELEASE};
    size_t len;
    char* kernelVersion = NULL;

    if (::sysctl(mib, 2, NULL, &len, NULL, 0) < 0) {
        fprintf(stderr, "%s: Error during sysctl probe call!\n", __PRETTY_FUNCTION__);
        fflush(stdout);
        exit(4);
    }

    kernelVersion = static_cast<char*>(malloc(len));
    if (kernelVersion == NULL) {
        fprintf(stderr, "%s: Error during malloc!\n", __PRETTY_FUNCTION__);
        fflush(stdout);
        exit(4);
    }
    if (::sysctl(mib, 2, kernelVersion, &len, NULL, 0) < 0) {
        fprintf(stderr, "%s: Error during sysctl get verstring call!\n", __PRETTY_FUNCTION__);
        fflush(stdout);
        exit(4);
    }
    std::string kernel = kernelVersion;
    free(kernelVersion);
    if(std::regex_match(kernel, match, version)) {
        auto kernelMajor = std::stoi(match[1].str());
        if(kernelMajor >= 20) {
            return "macOS " + std::to_string(kernelMajor-9) + ".x";
        }
        else {
            return std::string("macOS 10.") + std::to_string(kernelMajor-4);
        }
    }
    return "unknown macOS";
#endif

#ifdef HOST_OS_EMSCRIPTEN
    return "Emscripten";
#endif
}

static void append2Digits(std::string& s, unsigned val)
{
    if(val < 10) {
        s += '0';
        s += (char)('0'+val);
    }
    else {
        auto v1 = val / 10;
        auto v2 = val % 10;
        s += (char)('0'+v1);
        s += (char)('0'+v2);
    }
}

}

std::string appName()
{
    if(g_appName.empty()) {
        throw std::runtime_error("No application name set!");
    }
    return g_appName;
}

std::string userAgent()
{
    static std::string userAgent = "Cadmium/1 (" + getOS() + ") " + CADMIUM_VERSION;
    return userAgent;
}

int64_t currentTime()
{
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string formattedDuration(int64_t seconds)
{
    std::string result;
    result.reserve(9);
    if(seconds < 0) {
        seconds = std::abs(seconds);
        result += '-';
    }
    else {
        if(seconds / 3600 < 10) {
            result += '0';
        }
    }
    result += std::to_string(seconds / 3600 );
    result +=  ':';
    append2Digits(result, (unsigned)((seconds % 3600)/60));
    result +=  ':';
    append2Digits(result, (unsigned)(seconds % 60));
    return result;
}

std::string formattedDate(int64_t unixTimestamp)
{
    std::string result;
    struct std::tm* ti = nullptr;
    std::time_t t = unixTimestamp;
    result.reserve(9);
    ti = localtime (&t);
    append2Digits(result, 19+ti->tm_year/100);
    append2Digits(result, ti->tm_year%100);
    result += '-';
    append2Digits(result, ti->tm_mon+1);
    result += '-';
    append2Digits(result, ti->tm_mday);
    return result;
}

void dataPath(const std::string& path)
{
    g_dataPath = path;
}

std::string dataPath()
{
    if(!g_dataPath.empty()) {
        fs::create_directories(g_dataPath);
        return g_dataPath;
    }
    std::string dir;
#ifdef HOST_OS_WINDOWS
    auto localAppData = getSysEnv("LOCALAPPDATA");
    if (!localAppData.empty()) {
        dir = (fs::path(localAppData) / appName()).string();
    }
    else {
        throw std::runtime_error("Need %LOCALAPPDATA% to create configuration directory!");
    }
#else
    auto home = ::getenv("HOME");
    if(home) {
#ifdef HOST_OS_MACOS
        dir = fs::path(home) / "Library/Application Support" / appName();
#elif defined(HOST_OS_LINUX)
        dir = fs::path(home) / ".local/share" / appName();
#elif defined(HOST_OS_EMSCRIPTEN)
        throw std::runtime_error("Web client does not have an application directory!");
#else
#error "Unsupported OS!"
#endif
    }
    else {
        throw std::runtime_error("Need $HOME to create configuration directory!");
    }
#endif
    fs::create_directories(dir);
    return dir;
}

bool isInstanceRunning()
{
#if defined(HOST_OS_WINDOWS) || defined(HOST_OS_EMSCRIPTEN)
    return false;
#else
    auto lockFile = fs::path(dataPath()) / (appName() + ".pid");
    auto fd = open(lockFile.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        throw std::runtime_error("Couldn't open lock file: " + lockFile.string());
    }
    else {
        //auto pid = std::to_string(::getpid());
        //write(fd, pid.c_str(), pid.size());
    }
    struct ::flock fl;
    fl.l_start = 0;
    fl.l_len = 0;
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    auto rc = ::fcntl(fd, F_SETLK, &fl);
    return rc == -1 ? true : false;
#endif
}

int openURL(std::string url)
{
    std::string command;
#ifdef HOST_OS_MACOS
    command = "open";
#elif defined(HOST_OS_WINDOWS)
    command = "start";
#elif defined(HOST_OS_LINUX) || defined(HOST_OS_UNIX)
    command = "xdg-open";
#endif
    return system((command + " " + url).c_str());
}
