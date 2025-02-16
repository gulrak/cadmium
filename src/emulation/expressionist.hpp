//---------------------------------------------------------------------------------------
// expressionist.hpp
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

#pragma once

#include <concepts>
#include <cstdint>
#include <memory>
#include <iosfwd>
#include <stack>
#include <string>
#include <unordered_map>
#include <variant>

namespace emu {

template<typename T>
concept AllowedSymbolType = std::same_as<T, uint8_t*> ||
                            std::same_as<T, uint16_t*> ||
                            std::same_as<T, uint32_t*> ||
                            std::same_as<T, int64_t>;

class Expressionist {
public:
    using Value = std::variant<int64_t, const uint8_t*, const uint16_t*, const uint32_t*>;
    enum UnaryOperation { Neg, Not, Inv };
    enum BinaryOperation { Add, Sub, Mul, Div, Shl, Shr, BitAnd, BitOr, BitXor, Less, LessEqual, Greater, GreaterEqual, Equal, NotEqual, Index };
    struct Expr {
        virtual ~Expr() = default;
        virtual int64_t eval() const = 0;
        virtual void dump(std::ostream& out) const = 0;
    };
    using CompiledExpression = std::pair<std::unique_ptr<Expr>, std::string>;

    template<AllowedSymbolType T>
    void define(std::string ident, T value, uint32_t mask = 0)
    {
        _symbols.emplace(ident, DefinedSymbol{value, mask});
    }
    CompiledExpression parseExpression(const std::string& expr);

private:
    enum class TokenType { End, Number, Identifier, Operator, LeftParen, RightParen, LeftBracket, RightBracket };

    struct Token {
        TokenType type;
        std::string text;
        explicit operator bool () const
        {
            return type!=TokenType::End && !text.empty();
        }
    };

    struct OpStackEntry {
        std::string token;
        bool isParen;
        bool isUnary;
        int precedence;
        bool rightAssoc;
    };

    struct BinOpInfo
    {
        BinaryOperation op;
        int precedence;
        bool rightAssoc;
    };

    struct UnOpInfo
    {
        UnaryOperation op;
        int precedence;
        bool rightAssoc;
    };

    struct DefinedSymbol
    {
        Value value;
        uint32_t mask{0};
    };

    Token nextToken();
    void applyOperator();
    static BinOpInfo getBinaryOpInfo(const std::string& s);
    static UnOpInfo getUnaryOpInfo(const std::string& s);

    std::string::const_iterator _iter;
    std::string::const_iterator _end;
    std::stack<std::unique_ptr<Expr>> _operandStack;
    std::stack<OpStackEntry> _opStack;
    std::unordered_map<std::string, DefinedSymbol> _symbols;
};

}
