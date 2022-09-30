//---------------------------------------------------------------------------------------
// ghc/utf8.hpp
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

#include <cstdint>
#include <string>

namespace ghc {
namespace utf8 {

inline void append(std::string& str, uint32_t unicode)
{
    if (unicode <= 0x7f) {
        str.push_back(static_cast<char>(unicode));
    }
    else if (unicode >= 0x80 && unicode <= 0x7ff) {
        str.push_back(static_cast<char>((unicode >> 6) + 192));
        str.push_back(static_cast<char>((unicode & 0x3f) + 128));
    }
    else if ((unicode >= 0x800 && unicode <= 0xd7ff) || (unicode >= 0xe000 && unicode <= 0xffff)) {
        str.push_back(static_cast<char>((unicode >> 12) + 224));
        str.push_back(static_cast<char>(((unicode & 0xfff) >> 6) + 128));
        str.push_back(static_cast<char>((unicode & 0x3f) + 128));
    }
    else if (unicode >= 0x10000 && unicode <= 0x10ffff) {
        str.push_back(static_cast<char>((unicode >> 18) + 240));
        str.push_back(static_cast<char>(((unicode & 0x3ffff) >> 12) + 128));
        str.push_back(static_cast<char>(((unicode & 0xfff) >> 6) + 128));
        str.push_back(static_cast<char>((unicode & 0x3f) + 128));
    }
    else {
        append(str, 0xfffd);
    }
}

namespace detail {
// Thanks to Bjoern Hoehrmann (https://bjoern.hoehrmann.de/utf-8/decoder/dfa/)
// and Taylor R Campbell for the ideas to this DFA approach of UTF-8 decoding;
// Generating debugging and shrinking my own DFA from scratch was a day of fun!
enum utf8_states_t { S_STRT = 0, S_RJCT = 8 };
inline unsigned consumeUtf8Fragment(const unsigned state, const uint8_t fragment, uint32_t& codepoint)
{
    static const uint32_t utf8_state_info[] = {
        // encoded states
        0x11111111u, 0x11111111u, 0x77777777u, 0x77777777u, 0x88888888u, 0x88888888u, 0x88888888u, 0x88888888u, 0x22222299u, 0x22222222u, 0x22222222u, 0x22222222u, 0x3333333au, 0x33433333u, 0x9995666bu, 0x99999999u,
        0x88888880u, 0x22818108u, 0x88888881u, 0x88888882u, 0x88888884u, 0x88888887u, 0x88888886u, 0x82218108u, 0x82281108u, 0x88888888u, 0x88888883u, 0x88888885u, 0u,          0u,          0u,          0u,
    };
    uint8_t category = fragment < 128 ? 0 : (utf8_state_info[(fragment >> 3) & 0xf] >> ((fragment & 7) << 2)) & 0xf;
    codepoint = (state ? (codepoint << 6) | (fragment & 0x3fu) : (0xffu >> category) & fragment);
    return state == S_RJCT ? static_cast<unsigned>(S_RJCT) : static_cast<unsigned>((utf8_state_info[category + 16] >> (state << 2)) & 0xf);
}
}

inline bool isValid(const std::string& utf8String)
{
    std::string::const_iterator iter = utf8String.begin();
    unsigned utf8_state = detail::S_STRT;
    std::uint32_t codepoint = 0;
    while (iter < utf8String.end()) {
        if ((utf8_state = detail::consumeUtf8Fragment(utf8_state, static_cast<uint8_t>(*iter++), codepoint)) == detail::S_RJCT) {
            return false;
        }
    }
    if (utf8_state) {
        return false;
    }
    return true;
}

inline uint32_t increment(const char*& iter, const char* end)
{
    unsigned utf8_state = detail::S_STRT;
    std::uint32_t codepoint = 0;
    while (iter != end) {
        if ((utf8_state = detail::consumeUtf8Fragment(utf8_state, (uint8_t)*iter++, codepoint)) == detail::S_STRT) {
            return codepoint;
        }
        else if (utf8_state == detail::S_RJCT) {
            return 0xfffd;
        }
    }
    return 0xfffd;
}

inline uint32_t fetchCodepoint(const char*& text, const char* end)
{
    return increment(text, end);
}

inline int length(const char* iter, const char* end)
{
    int len = 0;
    while (iter < end) {
        increment(iter, end);
        ++len;
    }
    return len;
}

inline int length(const std::string& text)
{
    return length(text.data(), text.data() + text.size());
}

inline std::wstring toWString(const std::string& utf8String)
{
    std::wstring result;
    result.reserve(utf8String.length());
    auto iter = utf8String.cbegin();
    unsigned utf8_state = detail::S_STRT;
    std::uint32_t codepoint = 0;
    while (iter < utf8String.cend()) {
        if ((utf8_state = detail::consumeUtf8Fragment(utf8_state, static_cast<uint8_t>(*iter++), codepoint)) == detail::S_STRT) {
            if (codepoint <= 0xffff) {
                result += static_cast<typename std::wstring::value_type>(codepoint);
            }
            else {
                codepoint -= 0x10000;
                result += static_cast<typename std::wstring::value_type>((codepoint >> 10) + 0xd800);
                result += static_cast<typename std::wstring::value_type>((codepoint & 0x3ff) + 0xdc00);
            }
            codepoint = 0;
        }
        else if (utf8_state == detail::S_RJCT) {
            result += static_cast<typename std::wstring::value_type>(0xfffd);
            utf8_state = detail::S_STRT;
            codepoint = 0;
        }
    }
    if (utf8_state) {
        result += static_cast<typename std::wstring::value_type>(0xfffd);
    }
    return result;
}

inline std::string heuristicUtf8(const std::string& str)
{
    if(utf8::isValid(str)) {
        return str;
    }
    else {
        std::string result;
        result.reserve(str.size());
        for(char c : str) {
            utf8::append(result, static_cast<uint8_t>(c));
        }
        return result;
    }
}

}  // namespace utf8
} // namespace ghc
