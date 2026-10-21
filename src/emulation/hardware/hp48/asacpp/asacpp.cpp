//---------------------------------------------------------------------------------------
// src/emulation/hardware/hp48/asacpp/asacpp.cpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2026, Steffen Schümann <s.schuemann@pobox.com>
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

#include "asacpp.hpp"

#include <emulation/hardware/hp48/saturndecoder.hpp>
#include <ghc/filesystem.hpp>
#include <chiplet/sha1.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <set>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using hp48::InstructionInfo;
using hp48::VariableType;
namespace fs = ghc::filesystem;

struct SourceLine {
    std::string file;
    int line = 0;
    std::string text;
    std::string statement;
    std::uint32_t address = 0;
    std::string originFile;
    int originLine = 0;
};

struct AssembledLine {
    SourceLine source;
    std::vector<std::uint8_t> nibbles;
};

struct MacroDefinition {
    std::vector<std::string> parameters;
    std::vector<SourceLine> body;
};

struct ConditionalFrame {
    bool parentActive = true;
    bool conditionTrue = false;
    bool branchActive = false;
    bool elseSeen = false;
};

bool isIdentifier(std::string_view value);

struct ExpressionParser {
    const std::map<std::string, std::int64_t>& symbols;
    std::string_view text;
    std::size_t pos = 0;

    std::int64_t parse()
    {
        const std::int64_t value = parseAddSub();
        skipSpace();
        if (pos != text.size()) {
            throw std::runtime_error("unexpected token in expression: " + std::string(text.substr(pos)));
        }
        return value;
    }

    std::int64_t parseAddSub()
    {
        std::int64_t value = parseUnary();
        for (;;) {
            skipSpace();
            if (consume('+')) {
                value += parseUnary();
            } else if (consume('-')) {
                value -= parseUnary();
            } else {
                return value;
            }
        }
    }

    std::int64_t parseUnary()
    {
        skipSpace();
        if (consume('+')) {
            return parseUnary();
        }
        if (consume('-')) {
            return -parseUnary();
        }
        return parsePrimary();
    }

    std::int64_t parsePrimary()
    {
        skipSpace();
        if (consume('(')) {
            const std::int64_t value = parseAddSub();
            if (!consume(')')) {
                throw std::runtime_error("missing ')' in expression");
            }
            return value;
        }
        if (peek() == '\'') {
            ++pos;
            if (pos >= text.size()) {
                throw std::runtime_error("empty character literal");
            }
            const unsigned char ch = static_cast<unsigned char>(text[pos++]);
            if (!consume('\'')) {
                throw std::runtime_error("unterminated character literal");
            }
            return ch;
        }
        if (peek() == '#') {
            ++pos;
            return parseDigits(16);
        }
        if (peek() == '!') {
            ++pos;
            return parseDigits(2);
        }
        if (std::isdigit(static_cast<unsigned char>(peek()))) {
            return parseDigits(10);
        }
        if (isIdentifierStart(peek())) {
            const std::string name = parseIdentifier();
            const auto it = symbols.find(name);
            if (it == symbols.end()) {
                throw std::runtime_error("unknown symbol '" + name + "'");
            }
            return it->second;
        }
        throw std::runtime_error("expected expression");
    }

    std::int64_t parseDigits(int base)
    {
        const std::size_t start = pos;
        while (pos < text.size() && isDigitForBase(text[pos], base)) {
            ++pos;
        }
        if (start == pos) {
            throw std::runtime_error("expected digits");
        }
        return static_cast<std::int64_t>(std::stoull(std::string(text.substr(start, pos - start)), nullptr, base));
    }

    static bool isDigitForBase(char ch, int base)
    {
        if (base <= 10) {
            return ch >= '0' && ch < '0' + base;
        }
        return std::isxdigit(static_cast<unsigned char>(ch));
    }

    std::string parseIdentifier()
    {
        const std::size_t start = pos;
        ++pos;
        while (pos < text.size() && isIdentifierContinue(text[pos])) {
            ++pos;
        }
        return std::string(text.substr(start, pos - start));
    }

    char peek() const
    {
        return pos < text.size() ? text[pos] : '\0';
    }

    bool consume(char ch)
    {
        skipSpace();
        if (peek() == ch) {
            ++pos;
            return true;
        }
        return false;
    }

    void skipSpace()
    {
        while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) {
            ++pos;
        }
    }

    static bool isIdentifierStart(char ch)
    {
        return std::isalpha(static_cast<unsigned char>(ch)) || ch == '_' || ch == '.' || ch == '$';
    }

    static bool isIdentifierContinue(char ch)
    {
        return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '.';
    }
};

std::string trim(std::string_view value)
{
    std::size_t first = 0;
    while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first]))) {
        ++first;
    }
    std::size_t last = value.size();
    while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1]))) {
        --last;
    }
    return std::string(value.substr(first, last - first));
}

std::string lowerAscii(std::string_view value)
{
    std::string result;
    result.reserve(value.size());
    for (char ch : value) {
        result += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return result;
}

bool startsWithInsensitive(std::string_view value, std::string_view prefix)
{
    if (value.size() < prefix.size()) {
        return false;
    }
    for (std::size_t i = 0; i < prefix.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(value[i]))
            != std::tolower(static_cast<unsigned char>(prefix[i]))) {
            return false;
        }
    }
    return true;
}

std::string stripComment(std::string_view line)
{
    bool inChar = false;
    bool inString = false;
    bool escaped = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        if (escaped) {
            escaped = false;
        } else if ((inChar || inString) && ch == '\\') {
            escaped = true;
        } else if (ch == '\'' && !inString) {
            inChar = !inChar;
        } else if (ch == '"' && !inChar) {
            inString = !inString;
        } else if (ch == ';' && !inChar && !inString) {
            return trim(line.substr(0, i));
        }
    }
    return trim(line);
}

std::string normalizeStatement(std::string value)
{
    value = trim(value);
    std::string out;
    bool previousSpace = false;
    bool inChar = false;
    bool inString = false;
    bool escaped = false;
    for (char ch : value) {
        if (escaped) {
            out += ch;
            escaped = false;
            previousSpace = false;
            continue;
        }
        if ((inChar || inString) && ch == '\\') {
            out += ch;
            escaped = true;
            previousSpace = false;
            continue;
        }
        if (ch == '\'' && !inString) {
            if (previousSpace && !out.empty() && out.back() != ',') {
                out += ' ';
            }
            out += ch;
            inChar = !inChar;
            previousSpace = false;
            continue;
        }
        if (ch == '"' && !inChar) {
            if (previousSpace && !out.empty() && out.back() != ',') {
                out += ' ';
            }
            out += ch;
            inString = !inString;
            previousSpace = false;
            continue;
        }
        if (inChar || inString) {
            out += ch;
            previousSpace = false;
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(ch))) {
            if (!out.empty()) {
                previousSpace = true;
            }
            continue;
        }
        if (ch == ',') {
            if (!out.empty() && out.back() == ' ') {
                out.pop_back();
            }
            out += ',';
            previousSpace = false;
            continue;
        }
        if (previousSpace && !out.empty() && out.back() != ',' && ch != ',') {
            out += ' ';
        }
        out += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        previousSpace = false;
    }
    return out;
}

bool startsWith(std::string_view value, std::string_view prefix)
{
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

std::vector<std::string> splitCommaOperands(std::string_view value)
{
    std::vector<std::string> result;
    std::size_t start = 0;
    bool inChar = false;
    bool inString = false;
    bool escaped = false;
    for (std::size_t i = 0; i < value.size(); ++i) {
        const char ch = value[i];
        if (escaped) {
            escaped = false;
        } else if ((inChar || inString) && ch == '\\') {
            escaped = true;
        } else if (ch == '\'' && !inString) {
            inChar = !inChar;
        } else if (ch == '"' && !inChar) {
            inString = !inString;
        } else if (ch == ',' && !inChar && !inString) {
            result.push_back(trim(value.substr(start, i - start)));
            start = i + 1;
        }
    }
    result.push_back(trim(value.substr(start)));
    return result;
}

std::string parseIncludePath(std::string_view statement)
{
    const std::string trimmed = trim(statement);
    if (!startsWithInsensitive(trimmed, "include")) {
        return {};
    }
    if (trimmed.size() > 7 && !std::isspace(static_cast<unsigned char>(trimmed[7]))) {
        return {};
    }
    std::string path = trimmed.size() == 7 ? "" : trim(std::string_view(trimmed).substr(7));
    if (path.empty()) {
        throw std::runtime_error("missing include path");
    }
    if ((path.front() == '"' && path.back() == '"') || (path.front() == '<' && path.back() == '>')) {
        path = path.substr(1, path.size() - 2);
    }
    if (path.empty()) {
        throw std::runtime_error("empty include path");
    }
    return path;
}

bool parseMacroHeader(std::string_view statement, std::string& name, std::vector<std::string>& parameters)
{
    const std::string normalized = normalizeStatement(std::string(statement));
    if (!startsWith(normalized, "macro ")) {
        return false;
    }
    const std::string rest = trim(std::string_view(normalized).substr(6));
    if (rest.empty()) {
        throw std::runtime_error("missing macro name");
    }
    const auto space = rest.find(' ');
    name = space == std::string::npos ? rest : rest.substr(0, space);
    const std::string parameterText = space == std::string::npos ? "" : trim(std::string_view(rest).substr(space + 1));
    parameters = parameterText.empty() ? std::vector<std::string>{} : splitCommaOperands(parameterText);
    for (const auto& parameter : parameters) {
        if (parameter.empty()) {
            throw std::runtime_error("empty macro parameter");
        }
    }
    return true;
}

std::string replaceMacroArguments(
    std::string_view text,
    const std::map<std::string, std::string>& arguments)
{
    std::string result;
    bool inChar = false;
    for (std::size_t i = 0; i < text.size();) {
        const char ch = text[i];
        if (ch == '\'') {
            inChar = !inChar;
            result += ch;
            ++i;
            continue;
        }
        if (!inChar && ExpressionParser::isIdentifierStart(ch)) {
            const std::size_t start = i;
            ++i;
            while (i < text.size() && ExpressionParser::isIdentifierContinue(text[i])) {
                ++i;
            }
            const std::string token(text.substr(start, i - start));
            const auto it = arguments.find(lowerAscii(token));
            result += it == arguments.end() ? token : it->second;
            continue;
        }
        result += ch;
        ++i;
    }
    return result;
}

bool isIdentifier(std::string_view value)
{
    if (value.empty() || !ExpressionParser::isIdentifierStart(value.front())) {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(), [](char ch) {
        return ExpressionParser::isIdentifierContinue(ch);
    });
}

std::string extractLeadingLabel(std::string_view statement)
{
    const auto colon = statement.find(':');
    if (colon == std::string_view::npos) {
        return {};
    }
    const std::string name = trim(statement.substr(0, colon));
    return isIdentifier(name) ? name : std::string{};
}

std::set<std::string> collectLocalLabels(const std::vector<SourceLine>& lines)
{
    std::set<std::string> result;
    for (const auto& line : lines) {
        std::string statement = normalizeStatement(line.statement);
        for (;;) {
            const std::string label = extractLeadingLabel(statement);
            if (label.empty()) {
                break;
            }
            if (startsWith(label, ".")) {
                result.insert(label);
            }
            const auto colon = statement.find(':');
            statement = trim(std::string_view(statement).substr(colon + 1));
            if (statement.empty()) {
                break;
            }
        }
    }
    return result;
}

std::string rewriteMacroLocals(
    std::string_view text,
    const std::set<std::string>& localLabels,
    std::string_view suffix)
{
    if (localLabels.empty()) {
        return std::string(text);
    }
    std::string result;
    bool inChar = false;
    for (std::size_t i = 0; i < text.size();) {
        const char ch = text[i];
        if (ch == '\'') {
            inChar = !inChar;
            result += ch;
            ++i;
            continue;
        }
        if (!inChar && ExpressionParser::isIdentifierStart(ch)) {
            const std::size_t start = i;
            ++i;
            while (i < text.size() && ExpressionParser::isIdentifierContinue(text[i])) {
                ++i;
            }
            const std::string token(text.substr(start, i - start));
            if (startsWith(token, ".") && localLabels.find(lowerAscii(token)) != localLabels.end()) {
                result += token;
                result += suffix;
            } else {
                result += token;
            }
            continue;
        }
        result += ch;
        ++i;
    }
    return result;
}

std::string reversedRegPairAlias(std::string_view pair)
{
    if (pair == "a,b") {
        return "b,a";
    }
    if (pair == "b,c") {
        return "c,b";
    }
    if (pair == "c,a") {
        return "a,c";
    }
    if (pair == "d,c") {
        return "c,d";
    }
    return {};
}

std::string reversedDaregPairAlias(std::string_view pair)
{
    if (pair == "d0,a") {
        return "a,d0";
    }
    if (pair == "d1,a") {
        return "a,d1";
    }
    if (pair == "d0,c") {
        return "c,d0";
    }
    if (pair == "d1,c") {
        return "c,d1";
    }
    return {};
}

bool isTempReg(std::string_view value)
{
    return value == "r0" || value == "r1" || value == "r2" || value == "r3" || value == "r4";
}

std::size_t findConstantAssignment(const std::string& stmt)
{
    const auto assign = stmt.find(":=");
    const auto colon = stmt.find(':');
    if (assign != std::string::npos && (colon == std::string::npos || assign <= colon)) {
        return assign;
    }
    const auto equals = stmt.find('=');
    if (equals != std::string::npos && (colon == std::string::npos || equals <= colon)) {
        return equals;
    }
    return std::string::npos;
}

std::size_t constantAssignmentWidth(const std::string& stmt, std::size_t assignment)
{
    return assignment + 1 < stmt.size() && stmt[assignment] == ':' && stmt[assignment + 1] == '=' ? 2 : 1;
}

bool parseConstantAssignment(const std::string& stmt, std::string& name, std::string& expr)
{
    const auto assign = findConstantAssignment(stmt);
    const auto colon = stmt.find(':');
    if (assign == std::string::npos || (colon != std::string::npos && colon < assign)) {
        return false;
    }
    name = trim(stmt.substr(0, assign));
    expr = trim(stmt.substr(assign + constantAssignmentWidth(stmt, assign)));
    return !name.empty();
}

std::pair<std::string, std::string> parseCommandLineDefine(const std::string& value)
{
    const auto assign = value.find('=');
    if (assign == std::string::npos) {
        throw std::runtime_error("define must use NAME=EXPR");
    }
    const std::string name = trim(std::string_view(value).substr(0, assign));
    const std::string expr = trim(std::string_view(value).substr(assign + 1));
    if (name.empty()) {
        throw std::runtime_error("define name must not be empty");
    }
    if (expr.empty()) {
        throw std::runtime_error("define expression must not be empty");
    }
    if (!isIdentifier(name) || startsWith(name, ".") || name == "$") {
        throw std::runtime_error("invalid define name '" + name + "'");
    }
    return {name, expr};
}

bool parseIfDirective(const std::string& stmt, std::string& expr)
{
    if (!startsWith(stmt, "if ")) {
        return false;
    }
    expr = trim(std::string_view(stmt).substr(3));
    if (expr.empty()) {
        throw std::runtime_error("missing if expression");
    }
    return true;
}

std::string applyInstructionAliases(const std::string& stmt)
{
    static const std::array<std::pair<std::string_view, std::string_view>, 8> EXACT_ALIASES{{
        {"move.a @a,pc", "jump.a @a"},
        {"move.a @c,pc", "jump.a @c"},
        {"move.a a,pc", "jump.a a"},
        {"move.a c,pc", "jump.a c"},
        {"swap.a pc,a", "swap.a a,pc"},
        {"swap.a pc,c", "swap.a c,pc"},
        {"swap.x st,c", "swap.x c,st"},
        {"retsxm", "retsetxm"},
    }};
    for (const auto& [alias, canonical] : EXACT_ALIASES) {
        if (stmt == alias) {
            return std::string(canonical);
        }
    }

    if (startsWith(stmt, "move ")) {
        const auto operands = splitCommaOperands(stmt.substr(5));
        if (operands.size() == 2 && operands[1] == "p") {
            return "move.1 " + operands[0] + ",p";
        }
    }

    if (startsWith(stmt, "swap.1 ")) {
        const auto operands = splitCommaOperands(stmt.substr(7));
        if (operands.size() == 3 && operands[0] == "c" && operands[1] == "p") {
            return "swap.1 p,c," + operands[2];
        }
    }

    if (startsWith(stmt, "swap.w ")) {
        const auto operands = splitCommaOperands(stmt.substr(7));
        if (operands.size() == 2 && isTempReg(operands[0]) && (operands[1] == "a" || operands[1] == "c")) {
            return "swap.w " + operands[1] + "," + operands[0];
        }
    }

    if (startsWith(stmt, "swap.a ") || startsWith(stmt, "swap.4 ")) {
        const auto space = stmt.find(' ');
        const std::string mnemonic = stmt.substr(0, space);
        const std::string operandsText = stmt.substr(space + 1);
        if (const std::string canonical = reversedRegPairAlias(operandsText); !canonical.empty()) {
            return mnemonic + " " + canonical;
        }
        if (const std::string canonical = reversedDaregPairAlias(operandsText); !canonical.empty()) {
            return mnemonic + " " + canonical;
        }
        const auto operands = splitCommaOperands(operandsText);
        if (operands.size() == 2 && isTempReg(operands[0]) && (operands[1] == "a" || operands[1] == "c")) {
            return mnemonic + " " + operands[1] + "," + operands[0];
        }
    }

    if (startsWith(stmt, "swap.")) {
        const auto space = stmt.find(' ');
        if (space != std::string::npos) {
            const std::string mnemonic = stmt.substr(0, space);
            const std::string operandsText = stmt.substr(space + 1);
            if (const std::string canonical = reversedRegPairAlias(operandsText); !canonical.empty()) {
                return mnemonic + " " + canonical;
            }
            const auto operands = splitCommaOperands(operandsText);
            if (operands.size() == 2 && isTempReg(operands[0]) && (operands[1] == "a" || operands[1] == "c")) {
                return mnemonic + " " + operands[1] + "," + operands[0];
            }
        }
    }

    return stmt;
}

std::vector<std::uint8_t> valueToNibbles(std::int64_t value, int count)
{
    std::vector<std::uint8_t> result;
    result.reserve(static_cast<std::size_t>(count));
    auto unsignedValue = static_cast<std::uint64_t>(value);
    for (int i = 0; i < count; ++i) {
        result.push_back(static_cast<std::uint8_t>(unsignedValue & 0xfU));
        unsignedValue >>= 4U;
    }
    return result;
}

std::uint8_t opcodeNibble(const InstructionInfo& info, int position)
{
    const int total = std::abs(info.opcodeSize);
    const int shift = 4 * (total - 1 - position);
    return static_cast<std::uint8_t>((info.opcode >> shift) & 0xfU);
}

int hexDigitValue(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

std::int64_t evalExpression(std::string_view expr, const std::map<std::string, std::int64_t>& symbols)
{
    ExpressionParser parser{symbols, expr};
    return parser.parse();
}

void requireDataWidth(std::uint64_t value, int count)
{
    if (count < 0) {
        throw std::runtime_error("negative data width");
    }
    if (count >= 16) {
        return;
    }
    const std::uint64_t limit = 1ULL << (static_cast<unsigned>(count) * 4U);
    if (value >= limit) {
        throw std::runtime_error("data value does not fit in " + std::to_string(count) + " nibble(s)");
    }
}

void requireRawHexWidth(std::string_view text, int count)
{
    const auto digits = text.size() > 1 ? text.substr(1) : std::string_view{};
    const auto firstNonZero = digits.find_first_not_of('0');
    const std::size_t significantDigits = firstNonZero == std::string_view::npos ? 0 : digits.size() - firstNonZero;
    if (significantDigits > static_cast<std::size_t>(count)) {
        throw std::runtime_error("hex literal does not fit in " + std::to_string(count) + " nibble(s)");
    }
}

void requireRawBinaryWidth(std::string_view text, int count)
{
    const auto digits = text.size() > 1 ? text.substr(1) : std::string_view{};
    const auto firstNonZero = digits.find_first_not_of('0');
    const std::size_t significantBits = firstNonZero == std::string_view::npos ? 0 : digits.size() - firstNonZero;
    if (significantBits > static_cast<std::size_t>(count) * 4U) {
        throw std::runtime_error("binary literal does not fit in " + std::to_string(count) + " nibble(s)");
    }
}

std::vector<std::uint8_t> dataExpressionToNibbles(
    std::string_view expr,
    int count,
    const std::map<std::string, std::int64_t>& symbols)
{
    const std::string trimmed = trim(expr);
    if (trimmed.size() > 1 && trimmed[0] == '#') {
        bool isPlainHex = true;
        for (std::size_t i = 1; i < trimmed.size(); ++i) {
            if (hexDigitValue(trimmed[i]) < 0) {
                isPlainHex = false;
                break;
            }
        }
        if (isPlainHex) {
            requireRawHexWidth(trimmed, count);
            std::vector<std::uint8_t> result;
            result.reserve(static_cast<std::size_t>(count));
            for (int i = 0; i < count; ++i) {
                const auto sourceIndex = static_cast<std::ptrdiff_t>(trimmed.size()) - 1 - i;
                result.push_back(sourceIndex > 0
                    ? static_cast<std::uint8_t>(hexDigitValue(trimmed[static_cast<std::size_t>(sourceIndex)]))
                    : 0);
            }
            return result;
        }
    }
    if (trimmed.size() > 1 && trimmed[0] == '!') {
        bool isPlainBinary = true;
        for (std::size_t i = 1; i < trimmed.size(); ++i) {
            if (trimmed[i] != '0' && trimmed[i] != '1') {
                isPlainBinary = false;
                break;
            }
        }
        if (isPlainBinary) {
            requireRawBinaryWidth(trimmed, count);
            std::vector<std::uint8_t> result(static_cast<std::size_t>(count), 0);
            for (std::size_t i = 1; i < trimmed.size(); ++i) {
                if (trimmed[trimmed.size() - i] == '1') {
                    const auto bitIndex = static_cast<int>(i - 1);
                    const int nibbleIndex = bitIndex / 4;
                    if (nibbleIndex < count) {
                        result[static_cast<std::size_t>(nibbleIndex)] |= static_cast<std::uint8_t>(1U << (bitIndex % 4));
                    }
                }
            }
            return result;
        }
    }
    const auto value = evalExpression(trimmed, symbols);
    if (value >= 0) {
        requireDataWidth(static_cast<std::uint64_t>(value), count);
    }
    return valueToNibbles(value, count);
}

std::uint8_t parseHexByte(std::string_view text, std::size_t& pos)
{
    if (pos + 2 > text.size() - 1) {
        throw std::runtime_error("incomplete \\x escape in string literal");
    }
    const int hi = hexDigitValue(text[pos]);
    const int lo = hexDigitValue(text[pos + 1]);
    if (hi < 0 || lo < 0) {
        throw std::runtime_error("invalid \\x escape in string literal");
    }
    pos += 2;
    return static_cast<std::uint8_t>((hi << 4U) | lo);
}

std::vector<std::uint8_t> parseAsciiStringBytes(std::string_view text)
{
    if (text.size() < 2 || text.front() != '"' || text.back() != '"') {
        throw std::runtime_error("expected string literal");
    }
    std::vector<std::uint8_t> bytes;
    for (std::size_t i = 1; i + 1 < text.size();) {
        unsigned char ch = static_cast<unsigned char>(text[i++]);
        if (ch == '\\') {
            if (i >= text.size() - 1) {
                throw std::runtime_error("unterminated escape in string literal");
            }
            const char escape = text[i++];
            switch (escape) {
            case '\\': ch = '\\'; break;
            case '"': ch = '"'; break;
            case 'n': ch = '\n'; break;
            case 'r': ch = '\r'; break;
            case 't': ch = '\t'; break;
            case '0': ch = '\0'; break;
            case 'x': ch = parseHexByte(text, i); break;
            default:
                throw std::runtime_error(std::string("unknown string escape \\") + escape);
            }
        }
        if (ch >= 0x80) {
            throw std::runtime_error("string literal contains a non-ASCII byte");
        }
        bytes.push_back(static_cast<std::uint8_t>(ch));
    }
    return bytes;
}

std::vector<std::uint8_t> dataStringToNibbles(
    std::string_view expr,
    const std::map<std::string, std::int64_t>& symbols)
{
    std::vector<std::uint8_t> result;
    const auto operands = splitCommaOperands(expr);
    for (const auto& operand : operands) {
        if (operand.empty()) {
            throw std::runtime_error("empty data.s operand");
        }
        if (operand.front() == '"') {
            for (const auto byte : parseAsciiStringBytes(operand)) {
                auto nibbles = valueToNibbles(byte, 2);
                result.insert(result.end(), nibbles.begin(), nibbles.end());
            }
        } else {
            auto nibbles = dataExpressionToNibbles(operand, 2, symbols);
            result.insert(result.end(), nibbles.begin(), nibbles.end());
        }
    }
    return result;
}

std::size_t dataStringSize(std::string_view expr)
{
    std::size_t nibbles = 0;
    const auto operands = splitCommaOperands(expr);
    for (const auto& operand : operands) {
        if (operand.empty()) {
            throw std::runtime_error("empty data.s operand");
        }
        if (operand.front() == '"') {
            nibbles += parseAsciiStringBytes(operand).size() * 2U;
        } else {
            nibbles += 2;
        }
    }
    return nibbles;
}

const std::array<std::string_view, 16> FIELD{"p", "wp", "xs", "x", "s", "m", "b", "w"};
const std::array<std::string_view, 16> REGPAIR4{"a,b", "b,c", "c,a", "d,c"};
const std::array<std::string_view, 16> REGPAIR4REV{"b,a", "c,b", "a,c", "c,d"};
const std::array<std::string_view, 16> REGPAIR8{"b,a", "c,b", "a,c", "c,d", "a,b", "b,c", "c,a", "d,c"};
const std::array<std::string_view, 16> REGPAIR8SPLIT{"b,a", "c,b", "a,c", "c,d", {}, {}, {}, {}, "a,b", "b,c", "c,a", "d,c"};
const std::array<std::string_view, 16> REGPAIR12{"b,a", "c,b", "a,c", "c,d", "a,a", "b,b", "c,c", "d,d", "a,b", "b,c", "c,a", "d,c"};
const std::array<std::string_view, 16> REG{"a", "b", "c", "d"};
const std::array<std::string_view, 16> MRPAIR{"a,@d0", "a,@d1", "@d0,a", "@d1,a", "c,@d0", "c,@d1", "@d0,c", "@d1,c"};
const std::array<std::string_view, 16> DAREGPAIR{"a,d0", "a,d1", {}, {}, "c,d0", "c,d1"};
const std::array<std::string_view, 16> DAREGPAIRREV{"d0,a", "d1,a", {}, {}, "d0,c", "d1,c"};
const std::array<std::string_view, 16> TEMPREG{"r0", "r1", "r2", "r3", "r4"};
const std::array<const std::array<std::string_view, 16>*, 11> TABLES{
    &FIELD, &REGPAIR4, &REGPAIR4REV, &REGPAIR8, &REGPAIR8SPLIT, &REGPAIR12,
    &REG, &MRPAIR, &DAREGPAIR, &DAREGPAIRREV, &TEMPREG
};

bool tableOperandValue(VariableType type, std::string_view text, std::uint32_t& value)
{
    if (type >= hp48::vt_maxtables) {
        return false;
    }
    const auto& table = *TABLES[static_cast<std::size_t>(type)];
    for (std::uint32_t i = 0; i < table.size(); ++i) {
        if (!table[i].empty() && table[i] == text) {
            value = i;
            return true;
        }
    }
    return false;
}

std::string regexEscape(std::string_view text)
{
    std::string out;
    for (char ch : text) {
        if (std::string_view(".^$|()[]{}*+?\\").find(ch) != std::string_view::npos) {
            out += '\\';
        }
        if (std::isspace(static_cast<unsigned char>(ch))) {
            out += "\\s+";
        } else {
            out += ch;
        }
    }
    return out;
}

std::string tableRegex(VariableType type)
{
    std::vector<std::string> values;
    const auto& table = *TABLES[static_cast<std::size_t>(type)];
    for (auto item : table) {
        if (!item.empty()) {
            values.push_back(regexEscape(item));
        }
    }
    std::sort(values.begin(), values.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.size() > rhs.size();
    });
    std::string result = "(";
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            result += '|';
        }
        result += values[i];
    }
    result += ')';
    return result;
}

struct InstructionPattern {
    const InstructionInfo* info = nullptr;
    std::regex regex;
    std::vector<int> groupToVariable;
};

InstructionPattern instructionRegex(const InstructionInfo& info)
{
    std::string pattern = "^";
    std::vector<int> groupToVariable;
    const std::string fmt = normalizeStatement(info.fmt);
    for (std::size_t i = 0; i < fmt.size();) {
        if (i + 2 < fmt.size() && fmt[i] == '{' && fmt[i + 2] == '}'
            && fmt[i + 1] >= '0' && fmt[i + 1] <= '2') {
            const int index = fmt[i + 1] - '0';
            const auto type = info.varTypes[index];
            if (type < hp48::vt_maxtables) {
                pattern += tableRegex(type);
            } else {
                pattern += "([^,\\s]+)";
            }
            groupToVariable.push_back(index);
            i += 3;
        } else {
            pattern += regexEscape(std::string_view(&fmt[i], 1));
            ++i;
        }
    }
    pattern += "$";
    return {&info, std::regex(pattern, std::regex::icase), groupToVariable};
}

const std::vector<InstructionPattern>& instructionPatterns()
{
    static const std::vector<InstructionPattern> patterns = [] {
        std::vector<InstructionPattern> result;
        const auto& infos = hp48::SaturnDecoder::instructionInfos();
        result.reserve(infos.size());
        for (const auto& info : infos) {
            result.push_back(instructionRegex(info));
        }
        return result;
    }();
    return patterns;
}

std::uint32_t encodePcofs(std::int64_t target, std::uint32_t address, const InstructionInfo& info, int index)
{
    const int width = info.varSizes[index] * 4;
    const std::int64_t distance = target - static_cast<std::int64_t>(address) - info.varArgs[index];
    const std::int64_t minValue = -(1LL << (width - 1));
    const std::int64_t maxValue = (1LL << (width - 1)) - 1;
    if (distance < minValue || distance > maxValue) {
        throw std::runtime_error("PC-relative target is out of range");
    }
    return static_cast<std::uint32_t>(distance) & ((1U << width) - 1U);
}

class Assembler {
public:
    void addDefine(const std::string& name, const std::string& expr)
    {
        _defines[name] = expr;
    }

    void addIncludeDirectory(const std::string& path)
    {
        _includePaths.push_back(fs::path(path));
    }

    void clearIncludeDirectories()
    {
        _includePaths.clear();
    }

    void loadFile(const std::string& path)
    {
        _lines.clear();
        _assembled.clear();
        std::set<std::string> includeStack;
        loadFileRecursive(fs::path(path), includeStack);
        expandMacros();
        filterConditionals();
    }

    void loadSource(const std::string& path, std::span<const char> source)
    {
        _lines.clear();
        _assembled.clear();
        std::set<std::string> includeStack;
        std::string sourceText(source.begin(), source.end());
        std::istringstream input(sourceText);
        loadStream(fs::path(path), input, includeStack);
        expandMacros();
        filterConditionals();
    }

    void assemble()
    {
        firstPass();
        secondPass();
    }

    void assemble(const std::string& outputPath, const std::string& listingPath)
    {
        assemble();
        writeBinary(outputPath);
        if (!listingPath.empty()) {
            writeListing(listingPath);
        }
    }

    void assembleHpAscii(const std::string& outputPath, const std::string& listingPath)
    {
        assemble();
        writeHpAscii(outputPath);
        if (!listingPath.empty()) {
            writeListing(listingPath);
        }
    }

    void writePreprocessed(const std::string& outputPath) const
    {
        std::ofstream output(outputPath);
        if (!output) {
            throw std::runtime_error("failed to open output " + outputPath);
        }
        writePreprocessedTo(output);
    }

    void writePreprocessedTo(std::ostream& output) const
    {

        std::string lastFile;
        int lastLine = 0;
        std::string lastOriginFile;
        int lastOriginLine = 0;
        bool haveLast = false;
        for (const auto& line : _lines) {
            const std::string originFile = line.originFile.empty() ? line.file : line.originFile;
            const int originLine = line.originLine == 0 ? line.line : line.originLine;
            const bool sequential = haveLast
                && line.file == lastFile
                && line.line == lastLine + 1
                && originFile == lastOriginFile
                && originLine == lastOriginLine + 1;
            if (!sequential) {
                output << "; #line " << line.file << ':' << line.line;
                if (originFile != line.file || originLine != line.line) {
                    output << " expanded from " << originFile << ':' << originLine;
                }
                output << '\n';
            }
            output << line.text << '\n';
            lastFile = line.file;
            lastLine = line.line;
            lastOriginFile = originFile;
            lastOriginLine = originLine;
            haveLast = true;
        }
    }

    std::string preprocessedSourceText() const
    {
        std::ostringstream output;
        writePreprocessedTo(output);
        return output.str();
    }

    std::string listingText() const
    {
        std::ostringstream output;
        writeListingTo(output);
        return output.str();
    }

    std::string hpAsciiText() const
    {
        std::ostringstream output;
        writeHpAsciiTo(output);
        return output.str();
    }

    std::vector<std::uint8_t> packedCode() const
    {
        return packNibbles(assembledNibbles());
    }

    std::optional<std::int64_t> symbolValue(std::string_view name) const
    {
        const auto found = _symbols.find(std::string(name));
        if (found != _symbols.end()) {
            return found->second;
        }
        const auto defined = _defines.find(std::string(name));
        if (defined == _defines.end()) {
            return std::nullopt;
        }
        try {
            return evalExpression(defined->second, _symbols);
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }

    std::pair<std::uint32_t, std::uint32_t> addrForLine(std::uint32_t line) const
    {
        for (const auto& assembledLine : _assembled) {
            if (assembledLine.source.line == static_cast<int>(line)) {
                return {assembledLine.source.address, assembledLine.source.address
                    + static_cast<std::uint32_t>(assembledLine.nibbles.size())};
            }
        }
        return {0, 0};
    }

    std::uint32_t lineForAddr(std::uint32_t addr) const
    {
        for (const auto& assembledLine : _assembled) {
            const auto begin = assembledLine.source.address;
            const auto end = begin + static_cast<std::uint32_t>(assembledLine.nibbles.size());
            if (assembledLine.nibbles.empty()) {
                continue;
            }
            if (addr >= begin && addr < end) {
                return static_cast<std::uint32_t>(assembledLine.source.line);
            }
        }
        return 0;
    }

private:
    std::vector<SourceLine> _lines;
    std::vector<AssembledLine> _assembled;
    std::map<std::string, std::int64_t> _symbols;
    std::map<std::string, std::string> _defines;
    std::vector<fs::path> _includePaths;
    std::size_t _macroExpansionCounter = 0;
    std::string _currentGlobalLabel;
    std::uint32_t _currentAddress = 0;

    std::vector<std::uint8_t> assembledNibbles() const
    {
        std::vector<std::uint8_t> nibbles;
        for (const auto& line : _assembled) {
            nibbles.insert(nibbles.end(), line.nibbles.begin(), line.nibbles.end());
        }
        return nibbles;
    }

    static std::vector<std::uint8_t> packNibbles(const std::vector<std::uint8_t>& nibbles)
    {
        std::vector<std::uint8_t> bytes;
        bytes.reserve((nibbles.size() + 1U) / 2U);
        for (std::size_t i = 0; i < nibbles.size(); i += 2) {
            const std::uint8_t lo = nibbles[i];
            const std::uint8_t hi = (i + 1 < nibbles.size()) ? nibbles[i + 1] : 0;
            bytes.push_back(static_cast<std::uint8_t>(lo | (hi << 4U)));
        }
        return bytes;
    }

    fs::path resolveIncludePath(const fs::path& includingFile, const fs::path& includedPath) const
    {
        if (includedPath.is_absolute()) {
            return includedPath.lexically_normal();
        }
        const fs::path localPath = (includingFile.parent_path() / includedPath).lexically_normal();
        if (fs::exists(localPath)) {
            return localPath;
        }
        for (const auto& includePath : _includePaths) {
            const fs::path candidate = (includePath / includedPath).lexically_normal();
            if (fs::exists(candidate)) {
                return candidate;
            }
        }
        return localPath;
    }

    void loadFileRecursive(const fs::path& rawPath, std::set<std::string>& includeStack)
    {
        const fs::path path = rawPath.lexically_normal();
        const std::string pathText = path.string();
        std::ifstream input(pathText);
        if (!input) {
            throw std::runtime_error("failed to open " + pathText);
        }
        loadStream(path, input, includeStack);
    }

    void loadStream(const fs::path& rawPath, std::istream& input, std::set<std::string>& includeStack)
    {
        const fs::path path = rawPath.lexically_normal();
        const std::string pathText = path.string();
        if (includeStack.find(pathText) != includeStack.end()) {
            throw std::runtime_error("recursive include of " + pathText);
        }
        includeStack.insert(pathText);
        std::string line;
        int lineNumber = 0;
        while (std::getline(input, line)) {
            ++lineNumber;
            const std::string statement = stripComment(line);
            try {
                const std::string includedPath = parseIncludePath(statement);
                if (!includedPath.empty()) {
                    const fs::path includePath = resolveIncludePath(path, fs::path(includedPath));
                    loadFileRecursive(includePath, includeStack);
                } else {
                    _lines.push_back(SourceLine{pathText, lineNumber, line, statement, 0, pathText, lineNumber});
                }
            } catch (const std::exception& error) {
                throw std::runtime_error(pathText + ":" + std::to_string(lineNumber) + ": " + error.what());
            }
        }
        includeStack.erase(pathText);
    }

    void expandMacros()
    {
        std::map<std::string, MacroDefinition> macros;
        std::vector<SourceLine> expanded;
        for (std::size_t i = 0; i < _lines.size(); ++i) {
            const SourceLine& line = _lines[i];
            std::string macroName;
            std::vector<std::string> macroParameters;
            try {
                if (parseMacroHeader(line.statement, macroName, macroParameters)) {
                    MacroDefinition definition{macroParameters, {}};
                    bool foundEnd = false;
                    while (++i < _lines.size()) {
                        if (normalizeStatement(_lines[i].statement) == "endmacro") {
                            foundEnd = true;
                            break;
                        }
                        definition.body.push_back(_lines[i]);
                    }
                    if (!foundEnd) {
                        throw std::runtime_error("missing endmacro for macro " + macroName);
                    }
                    macros[macroName] = definition;
                    continue;
                }
                if (normalizeStatement(line.statement) == "endmacro") {
                    throw std::runtime_error("endmacro without matching macro");
                }
                if (expandMacroInvocation(line, macros, expanded)) {
                    continue;
                }
                expanded.push_back(line);
            } catch (const std::exception& error) {
                throw std::runtime_error(line.file + ":" + std::to_string(line.line) + ": " + error.what());
            }
        }
        _lines = std::move(expanded);
    }

    bool expandMacroInvocation(
        const SourceLine& line,
        const std::map<std::string, MacroDefinition>& macros,
        std::vector<SourceLine>& expanded)
    {
        const std::string stmt = normalizeStatement(line.statement);
        if (stmt.empty()) {
            return false;
        }
        const auto space = stmt.find(' ');
        const std::string name = space == std::string::npos ? stmt : stmt.substr(0, space);
        const auto macro = macros.find(name);
        if (macro == macros.end()) {
            return false;
        }

        const std::string argumentText = space == std::string::npos ? "" : trim(std::string_view(stmt).substr(space + 1));
        const std::vector<std::string> arguments = argumentText.empty() ? std::vector<std::string>{}
            : splitCommaOperands(argumentText);
        if (arguments.size() != macro->second.parameters.size()) {
            throw std::runtime_error("macro " + name + " expects "
                + std::to_string(macro->second.parameters.size()) + " argument(s), got "
                + std::to_string(arguments.size()));
        }

        std::map<std::string, std::string> argumentMap;
        for (std::size_t i = 0; i < arguments.size(); ++i) {
            argumentMap[macro->second.parameters[i]] = arguments[i];
        }
        const std::set<std::string> localLabels = collectLocalLabels(macro->second.body);
        const std::string localSuffix = "__macro" + std::to_string(++_macroExpansionCounter);
        for (const auto& bodyLine : macro->second.body) {
            const std::string localText = rewriteMacroLocals(bodyLine.text, localLabels, localSuffix);
            const std::string text = replaceMacroArguments(localText, argumentMap);
            const std::string originFile = bodyLine.originFile.empty() ? bodyLine.file : bodyLine.originFile;
            const int originLine = bodyLine.originLine == 0 ? bodyLine.line : bodyLine.originLine;
            expanded.push_back(SourceLine{line.file, line.line, text, stripComment(text), 0, originFile, originLine});
        }
        return true;
    }

    void filterConditionals()
    {
        std::map<std::string, std::int64_t> preprocessorSymbols;
        for (const auto& [name, expr] : _defines) {
            preprocessorSymbols[name] = evalExpression(expr, preprocessorSymbols);
        }
        std::vector<ConditionalFrame> stack;
        std::vector<SourceLine> filtered;
        auto isActive = [&stack] {
            return stack.empty() || stack.back().branchActive;
        };

        for (const auto& line : _lines) {
            try {
                const std::string stmt = normalizeStatement(line.statement);
                std::string ifExpr;
                if (parseIfDirective(stmt, ifExpr)) {
                    const bool parentActive = isActive();
                    const bool conditionTrue = parentActive && evalExpression(ifExpr, preprocessorSymbols) != 0;
                    stack.push_back(ConditionalFrame{parentActive, conditionTrue, parentActive && conditionTrue, false});
                    continue;
                }
                if (stmt == "else") {
                    if (stack.empty()) {
                        throw std::runtime_error("else without matching if");
                    }
                    auto& frame = stack.back();
                    if (frame.elseSeen) {
                        throw std::runtime_error("duplicate else");
                    }
                    frame.elseSeen = true;
                    frame.branchActive = frame.parentActive && !frame.conditionTrue;
                    continue;
                }
                if (stmt == "endif") {
                    if (stack.empty()) {
                        throw std::runtime_error("endif without matching if");
                    }
                    stack.pop_back();
                    continue;
                }
                if (!isActive()) {
                    continue;
                }
                std::string name;
                std::string expr;
                if (parseConstantAssignment(stmt, name, expr)) {
                    try {
                        preprocessorSymbols[name] = evalExpression(expr, preprocessorSymbols);
                    } catch (const std::exception&) {
                        // Address-dependent constants are resolved by the assembly passes.
                    }
                }
                filtered.push_back(line);
            } catch (const std::exception& error) {
                throw std::runtime_error(line.file + ":" + std::to_string(line.line) + ": " + error.what());
            }
        }
        if (!stack.empty()) {
            throw std::runtime_error("unterminated if");
        }
        _lines = std::move(filtered);
    }

    void firstPass()
    {
        std::uint32_t address = 0;
        _currentGlobalLabel.clear();
        _symbols.clear();
        for (const auto& [name, expr] : _defines) {
            _symbols[name] = evalExpression(expr, _symbols);
        }
        for (auto& line : _lines) {
            try {
                line.address = address;
                _currentAddress = address;
                std::string stmt = normalizeStatement(line.statement);
                if (stmt.empty()) {
                    continue;
                }
                if (stmt[0] == '#' && stmt.back() == ':') {
                    address = static_cast<std::uint32_t>(evalScopedExpression(stmt.substr(0, stmt.size() - 1)));
                    line.address = address;
                    _currentAddress = address;
                    continue;
                }
                extractLabelsAndConstants(stmt, address);
                if (stmt.empty()) {
                    continue;
                }
                stmt = applyInstructionAliases(stmt);
                line.address = address;
                address += static_cast<std::uint32_t>(estimateSize(stmt, address));
            } catch (const std::exception& error) {
                throw std::runtime_error(line.file + ":" + std::to_string(line.line) + ": " + error.what());
            }
        }
    }

    void extractLabelsAndConstants(std::string& stmt, std::uint32_t address)
    {
        for (;;) {
            const auto assign = findConstantAssignment(stmt);
            const auto colon = stmt.find(':');
            if (assign != std::string::npos) {
                const std::string name = trim(stmt.substr(0, assign));
                const std::string expr = trim(stmt.substr(assign + constantAssignmentWidth(stmt, assign)));
                _symbols[qualifySymbolName(name)] = evalScopedExpression(expr);
                stmt.clear();
                return;
            }
            if (colon == std::string::npos) {
                return;
            }
            const std::string name = trim(stmt.substr(0, colon));
            if (name.empty()) {
                return;
            }
            const std::string symbol = qualifySymbolName(name);
            _symbols[symbol] = address;
            if (!startsWith(name, ".")) {
                _currentGlobalLabel = name;
            }
            stmt = trim(stmt.substr(colon + 1));
            if (stmt.empty()) {
                return;
            }
        }
    }

    std::string qualifySymbolName(const std::string& name) const
    {
        if (!startsWith(name, ".")) {
            return name;
        }
        if (_currentGlobalLabel.empty()) {
            throw std::runtime_error("local label '" + name + "' has no preceding global label");
        }
        return _currentGlobalLabel + name;
    }

    std::size_t estimateSize(std::string stmt, std::uint32_t address)
    {
        stmt = applyInstructionAliases(stmt);
        if (isAlignmentDirective(stmt)) {
            return address % 2U;
        }
        if (isDataDirective(stmt)) {
            return dataSize(stmt);
        }
        const auto [info, operands] = findInstruction(stmt, 0);
        if (info == nullptr) {
            throw std::runtime_error("unknown statement in first pass: " + stmt);
        }
        if (info->opcodeSize < 0) {
            return static_cast<std::size_t>(std::abs(info->opcodeSize) + evalScopedExpression(operands[0]));
        }
        return static_cast<std::size_t>(info->opcodeSize);
    }

    std::map<std::string, std::int64_t> scopedSymbols() const
    {
        auto symbols = _symbols;
        symbols["$"] = _currentAddress;
        if (!_currentGlobalLabel.empty()) {
            const std::string prefix = _currentGlobalLabel + ".";
            for (const auto& [name, value] : _symbols) {
                if (startsWith(name, prefix)) {
                    symbols["." + name.substr(prefix.size())] = value;
                }
            }
        }
        return symbols;
    }

    std::int64_t evalScopedExpression(std::string_view expr) const
    {
        return evalExpression(expr, scopedSymbols());
    }

    bool isDataDirective(const std::string& stmt) const
    {
        return stmt.rfind("data.", 0) == 0 || stmt.rfind("dat.", 0) == 0;
    }

    bool isAlignmentDirective(const std::string& stmt) const
    {
        return stmt == "even";
    }

    std::size_t dataSize(const std::string& stmt) const
    {
        const auto space = stmt.find(' ');
        if (space == std::string::npos) {
            throw std::runtime_error("missing data operand");
        }
        const std::string directive = stmt.substr(0, space);
        if (directive == "data.b" || directive == "dat.b") {
            return 2;
        }
        if (directive == "data.a" || directive == "dat.a") {
            return 5;
        }
        if (directive == "data.w" || directive == "dat.w") {
            return 16;
        }
        if (directive == "data.s" || directive == "dat.s") {
            return dataStringSize(stmt.substr(space + 1));
        }
        if (directive.rfind("dat.", 0) == 0 || directive.rfind("data.", 0) == 0) {
            return static_cast<std::size_t>(std::stoul(directive.substr(directive.find('.') + 1)));
        }
        throw std::runtime_error("unknown data directive " + directive);
    }

    std::pair<const InstructionInfo*, std::vector<std::string>> findInstruction(const std::string& stmt, std::uint32_t address) const
    {
        (void)address;
        for (const auto& pattern : instructionPatterns()) {
            std::smatch match;
            if (std::regex_match(stmt, match, pattern.regex)) {
                std::vector<std::string> operands(pattern.info->numVars);
                for (std::size_t i = 1; i < match.size(); ++i) {
                    const int variable = pattern.groupToVariable[i - 1];
                    operands[static_cast<std::size_t>(variable)] = match[i].str();
                }
                return {pattern.info, operands};
            }
        }
        return {nullptr, {}};
    }

    void secondPass()
    {
        _assembled.clear();
        _currentGlobalLabel.clear();
        for (const auto& originalLine : _lines) {
            try {
                SourceLine line = originalLine;
                _currentAddress = line.address;
                std::string stmt = normalizeStatement(line.statement);
                if (stmt.empty()) {
                    _assembled.push_back({line, {}});
                    continue;
                }
                if (stmt[0] == '#' && stmt.back() == ':') {
                    _assembled.push_back({line, {}});
                    continue;
                }
                removeLeadingLabelsAndConstants(stmt);
                if (stmt.empty()) {
                    _assembled.push_back({line, {}});
                    continue;
                }
                stmt = applyInstructionAliases(stmt);
                if (isAlignmentDirective(stmt)) {
                    _assembled.push_back({line, assembleAlignment(line.address)});
                } else if (isDataDirective(stmt)) {
                    _assembled.push_back({line, assembleData(stmt)});
                } else {
                    _assembled.push_back({line, assembleInstruction(stmt, line.address)});
                }
            } catch (const std::exception& error) {
                throw std::runtime_error(
                    originalLine.file + ":" + std::to_string(originalLine.line) + ": " + error.what());
            }
        }
    }

    void removeLeadingLabelsAndConstants(std::string& stmt)
    {
        for (;;) {
            const auto assign = findConstantAssignment(stmt);
            if (assign != std::string::npos) {
                stmt.clear();
                return;
            }
            const auto colon = stmt.find(':');
            if (colon == std::string::npos) {
                return;
            }
            const std::string name = trim(stmt.substr(0, colon));
            if (!name.empty() && !startsWith(name, ".")) {
                _currentGlobalLabel = name;
            } else if (!name.empty()) {
                (void)qualifySymbolName(name);
            }
            stmt = trim(stmt.substr(colon + 1));
            if (stmt.empty()) {
                return;
            }
        }
    }

    std::vector<std::uint8_t> assembleData(const std::string& stmt) const
    {
        const auto space = stmt.find(' ');
        const std::string directive = stmt.substr(0, space);
        const std::string expr = trim(stmt.substr(space + 1));
        if (directive == "data.b" || directive == "dat.b") {
            return dataExpressionToNibbles(expr, 2, scopedSymbols());
        }
        if (directive == "data.a" || directive == "dat.a") {
            return dataExpressionToNibbles(expr, 5, scopedSymbols());
        }
        if (directive == "data.w" || directive == "dat.w") {
            return dataExpressionToNibbles(expr, 16, scopedSymbols());
        }
        if (directive == "data.s" || directive == "dat.s") {
            return dataStringToNibbles(expr, scopedSymbols());
        }
        const int nibbles = static_cast<int>(std::stoul(directive.substr(directive.find('.') + 1)));
        return dataExpressionToNibbles(expr, nibbles, scopedSymbols());
    }

    std::vector<std::uint8_t> assembleAlignment(std::uint32_t address) const
    {
        return address % 2U ? std::vector<std::uint8_t>{0} : std::vector<std::uint8_t>{};
    }

    std::vector<std::uint8_t> assembleInstruction(const std::string& stmt, std::uint32_t address) const
    {
        auto [info, operands] = findInstruction(applyInstructionAliases(stmt), address);
        if (info == nullptr) {
            throw std::runtime_error("unknown instruction: " + stmt);
        }
        const int baseTotal = std::abs(info->opcodeSize);
        std::array<std::uint32_t, 3> parsed{};
        std::array<std::uint32_t, 3> semantic{};
        for (int i = 0; i < info->numVars; ++i) {
            parsed[i] = parseOperand(*info, i, operands[static_cast<std::size_t>(i)], address);
            semantic[i] = parsed[i];
            if (info->varTypes[i] == hp48::vt_nzconst) {
                semantic[i] = parsed[i] + 1U;
            }
        }
        const int total = info->opcodeSize < 0
            ? baseTotal + static_cast<int>(semantic[0])
            : baseTotal;
        std::vector<std::uint8_t> nibbles(static_cast<std::size_t>(total), 0);
        std::vector<bool> variable(static_cast<std::size_t>(total), false);
        for (int i = 0; i < info->numVars; ++i) {
            const int varSize = info->varTypes[i] == hp48::vt_varconst
                ? static_cast<int>(semantic[0])
                : info->varSizes[i];
            for (int j = 0; j < varSize; ++j) {
                variable[info->varOffsets[i] + j] = true;
            }
        }
        for (int i = 0; i < total; ++i) {
            if (!variable[i]) {
                nibbles[i] = opcodeNibble(*info, i);
            }
        }
        for (int i = 0; i < info->numVars; ++i) {
            const int varSize = info->varTypes[i] == hp48::vt_varconst
                ? static_cast<int>(semantic[0])
                : info->varSizes[i];
            for (int j = 0; j < varSize; ++j) {
                nibbles[info->varOffsets[i] + j] = static_cast<std::uint8_t>((parsed[i] >> (j * 4)) & 0xfU);
            }
        }
        return nibbles;
    }

    std::uint32_t parseOperand(const InstructionInfo& info, int index, const std::string& text, std::uint32_t address) const
    {
        const VariableType type = info.varTypes[index];
        std::uint32_t value = 0;
        if (type < hp48::vt_maxtables) {
            if (!tableOperandValue(type, text, value)) {
                throw std::runtime_error("invalid table operand '" + text + "'");
            }
            return value + info.varArgs[index];
        }
        if (type == hp48::vt_const || type == hp48::vt_hwflags || type == hp48::vt_varconst) {
            return static_cast<std::uint32_t>(evalScopedExpression(text));
        }
        if (type == hp48::vt_nzconst) {
            const auto evaluated = evalScopedExpression(text);
            if (evaluated <= 0) {
                throw std::runtime_error("non-zero constant operand must be positive");
            }
            return static_cast<std::uint32_t>(evaluated - 1);
        }
        if (type == hp48::vt_pcofs) {
            return encodePcofs(evalScopedExpression(text), address, info, index);
        }
        throw std::runtime_error("unsupported operand type");
    }

    void writeBinary(const std::string& path) const
    {
        const std::vector<std::uint8_t> bytes = packedCode();
        std::ofstream output(path, std::ios::binary);
        if (!output) {
            throw std::runtime_error("failed to open output " + path);
        }
        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    static std::uint16_t updateHpCrc(std::uint16_t crc, std::uint8_t nibble)
    {
        return static_cast<std::uint16_t>((crc >> 4U) ^ (((crc ^ nibble) & 0xfU) * 0x1081U));
    }

    static std::uint8_t packedByteAt(const std::vector<std::uint8_t>& nibbles, std::size_t offset)
    {
        return static_cast<std::uint8_t>(nibbles[offset] | (nibbles[offset + 1] << 4U));
    }

    static std::vector<std::uint8_t> hpAsciiObjectNibbles(const std::vector<std::uint8_t>& nibbles)
    {
        static constexpr std::string_view HEADER = "HPHP48-";
        static constexpr std::size_t HEADER_NIBBLES = 16;
        bool hasTransferHeader = nibbles.size() >= HEADER_NIBBLES;
        for (std::size_t i = 0; hasTransferHeader && i < HEADER.size(); ++i) {
            if (packedByteAt(nibbles, i * 2U) != static_cast<std::uint8_t>(HEADER[i])) {
                hasTransferHeader = false;
            }
        }
        if (hasTransferHeader) {
            return {nibbles.begin() + static_cast<std::ptrdiff_t>(HEADER_NIBBLES), nibbles.end()};
        }
        return nibbles;
    }

    static std::string hpAsciiPayload(std::vector<std::uint8_t> nibbles)
    {
        static constexpr char HEX[] = "0123456789ABCDEF";
        nibbles = hpAsciiObjectNibbles(nibbles);
        std::uint16_t crc = 0;
        std::string payload;
        payload.reserve(nibbles.size() + 4U);
        for (std::uint8_t nibble : nibbles) {
            crc = updateHpCrc(crc, nibble);
            payload.push_back(HEX[nibble & 0xfU]);
        }
        for (int i = 0; i < 4; ++i) {
            payload.push_back(HEX[(crc >> (i * 4)) & 0xfU]);
        }
        return payload;
    }

    void writeHpAscii(const std::string& path) const
    {
        std::ofstream output(path);
        if (!output) {
            throw std::runtime_error("failed to open output " + path);
        }
        writeHpAsciiTo(output);
    }

    void writeHpAsciiTo(std::ostream& output) const
    {
        const std::string payload = hpAsciiPayload(assembledNibbles());
        output << "%%HP: T(3)A(D)F(.);\n\"";
        for (std::size_t i = 0; i < payload.size(); ++i) {
            if (i != 0 && i % 64U == 0) {
                output << '\n';
            }
            output << payload[i];
        }
        output << "\"\n";
    }

    void writeListing(const std::string& path) const
    {
        std::ofstream output(path);
        if (!output) {
            throw std::runtime_error("failed to open listing " + path);
        }
        writeListingTo(output);
    }

    void writeListingTo(std::ostream& output) const
    {
        for (const auto& line : _assembled) {
            output << std::hex;
            output.width(5);
            output.fill('0');
            output << line.source.address << " ";
            output.fill(' ');
            output.width(18);
            std::ostringstream bytes;
            for (auto nibble : line.nibbles) {
                bytes << std::hex << static_cast<int>(nibble);
            }
            output << std::left << bytes.str() << " " << line.source.text << "\n";
            output << std::right;
        }
    }
};

} // namespace

struct AsaCpp::Impl {
    Assembler assembler;
    std::vector<std::uint8_t> code;
    Sha1::Digest sha1;
};

namespace {

void setOk(TranslatorResult& result)
{
    result.reset();
}

TranslatorResult::Location::Type locationTypeForIndex(std::size_t index)
{
    return index == 0 ? TranslatorResult::Location::eROOT : TranslatorResult::Location::eINCLUDED;
}

void setError(TranslatorResult& result, const fs::path& rootFile, const std::string& message)
{
    result.reset();
    result.resultType = TranslatorResult::eERROR;
    result.errorMessage = message;

    const std::regex locationRegex(R"(([^:\n]+):([0-9]+): )");
    std::size_t index = 0;
    for (auto it = std::sregex_iterator(message.begin(), message.end(), locationRegex);
         it != std::sregex_iterator();
         ++it) {
        result.locations.push_back(TranslatorResult::Location{
            (*it)[1].str(),
            std::stoi((*it)[2].str()),
            0,
            locationTypeForIndex(index++),
        });
    }
    if (result.locations.empty()) {
        result.locations.push_back(TranslatorResult::Location{
            rootFile.string(),
            0,
            0,
            TranslatorResult::Location::eROOT,
        });
    }
}

Sha1::Digest sha1For(std::span<const std::uint8_t> bytes)
{
    Sha1 sha1;
    sha1.add(bytes.data(), static_cast<std::uint32_t>(bytes.size())).finalize();
    return static_cast<Sha1::Digest>(sha1);
}

} // namespace

AsaCpp::AsaCpp()
    : _impl(new Impl)
{
}

AsaCpp::~AsaCpp()
{
    delete _impl;
}

AsaCpp::AsaCpp(AsaCpp&& other) noexcept
    : _impl(other._impl)
    , _translationResult(std::move(other._translationResult))
{
    other._impl = nullptr;
}

AsaCpp& AsaCpp::operator=(AsaCpp&& other) noexcept
{
    if (this != &other) {
        delete _impl;
        _impl = other._impl;
        _translationResult = std::move(other._translationResult);
        other._impl = nullptr;
    }
    return *this;
}

const TranslatorResult& AsaCpp::preprocess(const fs::path& filename, std::span<const char> source)
{
    try {
        _impl->code.clear();
        _impl->sha1 = {};
        _impl->assembler.loadSource(filename.string(), source);
        setOk(_translationResult);
    } catch (const std::exception& error) {
        setError(_translationResult, filename, error.what());
    }
    return _translationResult;
}

const TranslatorResult& AsaCpp::preprocess(const fs::path& filename)
{
    try {
        _impl->code.clear();
        _impl->sha1 = {};
        _impl->assembler.loadFile(filename.string());
        setOk(_translationResult);
    } catch (const std::exception& error) {
        setError(_translationResult, filename, error.what());
    }
    return _translationResult;
}

const TranslatorResult& AsaCpp::compile(const fs::path& filename, std::span<const char> source)
{
    const auto& result = preprocess(filename, source);
    if (result.resultType == TranslatorResult::eERROR) {
        return _translationResult;
    }
    try {
        _impl->assembler.assemble();
        _impl->code = _impl->assembler.packedCode();
        _impl->sha1 = sha1For(_impl->code);
        setOk(_translationResult);
    } catch (const std::exception& error) {
        _impl->code.clear();
        _impl->sha1 = {};
        setError(_translationResult, filename, error.what());
    }
    return _translationResult;
}

const TranslatorResult& AsaCpp::compile(const fs::path& filename)
{
    const auto& result = preprocess(filename);
    if (result.resultType == TranslatorResult::eERROR) {
        return _translationResult;
    }
    try {
        _impl->assembler.assemble();
        _impl->code = _impl->assembler.packedCode();
        _impl->sha1 = sha1For(_impl->code);
        setOk(_translationResult);
    } catch (const std::exception& error) {
        _impl->code.clear();
        _impl->sha1 = {};
        setError(_translationResult, filename, error.what());
    }
    return _translationResult;
}

void AsaCpp::define(std::string name, std::int64_t val)
{
    defineExpression(std::move(name), std::to_string(val));
}

void AsaCpp::defineExpression(std::string name, std::string expr)
{
    const auto [validName, validExpr] = parseCommandLineDefine(name + "=" + expr);
    _impl->assembler.addDefine(validName, validExpr);
}

std::optional<double> AsaCpp::definedValue(std::string_view name) const
{
    const auto value = _impl->assembler.symbolValue(name);
    if (!value) {
        return std::nullopt;
    }
    return static_cast<double>(*value);
}

std::optional<std::int32_t> AsaCpp::definedInteger(std::string_view name) const
{
    const auto value = _impl->assembler.symbolValue(name);
    if (!value || *value < std::numeric_limits<std::int32_t>::min()
        || *value > std::numeric_limits<std::int32_t>::max()) {
        return std::nullopt;
    }
    return static_cast<std::int32_t>(*value);
}

void AsaCpp::setIncludePaths(const std::vector<std::string>& paths)
{
    _impl->assembler.clearIncludeDirectories();
    for (const auto& path : paths) {
        _impl->assembler.addIncludeDirectory(path);
    }
}

std::uint32_t AsaCpp::codeSize() const
{
    return static_cast<std::uint32_t>(_impl->code.size());
}

std::span<const std::uint8_t> AsaCpp::code() const
{
    return _impl->code;
}

const Sha1::Digest& AsaCpp::sha1() const
{
    return _impl->sha1;
}

std::pair<std::uint32_t, std::uint32_t> AsaCpp::addrForLine(std::uint32_t line) const
{
    return _impl->assembler.addrForLine(line);
}

std::uint32_t AsaCpp::lineForAddr(std::uint32_t addr) const
{
    return _impl->assembler.lineForAddr(addr);
}

std::string AsaCpp::listing() const
{
    return _impl->assembler.listingText();
}

std::string AsaCpp::preprocessedSource() const
{
    return _impl->assembler.preprocessedSourceText();
}

std::string AsaCpp::hpAscii() const
{
    return _impl->assembler.hpAsciiText();
}
