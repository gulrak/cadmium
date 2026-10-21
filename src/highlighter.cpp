//---------------------------------------------------------------------------------------
// src/highlighter.cpp
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
#include <string_view>

namespace utf8 = ghc::utf8;

static std::unordered_set<std::string> g_octoOpcodes = {
    "!=", "&=", "+=", "-=", "-key", ":", ":=", ";", "<", "<<=", "<=", "=-", "==", ">", ">=", ">>=", "^=", "|=",
    "again", "audio", "bcd", "begin", "bighex", "buzzer", "clear", "delay", "else", "end", "hex", "hires", "if",
    "jump", "jump0", "key", "load", "loadflags", "loop", "lores", "native", "pitch", "plane", "random", "return",
    "save", "saveflags", "scroll-down", "scroll-left", "scroll-right", "scroll-up", "sprite", "then", "while"
};
static std::unordered_set<std::string> g_octoDirectives = {
    ":alias", ":asm", ":assert", ":breakpoint", ":byte", ":calc", ":call", ":const", ":macro", ":monitor", ":next", ":org",
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

Highlighter::Highlighter() = default;

static bool isAsmIdentifierChar(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

Highlighter::LineState Highlighter::highlightLine(const char* text, const char* end, LineState state)
{
    _highlighting.resize(end - text);
    size_t index = 0;
    while (text < end && *text != '\n') {
        auto segmentState = state;
        const auto* segmentEnd = nextModeChange(text, end, state);
        index = highlightSegment(segmentState.asmPhase == ASM_BODY ? segmentState.asmDialect : _dialect, text, segmentEnd, index);
        text = segmentEnd;
    }
    return state;
}

size_t Highlighter::highlightSegment(Dialect dialect, const char* text, const char* end, size_t index)
{
    switch (dialect) {
        case eCHIP8OCTO: return highlightLineOcto(text, end, index);
        case eCHIP8CHIPPER: return highlightLineChipper(text, end, index);
        case eCDP1802: return highlightLine1802(text, end, index);
        case eM6800: return highlightLine6800(text, end, index);
        case eNONE: break;
    }
    return highlightPlain(text, end, index);
}

size_t Highlighter::highlightPlain(const char* text, const char* end, size_t index)
{
    while (text < end && *text != '\n')
        utf8::fetchCodepoint(text, end), _highlighting[index++].front = _colors[eNORMAL];
    return index;
}

// Scans a single line starting in `state` and returns the position where the
// highlighting dialect changes (with `state` updated), or `end` if it doesn't.
// The rules mirror Chiplet's Octo lexer for `:asm <cpu> { ... }` blocks.
const char* Highlighter::nextModeChange(const char* text, const char* end, LineState& state) const
{
    if (_dialect != eCHIP8OCTO)
        return end;
    while (text < end) {
        char c = *text;
        if (state.asmPhase == ASM_BODY) {
            if (c == '.' && text + 1 < end && text[1] == '.')
                return end;
            if (c == '\'') {
                ++text;
                while (text < end && *text != '\'' && *text != '\n') {
                    if (*text++ == '\\' && text < end && *text != '\n')
                        ++text;
                }
                if (text < end && *text == '\'')
                    ++text;
                continue;
            }
            if (c == '}') {
                state = {};
                return text;
            }
            ++text;
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            ++text;
            continue;
        }
        if (state.asmPhase == ASM_EXPECT_CPU) {
            const auto* cpuStart = text;
            while (text < end && isAsmIdentifierChar(*text))
                ++text;
            if (text == cpuStart)
                state = {};
            else
                state = {ASM_EXPECT_BRACE, std::string_view(cpuStart, text - cpuStart) == "cdp1802" ? eCDP1802 : eNONE};
            continue;
        }
        if (state.asmPhase == ASM_EXPECT_BRACE) {
            if (c == '{') {
                state.asmPhase = ASM_BODY;
                return text + 1;
            }
            state = {};
        }
        if (c == '#')
            return end;
        const auto* token = text;
        while (text < end && static_cast<unsigned char>(*text) > ' ')
            ++text;
        if (std::string_view(token, text - token) == ":asm")
            state.asmPhase = ASM_EXPECT_CPU;
    }
    return end;
}

Highlighter::LineState Highlighter::scanLineState(const char* text, const char* end, LineState state) const
{
    while (text < end)
        text = nextModeChange(text, end, state);
    return state;
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
        double value = 0.0;
        auto res = fast_float::from_chars(text, end, value);
        if (res.ec != std::errc() || res.ptr == text)
            return {0, 0.0};
        return {static_cast<std::size_t>(res.ptr - text), value};
    }
}

size_t Highlighter::highlightLineOcto(const char* text, const char* end, size_t index)
{
    bool wasColon = false;
    bool wasAsm = false;
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
            std::string_view tok(token, text - token);
            Color col = _colors[eNORMAL];
            if (wasAsm)
                col = _colors[eDIRECTIVE];
            else if (cp == ':' && len == 1 || wasColon)
                col = _colors[eLABEL], isColon = true;
            else if (cp >= '0' && cp <= '9')
                col = _colors[eNUMBER];
            else if (len == 1 && (cp == 'i' || cp == 'I'))
                col = _colors[eREGISTER];
            else if (len == 2 && (cp == 'v' || cp == 'V') && isHexDigit(*(token + 1)))
                col = _colors[eREGISTER];
            else if (g_octoOpcodes.contains(std::string(tok)))
                col = _colors[eOPCODE];
            else if (g_octoDirectives.contains(std::string(tok)))
                col = _colors[eDIRECTIVE];
            while (start < index)
                _highlighting[start++].front = col;
            wasColon = isColon;
            wasAsm = tok == ":asm";
        }
    }
    return index;
}

size_t Highlighter::highlightLineChipper(const char* text, const char* end, size_t index)
{
    return highlightPlain(text, end, index);
}

static std::unordered_set<std::string> g_cdp1802Opcodes = {
    "ADC",  "ADCI", "ADD",  "ADI",  "AND",  "ANI",  "B1",   "B2",   "B3",  "B4",   "BDF",  "BN1",  "BN2",  "BN3",  "BN4", "BNF", "BNQ",  "BNZ", "BQ",  "BR",
    "BZ",   "DEC",  "DIS",  "GHI",  "GLO",  "IDL",  "INC",  "INP",  "IRX", "LBDF", "LBNF", "LBNQ", "LBNZ", "LBQ",  "LBR", "LBZ", "LDA",  "LDI", "LDN", "LDX",
    "LDXA", "LSDF", "LSIE", "LSKP", "LSNF", "LSNQ", "LSNZ", "LSQ",  "LSZ", "MARK", "NOP",  "OR",   "ORI",  "OUT",  "PHI", "PLO", "REQ",  "RET", "SAV", "SD",
    "SDB",  "SDBI", "SDI",  "SEP",  "SEQ",  "SEX",  "SHL",  "SHLC", "SHR", "SHRC", "SKP",  "SM",   "SMB",  "SMBI", "SMI", "STR", "STXD", "XOR", "XRI"
};

static std::unordered_set<std::string> g_cdp1802Directives = {
    "#include", "define", "#undef", "#if", "#ifdef", "#ifndef", "#else", "#elif", "#endif", "#error",
    "ORG", "DC", "DB", "PAGE", "END", "EQU"
};

// Follows Chiplet's CDP1802 syntax: `..` starts a comment, `;` separates statements,
// `,` separates inline data, `'...'` are strings (or based numbers with X/D/B prefix)
size_t Highlighter::highlightLine1802(const char* text, const char* end, size_t index)
{
    while (text < end && *text != '\n') {
        const char* token = text;
        uint32_t cp = utf8::fetchCodepoint(text, end);
        if (cp == ' ' || cp == '\t' || cp == ';' || cp == ',')
            _highlighting[index++].front = _colors[eNORMAL];
        else if (cp == '.' && text < end && *text == '.') {
            _highlighting[index++].front = _colors[eCOMMENT];
            while (text < end && *text != '\n')
                utf8::fetchCodepoint(text, end), _highlighting[index++].front = _colors[eCOMMENT];
        }
        else {
            auto start = index++;
            bool inString = cp == '\'';
            bool hasString = inString;
            while (text < end && *text != '\n') {
                if (inString) {
                    if (*text == '\\' && text + 1 < end && text[1] != '\n')
                        utf8::fetchCodepoint(text, end), ++index;
                    else if (*text == '\'')
                        inString = false;
                }
                else {
                    if (static_cast<unsigned char>(*text) <= ' ' || *text == ';' || *text == ',' || (*text == '.' && text + 1 < end && text[1] == '.'))
                        break;
                    if (*text == '\'')
                        inString = hasString = true;
                }
                utf8::fetchCodepoint(text, end), ++index;
            }
            auto len = index - start;
            std::string tok(token, text - token);
            for (auto& c : tok)
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            Color col = _colors[eNORMAL];
            if (hasString)
                col = _colors[tok.size() > 1 && tok[1] == '\'' && (tok[0] == 'X' || tok[0] == 'D' || tok[0] == 'B') ? eNUMBER : eSTRING];
            else if (tok.size() > 1 && tok.back() == ':')
                col = _colors[eLABEL];
            else if (cp == '$' || cp == '%' || (cp >= '0' && cp <= '9'))
                col = _colors[eNUMBER];
            else if (len > 2 && (cp == '#' || (*(token + 1)>= '0' && *(token + 1) <= '9'))) {
                _highlighting[start++].front = _colors[eNORMAL];
                col = _colors[eNUMBER];
            }
            else if ((len == 2 || len == 3) && (cp == 'r' || cp == 'R') && isHexDigit(*(token + 1)))
                col = _colors[eREGISTER];
            else if (g_cdp1802Opcodes.contains(tok))
                col = _colors[eOPCODE];
            else if (g_cdp1802Directives.contains(tok))
                col = _colors[eDIRECTIVE];
            while (start < index)
                _highlighting[start++].front = col;
        }
    }
    return index;
}

size_t Highlighter::highlightLine6800(const char* text, const char* end, size_t index)
{
    return highlightPlain(text, end, index);
}

void Highlighter::drawHighlightedTextLine(Font& font, const char* textRoot, const char* text, const char* end, Vector2 position, float width, int columnOffset, int lineHeight, LineState state)
{
    uint32_t selStart = _selectionStart > _selectionEnd ? _selectionEnd : _selectionStart;
    uint32_t selEnd = _selectionStart > _selectionEnd ? _selectionStart : _selectionEnd;
    highlightLine(text, end, state);
    float textOffsetX = 0.0f;
    size_t index = 0;
    while (text < end && textOffsetX < width && *text != '\n') {
        uint32_t offset = text - textRoot;
        int codepoint = (int)utf8::fetchCodepoint(text, end);
        if (columnOffset <= 0) {
            if (_selectionStart < _selectionEnd) {
                if (offset >= selStart && offset < selEnd)
                    DrawRectangleRec({position.x + textOffsetX, position.y - 2, 6, (float)lineHeight}, _selected);
            }
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
    _dialect = dialect;
}
