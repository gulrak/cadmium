//---------------------------------------------------------------------------------------
// src/libraryquery.cpp
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
#include "libraryquery.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <optional>

namespace {

enum class Field { NAME, DESCRIPTION, AUTHOR, RELEASE, ORIGIN, REFERENCE, LICENSE, COPYRIGHT, URL, TAG, VARIANT, SIZE, SHA1, FILE, PROPERTY };
// VALUE is a property of a config: a number, a flag or text, as its value is
enum class FieldType { TEXT, SET, DATE, NUMBER, VALUE };
enum class Level { PROGRAM, ROM, CONFIG };
enum class Operator { EQUAL, NOT_EQUAL, LESS, LESS_EQUAL, GREATER, GREATER_EQUAL, CONTAINS };

struct FieldInfo
{
    std::string_view name;
    Field field;
    FieldType type;
    Level level{Level::PROGRAM};
};

// field names and their aliases
constexpr std::array FIELDS = {
    FieldInfo{"name", Field::NAME, FieldType::TEXT},
    FieldInfo{"title", Field::NAME, FieldType::TEXT},
    FieldInfo{"description", Field::DESCRIPTION, FieldType::TEXT},
    FieldInfo{"desc", Field::DESCRIPTION, FieldType::TEXT},
    FieldInfo{"author", Field::AUTHOR, FieldType::SET},
    FieldInfo{"by", Field::AUTHOR, FieldType::SET},
    FieldInfo{"release", Field::RELEASE, FieldType::DATE},
    FieldInfo{"year", Field::RELEASE, FieldType::DATE},
    FieldInfo{"date", Field::RELEASE, FieldType::DATE},
    FieldInfo{"origin", Field::ORIGIN, FieldType::TEXT},
    FieldInfo{"jam", Field::REFERENCE, FieldType::TEXT},
    FieldInfo{"ref", Field::REFERENCE, FieldType::TEXT},
    FieldInfo{"reference", Field::REFERENCE, FieldType::TEXT},
    FieldInfo{"license", Field::LICENSE, FieldType::TEXT},
    FieldInfo{"copyright", Field::COPYRIGHT, FieldType::TEXT},
    FieldInfo{"url", Field::URL, FieldType::TEXT},
    FieldInfo{"tag", Field::TAG, FieldType::SET, Level::ROM},
    FieldInfo{"variant", Field::VARIANT, FieldType::TEXT, Level::CONFIG},
    FieldInfo{"preset", Field::VARIANT, FieldType::TEXT, Level::CONFIG},
    FieldInfo{"size", Field::SIZE, FieldType::NUMBER, Level::ROM},
    FieldInfo{"sha1", Field::SHA1, FieldType::TEXT, Level::ROM},
    FieldInfo{"sha", Field::SHA1, FieldType::TEXT, Level::ROM},
    FieldInfo{"file", Field::FILE, FieldType::SET, Level::ROM},
};

constexpr FieldInfo PROPERTY_FIELD{"", Field::PROPERTY, FieldType::VALUE, Level::CONFIG};

std::string lower(std::string_view text)
{
    std::string result(text);
    std::ranges::transform(result, result.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

const FieldInfo* findField(std::string_view name)
{
    const auto key = lower(name);
    auto iter = std::ranges::find(FIELDS, std::string_view(key), &FieldInfo::name);
    return iter != FIELDS.end() ? &*iter : nullptr;
}

// date range as yyyymmdd numbers
struct DateRange
{
    int first;
    int last;
};

// 1978, 1978-5, 1978-05-12, 197x, 19??
std::optional<DateRange> parseDate(std::string_view text)
{
    if (text.size() < 4) {
        return std::nullopt;
    }
    int firstYear = 0, lastYear = 0;
    for (size_t i = 0; i < 4; ++i) {
        const char c = text[i];
        if (std::isdigit(static_cast<unsigned char>(c))) {
            firstYear = firstYear * 10 + (c - '0');
            lastYear = lastYear * 10 + (c - '0');
        }
        else if (c == 'x' || c == 'X' || c == '?') {
            firstYear = firstYear * 10;
            lastYear = lastYear * 10 + 9;
        }
        else {
            return std::nullopt;
        }
    }
    std::array<int, 2> parts{0, 0};
    size_t count = 0;
    auto rest = text.substr(4);
    while (!rest.empty()) {
        if (count == 2 || rest.front() != '-') {
            return std::nullopt;
        }
        rest.remove_prefix(1);
        int value = 0;
        size_t digits = 0;
        while (digits < rest.size() && std::isdigit(static_cast<unsigned char>(rest[digits]))) {
            value = value * 10 + (rest[digits++] - '0');
        }
        if (digits < 1 || digits > 2 || value < 1 || value > (count == 0 ? 12 : 31)) {
            return std::nullopt;
        }
        parts[count++] = value;
        rest.remove_prefix(digits);
    }
    if (count >= 1 && firstYear != lastYear) {
        return std::nullopt;  // a month of an unknown year isn't a range
    }
    const int firstMonth = count >= 1 ? parts[0] : 1, lastMonth = count >= 1 ? parts[0] : 12;
    const int firstDay = count == 2 ? parts[1] : 1, lastDay = count == 2 ? parts[1] : 31;
    return DateRange{firstYear * 10000 + firstMonth * 100 + firstDay, lastYear * 10000 + lastMonth * 100 + lastDay};
}

bool containsText(std::string_view text, std::string_view lowerNeedle)
{
    return lower(text).find(lowerNeedle) != std::string::npos;
}

std::optional<double> parseNumber(std::string_view text, bool strict)
{
    const std::string value(text);
    const char* start = value.c_str();
    char* end = nullptr;
    double number = 0;
    if (value.size() > 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {
        number = static_cast<double>(std::strtol(start + 2, &end, 16));
        if (end == start + 2) {
            return std::nullopt;
        }
    }
    else {
        number = std::strtod(start, &end);
        if (end == start) {
            return std::nullopt;
        }
    }
    std::string_view rest(end);
    if (strict) {
        if (rest == "k" || rest == "K") {
            return number * 1024;
        }
        if (!rest.empty()) {
            return std::nullopt;
        }
    }
    else if (!rest.empty() && std::isalnum(static_cast<unsigned char>(rest.front()))) {
        return std::nullopt;
    }
    return number;
}

std::optional<bool> parseFlag(std::string_view text)
{
    const auto value = lower(text);
    if (value == "true" || value == "yes" || value == "on") {
        return true;
    }
    if (value == "false" || value == "no" || value == "off") {
        return false;
    }
    return std::nullopt;
}

//---------------------------------------------------------------------------------------
// Tokens
//---------------------------------------------------------------------------------------

struct Token
{
    enum Type { WORD, OPERATOR, OPEN, CLOSE, MINUS, END };
    Type type{END};
    std::string text;
    size_t position{0};
    bool quoted{false};
};

bool isOperatorChar(char c)
{
    return c == '=' || c == '!' || c == '<' || c == '>' || c == '~';
}

std::vector<Token> tokenize(std::string_view text)
{
    std::vector<Token> tokens;
    size_t i = 0;
    while (i < text.size()) {
        const char c = text[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            ++i;
        }
        else if (c == '(' || c == ')') {
            tokens.push_back({c == '(' ? Token::OPEN : Token::CLOSE, std::string(1, c), i});
            ++i;
        }
        else if (isOperatorChar(c)) {
            const size_t start = i++;
            if (i < text.size() && text[i] == '=' && c != '=' && c != '~') {
                ++i;
            }
            const auto op = text.substr(start, i - start);
            if (op == "!") {
                throw LibraryQuery::Error("'!' needs '=' to compare", start);
            }
            tokens.push_back({Token::OPERATOR, std::string(op), start});
        }
        else if (c == '"') {
            const size_t start = i++;
            std::string value;
            while (i < text.size() && text[i] != '"') {
                value += text[i++];
            }
            if (i >= text.size()) {
                throw LibraryQuery::Error("quote without its closing quote", start);
            }
            ++i;
            tokens.push_back({Token::WORD, value, start, true});
        }
        else if (c == '-' && i + 1 < text.size() && !std::isspace(static_cast<unsigned char>(text[i + 1]))) {
            // a minus in front of a term negates it, like in search engines (inside a word it is part of it)
            tokens.push_back({Token::MINUS, "-", i});
            ++i;
        }
        else {
            const size_t start = i;
            while (i < text.size() && !std::isspace(static_cast<unsigned char>(text[i])) && text[i] != '(' && text[i] != ')' && text[i] != '"' && !isOperatorChar(text[i])) {
                ++i;
            }
            tokens.push_back({Token::WORD, std::string(text.substr(start, i - start)), start});
        }
    }
    tokens.push_back({Token::END, "", text.size()});
    return tokens;
}

}

//---------------------------------------------------------------------------------------
// Expression Tree
//---------------------------------------------------------------------------------------

struct LibraryQuery::Node
{
    enum class Kind { AND, OR, NOT, TEXT, FIELD, PRESENT, MISSING, ANALYZED, USES };
    Kind kind{Kind::TEXT};
    std::vector<std::shared_ptr<Node>> children;
    std::string text;      // the lowercase text of a TEXT node or the value of a FIELD node
    bool phrase{false};    // a TEXT node of plain words, adjacent ones join it
    Field field{Field::NAME};
    FieldType type{FieldType::TEXT};
    std::string property;  // the lowercase name of a PROPERTY field
    Operator op{Operator::EQUAL};
    bool none{false};      // compares with the empty value
    std::optional<DateRange> date;
    std::optional<double> number;
    uint16_t opcodeMask{0};   // of USES, the bits of the pattern's hex digits
    uint16_t opcodeValue{0};

    bool matches(const ProgramFacts& program, const RomFacts* rom, const ConfigFacts* config) const;
    bool matchesField(const ProgramFacts& program, const RomFacts* rom, const ConfigFacts* config) const;
    bool compareValue(std::string_view value) const;
};

namespace {

using NodePtr = std::shared_ptr<LibraryQuery::Node>;
using Kind = LibraryQuery::Node::Kind;

class Parser
{
public:
    Parser(std::string_view text, const std::set<std::string>& properties)
        : _tokens(tokenize(text))
        , _properties(properties)
    {
    }

    bool usesRomFields() const { return _usesRomFields; }

    NodePtr parse(bool& usesPresence)
    {
        if (_tokens.front().type == Token::END) {
            return nullptr;
        }
        auto root = parseOr();
        if (peek().type != Token::END) {
            throw LibraryQuery::Error(peek().type == Token::CLOSE ? "')' without its '('" : "unexpected '" + peek().text + "'", peek().position);
        }
        usesPresence = _usesPresence;
        return root;
    }

private:
    const Token& peek(size_t offset = 0) const { return _tokens[std::min(_index + offset, _tokens.size() - 1)]; }
    const Token& next() { return _tokens[std::min(_index++, _tokens.size() - 1)]; }

    bool isKeyword(const Token& token, std::string_view keyword) const
    {
        return token.type == Token::WORD && !token.quoted && lower(token.text) == keyword;
    }

    static NodePtr combine(Kind kind, std::vector<NodePtr> children)
    {
        if (children.size() == 1) {
            return children.front();
        }
        auto node = std::make_shared<LibraryQuery::Node>();
        node->kind = kind;
        node->children = std::move(children);
        return node;
    }

    NodePtr parseOr()
    {
        std::vector<NodePtr> terms{parseAnd()};
        while (isKeyword(peek(), "or")) {
            next();
            terms.push_back(parseAnd());
        }
        return combine(Kind::OR, std::move(terms));
    }

    NodePtr parseAnd()
    {
        std::vector<NodePtr> terms;
        bool explicitAnd = false;
        while (peek().type != Token::END && peek().type != Token::CLOSE && !isKeyword(peek(), "or")) {
            if (isKeyword(peek(), "and")) {
                if (terms.empty()) {
                    throw LibraryQuery::Error("'and' without a term before it", peek().position);
                }
                next();
                explicitAnd = true;
                continue;
            }
            auto term = parseUnary();
            // adjacent plain words are one phrase
            if (!explicitAnd && term->phrase && !terms.empty() && terms.back()->phrase) {
                terms.back()->text += " " + term->text;
            }
            else {
                terms.push_back(term);
            }
            explicitAnd = false;
        }
        if (terms.empty() || explicitAnd) {
            throw LibraryQuery::Error(explicitAnd ? "'and' without a term after it" : "a term is missing here", peek().position);
        }
        return combine(Kind::AND, std::move(terms));
    }

    NodePtr parseUnary()
    {
        if (peek().type == Token::MINUS || isKeyword(peek(), "not")) {
            const auto& token = next();
            if (peek().type == Token::END || peek().type == Token::CLOSE) {
                throw LibraryQuery::Error("'" + token.text + "' without a term to negate", token.position);
            }
            auto node = std::make_shared<LibraryQuery::Node>();
            node->kind = Kind::NOT;
            node->children.push_back(parseUnary());
            return node;
        }
        return parsePrimary();
    }

    NodePtr parsePrimary()
    {
        const auto& token = next();
        if (token.type == Token::OPEN) {
            auto node = parseOr();
            if (peek().type != Token::CLOSE) {
                throw LibraryQuery::Error("'(' without its ')'", token.position);
            }
            next();
            node->phrase = false;
            return node;
        }
        if (token.type == Token::OPERATOR) {
            throw LibraryQuery::Error("'" + token.text + "' needs a field before it, like release " + token.text + " 1980", token.position);
        }
        if (token.type != Token::WORD) {
            throw LibraryQuery::Error("a term is missing here", token.position);
        }
        auto node = std::make_shared<LibraryQuery::Node>();
        if (!token.quoted && (lower(token.text) == "missing" || lower(token.text) == "present")) {
            node->kind = lower(token.text) == "missing" ? Kind::MISSING : Kind::PRESENT;
            _usesPresence = true;
            _usesRomFields = true;
            return node;
        }
        if (!token.quoted && lower(token.text) == "analyzed") {
            node->kind = Kind::ANALYZED;
            _usesRomFields = true;
            return node;
        }
        if (!token.quoted && lower(token.text) == "uses") {
            return parseUses(token);
        }
        const auto* field = token.quoted ? nullptr : findField(token.text);
        if (!field && !token.quoted && _properties.contains(lower(token.text))) {
            field = &PROPERTY_FIELD;
        }
        const bool range = field && field->type == FieldType::DATE && peek().type == Token::WORD && peek().text.find("..") != std::string::npos;
        if (peek().type == Token::OPERATOR || range) {
            if (!field) {
                throw LibraryQuery::Error("unknown field '" + token.text + "'", token.position);
            }
            return parseComparison(*field, token);
        }
        node->kind = Kind::TEXT;
        node->text = lower(token.text);
        node->phrase = !token.quoted;
        return node;
    }

    // uses F?75: opcode pattern like "chiplet --find" takes them, hex digits match, other characters are wildcards
    NodePtr parseUses(const Token& usesToken)
    {
        const auto& pattern = next();
        if (pattern.type != Token::WORD || pattern.text.size() != 4) {
            throw LibraryQuery::Error("'uses' needs an opcode pattern of four characters like F?75 or 8xy6", pattern.type == Token::END ? usesToken.position : pattern.position);
        }
        auto node = std::make_shared<LibraryQuery::Node>();
        node->kind = Kind::USES;
        for (const char c : pattern.text) {
            node->opcodeMask <<= 4;
            node->opcodeValue <<= 4;
            if (std::isxdigit(static_cast<unsigned char>(c))) {
                node->opcodeMask |= 0xF;
                node->opcodeValue |= static_cast<uint16_t>(std::isdigit(static_cast<unsigned char>(c)) ? c - '0' : std::tolower(c) - 'a' + 10);
            }
        }
        _usesRomFields = true;
        return node;
    }

    NodePtr parseComparison(const FieldInfo& field, const Token& fieldToken)
    {
        auto node = std::make_shared<LibraryQuery::Node>();
        node->kind = Kind::FIELD;
        node->field = field.field;
        node->type = field.type;
        node->property = field.field == Field::PROPERTY ? lower(fieldToken.text) : std::string();
        if (field.level != Level::PROGRAM) {
            _usesRomFields = true;
        }
        size_t opPosition = peek().position;
        std::string op = "=";
        if (peek().type == Token::OPERATOR) {
            op = next().text;
        }
        static constexpr std::array<std::pair<std::string_view, Operator>, 7> operators{{
            {"=", Operator::EQUAL}, {"!=", Operator::NOT_EQUAL}, {"<", Operator::LESS}, {"<=", Operator::LESS_EQUAL},
            {">", Operator::GREATER}, {">=", Operator::GREATER_EQUAL}, {"~", Operator::CONTAINS}}};
        auto opIter = std::ranges::find(operators, std::string_view(op), &std::pair<std::string_view, Operator>::first);
        if (opIter == operators.end()) {
            throw LibraryQuery::Error("unknown operator '" + op + "'", opPosition);
        }
        node->op = opIter->second;
        const auto& value = next();
        if (value.type != Token::WORD) {
            throw LibraryQuery::Error("'" + fieldToken.text + " " + op + "' needs a value", value.position);
        }
        node->text = lower(value.text);
        node->none = !value.quoted && node->text == "none";
        const bool ordering = node->op != Operator::EQUAL && node->op != Operator::NOT_EQUAL && node->op != Operator::CONTAINS;
        if (node->none && node->op != Operator::EQUAL && node->op != Operator::NOT_EQUAL) {
            throw LibraryQuery::Error("none can only be compared with = or !=", value.position);
        }
        const bool ordered = field.type == FieldType::DATE || field.type == FieldType::NUMBER || field.type == FieldType::VALUE;
        if (!ordered && ordering) {
            throw LibraryQuery::Error("'" + fieldToken.text + "' can't be compared with '" + op + "', only with =, != or ~", opPosition);
        }
        if (field.type == FieldType::DATE && node->op != Operator::CONTAINS && !node->none) {
            node->date = parseDateValue(value, ordering);
        }
        if (node->op != Operator::CONTAINS && !node->none && (field.type == FieldType::NUMBER || field.type == FieldType::VALUE)) {
            node->number = parseNumber(value.text, true);
            if (!node->number && (field.type == FieldType::NUMBER || ordering)) {
                throw LibraryQuery::Error("'" + value.text + "' is no number" + (field.type == FieldType::NUMBER ? " like 512, 0x200 or 2k" : ""), value.position);
            }
        }
        return node;
    }

    // date or range of dates like 1990..1999, ..1979 or 1990..
    static DateRange parseDateValue(const Token& value, bool ordering)
    {
        static const std::string hint = " is no date like 1980, 1980-05, 198x or 1990..1999";
        const auto separator = value.text.find("..");
        if (separator == std::string::npos) {
            if (auto date = parseDate(value.text)) {
                return *date;
            }
            throw LibraryQuery::Error("'" + value.text + "'" + hint, value.position);
        }
        if (ordering) {
            throw LibraryQuery::Error("a range of dates can only be compared with = or !=", value.position);
        }
        const auto first = value.text.substr(0, separator), last = value.text.substr(separator + 2);
        auto from = first.empty() ? DateRange{0, 0} : parseDate(first).value_or(DateRange{-1, -1});
        auto to = last.empty() ? DateRange{99999999, 99999999} : parseDate(last).value_or(DateRange{-1, -1});
        if (from.first < 0 || to.first < 0 || (first.empty() && last.empty())) {
            throw LibraryQuery::Error("'" + value.text + "'" + hint, value.position);
        }
        return DateRange{from.first, to.last};
    }

    std::vector<Token> _tokens;
    const std::set<std::string>& _properties;
    size_t _index{0};
    bool _usesPresence{false};
    bool _usesRomFields{false};
};

}

//---------------------------------------------------------------------------------------
// Evaluation
//---------------------------------------------------------------------------------------

bool LibraryQuery::Node::matches(const ProgramFacts& program, const RomFacts* rom, const ConfigFacts* config) const
{
    switch (kind) {
        case Kind::AND:
            return std::ranges::all_of(children, [&](const auto& child) { return child->matches(program, rom, config); });
        case Kind::OR:
            return std::ranges::any_of(children, [&](const auto& child) { return child->matches(program, rom, config); });
        case Kind::NOT:
            return !children.front()->matches(program, rom, config);
        case Kind::PRESENT:
            return rom ? rom->present : program.present;
        case Kind::MISSING:
            return rom ? !rom->present : !program.present;
        case Kind::ANALYZED:
            return rom && rom->analyzed;
        case Kind::USES:
            return rom && rom->analyzed && std::ranges::any_of(rom->opcodes, [this](uint16_t opcode) { return (opcode & opcodeMask) == opcodeValue; });
        case Kind::TEXT: {
            if (containsText(program.name, text) || containsText(program.description, text)) {
                return true;
            }
            std::string authors;
            for (const auto& author : program.authors) {
                authors += (authors.empty() ? "" : ", ") + author;
            }
            return containsText(authors, text);
        }
        case Kind::FIELD:
            return matchesField(program, rom, config);
    }
    return false;
}

// property value against the value of the query: numbers, flags or text.
bool LibraryQuery::Node::compareValue(std::string_view value) const
{
    if (op == Operator::CONTAINS) {
        return containsText(value, text);
    }
    const auto factNumber = parseNumber(value, false);
    if (number && factNumber) {
        switch (op) {
            case Operator::EQUAL: return *factNumber == *number;
            case Operator::NOT_EQUAL: return *factNumber != *number;
            case Operator::LESS: return *factNumber < *number;
            case Operator::LESS_EQUAL: return *factNumber <= *number;
            case Operator::GREATER: return *factNumber > *number;
            case Operator::GREATER_EQUAL: return *factNumber >= *number;
            case Operator::CONTAINS: break;
        }
        return false;
    }
    if (op != Operator::EQUAL && op != Operator::NOT_EQUAL) {
        return false;  // ordering needs numbers on both sides
    }
    const auto factFlag = parseFlag(value), queryFlag = parseFlag(text);
    const bool equal = factFlag && queryFlag ? *factFlag == *queryFlag : lower(value) == text;
    return (op == Operator::EQUAL) == equal;
}

bool LibraryQuery::Node::matchesField(const ProgramFacts& program, const RomFacts* rom, const ConfigFacts* config) const
{
    std::vector<std::string_view> values;
    switch (field) {
        case Field::NAME: values = {program.name}; break;
        case Field::DESCRIPTION: values = {program.description}; break;
        case Field::AUTHOR: values.assign(program.authors.begin(), program.authors.end()); break;
        case Field::RELEASE: values = {program.release}; break;
        case Field::ORIGIN: values = {program.origin}; break;
        case Field::REFERENCE: values = {program.originReference}; break;
        case Field::LICENSE: values = {program.license}; break;
        case Field::COPYRIGHT: values = {program.copyright}; break;
        case Field::URL: values = {program.url}; break;
        case Field::TAG:
            values.assign(program.tags.begin(), program.tags.end());
            if (rom) {
                values.insert(values.end(), rom->tags.begin(), rom->tags.end());
            }
            break;
        case Field::SHA1:
            if (rom) {
                values = {rom->sha1};
            }
            break;
        case Field::FILE:
            if (rom) {
                values.assign(rom->files.begin(), rom->files.end());
            }
            break;
        case Field::VARIANT:
            if (config) {
                values = {config->preset};
            }
            break;
        case Field::SIZE:
            if (!rom || rom->size <= 0) {
                return none && op == Operator::EQUAL;
            }
            return none ? op == Operator::NOT_EQUAL : compareValue(std::to_string(rom->size));
        case Field::PROPERTY: {
            const std::string* value = nullptr;
            if (config) {
                if (auto iter = config->properties.find(property); iter != config->properties.end()) {
                    value = &iter->second;
                }
            }
            if (!value) {
                return none && op == Operator::EQUAL;  // the core of the variant has no such property
            }
            return none ? op == Operator::NOT_EQUAL : compareValue(*value);
        }
    }
    std::erase_if(values, [](std::string_view value) { return value.empty(); });
    if (none) {
        return (op == Operator::EQUAL) == values.empty();
    }
    if (op == Operator::CONTAINS) {
        return std::ranges::any_of(values, [&](std::string_view value) { return containsText(value, text); });
    }
    if (type != FieldType::DATE) {
        // the origin "game jam" is stored as "gamejam", spaces and case don't matter for it
        auto normalized = [&](std::string_view value) {
            auto result = lower(value);
            if (field == Field::ORIGIN) {
                std::erase(result, ' ');
            }
            return result;
        };
        auto expected = field == Field::ORIGIN ? normalized(text) : text;
        auto equals = [&](std::string_view value) {
            const auto candidate = normalized(value);
            if (field == Field::SHA1) {
                return candidate.starts_with(expected);  // a prefix is enough to name a ROM
            }
            if (field == Field::FILE) {
                // a file name matches by its name alone or with its path
                const auto slash = candidate.find_last_of("/\\");
                return candidate == expected || (slash != std::string::npos && candidate.substr(slash + 1) == expected);
            }
            return candidate == expected;
        };
        const bool equal = std::ranges::any_of(values, equals);
        return op == Operator::EQUAL ? equal : !equal;
    }
    // dates: the whole range of the release has to fit, a release that is no date never matches
    if (values.empty()) {
        return false;
    }
    const auto release = parseDate(values.front());
    if (!release || !date) {
        return false;
    }
    const bool within = release->first >= date->first && release->last <= date->last;
    switch (op) {
        case Operator::EQUAL: return within;
        case Operator::NOT_EQUAL: return !within;
        case Operator::LESS: return release->last < date->first;
        case Operator::LESS_EQUAL: return release->last <= date->last;
        case Operator::GREATER: return release->first > date->last;
        case Operator::GREATER_EQUAL: return release->first >= date->first;
        case Operator::CONTAINS: break;
    }
    return false;
}

LibraryQuery LibraryQuery::parse(std::string_view text, const std::set<std::string>& properties)
{
    LibraryQuery query;
    Parser parser(text, properties);
    query._root = parser.parse(query._usesPresence);
    query._usesRomFields = parser.usesRomFields();
    return query;
}

std::string LibraryQuery::quoted(std::string_view value)
{
    if (const auto position = value.find('"'); position != std::string_view::npos) {
        throw Error("a value can't contain a quote", position);
    }
    return "\"" + std::string(value) + "\"";
}

std::string LibraryQuery::combine(const std::vector<std::string>& queries)
{
    std::vector<std::string_view> parts;
    for (const auto& query : queries) {
        if (std::ranges::any_of(query, [](char c) { return !std::isspace(static_cast<unsigned char>(c)); })) {
            parts.push_back(query);
        }
    }
    if (parts.size() == 1) {
        return std::string(parts.front());
    }
    // each in parentheses, so an "or" in one of them doesn't reach into the others
    std::string result;
    for (const auto part : parts) {
        result += (result.empty() ? "(" : " (") + std::string(part) + ")";
    }
    return result;
}

bool LibraryQuery::matchesRom(const ProgramFacts& program, const RomFacts& rom) const
{
    if (rom.configs.empty()) {
        return _root->matches(program, &rom, nullptr);
    }
    return std::ranges::any_of(rom.configs, [&](const ConfigFacts& config) { return _root->matches(program, &rom, &config); });
}

bool LibraryQuery::matches(const ProgramFacts& program) const
{
    if (!_root) {
        return true;
    }
    if (program.roms.empty()) {
        return _root->matches(program, nullptr, nullptr);
    }
    return std::ranges::any_of(program.roms, [&](const RomFacts& rom) { return matchesRom(program, rom); });
}

std::vector<size_t> LibraryQuery::matchingRoms(const ProgramFacts& program) const
{
    std::vector<size_t> result;
    for (size_t i = 0; i < program.roms.size(); ++i) {
        if (!_root || matchesRom(program, program.roms[i])) {
            result.push_back(i);
        }
    }
    return result;
}
