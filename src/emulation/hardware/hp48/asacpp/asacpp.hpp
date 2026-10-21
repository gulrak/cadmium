//---------------------------------------------------------------------------------------
// src/emulation/hardware/hp48/asacpp/asacpp.hpp
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

#pragma once

#include <ghc/filesystem.hpp>
#include <chiplet/sha1.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fs = ghc::filesystem;

struct SourceLocation {
    std::string file;
    int line{};
    int column{};
};

struct TranslatorResult {
    enum ResultType { eOK, eINFO, eWARNING, eERROR };

    struct Location {
        enum Type { eROOT, eINCLUDED, eINSTANTIATED };
        std::string file;
        int line{};
        int column{};
        Type type{eROOT};
    };

    ResultType resultType{eOK};
    std::string errorMessage;
    std::vector<Location> locations;

    void reset()
    {
        resultType = eOK;
        errorMessage.clear();
        locations.clear();
    }
};

class AsaCpp {
public:
    AsaCpp();
    ~AsaCpp();

    AsaCpp(const AsaCpp&) = delete;
    AsaCpp& operator=(const AsaCpp&) = delete;
    AsaCpp(AsaCpp&&) noexcept;
    AsaCpp& operator=(AsaCpp&&) noexcept;

    const TranslatorResult& preprocess(const fs::path& filename, std::span<const char> source);
    const TranslatorResult& preprocess(const fs::path& filename);
    const TranslatorResult& compile(const fs::path& filename, std::span<const char> source);
    const TranslatorResult& compile(const fs::path& filename);

    void define(std::string name, std::int64_t val = 1);
    void defineExpression(std::string name, std::string expr);
    std::optional<double> definedValue(std::string_view name) const;
    std::optional<std::int32_t> definedInteger(std::string_view name) const;

    const TranslatorResult& compileResult() const { return _translationResult; }
    bool isError() const { return _translationResult.resultType == TranslatorResult::eERROR; }

    void setIncludePaths(const std::vector<std::string>& paths);

    std::uint32_t codeSize() const;
    std::span<const std::uint8_t> code() const;
    const Sha1::Digest& sha1() const;

    std::pair<std::uint32_t, std::uint32_t> addrForLine(std::uint32_t line) const;
    std::uint32_t lineForAddr(std::uint32_t addr) const;

    std::string listing() const;
    std::string preprocessedSource() const;
    std::string hpAscii() const;

private:
    struct Impl;
    Impl* _impl{};
    TranslatorResult _translationResult;
};
