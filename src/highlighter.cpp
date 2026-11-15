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
#include <chiplet/sourcesyntax.hpp>
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

// Highlights assembly as Chiplet describes the language (emu::SourceSyntax, the words from the assemblers' own tables):
// labels ("name:", and a first-column name that is no keyword where the language has those), the mnemonic or directive
// of a statement, then its operands as numbers, registers, strings or plain words. A word the language doesn't know stays
// plain, also where a mnemonic is expected; so does the "???" of a disassembler.
void highlightAssembly(const emu::SourceSyntax& syntax, const char* text, const char* end, const LineStyles& out)
{
    if (syntax.firstColumnComment && text == out.lineStart && text < end && *text == syntax.firstColumnComment) {
        out.set(text, std::find(text, end, '\n'), Highlighter::eCOMMENT);
        return;
    }
    const bool atIsSeparator = syntax.numberPrefixes.find('@') == std::string_view::npos;  // Saturn's @d0, but 6800's @17 octal
    auto startsComment = [&](const char* at) { return !syntax.lineComment.empty() && std::string_view(at, end - at).starts_with(syntax.lineComment); };
    auto isSeparator = [&](char c) { return c == ' ' || c == '\t' || c == ',' || (c == '@' && atIsSeparator); };
    auto operandStyle = [&](std::string_view tok) {
        const auto first = tok.empty() ? '\0' : tok.front();
        if ((first >= '0' && first <= '9') || (first && syntax.numberPrefixes.find(first) != std::string_view::npos))
            return Highlighter::eNUMBER;
        return syntax.isRegister(tok) ? Highlighter::eREGISTER : Highlighter::eNORMAL;
    };
    bool expectKeyword = true;  // where the mnemonic or directive of a statement is
    while (text < end && *text != '\n') {
        const char* token = text;
        if (startsComment(text)) {
            out.set(text, std::find(text, end, '\n'), Highlighter::eCOMMENT);
            return;
        }
        if (syntax.statementSeparator && *text == syntax.statementSeparator) {
            out.set(text, text + 1, Highlighter::eNORMAL);
            ++text;
            expectKeyword = true;
            continue;
        }
        if (isSeparator(*text)) {
            out.set(text, text + 1, Highlighter::eNORMAL);
            ++text;
            continue;
        }
        // a word, quoted parts belong to it with their spaces (T'TEXT', X'1F', "HI THERE")
        char quote = 0;
        bool quoted = false;
        while (text < end && *text != '\n') {
            if (!quote) {
                if (isSeparator(*text) || startsComment(text) || (syntax.statementSeparator && *text == syntax.statementSeparator))
                    break;
                if (syntax.quotes.find(*text) != std::string_view::npos)
                    quote = *text, quoted = true;
            }
            else if (*text == quote) {
                quote = 0;
            }
            utf8::fetchCodepoint(text, end);
        }
        const std::string_view tok(token, text - token);
        if (quoted) {
            // a based number of the CDP1802 (X'1F'), or a string
            const bool based = tok.size() > 2 && syntax.quotes.find(tok[1]) != std::string_view::npos &&
                               syntax.basedNumberPrefixes.find(static_cast<char>(std::toupper(static_cast<unsigned char>(tok[0])))) != std::string_view::npos;
            out.set(token, text, based ? Highlighter::eNUMBER : Highlighter::eSTRING);
            expectKeyword = false;
            continue;
        }
        if (const auto assign = tok.find(":="); assign != std::string_view::npos) {
            // a symbol and its value (Saturn)
            out.set(token, token + assign, Highlighter::eLABEL);
            out.set(token + assign, token + assign + 2, Highlighter::eNORMAL);
            out.set(token + assign + 2, text, operandStyle(tok.substr(assign + 2)));
            expectKeyword = false;
            continue;
        }
        if (tok.size() > 1 && tok.back() == ':') {
            out.set(token, text, Highlighter::eLABEL);
            continue;
        }
        if (tok == "???") {
            out.set(token, text, Highlighter::eNORMAL);
            expectKeyword = false;
            continue;
        }
        if (syntax.directivePrefix && tok.front() == syntax.directivePrefix) {
            out.set(token, text, Highlighter::eDIRECTIVE);
            expectKeyword = false;
            continue;
        }
        if (expectKeyword) {
            const auto name = syntax.nameOf(tok);
            const bool directive = !name.empty() && syntax.isDirective(name);
            const bool known = directive || (!name.empty() && (syntax.isMnemonic(name) || syntax.isRegisterOperandMnemonic(name)));
            if (!known && token == out.lineStart && syntax.firstColumnLabels) {
                out.set(token, text, Highlighter::eLABEL);
                continue;
            }
            expectKeyword = false;
            if (!known) {
                out.set(token, text, Highlighter::eNORMAL);
                continue;
            }
            // the size suffix belongs to the mnemonic ("move.a"), what follows a name otherwise is an operand ("LDI#00")
            const char* keywordEnd = syntax.sizeSuffix ? text : token + name.size();
            out.set(token, keywordEnd, directive ? Highlighter::eDIRECTIVE : Highlighter::eOPCODE);
            if (keywordEnd < text)
                out.set(keywordEnd, text, operandStyle(std::string_view(keywordEnd, text - keywordEnd)));
            continue;
        }
        out.set(token, text, operandStyle(tok));
    }
}

void highlightSegment(Dialect dialect, const char* text, const char* end, const LineStyles& out)
{
    switch (dialect) {
        case Highlighter::eCHIP8OCTO: highlightOcto(text, end, out); return;
        case Highlighter::eCDP1802: highlightAssembly(*emu::SourceSyntax::forLanguage(emu::SourceLanguage::CDP1802), text, end, out); return;
        case Highlighter::eM6800: highlightAssembly(*emu::SourceSyntax::forLanguage(emu::SourceLanguage::M6800), text, end, out); return;
        case Highlighter::eSATURN: highlightAssembly(*emu::SourceSyntax::forLanguage(emu::SourceLanguage::SATURN), text, end, out); return;
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
