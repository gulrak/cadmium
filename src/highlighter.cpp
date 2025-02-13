//---------------------------------------------------------------------------------------
// highlighter.cpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2025, Steffen Schümann <s.schuemann@pobox.com>
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

#include "highlighter.hpp"
#include <chiplet/octocompiler.hpp>
#include <chiplet/utility.hpp>

#include <ghc/utf8.hpp>
#include <rlguipp/rlguipp.hpp>
#include <fast_float/fast_float.h>

#include <cctype>
#include <charconv>

namespace utf8 = ghc::utf8;

static std::unordered_set<std::string> g_octoOpcodes = {
    "!=", "&=", "+=", "-=", "-key", ":", ":=", ";", "<", "<<=", "<=", "=-", "==", ">", ">=", ">>=", "^=", "|=",
    "again", "audio", "bcd", "begin", "bighex", "buzzer", "clear", "delay", "else", "end", "hex", "hires", "if",
    "jump", "jump0", "key", "load", "loadflags", "loop", "lores", "native", "pitch", "plane", "random", "return",
    "save", "saveflags", "scroll-down", "scroll-left", "scroll-right", "scroll-up", "sprite", "then", "while"
};
static std::unordered_set<std::string> g_octoDirectives = {
    ":alias", ":assert", ":breakpoint", ":byte", ":calc", ":call", ":const", ":macro", ":monitor", ":next", ":org",
    ":pointer", ":proto", ":stringmode", ":unpack"
};

static std::unordered_set<std::string> g_chipperOpcodes = {
    "add", "add", "add", "alpha", "and", "bmode", "call", "ccol", "cls", "digisnd", "drw", "exit", "high",
    "jp", "jp", "jp", "ld", "ldhi", "ldpal", "low", "megaoff", "megaon", "or", "ret", "rnd", "scd", "scl", "scr",
    "scru", "scu", "se", "se", "shl", "shr", "sknp", "skp", "sne", "sprh", "sprw", "stopsnd", "sub", "subn", "xor"
};
static std::unordered_set<std::string> g_chipperDirectives = {
    "align", "da", "db", "define", "ds", "dw", "else", "end", "endif", "ifdef", "ifund", "include", "option",
    "binary", "chip8", "chip48", "hpasc", "hpbin", "schip10", "schip11", "string",
    "org", "undef", "used", "on", "off", "symbol", "xref", "yes", "no"
};

static std::unordered_set<std::string> g_1802Opcodes = {
    "ldn", "inc", "dec", "br", "bq", "bdf", "b1", "b2", "b3", "b4", "skp", "bnq", "bnz", "bnf",
    "bn1", "bn2", "bn3", "bn4", "lda", "str", "irx", "out", "inp", "ret", "dis", "ldxa", "stxd",
    "adc", "sdb", "shrc", "smb", "sav", "mark", "seq", "req", "adci", "sdbi", "shlc", "smbi",
    "glo", "ghi", "plo", "phi", "lbr", "lbq", "lbz", "lbdf", "nop", "lsnq", "lsnz", "lsnf", "lskp"
    "lbnq", "lbnz", "lbnf", "lsie", "lsq", "lsz", "lsdf", "sep", "sex", "ldx", "or", "and", "xor",
    "add", "sd", "shr", "sm", "ldi", "ori", "ani", "xri", "adi", "sdi", "shl", "smi", "illegal"
};
static std::unordered_set<std::string> g_1802Directives = {
};

Highlighter::Highlighter() = default;

void Highlighter::highlightLine(const char* text, const char* end)
{
    switch (_dialect) {
        case eCHIP8OCTO: highlightLineOcto(text, end); break;
        case eCHIP8CHIPPER: highlightLineChipper(text, end); break;
        case eCDP1802: highlightLine1802(text, end); break;
        case eM6800: highlightLine6800(text, end); break;
        case eNONE: break;
    }
}


static std::pair<std::size_t, double> parseChipperNumber(const char* text, const char* end)
{
    if (text == end) {
        return {0, 0};
    }
    int base = 10;
    const char* p = text;
    if (*p == '#') {
        base = 16;
        ++p;
    } else if (*p == '@') {
        base = 8;
        ++p;
    } else if (*p == '$') {
        base = 2;
        ++p;
    } else if (!std::isdigit(static_cast<unsigned char>(*p))) {
        return {0, 0};
    }
    if (p == end) {
        return {0, 0};
    }

    std::uint32_t result = 0;
    const char* digitStart = p;
    while (p != end) {
        char ch = *p;
        int digitValue = -1;
        if (ch >= '0' && ch <= '9') {
            digitValue = ch - '0';
        } else if (ch >= 'a' && ch <= 'f') {
            digitValue = 10 + (ch - 'a');
        } else if (ch >= 'A' && ch <= 'F') {
            digitValue = 10 + (ch - 'A');
        } else {
            break;
        }
        if (digitValue >= base)
            break;
        if (result > (std::numeric_limits<std::uint32_t>::max() - static_cast<std::uint32_t>(digitValue)) / base)
            return {0, 0};  // Overflow: number is too big for uint32_t.
        result = result * base + digitValue;
        ++p;
    }
    if (p == digitStart) {
        return {0, 0};
    }
    return {static_cast<std::size_t>(p - text), result};
}

static std::pair<std::size_t, double> parseOctoNumber(const char* text, const char* end) {
    if (text == end) {
        return {0, 0.0};
    }
    bool isInteger = false;
    int base = 10;
    const char* numStart = text;
    if (*text == '0') {
        if (text + 1 < end) {
            char c = *(text + 1);
            if (c == 'x' || c == 'X') {
                isInteger = true;
                base = 16;
                numStart = text + 2;
            } else if (c == 'b' || c == 'B') {
                isInteger = true;
                base = 2;
                numStart = text + 2;
            } else if (c == '.' || c == 'e' || c == 'E') {
                isInteger = false;
                numStart = text;
            } else if (std::isdigit(static_cast<unsigned char>(c))) {
                isInteger = true;
                base = 8;
                numStart = text;
            } else {
                isInteger = true;
                base = 10;
                numStart = text;
            }
        } else {
            isInteger = true;
            base = 10;
            numStart = text;
        }
    }
    else if (std::isdigit(static_cast<unsigned char>(*text))) {
        const char* scan = text;
        while (scan < end && std::isdigit(static_cast<unsigned char>(*scan)))
            ++scan;
        if (scan < end && (*scan == '.' || *scan == 'e' || *scan == 'E'))
            isInteger = false;
        else {
            isInteger = true;
            base = 10;
        }
        numStart = text;
    }
    else if (*text == '.') {
        if (text + 1 < end && std::isdigit(static_cast<unsigned char>(*(text + 1))))
            isInteger = false;
        else
            return {0, 0.0};
        numStart = text;
    }
    else {
        return {0, 0.0};
    }

    if (isInteger) {
        uint64_t value = 0;
        auto res = std::from_chars(numStart, end, value, base);
        if (res.ec != std::errc() || res.ptr == numStart)
            return {0, 0.0};
        return {static_cast<std::size_t>(res.ptr - text), value};
    }
    else {
        double d = 0.0;
        auto res = std::from_chars(text, end, d);
        if (res.ec != std::errc() || res.ptr == text)
            return {0, 0.0};
        return {static_cast<std::size_t>(res.ptr - text), d};
    }
}

void Highlighter::highlightLineOcto(const char* text, const char* end)
{
    static emu::OctoCompiler::Lexer lexer;
    lexer.setRange("", text, end);
    _highlighting.resize(end - text);
    size_t index = 0;
    bool wasColon = false;
    const char* token;
    while (text < end && *text != '\n') {
        token = text;
        uint32_t cp = utf8::fetchCodepoint(text, end);
        if (cp == ' ')
            ++index;
        else if (cp == '#') {
            while (text < end)
                utf8::fetchCodepoint(text, end), _highlighting[index++].front = _colors[eCOMMENT];
        }
        else {
            auto start = index++;
            bool isColon = false;
            while (text < end && *text > ' ')
                utf8::fetchCodepoint(text, end), ++index;
            auto len = index - start;
            Color col = _colors[eNORMAL];
            if (cp == ':' && len == 1 || wasColon)
                col = _colors[eLABEL], isColon = true;
            else if (cp >= '0' && cp <= '9')
                col = _colors[eNUMBER];
            else if (len == 1 && (cp == 'i' || cp == 'I'))
                col = _colors[eREGISTER];
            else if (len == 2 && (cp == 'v' || cp == 'V') && isHexDigit(*(token + 1)))
                col = _colors[eREGISTER];
            else if (g_octoOpcodes.contains(std::string(token, text - token)))
                col = _colors[eOPCODE];
            else if (g_octoDirectives.contains(std::string(token, text - token)))
                col = _colors[eDIRECTIVE];
            while (start < index)
                _highlighting[start++].front = col;
            wasColon = isColon;
        }
    }
}
void Highlighter::highlightLineChipper(const char* text, const char* end)
{

}

void Highlighter::highlightLine1802(const char* text, const char* end)
{

}

void Highlighter::highlightLine6800(const char* text, const char* end)
{

}

void Highlighter::drawHighlightedTextLine(Font& font, const char* textRoot, const char* text, const char* end, Vector2 position, float width, int columnOffset, int lineHeight)
{
    uint32_t selStart = _selectionStart > _selectionEnd ? _selectionEnd : _selectionStart;
    uint32_t selEnd = _selectionStart > _selectionEnd ? _selectionStart : _selectionEnd;
    highlightLine(text, end);
    float textOffsetX = 0.0f;
    size_t index = 0;
    while (text < end && textOffsetX < width && *text != '\n') {
        if (columnOffset <= 0) {
            if (_selectionStart < _selectionEnd) {
                uint32_t offset = text - textRoot;
                if (offset >= selStart && offset < selEnd)
                    DrawRectangleRec({position.x + textOffsetX, position.y - 2, 6, (float)lineHeight}, _selected);
            }
            int codepoint = (int)utf8::fetchCodepoint(text, end);
            if ((codepoint != ' ') && (codepoint != '\t')) {
                DrawTextCodepointClipped(font, codepoint, (Vector2){position.x + textOffsetX, position.y}, 8, _highlighting[index].front);
            }
        }
        --columnOffset;
        if (columnOffset < 0)
            textOffsetX += COLUMN_WIDTH;
        ++index;
    }
    uint32_t offset = text - textRoot;
    if (textOffsetX < width && offset >= selStart && offset < selEnd)
        DrawRectangleRec({position.x + textOffsetX, position.y - 2, width - textOffsetX, (float)lineHeight}, _selected);
}

void Highlighter::setDialect(Dialect dialect)
{
    switch (dialect) {
        case eCHIP8OCTO:

            break;
    }
}
