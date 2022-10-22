//---------------------------------------------------------------------------------------
// src/emulation/chip8decompiler.hpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2022, Steffen Schümann <s.schuemann@pobox.com>
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
// NOTE: This is currently only a wrapper over the c-octo octo-compiler.h implementation
//       by John Earnest
//---------------------------------------------------------------------------------------
#pragma once

#include <string>
#include <memory>

namespace emu {

class Chip8Compiler
{
public:
    Chip8Compiler();
    ~Chip8Compiler();

    bool compile(std::string text);
    bool isError() const;
    const std::string& errorMessage() const;
    uint16_t codeSize() const;
    const uint8_t* code() const;
    const std::string& sha1Hex() const;
    std::pair<uint32_t, uint32_t> addrForLine(uint32_t line) const;
    uint32_t lineForAddr(uint32_t addr) const;

private:
    void updateLineCoverage();
    class Private;
    std::unique_ptr<Private> _impl;
};

}