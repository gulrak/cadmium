//---------------------------------------------------------------------------------------
// src/emulation/keymatrix.hpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2023, Steffen Schümann <s.schuemann@pobox.com>
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

#include <array>
#include <cstdint>
#include <optional>
#include <sstream>

namespace emu {

template<int ROWS, int COLS>
class KeyMatrix
{
public:
    struct OutputWithConnection { uint16_t value; uint16_t connections; };
    void setRows(uint16_t levels, uint16_t connections)
    {
        uint16_t bit = 1;
        for(auto& state : _rowStates) {
            if(connections & bit) {
                state.input = levels & bit;
                state.output = levels & bit;
            }
            else {
                state.input.reset();
            }
            bit <<= 1;
        }
        updateStates();
    }
    OutputWithConnection getRows(uint16_t mask) const
    {
        uint16_t result = 0;
        uint16_t connections = 0;
        uint16_t bit = 1;
        for(auto& state : _rowStates) {
            if (state.output) {
                if(state.output.value())
                    result |= bit;
                connections |= bit;
            }
            bit <<= 1;
        }
        return {uint16_t(result & mask), uint16_t(connections & mask)};
    }
    void setCols(uint16_t levels, uint16_t connections)
    {
        uint16_t bit = 1;
        for(auto& state : _colStates) {
            if(connections & bit) {
                state.input = levels & bit;
                state.output = levels & bit;
            }
            else {
                state.input.reset();
            }
            bit <<= 1;
        }
        updateStates();
    }
    OutputWithConnection getCols(uint16_t mask) const
    {
        uint16_t result = 0;
        uint16_t connections = 0;
        uint16_t bit = 1;
        for(auto& state : _colStates) {
            if (state.output) {
                if(state.output.value())
                    result |= bit;
                connections |= bit;
            }
            bit <<= 1;
        }
        return {uint16_t(result & mask), uint16_t(connections & mask)};
    }
    void updateKeys(const std::array<bool,ROWS*COLS>& keys)
    {
        _switchStates = keys;
        updateStates();
    }
private:
    void updateStates()
    {
        int row = 0;
        for(auto& state : _colStates)
            if(!state.input)
                state.output.reset();
        for(auto& state : _rowStates) {
            if(state.input) {
                for(int i = 0; i < COLS; ++i) {
                    if(!_colStates[i].input && _switchStates[COLS*row + i]) {
                        _colStates[i].output = state.input;
                    }
                }
            }
            else
                state.output.reset();
            ++row;
        }
        int col = 0;
        for(auto& state : _colStates) {
            if(state.input) {
                for(int i = 0; i < ROWS; ++i) {
                    if(!_rowStates[i].input && _switchStates[COLS*i + col]) {
                        _rowStates[i].output = state.input;
                    }
                }
            }
            ++col;
        }
        //dump();
        return;
    }
    std::string dump() const
    {
        std::ostringstream os;
        for(int r = 0; r < ROWS; ++r) {
            os << "R" << r << ":" << (_rowStates[r].input ? (_rowStates[r].input.value() ? "IH " : "IL ") : (_rowStates[r].output ? (_rowStates[r].output.value() ? "OH " : "OL ") : "?? "));
            for(int c = 0; c < COLS; ++c) {
                os << (_switchStates[r*COLS + c] ? " X  " : " O  ");
            }
            os << "\n";
        }
        os << "      ";
        for(int c = 0; c < COLS; ++c) {
            os << c << (_colStates[c].input ? (_colStates[c].input.value() ? "IH " : "IL ") : (_colStates[c].output ? (_colStates[c].output.value() ? "OH " : "OL ") : "?? "));
        }
        //std::cout << os.str() << std::endl;
        return os.str();
    }
    struct Pin {
        std::optional<bool> input;
        std::optional<bool> output;
    };
    std::array<Pin,ROWS> _rowStates{};
    std::array<Pin,COLS> _colStates{};
    std::array<bool,ROWS*COLS> _switchStates{};
};


}
