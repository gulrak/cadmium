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

namespace {

bool isConstant(const emu::Expressionist::Value &value) {
    return std::visit([]([[maybe_unused]] const auto &val) -> bool {
        using T = std::decay_t<decltype(val)>;
        return !std::is_pointer_v<T>;
    }, value);
}

struct LiteralExpr final : emu::Expressionist::Expr
{
    emu::Expressionist::Value value;
    uint32_t mask{0};
    std::string valName;
    LiteralExpr(const emu::Expressionist::Value &v, std::string name, uint32_t bitMask) : value(v), valName(name), mask(bitMask) {}
    int64_t eval() const override
    {
        return std::visit([](auto val) -> int64_t {
            if constexpr (std::is_pointer_v<decltype(val)>) {
                return static_cast<int64_t>(*val);
            }
            else {
                return static_cast<int64_t>(val);
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
    UnaryExpr(const UnaryOperation uop, std::unique_ptr<Expr>&& expr, std::string name) : op(uop), operand(std::move(expr)), opName(name) {}
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
    BinaryExpr(const BinaryOperation bop, std::unique_ptr<Expr>&& lhs, std::unique_ptr<Expr>&& rhs, std::string name) : op(bop), lhs(std::move(lhs)), rhs(std::move(rhs)), opName(name) {}
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
        return std::visit([&](const auto &val) -> int64_t {
            if constexpr (std::is_pointer_v<decltype(val)>) {
                return val[index->eval() & mask];
            }
            else {
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

emu::Expressionist::Token emu::Expressionist::nextToken()
{
    while (_iter != _end && std::isspace(static_cast<unsigned char>(*_iter)))
        ++_iter;
    if (_iter == _end)
        return {TokenType::End, ""};
    char c = *_iter;
    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
        auto start = _iter;
        ++_iter;
        while (_iter != _end && (std::isalnum(static_cast<unsigned char>(*_iter)) || *_iter == '_')) {
            ++_iter;
        }
        return {TokenType::Identifier, std::string(start, _iter)};
    }
    if (std::isdigit(static_cast<unsigned char>(c))) {
        auto start = _iter;
        ++_iter;
        while (_iter != _end && std::isdigit(static_cast<unsigned char>(*_iter)))
            ++_iter;
        return {TokenType::Number, std::string(start, _iter)};
    }
    if (c == '(') {
        ++_iter;
        return {TokenType::LeftParen, "("};
    }
    if (c == ')') {
        ++_iter;
        return {TokenType::RightParen, ")"};
    }
    if (c == '[') {
        ++_iter;
        return {TokenType::LeftBracket, "["};
    }
    if (c == ']') {
        ++_iter;
        return {TokenType::RightBracket, "]"};
    }

    // All allowed operators: '!', '~', '*', '/', '+', '-',
    // '<<', '>>', '==', '<=', '<', '>=', '>', '&', '|' and '^'
    Token token;
    token.type = TokenType::Operator;
    if (c == '<' || c == '>') {
        const char first = c;
        ++_iter;
        if (_iter != _end) {
            char next = *_iter;
            // check for "<<", "<=", ">>" or ">="
            if (next == first || next == '=') {
                token.text = std::string{first, next};
                ++_iter;
                return token;
            }
        }
        // Otherwise, just a single-character operator.
        token.text = std::string{first};
        return token;
    }
    if (c == '=') {
        ++_iter;
        if (_iter != _end && *_iter == '=') {
            token.text = "==";
            ++_iter;
            return token;
        }
        token.text = "=";
        return token;
    }

    token.text = std::string{c};
    ++_iter;
    return token;
}

emu::Expressionist::BinOpInfo emu::Expressionist::getBinaryOpInfo(const std::string& s)
{
    if (s == "*")
        return {Mul, 7, false};
    if (s == "/")
        return {Div, 7, false};
    if (s == "+")
        return {Add, 6, false};
    if (s == "-")
        return {Sub, 6, false};
    if (s == "<<")
        return {Shl, 5, false};
    if (s == ">>")
        return {Shr, 5, false};
    if (s == "==")
        return {Equal, 3, false};
    if (s == "<")
        return {Less, 3, false};
    if (s == "<=")
        return {LessEqual, 3, false};
    if (s == ">")
        return {Greater, 3, false};
    if (s == ">=")
        return {GreaterEqual, 3, false};
    if (s == "&")
        return {BitAnd, 2, false};
    if (s == "|")
        return {BitAnd, 2, false};
    if (s == "^")
        return {BitAnd, 2, false};
    if (s == "[]")
        return {Index, 9, false};
    throw std::runtime_error("Unknown binary operator: " + s);
}

emu::Expressionist::UnOpInfo emu::Expressionist::getUnaryOpInfo(const std::string& s)
{
    if (s == "-")
        return {Neg, 8, true};
    if (s == "!")
        return {Not, 8, true};
    if (s == "~")
        return {Inv, 8, true};
    throw std::runtime_error("Unknown unary operator: " + s);
}

emu::Expressionist::CompiledExpression emu::Expressionist::parseExpression(const std::string& expr)
{
    Token tok;
    _iter = expr.begin();
    _end = expr.end();
    _operandStack = {};
    _opStack = {};
    try {
        bool expectOperand = true;
        while ((tok = nextToken())) {
            switch (tok.type) {
                case TokenType::Number: {
                    int64_t num = static_cast<int64_t>(std::stoul(tok.text));
                    _operandStack.push(std::make_unique<LiteralExpr>(num, tok.text, 0));
                    expectOperand = false;
                    break;
                }
                case TokenType::Identifier: {
                    auto iter = _symbols.find(tok.text);
                    if (iter == _symbols.end()) {
                        throw std::runtime_error("Unknown identifier");
                    }
                    _operandStack.push(std::make_unique<LiteralExpr>(iter->second.value, tok.text, iter->second.mask));
                    expectOperand = false;
                    break;
                }
                case TokenType::LeftParen: {
                    _opStack.push({.token = "", .isParen = true, .isUnary = false, .precedence = 0, .rightAssoc = false});
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
                    _opStack.push({.token = "[", .isParen = true, .isUnary = false, .precedence = 0, .rightAssoc = false});
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
                    _opStack.pop(); // pop '['
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
                            while (!_opStack.empty() && !_opStack.top().isParen && _opStack.top().precedence > info.precedence) {
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
                            if ((_opStack.top().precedence > info.precedence) || (_opStack.top().precedence == info.precedence && !_opStack.top().rightAssoc)) {
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
            _operandStack.push(std::make_unique<IndexExpr>(std::unique_ptr<LiteralExpr>(static_cast<LiteralExpr*>(left.release())), std::move(right), 0xFFF));
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
