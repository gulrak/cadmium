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
#include <chiplet/utility.hpp>

#include <ghc/utf8.hpp>

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <unordered_set>

namespace utf8 = ghc::utf8;

namespace {

using Dialect = Highlighter::Dialect;

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

static std::unordered_set<std::string_view> g_saturnRegisters = {
    "a", "b", "c", "d", "d0", "d1", "r0", "r1", "r2", "r3", "r4", "p", "pc", "st", "id"
};

// Progress of an Octo `:asm <cpu> { ... }` block, carried from line to line packed into the LineState
enum AsmPhase : uint8_t { ASM_NONE, ASM_EXPECT_CPU, ASM_EXPECT_BRACE, ASM_BODY };

struct AsmState
{
    AsmPhase phase{ASM_NONE};
    Dialect dialect{Highlighter::eNONE};
};

AsmState unpack(gui::SyntaxHighlighter::LineState state)
{
    return {static_cast<AsmPhase>(state & 0xff), static_cast<Dialect>(state >> 8)};
}

gui::SyntaxHighlighter::LineState pack(AsmState state)
{
    return static_cast<gui::SyntaxHighlighter::LineState>(state.phase) | static_cast<gui::SyntaxHighlighter::LineState>(state.dialect) << 8;
}

// The style ids of the bytes of the line that is highlighted
struct LineStyles
{
    const char* lineStart;
    std::span<uint8_t> styles;
    void set(const char* from, const char* to, Highlighter::Style style) const
    {
        std::fill(styles.begin() + (from - lineStart), styles.begin() + (to - lineStart), style);
    }
};

bool isAsmIdentifierChar(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

// Scans a single line starting in `state` and returns the position where the
// highlighting dialect changes (with `state` updated), or `end` if it doesn't.
// The rules mirror Chiplet's Octo lexer for `:asm <cpu> { ... }` blocks.
const char* nextModeChange(Dialect dialect, const char* text, const char* end, AsmState& state)
{
    if (dialect != Highlighter::eCHIP8OCTO)
        return end;
    while (text < end) {
        char c = *text;
        if (state.phase == ASM_BODY) {
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
        if (static_cast<unsigned char>(c) <= ' ') {
            ++text;
            continue;
        }
        if (state.phase == ASM_EXPECT_CPU) {
            const auto* cpuStart = text;
            while (text < end && isAsmIdentifierChar(*text))
                ++text;
            if (text == cpuStart)
                state = {};
            else
                state = {ASM_EXPECT_BRACE, std::string_view(cpuStart, text - cpuStart) == "cdp1802" ? Highlighter::eCDP1802 : Highlighter::eNONE};
            continue;
        }
        if (state.phase == ASM_EXPECT_BRACE) {
            if (c == '{') {
                state.phase = ASM_BODY;
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
            state.phase = ASM_EXPECT_CPU;
    }
    return end;
}

void highlightPlain(const char* text, const char* end, const LineStyles& out)
{
    out.set(text, end, Highlighter::eNORMAL);
}

void highlightOcto(const char* text, const char* end, const LineStyles& out)
{
    bool wasColon = false;
    bool wasAsm = false;
    while (text < end && *text != '\n') {
        const char* token = text;
        uint32_t cp = utf8::fetchCodepoint(text, end);
        if (cp == ' ')
            continue;
        if (cp == '#') {
            out.set(token, end, Highlighter::eCOMMENT);
            break;
        }
        bool isColon = false;
        size_t len = 1;
        while (text < end && *text > ' ')
            utf8::fetchCodepoint(text, end), ++len;
        std::string_view tok(token, text - token);
        auto style = Highlighter::eNORMAL;
        if (wasAsm)
            style = Highlighter::eDIRECTIVE;
        else if (cp == ':' && len == 1 || wasColon)
            style = Highlighter::eLABEL, isColon = true;
        else if (cp >= '0' && cp <= '9')
            style = Highlighter::eNUMBER;
        else if (len == 1 && (cp == 'i' || cp == 'I'))
            style = Highlighter::eREGISTER;
        else if (len == 2 && (cp == 'v' || cp == 'V') && isHexDigit(*(token + 1)))
            style = Highlighter::eREGISTER;
        else if (g_octoOpcodes.contains(std::string(tok)))
            style = Highlighter::eOPCODE;
        else if (g_octoDirectives.contains(std::string(tok)))
            style = Highlighter::eDIRECTIVE;
        out.set(token, text, style);
        wasColon = isColon;
        wasAsm = tok == ":asm";
    }
}

// Follows Chiplet's CDP1802 syntax: `..` starts a comment, `;` separates statements,
// `,` separates inline data, `'...'` are strings (or based numbers with X/D/B prefix)
void highlight1802(const char* text, const char* end, const LineStyles& out)
{
    while (text < end && *text != '\n') {
        const char* token = text;
        uint32_t cp = utf8::fetchCodepoint(text, end);
        if (cp == ' ' || cp == '\t' || cp == ';' || cp == ',') {
            out.set(token, text, Highlighter::eNORMAL);
        }
        else if (cp == '.' && text < end && *text == '.') {
            while (text < end && *text != '\n')
                ++text;
            out.set(token, text, Highlighter::eCOMMENT);
        }
        else {
            const char* firstEnd = text;
            size_t len = 1;
            bool inString = cp == '\'';
            bool hasString = inString;
            while (text < end && *text != '\n') {
                if (inString) {
                    if (*text == '\\' && text + 1 < end && text[1] != '\n')
                        utf8::fetchCodepoint(text, end), ++len;
                    else if (*text == '\'')
                        inString = false;
                }
                else {
                    if (static_cast<unsigned char>(*text) <= ' ' || *text == ';' || *text == ',' || (*text == '.' && text + 1 < end && text[1] == '.'))
                        break;
                    if (*text == '\'')
                        inString = hasString = true;
                }
                utf8::fetchCodepoint(text, end), ++len;
            }
            std::string tok(token, text - token);
            for (auto& c : tok)
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            const char* styleStart = token;
            auto style = Highlighter::eNORMAL;
            if (hasString)
                style = tok.size() > 1 && tok[1] == '\'' && (tok[0] == 'X' || tok[0] == 'D' || tok[0] == 'B') ? Highlighter::eNUMBER : Highlighter::eSTRING;
            else if (tok.size() > 1 && tok.back() == ':')
                style = Highlighter::eLABEL;
            else if (cp == '$' || cp == '%' || (cp >= '0' && cp <= '9'))
                style = Highlighter::eNUMBER;
            else if (len > 2 && (cp == '#' || (*(token + 1) >= '0' && *(token + 1) <= '9'))) {
                // the prefix stays plain, the rest is the number
                out.set(token, firstEnd, Highlighter::eNORMAL);
                styleStart = firstEnd;
                style = Highlighter::eNUMBER;
            }
            else if ((len == 2 || len == 3) && (cp == 'r' || cp == 'R') && isHexDigit(*(token + 1)))
                style = Highlighter::eREGISTER;
            else if (g_cdp1802Opcodes.contains(tok))
                style = Highlighter::eOPCODE;
            else if (g_cdp1802Directives.contains(tok))
                style = Highlighter::eDIRECTIVE;
            out.set(styleStart, text, style);
        }
    }
}

// Follows the M6800 disassembly: the mnemonic with its accumulator (`LDAA`), then one operand
// where `#` marks immediate values, `$` hex numbers and `,X` indexed addressing. A `label:` and
// `;` comments are taken as in common 6800 assemblers.
void highlight6800(const char* text, const char* end, const LineStyles& out)
{
    bool expectOpcode = true;
    while (text < end && *text != '\n') {
        const char* token = text;
        uint32_t cp = utf8::fetchCodepoint(text, end);
        if (cp == ' ' || cp == '\t' || cp == ',') {
            out.set(token, text, Highlighter::eNORMAL);
        }
        else if (cp == ';') {
            while (text < end && *text != '\n')
                ++text;
            out.set(token, text, Highlighter::eCOMMENT);
        }
        else {
            while (text < end && static_cast<unsigned char>(*text) > ' ' && *text != ',' && *text != ';')
                utf8::fetchCodepoint(text, end);
            std::string_view tok(token, text - token);
            auto style = Highlighter::eNORMAL;
            if (tok.size() > 1 && tok.back() == ':')
                style = Highlighter::eLABEL;
            else if (tok == "???")
                expectOpcode = false;  // an invalid opcode or operand stays plain like CHIP-8's "invalid"
            else if (expectOpcode)
                style = Highlighter::eOPCODE, expectOpcode = false;
            else if (cp == '#' || cp == '$' || cp == '%' || (cp >= '0' && cp <= '9'))
                style = Highlighter::eNUMBER;
            else if (tok == "X" || tok == "x")
                style = Highlighter::eREGISTER;
            out.set(token, text, style);
        }
    }
}

// Follows the ASAP syntax the Saturn disassembler emits: an optional `label:`, the
// mnemonic with its field suffix, then operands separated by `,`, where `#` marks
// hex numbers and `@` memory access. `;` starts a comment.
void highlightSaturn(const char* text, const char* end, const LineStyles& out)
{
    bool expectOpcode = true;
    while (text < end && *text != '\n') {
        const char* token = text;
        uint32_t cp = utf8::fetchCodepoint(text, end);
        if (cp == ' ' || cp == '\t' || cp == ',' || cp == '@') {
            out.set(token, text, Highlighter::eNORMAL);
        }
        else if (cp == ';') {
            while (text < end && *text != '\n')
                ++text;
            out.set(token, text, Highlighter::eCOMMENT);
        }
        else {
            while (text < end && static_cast<unsigned char>(*text) > ' ' && *text != ',' && *text != ';')
                utf8::fetchCodepoint(text, end);
            std::string_view tok(token, text - token);
            auto style = Highlighter::eNORMAL;
            if (tok.size() > 1 && tok.back() == ':')
                style = Highlighter::eLABEL;
            else if (expectOpcode)
                style = Highlighter::eOPCODE, expectOpcode = false;
            else if (cp == '#' || (cp >= '0' && cp <= '9'))
                style = Highlighter::eNUMBER;
            else if (g_saturnRegisters.contains(tok))
                style = Highlighter::eREGISTER;
            out.set(token, text, style);
        }
    }
}

void highlightSegment(Dialect dialect, const char* text, const char* end, const LineStyles& out)
{
    switch (dialect) {
        case Highlighter::eCHIP8OCTO: highlightOcto(text, end, out); return;
        case Highlighter::eCDP1802: highlight1802(text, end, out); return;
        case Highlighter::eM6800: highlight6800(text, end, out); return;
        case Highlighter::eSATURN: highlightSaturn(text, end, out); return;
        case Highlighter::eCHIP8CHIPPER:
        case Highlighter::eNONE: break;
    }
    highlightPlain(text, end, out);
}

}  // namespace

gui::SyntaxHighlighter::LineState Highlighter::nextLineState(std::string_view line, LineState state) const
{
    auto asmState = unpack(state);
    const char* text = line.data();
    const char* end = text + line.size();
    while (text < end)
        text = nextModeChange(_dialect, text, end, asmState);
    return pack(asmState);
}

void Highlighter::highlightLine(std::string_view line, LineState state, std::span<uint8_t> styles) const
{
    const char* text = line.data();
    const char* end = text + line.size();
    const LineStyles out{text, styles};
    auto asmState = unpack(state);
    while (text < end && *text != '\n') {
        const auto segmentState = asmState;
        const auto* segmentEnd = nextModeChange(_dialect, text, end, asmState);
        highlightSegment(segmentState.phase == ASM_BODY ? segmentState.dialect : _dialect, text, segmentEnd, out);
        text = segmentEnd;
    }
}
