//---------------------------------------------------------------------------------------
// src/fontgenerator.cpp
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
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char* argv[])
{
    if(argc < 2) {
        std::cerr << "Missing argument" << std::endl;
        exit(-1);
    }

    std::ifstream is(argv[1]);
    std::string line;
    uint32_t codepoint = 0;
    uint8_t buffer[5];
    uint8_t bit = 1;
    while(std::getline(is, line)) {
        if(line.rfind("char: ", 0) == 0) {
            if(codepoint) {
                std::cout << "    {" << codepoint;
                for(auto b : buffer) std::cout << "," << (int)b;
                std::cout << "}," << std::endl;
            }
            codepoint = std::strtoul(line.c_str() + 6, nullptr, 0);
            //std::cout << codepoint << std::endl;
            std::memset(buffer, 0, 5);
            bit = 1;
        }
        else if(line.size() >= 6 && (line[0] == '-' || line[0] == '#')) {
            for(int i = 0; i < 5; ++i) {
                if(line[i] != '-')
                    buffer[i] |= bit;
            }
            bit <<= 1;
        }
    }
    if(codepoint) {
        std::cout << "    {" << codepoint;
        for(auto b : buffer) std::cout << "," << (int)b;
        std::cout << "}," << std::endl;
    }
}
