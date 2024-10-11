//---------------------------------------------------------------------------------------
// src/emulation/utility.hpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2015, Steffen Schümann <s.schuemann@pobox.com>
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

#include <algorithm>
#include <string>
#include <sstream>
#include <vector>
#include <fstream>
#include <cmath>
#include <new>

#include <ghc/fs_fwd.hpp>
#include <sha1/sha1.hpp>
#include <fmt/format.h>
#include <magic/magic_enum.hpp>

namespace emu {

namespace fs = ghc::filesystem;

inline bool endsWith(const std::string& text, const std::string& suffix)
{
    return text.size() >= suffix.size() && 0 == text.compare(text.size() - suffix.size(), suffix.size(), suffix);
}

inline bool startsWith(const std::string& text, const std::string& prefix)
{
    return text.size() >= prefix.size() && 0 == text.compare(0, prefix.size(), prefix);
}

inline std::string trimLeft(std::string s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
    return s;
}

inline std::string trimRight(std::string s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
    return s;
}

inline std::string trim(std::string s)
{
    return trimRight(trimLeft(s));
}

inline std::string trimMultipleSpaces(std::string s)
{
    auto result = s;
    std::string::iterator end = std::unique(result.begin(), result.end(), [](char lhs, char rhs) { return (lhs == rhs) && (lhs == ' '); });
    result.erase(end, result.end());
    return result;
}

inline std::string toLower(std::string s)
{
    auto result = s;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return std::tolower(c); });
    return result;
}

inline std::string toUpper(std::string s)
{
    auto result = s;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c){ return std::toupper(c); });
    return result;
}

template <typename OutIter>
inline void split(const std::string& s, char delimiter, OutIter result)
{
    std::istringstream is(s);
    std::string part;
    while (std::getline(is, part, delimiter)) {
        *result++ = part;
    }
}

inline std::vector<std::string> split(const std::string& s, char delimiter)
{
    std::vector<std::string> result;
    split(s, delimiter, std::back_inserter(result));
    return result;
}

inline std::vector<uint8_t> loadFile(const std::string& file, size_t maxSize)
{
    std::ifstream is(file, std::ios::binary | std::ios::ate);
    auto size = is.tellg();
    is.seekg(0, std::ios::beg);
    if(size > maxSize) {
        return {};
    }
    std::vector<uint8_t> buffer(size);
    if (is.read((char*)buffer.data(), size)) {
        return buffer;
    }
    return {};
}

inline bool writeFile(const std::string& filename, const char* data, size_t size)
{
    std::ofstream os(filename, std::ios::binary | std::ios::trunc);
    if (os.write(data, size))
        return true;
    return false;
}

inline bool writeFile(const std::string& filename, const uint8_t* data, size_t size)
{
    return writeFile(filename, (const char*)data, size);
}

inline std::string loadTextFile(const std::string& file)
{
    std::ifstream is(file, std::ios::binary | std::ios::ate);
    std::streamsize size = is.tellg();
    is.seekg(0, std::ios::beg);

    std::string result(size, '\0');
    if (is.read(result.data(), size)) {
        return result;
    }

    return {};
}

static inline bool isDigit(char32_t c)
{
    return c >= '0' && c <= '9';
}

static inline bool isHexDigit(char32_t c)
{
    return isDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static inline uint16_t opcodeFromPattern(const std::string& pattern)
{
    uint16_t opcode = 0;
    for (auto c : pattern) {
        opcode <<= 4;
        if (isHexDigit(c))
            opcode += (c >= '0' && c <= '9') ? c - '0' : std::toupper(c) - 'A' + 10;
    }
    return opcode;
}

static inline uint16_t maskFromPattern(const std::string& pattern)
{
    uint16_t opcode = 0;
    for (auto c : pattern) {
        opcode <<= 4;
        if (isHexDigit(c))
            opcode += 15;
    }
    return opcode;
}

static inline bool comparePattern(const std::string& pattern, const std::string& opcode)
{
    int i;
    for (i = 0; i < 4; ++i) {
        if (isHexDigit(pattern[i]) && std::toupper(pattern[i]) != opcode[i])
            break;
    }
    return i == 4;
}

class byte_range
{
public:
    byte_range()
        : _data(nullptr)
        , _size(0)
    {
    }
    byte_range(uint8_t* data, size_t size)
        : _data(data)
        , _size(size)
    {
    }
    byte_range(uint8_t* data, uint8_t* end)
        : _data(data)
        , _size(end - data)
    {
    }

    bool empty() const { return _size == 0; }
    uint8_t* data() { return _data; }
    const uint8_t* data() const { return _data; }
    size_t size() const { return _size; }

    const uint8_t* begin() const { return _data; }
    const uint8_t* end() const { return _data + _size; }

private:
    uint8_t* _data;
    size_t _size;
};

inline std::string formatUnit(double val, const std::string& suffix, int minScale = -1)
{
    static const char* prefix[] = {"n", "u", "m", "", "k", "M", "G", "T"};
    if(std::isnan(val))
        return "";
    bool isNeg = val < 0;
    val = std::abs(val);
    if (val < 0.000000001)
        return "0" + suffix;
    auto scale = std::max(int(std::log10(val) - (val < 10.0 ? 4 : 1)) / 3, minScale);
    if (scale >= -3 && scale <= 4) {
        auto scaledVal = val / std::pow(10.0, scale * 3);
        return (isNeg ? "-" : "") + std::to_string(static_cast<int>(scaledVal + 0.5)) + prefix[scale + 3] + suffix;
    }
    return "<err>";
}

inline std::string calculateSha1Hex(const uint8_t* data, size_t size)
{
    char hex[SHA1_HEX_SIZE];
    Sha1 sum;
    sum.add(data, size);
    sum.finalize();
    sum.print_hex(hex);
    return hex;
}

inline std::string calculateSha1Hex(const std::string& str)
{
    char hex[SHA1_HEX_SIZE];
    Sha1 sum;
    sum.add(str.data(), str.size());
    sum.finalize();
    sum.print_hex(hex);
    return hex;
}

}
