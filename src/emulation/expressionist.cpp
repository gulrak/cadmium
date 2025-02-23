//---------------------------------------------------------------------------------------
// expressionist.cpp
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

#include "expressionist.hpp"

#include <ostream>
#include <sstream>


namespace {

constexpr int NO_PRECEDENCE = 20;

bool isConstant(const emu::Expressionist::Value& value)
{
    return std::visit(
        []([[maybe_unused]] const auto& val) -> bool {
            if constexpr (std::is_pointer_v<std::decay_t<decltype(val)>>
                || std::is_same_v<std::decay_t<decltype(val)>, std::function<int64_t(uint32_t)>>
                || std::is_same_v<std::decay_t<decltype(val)>, std::function<int64_t()>>) {
                return false;
            }
            else {
                return true;
            }
        },
        value);
}

struct LiteralExpr final : emu::Expressionist::Expr
{
    emu::Expressionist::Value value;
    uint32_t mask{0};
    std::string valName;
    LiteralExpr(emu::Expressionist::Value v, std::string name, uint32_t bitMask)
    : value(std::move(v))
    , mask(bitMask)
    , valName(std::move(name))
    {}
    int64_t eval() const override
    {
        return std::visit([]([[maybe_unused]] const auto& val) -> int64_t {
            if constexpr (std::is_pointer_v<std::decay_t<decltype(val)>>) {
                return static_cast<int64_t>(*val);
            }
            else if constexpr (std::is_same_v<std::decay_t<decltype(val)>, std::function<int64_t(uint32_t)>>) {
                return 0;
            }
            else if constexpr (std::is_same_v<std::decay_t<decltype(val)>, std::function<int64_t()>>) {
                return val();
            }
            else {
                return static_cast<int64_t>(val);
            }
        }, value);
    }
    bool isIndexBase() const
    {
        return std::visit([&]([[maybe_unused]] const auto& val) -> int64_t {
            if constexpr (std::is_pointer_v<std::decay_t<decltype(val)>>) {
                return mask != 0;
            }
            else if constexpr (std::is_same_v<std::decay_t<decltype(val)>, std::function<int64_t(uint32_t)>>) {
                return true;
            }
            else /*if constexpr (std::is_same_v<std::decay_t<decltype(val)>, std::function<int64_t()>>) {
                return false;
            }
            else*/ {
                return false;
            }
        }, value);
    }
    void dump(std::ostream &out) const override { out << valName; }
};

struct UnaryExpr final : emu::Expressionist::Expr
{
    using UnaryOperation = emu::Expressionist::UnaryOperation;
    UnaryOperation op;
    std::unique_ptr<Expr> operand;
    std::string opName;
    UnaryExpr(const UnaryOperation uop, std::unique_ptr<Expr>&& expr, std::string name)
    : op(uop)
    , operand(std::move(expr))
    , opName(std::move(name))
    {
        if (auto literal = dynamic_cast<LiteralExpr*>(operand.get()); literal && literal->isIndexBase()) {
            throw std::runtime_error("Index base cannot be used in unary operator");
        }
    }
    int64_t eval() const override
    {
        switch (op) {
            case UnaryOperation::Inv:
                return ~operand->eval();
            case UnaryOperation::Not:
                return !operand->eval();
            case UnaryOperation::Neg:
                return -operand->eval();
        }
        return 0;
    }
    void dump(std::ostream &out) const override
    {
        out << "[U" << opName << ":";
        operand->dump(out);
        out << "]";
    }
};

struct BinaryExpr final : emu::Expressionist::Expr
{
    using BinaryOperation = emu::Expressionist::BinaryOperation;
    BinaryOperation op;
    std::unique_ptr<Expr> lhs;
    std::unique_ptr<Expr> rhs;
    std::string opName;
    BinaryExpr(const BinaryOperation bop, std::unique_ptr<Expr>&& lhs, std::unique_ptr<Expr>&& rhs, std::string name)
    : op(bop)
    , lhs(std::move(lhs))
    , rhs(std::move(rhs))
    , opName(std::move(name))
    {
        if (auto literal = dynamic_cast<LiteralExpr*>(lhs.get()); literal && literal->isIndexBase()) {
            throw std::runtime_error("Index base cannot be used in binary operator");
        }
        if (auto literal = dynamic_cast<LiteralExpr*>(rhs.get()); literal && literal->isIndexBase()) {
            throw std::runtime_error("Index base cannot be used in binary operator");
        }
    }
    int64_t eval() const override
    {
        // Add, Sub, Mul, Div, Shl, Shr, BitAnd, BitOr, BitXor, Less, LessEqual, Greater, GreaterEqual, Equal, NotEqual
        switch (op) {
            case BinaryOperation::Add:
                return lhs->eval() + rhs->eval();
            case BinaryOperation::Sub:
                return lhs->eval() - rhs->eval();
            case BinaryOperation::Mul:
                return lhs->eval() * rhs->eval();
            case BinaryOperation::Div:
                return lhs->eval() / rhs->eval();
            case BinaryOperation::Shl:
                return lhs->eval() << rhs->eval();
            case BinaryOperation::Shr:
                return lhs->eval() >> rhs->eval();
            case BinaryOperation::BitAnd:
                return lhs->eval() & rhs->eval();
            case BinaryOperation::BitOr:
                return lhs->eval() | rhs->eval();
            case BinaryOperation::BitXor:
                return lhs->eval() ^ rhs->eval();
            case BinaryOperation::LogAnd:
                return lhs->eval() && rhs->eval();
            case BinaryOperation::LogOr:
                return lhs->eval() || rhs->eval();
            case BinaryOperation::Less:
                return lhs->eval() < rhs->eval();
            case BinaryOperation::LessEqual:
                return lhs->eval() <= rhs->eval();
            case BinaryOperation::Greater:
                return lhs->eval() > rhs->eval();
            case BinaryOperation::GreaterEqual:
                return lhs->eval() >= rhs->eval();
            case BinaryOperation::Equal:
                return lhs->eval() == rhs->eval();
            case BinaryOperation::NotEqual:
                return lhs->eval() != rhs->eval();
            case BinaryOperation::Index:
                return 0; // not actually implemented in BinaryExpr
        }
        return 0;
    }
    void dump(std::ostream &out) const override
    {
        out << "[B" << opName << ":";
        lhs->dump(out);
        out << ",";
        rhs->dump(out);
        out << "]";
    }
};

struct IndexExpr final : emu::Expressionist::Expr
{
    std::unique_ptr<LiteralExpr> base;
    std::unique_ptr<Expr> index;
    uint32_t mask;
    IndexExpr(std::unique_ptr<LiteralExpr>&& literalExpr, std::unique_ptr<Expr>&& indexExpr, uint32_t mask) : base(std::move(literalExpr)), index(std::move(indexExpr)), mask(mask) {}
    int64_t eval() const override
    {
        if (!base->isIndexBase()) {
            throw std::runtime_error("Operand is not indexable");
        }
        if (auto literal = dynamic_cast<LiteralExpr*>(index.get()); literal && literal->isIndexBase()) {
            throw std::runtime_error("Index base cannot be used as index");
        }
        return std::visit([&](const auto &val) -> int64_t {
            if constexpr (std::is_pointer_v<std::decay_t<decltype(val)>>) {
                return val[index->eval() & mask];
            }
            else if constexpr (std::is_same_v<std::decay_t<decltype(val)>, std::function<int64_t(uint32_t)>>) {
                return val(index->eval() & mask);
            }
            else /*if constexpr (std::is_same_v<std::decay_t<decltype(val)>, std::function<int64_t()>>) {
                return 0;
            }
            else*/ {
                return 0;
            }
        }, base->value);
    }
    void dump(std::ostream &out) const override
    {
        out << "[@:";
        base->dump(out);
        out << ",";
        index->dump(out);
        out << "]";
    }
};
}

emu::Expressionist::Token emu::Expressionist::nextToken(std::string_view::const_iterator& iter, std::string_view::const_iterator end)
{
    while (iter != end && std::isspace(static_cast<unsigned char>(*iter)))
        ++iter;
    if (iter == end)
        return {TokenType::End, ""};
    while (iter != end && std::isspace(static_cast<unsigned char>(*iter)))
        ++iter;
    if (iter == end)
        return {TokenType::End, ""};
    auto c = *iter;
    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
        auto start = iter;
        ++iter;
        while (iter != end && (std::isalnum(static_cast<unsigned char>(*iter)) || *iter == '_')) {
            ++iter;
        }
        return {TokenType::Identifier, std::string(start, iter)};
    }
    if (std::isdigit(static_cast<unsigned char>(c))) {
        auto start = iter;
        ++iter;
        while (iter != end && std::isdigit(static_cast<unsigned char>(*iter)))
            ++iter;
        return {TokenType::Number, std::string(start, iter)};
    }
    if (c == '(') {
        ++iter;
        return {TokenType::LeftParen, "("};
    }
    if (c == ')') {
        ++iter;
        return {TokenType::RightParen, ")"};
    }
    if (c == '[') {
        ++iter;
        return {TokenType::LeftBracket, "["};
    }
    if (c == ']') {
        ++iter;
        return {TokenType::RightBracket, "]"};
    }

    // All allowed operators: '!', '~', '*', '/', '+', '-',
    // '<<', '>>', '==', '<=', '<', '>=', '>', '&', '|', '&&', '||' and '^'
    Token token;
    token.type = TokenType::Operator;
    if (c == '<' || c == '>') {
        const char first = c;
        ++iter;
        if (iter != end) {
            // check for "<<", "<=", ">>" or ">="
            if (*iter == first || *iter == '=') {
                token.text = std::string{first, *iter};
                ++iter;
                return token;
            }
        }
        // Otherwise, just a single-character operator.
        token.text = std::string{first};
        return token;
    }
    if (c == '=') {
        ++iter;
        if (iter != end && *iter == '=') {
            token.text = "==";
            ++iter;
            return token;
        }
        token.text = "=";
        return token;
    }
    if (c == '&' || c == '|') {
        const char first = c;
        ++iter;
        if (iter != end && *iter == first) {
            token.text = std::string{first, *iter};
            ++iter;
            return token;
        }
        token.text = std::string{first};
        return token;
    }
    token.text = std::string{c};
    ++iter;
    return token;
}

emu::Expressionist::BinOpInfo emu::Expressionist::getBinaryOpInfo(const std::string& s)
{
    if (s == "*")
        return {Mul, 5, false};
    if (s == "/")
        return {Div, 5, false};
    if (s == "+")
        return {Add, 6, false};
    if (s == "-")
        return {Sub, 6, false};
    if (s == "<<")
        return {Shl, 7, false};
    if (s == ">>")
        return {Shr, 7, false};
    if (s == "==")
        return {Equal, 10, false};
    if (s == "!=")
        return {NotEqual, 10, false};
    if (s == "<")
        return {Less, 9, false};
    if (s == "<=")
        return {LessEqual, 9, false};
    if (s == ">")
        return {Greater, 9, false};
    if (s == ">=")
        return {GreaterEqual, 9, false};
    if (s == "&")
        return {BitAnd, 11, false};
    if (s == "^")
        return {BitXor, 12, false};
    if (s == "|")
        return {BitOr, 13, false};
    if (s == "&&")
        return {LogAnd, 14, false};
    if (s == "||")
        return {LogOr, 15, false};
    if (s == "[]")
        return {Index, 2, false};
    throw std::runtime_error("Unknown binary operator: " + s);
}

emu::Expressionist::UnOpInfo emu::Expressionist::getUnaryOpInfo(const std::string& s)
{
    if (s == "-")
        return {Neg, 3, true};
    if (s == "!")
        return {Not, 3, true};
    if (s == "~")
        return {Inv, 3, true};
    throw std::runtime_error("Unknown unary operator: " + s);
}

emu::Expressionist::CompiledExpression emu::Expressionist::parseExpression(std::string_view expr)
{
    Token tok;
    auto iter = expr.begin();
    const auto end = expr.end();
    _operandStack = {};
    _opStack = {};
    try {
        bool expectOperand = true;
        while ((tok = nextToken(iter, end))) {
            switch (tok.type) {
                case TokenType::Number: {
                    auto num = static_cast<int64_t>(std::stoul(tok.text));
                    _operandStack.push(std::make_unique<LiteralExpr>(num, tok.text, 0));
                    expectOperand = false;
                    break;
                }
                case TokenType::Identifier: {
                    auto symIter = _symbols.find(tok.text);
                    if (symIter == _symbols.end()) {
                        throw std::runtime_error("Unknown identifier");
                    }
                    _operandStack.push(std::make_unique<LiteralExpr>(symIter->second.value, tok.text, symIter->second.mask));
                    expectOperand = false;
                    break;
                }
                case TokenType::LeftParen: {
                    _opStack.push({.token = "", .isParen = true, .isUnary = false, .precedence = NO_PRECEDENCE, .rightAssoc = false});
                    expectOperand = true;
                    break;
                }
                case TokenType::RightParen: {
                    while (!_opStack.empty() && !_opStack.top().isParen) {
                        applyOperator();
                    }
                    if (_opStack.empty() || !_opStack.top().isParen)
                        throw std::runtime_error("Mismatched parentheses");
                    _opStack.pop();  // pop '('
                    expectOperand = false;
                    break;
                }
                case TokenType::LeftBracket: {
                    if (expectOperand)
                        throw std::runtime_error("Unexpected '[' operator: missing left-hand operand");
                    _opStack.push({.token = "[", .isParen = true, .isUnary = false, .precedence = NO_PRECEDENCE, .rightAssoc = false});
                    expectOperand = true;
                    break;
                }
                case TokenType::RightBracket: {
                    // When encountering a right bracket, pop until we find the matching '['.
                    while (!_opStack.empty() && !(_opStack.top().isParen && _opStack.top().token == "[")) {
                        applyOperator();
                    }
                    if (_opStack.empty())
                        throw std::runtime_error("Mismatched brackets");
                    _opStack.pop();  // pop '['
                    _opStack.push({.token = "[]", .isParen = false, .isUnary = false, .precedence = getBinaryOpInfo("[]").precedence, .rightAssoc = false});
                    applyOperator();
                    expectOperand = false;
                    break;
                }
                case TokenType::Operator: {
                    if (expectOperand) {
                        if (tok.text == "-" || tok.text == "!" || tok.text == "~") {
                            const auto info = getUnaryOpInfo(tok.text);
                            // For right-associative (unary) operators, only pop those with strictly higher precedence.
                            while (!_opStack.empty() && !_opStack.top().isParen && _opStack.top().precedence < info.precedence) {
                                applyOperator();
                            }
                            _opStack.push({.token = tok.text, .isParen = false, .isUnary = true, .precedence = info.precedence, .rightAssoc = info.rightAssoc});
                        }
                        else {
                            throw std::runtime_error("Unexpected binary operator in unary context: " + tok.text);
                        }
                        expectOperand = true;
                    }
                    else {
                        const auto info = getBinaryOpInfo(tok.text);
                        while (!_opStack.empty() && !_opStack.top().isParen) {
                            if ((_opStack.top().precedence < info.precedence) || (_opStack.top().precedence == info.precedence && !_opStack.top().rightAssoc)) {
                                applyOperator();
                            }
                            else {
                                break;
                            }
                        }
                        _opStack.push({.token = tok.text, .isParen = false, .isUnary = false, .precedence = info.precedence, .rightAssoc = info.rightAssoc});
                        expectOperand = true;
                    }
                    break;
                }
                default:
                    throw std::runtime_error("Unexpected token in expression: " + tok.text);
            }
        }
        while (!_opStack.empty()) {
            if (_opStack.top().isParen)
                throw std::runtime_error("Mismatched parentheses");
            applyOperator();
        }
        if (_operandStack.size() != 1)
            throw std::runtime_error("Invalid expression");
    }
    catch (const std::exception& e) {
        return {nullptr, e.what()};
    }
    return {std::move(_operandStack.top()), std::string()};
}

void emu::Expressionist::applyOperator()
{
    if (_opStack.empty())
        throw std::runtime_error("Operator stack empty when trying to apply operator");
    OpStackEntry opEntry = _opStack.top();
    _opStack.pop();
    if (opEntry.isUnary) {
        if (_operandStack.empty())
            throw std::runtime_error("Not enough operands for unary operator " + opEntry.token);
        auto operand = std::move(_operandStack.top());
        _operandStack.pop();
        auto info = getUnaryOpInfo(opEntry.token);
        _operandStack.push(std::make_unique<UnaryExpr>(info.op, std::move(operand), opEntry.token));
    }
    else {
        if (_operandStack.size() < 2)
            throw std::runtime_error("Not enough operands for binary operator " + opEntry.token);
        auto right = std::move(_operandStack.top());
        _operandStack.pop();
        auto left = std::move(_operandStack.top());
        _operandStack.pop();
        auto info = getBinaryOpInfo(opEntry.token);
        if (opEntry.token == "[]") {
            if (dynamic_cast<const LiteralExpr*>(left.get()) == nullptr)
                throw std::runtime_error("Unexpected index operator");
            _operandStack.push(std::make_unique<IndexExpr>(std::unique_ptr<LiteralExpr>(dynamic_cast<LiteralExpr*>(left.release())), std::move(right), 0xFFF));
        }
        else {
            auto binExp = std::make_unique<BinaryExpr>(info.op, std::move(left), std::move(right), opEntry.token);
            if (const LiteralExpr *lhs, *rhs;
                (lhs = dynamic_cast<const LiteralExpr*>(binExp->lhs.get())) != nullptr &&
                (rhs = dynamic_cast<const LiteralExpr*>(binExp->rhs.get())) != nullptr &&
                isConstant(lhs->value) && isConstant(rhs->value)) {
                _operandStack.push(std::make_unique<LiteralExpr>(binExp->eval(), "(" + std::to_string(binExp->eval()) + ":" + lhs->valName + opEntry.token + rhs->valName + ")",0));
                }
            else {
                _operandStack.push(std::move(binExp));
            }
        }
    }
}

std::string emu::Expressionist::format(std::string_view fmt)
{
    return format(fmt, [&](std::string_view expression) -> std::string {
        std::string result;
        try {
            auto [expr, error] = parseExpression(expression);
            if (expr) {
                result = std::to_string(expr->eval());
            }
        }
        catch (std::runtime_error& ex) {
            // TODO: report error
        }
        return result;
    });
}

std::string emu::Expressionist::format(std::string_view fmt, const std::function<std::string(std::string_view)>& eval)
{
    std::ostringstream oss;
    auto i = 0u;
    const auto n = fmt.size();

    while (i < n) {
        char c = fmt[i];
        if (c == '{') {
            // Check for double curly brace for literal '{'
            if (i + 1 < n && fmt[i+1] == '{') {
                oss << '{';
                i += 2;
            } else {
                // Start of an expression placeholder.
                auto expr_start = i + 1;
                auto expr_end = expr_start;
                // Find the matching unescaped '}'
                bool foundClosing = false;
                while (expr_end < n) {
                    if (fmt[expr_end] == '}') {
                        foundClosing = true;
                        break;
                    }
                    ++expr_end;
                }
                if (!foundClosing)
                    throw std::runtime_error("Unmatched '{' in format string");

                // Extract the expression inside the braces.
                std::string_view expr = fmt.substr(expr_start, expr_end - expr_start);
                // Replace the expression with the result from the lambda.
                oss << eval(expr);
                i = expr_end + 1;
            }
        } else if (c == '}') {
            // Check for double curly brace for literal '}'
            if (i + 1 < n && fmt[i+1] == '}') {
                oss << '}';
                i += 2;
            } else {
                throw std::runtime_error("Unmatched '}' in format string");
            }
        } else {
            oss << c;
            ++i;
        }
    }
    return oss.str();
}

