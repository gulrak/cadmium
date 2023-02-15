//---------------------------------------------------------------------------------------
// src/emulation/chip8dream.cpp
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

#include <emulation/chip8dream.hpp>
#include <emulation/logger.hpp>
#include <emulation/hardware/mc682x.hpp>
#include <emulation/hardware/keymatrix.hpp>
#include <emulation/utility.hpp>

#include <nlohmann/json.hpp>

//#define USE_CHIPOSLO

#include <atomic>
#include <thread>

namespace emu {

class Chip8Dream::Private {
public:
    static constexpr uint16_t FETCH_LOOP_ENTRY = 0xC00C;
    explicit Private(Chip8EmulatorHost& host, M6800Bus<>& bus, const Chip8EmulatorOptions& options) : _host(host), _cpu(bus)/*, _video(Cdp186x::eCDP1861, _cpu, options)*/ {}
    Chip8EmulatorHost& _host;
    CadmiumM6800 _cpu;
    MC682x _pia;
    KeyMatrix<4,4> _keyMatrix;
    bool _ic20aNAnd{false};
    int64_t _irqStart{0};
    int64_t _nextFrame{0};
    std::atomic<float> _wavePhase{0};
    std::array<uint8_t,MAX_MEMORY_SIZE> _ram{};
    std::array<uint8_t,1024> _rom{};
    std::array<uint8_t,256*192> _screenBuffer;
};


static const uint8_t dream6800Rom[] = {
    // Copyright (c) 1978, Michael J. Bauer
    0x8d, 0x77, 0xce, 0x02, 0x00, 0xdf, 0x22, 0xce, 0x00, 0x5f, 0xdf, 0x24, 0xde, 0x22, 0xee, 0x00, 0xdf, 0x28, 0xdf, 0x14, 0xbd, 0xc0, 0xd0, 0x96, 0x14, 0x84, 0x0f, 0x97, 0x14, 0x8d, 0x21, 0x97, 0x2e, 0xdf, 0x2a, 0x96, 0x29, 0x44, 0x44, 0x44,
    0x44, 0x8d, 0x15, 0x97, 0x2f, 0xce, 0xc0, 0x48, 0x96, 0x28, 0x84, 0xf0, 0x08, 0x08, 0x80, 0x10, 0x24, 0xfa, 0xee, 0x00, 0xad, 0x00, 0x20, 0xcc, 0xce, 0x00, 0x2f, 0x08, 0x4a, 0x2a, 0xfc, 0xa6, 0x00, 0x39, 0xc0, 0x6a, 0xc0, 0xa2, 0xc0, 0xac,
    0xc0, 0xba, 0xc0, 0xc1, 0xc0, 0xc8, 0xc0, 0xee, 0xc0, 0xf2, 0xc0, 0xfe, 0xc0, 0xcc, 0xc0, 0xa7, 0xc0, 0x97, 0xc0, 0xf8, 0xc2, 0x1f, 0xc0, 0xd7, 0xc1, 0x5f, 0xd6, 0x28, 0x26, 0x25, 0x96, 0x29, 0x81, 0xe0, 0x27, 0x05, 0x81, 0xee, 0x27, 0x0e,
    0x39, 0x4f, 0xce, 0x01, 0x00, 0xa7, 0x00, 0x08, 0x8c, 0x02, 0x00, 0x26, 0xf8, 0x39, 0x30, 0x9e, 0x24, 0x32, 0x97, 0x22, 0x32, 0x97, 0x23, 0x9f, 0x24, 0x35, 0x39, 0xde, 0x14, 0x6e, 0x00, 0x96, 0x30, 0x5f, 0x9b, 0x15, 0x97, 0x15, 0xd9, 0x14,
    0xd7, 0x14, 0xde, 0x14, 0xdf, 0x22, 0x39, 0xde, 0x14, 0xdf, 0x26, 0x39, 0x30, 0x9e, 0x24, 0x96, 0x23, 0x36, 0x96, 0x22, 0x36, 0x9f, 0x24, 0x35, 0x20, 0xe8, 0x96, 0x29, 0x91, 0x2e, 0x27, 0x10, 0x39, 0x96, 0x29, 0x91, 0x2e, 0x26, 0x09, 0x39,
    0x96, 0x2f, 0x20, 0xf0, 0x96, 0x2f, 0x20, 0xf3, 0xde, 0x22, 0x08, 0x08, 0xdf, 0x22, 0x39, 0xbd, 0xc2, 0x97, 0x7d, 0x00, 0x18, 0x27, 0x07, 0xc6, 0xa1, 0xd1, 0x29, 0x27, 0xeb, 0x39, 0xc6, 0x9e, 0xd1, 0x29, 0x27, 0xd0, 0x20, 0xd5, 0x96, 0x29,
    0x20, 0x3b, 0x96, 0x29, 0x9b, 0x2e, 0x20, 0x35, 0x8d, 0x38, 0x94, 0x29, 0x20, 0x2f, 0x96, 0x2e, 0xd6, 0x29, 0xc4, 0x0f, 0x26, 0x02, 0x96, 0x2f, 0x5a, 0x26, 0x02, 0x9a, 0x2f, 0x5a, 0x26, 0x02, 0x94, 0x2f, 0x5a, 0x5a, 0x26, 0x0a, 0x7f, 0x00,
    0x3f, 0x9b, 0x2f, 0x24, 0x03, 0x7c, 0x00, 0x3f, 0x5a, 0x26, 0x0a, 0x7f, 0x00, 0x3f, 0x90, 0x2f, 0x25, 0x03, 0x7c, 0x00, 0x3f, 0xde, 0x2a, 0xa7, 0x00, 0x39, 0x86, 0xc0, 0x97, 0x2c, 0x7c, 0x00, 0x2d, 0xde, 0x2c, 0x96, 0x0d, 0xab, 0x00, 0xa8,
    0xff, 0x97, 0x0d, 0x39, 0x07, 0xc1, 0x79, 0x0a, 0xc1, 0x7d, 0x15, 0xc1, 0x82, 0x18, 0xc1, 0x85, 0x1e, 0xc1, 0x89, 0x29, 0xc1, 0x93, 0x33, 0xc1, 0xde, 0x55, 0xc1, 0xfa, 0x65, 0xc2, 0x04, 0xce, 0xc1, 0x44, 0xc6, 0x09, 0xa6, 0x00, 0x91, 0x29,
    0x27, 0x09, 0x08, 0x08, 0x08, 0x5a, 0x26, 0xf4, 0x7e, 0xc3, 0x60, 0xee, 0x01, 0x96, 0x2e, 0x6e, 0x00, 0x96, 0x20, 0x20, 0xb0, 0xbd, 0xc2, 0xc4, 0x20, 0xab, 0x97, 0x20, 0x39, 0x16, 0x7e, 0xc2, 0xe1, 0x5f, 0x9b, 0x27, 0x97, 0x27, 0xd9, 0x26,
    0xd7, 0x26, 0x39, 0xce, 0xc1, 0xbc, 0x84, 0x0f, 0x08, 0x08, 0x4a, 0x2a, 0xfb, 0xee, 0x00, 0xdf, 0x1e, 0xce, 0x00, 0x08, 0xdf, 0x26, 0xc6, 0x05, 0x96, 0x1e, 0x84, 0xe0, 0xa7, 0x04, 0x09, 0x86, 0x03, 0x79, 0x00, 0x1f, 0x79, 0x00, 0x1e, 0x4a,
    0x26, 0xf7, 0x5a, 0x26, 0xeb, 0x39, 0xf6, 0xdf, 0x49, 0x25, 0xf3, 0x9f, 0xe7, 0x9f, 0x3e, 0xd9, 0xe7, 0xcf, 0xf7, 0xcf, 0x24, 0x9f, 0xf7, 0xdf, 0xe7, 0xdf, 0xb7, 0xdf, 0xd7, 0xdd, 0xf2, 0x4f, 0xd6, 0xdd, 0xf3, 0xcf, 0x93, 0x4f, 0xde, 0x26,
    0xc6, 0x64, 0x8d, 0x06, 0xc6, 0x0a, 0x8d, 0x02, 0xc6, 0x01, 0xd7, 0x0e, 0x5f, 0x91, 0x0e, 0x25, 0x05, 0x5c, 0x90, 0x0e, 0x20, 0xf7, 0xe7, 0x00, 0x08, 0x39, 0x0f, 0x9f, 0x12, 0x8e, 0x00, 0x2f, 0xde, 0x26, 0x20, 0x09, 0x0f, 0x9f, 0x12, 0x9e,
    0x26, 0x34, 0xce, 0x00, 0x30, 0xd6, 0x2b, 0xc4, 0x0f, 0x32, 0xa7, 0x00, 0x08, 0x7c, 0x00, 0x27, 0x5a, 0x2a, 0xf6, 0x9e, 0x12, 0x0e, 0x39, 0xd6, 0x29, 0x7f, 0x00, 0x3f, 0xde, 0x26, 0x86, 0x01, 0x97, 0x1c, 0xc4, 0x0f, 0x26, 0x02, 0xc6, 0x10,
    0x37, 0xdf, 0x14, 0xa6, 0x00, 0x97, 0x1e, 0x7f, 0x00, 0x1f, 0xd6, 0x2e, 0xc4, 0x07, 0x27, 0x09, 0x74, 0x00, 0x1e, 0x76, 0x00, 0x1f, 0x5a, 0x26, 0xf5, 0xd6, 0x2e, 0x8d, 0x28, 0x96, 0x1e, 0x8d, 0x15, 0xd6, 0x2e, 0xcb, 0x08, 0x8d, 0x1e, 0x96,
    0x1f, 0x8d, 0x0b, 0x7c, 0x00, 0x2f, 0xde, 0x14, 0x08, 0x33, 0x5a, 0x26, 0xcb, 0x39, 0x16, 0xe8, 0x00, 0xaa, 0x00, 0xe7, 0x00, 0x11, 0x27, 0x04, 0x86, 0x01, 0x97, 0x3f, 0x39, 0x96, 0x2f, 0x84, 0x1f, 0x48, 0x48, 0x48, 0xc4, 0x3f, 0x54, 0x54,
    0x54, 0x1b, 0x97, 0x1d, 0xde, 0x1c, 0x39, 0xc6, 0xf0, 0xce, 0x80, 0x10, 0x6f, 0x01, 0xe7, 0x00, 0xc6, 0x06, 0xe7, 0x01, 0x6f, 0x00, 0x39, 0x8d, 0xee, 0x7f, 0x00, 0x18, 0x8d, 0x55, 0xe6, 0x00, 0x8d, 0x15, 0x97, 0x17, 0xc6, 0x0f, 0x8d, 0xe1,
    0xe6, 0x00, 0x54, 0x54, 0x54, 0x54, 0x8d, 0x07, 0x48, 0x48, 0x9b, 0x17, 0x97, 0x17, 0x39, 0xc1, 0x0f, 0x26, 0x02, 0xd7, 0x18, 0x86, 0xff, 0x4c, 0x54, 0x25, 0xfc, 0x39, 0xdf, 0x12, 0x8d, 0xbf, 0xa6, 0x01, 0x2b, 0x07, 0x48, 0x2a, 0xf9, 0x6d,
    0x00, 0x20, 0x07, 0x8d, 0xc2, 0x7d, 0x00, 0x18, 0x26, 0xec, 0x8d, 0x03, 0xde, 0x12, 0x39, 0xc6, 0x04, 0xd7, 0x21, 0xc6, 0x41, 0xf7, 0x80, 0x12, 0x7d, 0x00, 0x21, 0x26, 0xfb, 0xc6, 0x01, 0xf7, 0x80, 0x12, 0x39, 0x8d, 0x00, 0x37, 0xc6, 0xc8,
    0x5a, 0x01, 0x26, 0xfc, 0x33, 0x39, 0xce, 0x80, 0x12, 0xc6, 0x3b, 0xe7, 0x01, 0xc6, 0x7f, 0xe7, 0x00, 0xa7, 0x01, 0xc6, 0x01, 0xe7, 0x00, 0x39, 0x8d, 0x13, 0xa6, 0x00, 0x2b, 0xfc, 0x8d, 0xdd, 0xc6, 0x09, 0x0d, 0x69, 0x00, 0x46, 0x8d, 0xd3,
    0x5a, 0x26, 0xf7, 0x20, 0x17, 0xdf, 0x12, 0xce, 0x80, 0x12, 0x39, 0x8d, 0xf8, 0x36, 0x6a, 0x00, 0xc6, 0x0a, 0x8d, 0xbf, 0xa7, 0x00, 0x0d, 0x46, 0x5a, 0x26, 0xf7, 0x32, 0xde, 0x12, 0x39, 0x20, 0x83, 0x86, 0x37, 0x8d, 0xb9, 0xde, 0x02, 0x39,
    0x8d, 0xf7, 0xa6, 0x00, 0x8d, 0xdd, 0x08, 0x9c, 0x04, 0x26, 0xf7, 0x20, 0x0b, 0x8d, 0xea, 0x8d, 0xb7, 0xa7, 0x00, 0x08, 0x9c, 0x04, 0x26, 0xf7, 0x8e, 0x00, 0x7f, 0xce, 0xc3, 0xe9, 0xdf, 0x00, 0x86, 0x3f, 0x8d, 0x92, 0x8d, 0x43, 0x0e, 0x8d,
    0xce, 0x4d, 0x2a, 0x10, 0x8d, 0xc9, 0x84, 0x03, 0x27, 0x23, 0x4a, 0x27, 0xd8, 0x4a, 0x27, 0xc8, 0xde, 0x06, 0x6e, 0x00, 0x8d, 0x0c, 0x97, 0x06, 0x8d, 0x06, 0x97, 0x07, 0x8d, 0x23, 0x20, 0xdf, 0x8d, 0xad, 0x48, 0x48, 0x48, 0x48, 0x97, 0x0f,
    0x8d, 0xa5, 0x9b, 0x0f, 0x39, 0x8d, 0x12, 0xde, 0x06, 0x8d, 0x25, 0x8d, 0x9a, 0x4d, 0x2b, 0x04, 0x8d, 0xe8, 0xa7, 0x00, 0x08, 0xdf, 0x06, 0x20, 0xec, 0x86, 0x10, 0x8d, 0x2b, 0xce, 0x01, 0xc8, 0x86, 0xff, 0xbd, 0xc0, 0x7d, 0xce, 0x00, 0x06,
    0x8d, 0x06, 0x08, 0x8d, 0x03, 0x8d, 0x15, 0x39, 0xa6, 0x00, 0x36, 0x44, 0x44, 0x44, 0x44, 0x8d, 0x01, 0x32, 0xdf, 0x12, 0xbd, 0xc1, 0x93, 0xc6, 0x05, 0xbd, 0xc2, 0x24, 0x86, 0x04, 0x9b, 0x2e, 0x97, 0x2e, 0x86, 0x1a, 0x97, 0x2f, 0xde, 0x12,
    0x39, 0x7a, 0x00, 0x20, 0x7a, 0x00, 0x21, 0x7d, 0x80, 0x12, 0x3b, 0xde, 0x00, 0x6e, 0x00, 0x00, 0xc3, 0xf3, 0x00, 0x80, 0x00, 0x83, 0xc3, 0x60
};

static const uint8_t dream6800ChipOslo[] = {
    /*
     * MIT License
     * Copyright (c) 1978, Michael J. Bauer
     * Copyright (c) 2020, Tobias V. Langhoff
     *
     * Permission is hereby granted, free of charge, to any person obtaining a copy
     * of this software and associated documentation files (the "Software"), to deal
     * in the Software without restriction, including without limitation the rights
     * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
     * copies of the Software, and to permit persons to whom the Software is
     * furnished to do so, subject to the following conditions:
     *
     * The above copyright notice and this permission notice shall be included in all
     * copies or substantial portions of the Software.
     *
     * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
     * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
     * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
     * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
     * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
     * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
     * SOFTWARE.
     */
    0x8d, 0x77, 0xce, 0x02, 0x00, 0xdf, 0x22, 0xce, 0x00, 0x5f, 0xdf, 0x24, 0xde, 0x22, 0xee, 0x00, 0xdf, 0x28, 0xdf, 0x14, 0xbd, 0xc0, 0xc7, 0xd6, 0x14, 0xc4, 0x0f, 0xd7, 0x14, 0x8d, 0x24, 0xd7, 0x2e, 0xd7, 0x0a, 0xdf, 0x2a, 0xd6, 0x29, 0x17,
    0x54, 0x54, 0x54, 0x54, 0x8d, 0x15, 0xd7, 0x2f, 0xce, 0xc0, 0x4b, 0xd6, 0x28, 0xc4, 0xf0, 0x08, 0x08, 0xc0, 0x10, 0x24, 0xfa, 0xee, 0x00, 0xad, 0x00, 0x20, 0xc9, 0xce, 0x00, 0x2f, 0x08, 0x5a, 0x2a, 0xfc, 0xe6, 0x00, 0x39, 0xc0, 0x6d, 0xc0,
    0xa2, 0xc0, 0xac, 0xc0, 0xba, 0xc0, 0xe1, 0xc0, 0xbf, 0xc1, 0x22, 0xc0, 0xe6, 0xc0, 0xf0, 0xc0, 0xc3, 0xc0, 0xa7, 0xc0, 0x97, 0xc0, 0xea, 0xc2, 0x1f, 0xc0, 0xce, 0xc1, 0x5f, 0xd6, 0x28, 0x26, 0x22, 0x81, 0xee, 0x27, 0x11, 0x81, 0xe0, 0x26,
    0x0c, 0x4f, 0xce, 0x01, 0x00, 0xa7, 0x00, 0x08, 0x8c, 0x02, 0x00, 0x26, 0xf8, 0x39, 0x30, 0x9e, 0x24, 0x32, 0x97, 0x22, 0x32, 0x97, 0x23, 0x9f, 0x24, 0x35, 0x39, 0xde, 0x14, 0x6e, 0x00, 0x96, 0x30, 0x5f, 0x9b, 0x15, 0x97, 0x15, 0xd9, 0x14,
    0xd7, 0x14, 0xde, 0x14, 0xdf, 0x22, 0x39, 0xde, 0x14, 0xdf, 0x26, 0x39, 0x30, 0x9e, 0x24, 0x96, 0x23, 0x36, 0x96, 0x22, 0x36, 0x9f, 0x24, 0x35, 0x20, 0xe8, 0x91, 0x2e, 0x27, 0x09, 0x39, 0x96, 0x2f, 0x20, 0xf7, 0x96, 0x2f, 0x20, 0x1a, 0xde,
    0x22, 0x08, 0x08, 0xdf, 0x22, 0x39, 0xbd, 0xc2, 0x97, 0x7d, 0x00, 0x18, 0x27, 0x07, 0xc6, 0xa1, 0xd1, 0x29, 0x27, 0xeb, 0x39, 0x81, 0x9e, 0x27, 0xd9, 0x91, 0x2e, 0x26, 0xe2, 0x39, 0x9b, 0x2e, 0x20, 0x38, 0x8d, 0x46, 0x94, 0x29, 0x20, 0x32,
    0x16, 0x96, 0x2f, 0xc4, 0x0f, 0x27, 0x2b, 0xce, 0x0a, 0x39, 0xc1, 0x05, 0x26, 0x05, 0x96, 0x2e, 0xce, 0x2f, 0x7e, 0xc1, 0x07, 0x26, 0x03, 0xce, 0x0a, 0x7e, 0xdf, 0x41, 0xce, 0xc1, 0x27, 0xdf, 0x43, 0x08, 0x5a, 0x26, 0xfc, 0xe6, 0x03, 0xd7,
    0x40, 0x7f, 0x00, 0x3f, 0xbd, 0x00, 0x40, 0x79, 0x00, 0x3f, 0xde, 0x2a, 0xa7, 0x00, 0x39, 0x59, 0x5c, 0x56, 0x39, 0x9a, 0x94, 0x98, 0x9b, 0x90, 0x44, 0x90, 0x86, 0xc0, 0x97, 0x47, 0x7c, 0x00, 0x48, 0xde, 0x47, 0x96, 0x0d, 0xab, 0x00, 0xa8,
    0xff, 0x97, 0x0d, 0x39, 0x07, 0xc1, 0x79, 0x0a, 0xc1, 0x7d, 0x15, 0xc1, 0x82, 0x18, 0xc1, 0x85, 0x1e, 0xc1, 0x89, 0x29, 0xc1, 0x93, 0x33, 0xc1, 0xde, 0x55, 0xc1, 0xfa, 0x65, 0xc2, 0x04, 0xce, 0xc1, 0x44, 0xc6, 0x09, 0xa6, 0x00, 0x91, 0x29,
    0x27, 0x09, 0x08, 0x08, 0x08, 0x5a, 0x26, 0xf4, 0x7e, 0xc3, 0x60, 0xee, 0x01, 0x96, 0x2e, 0x6e, 0x00, 0x96, 0x20, 0x20, 0xa5, 0xbd, 0xc2, 0xc4, 0x20, 0xa0, 0x97, 0x20, 0x39, 0x16, 0x7e, 0xc2, 0xe1, 0x5f, 0x9b, 0x27, 0x97, 0x27, 0xd9, 0x26,
    0xd7, 0x26, 0x39, 0xce, 0xc1, 0xbc, 0x84, 0x0f, 0x08, 0x08, 0x4a, 0x2a, 0xfb, 0xee, 0x00, 0xdf, 0x1e, 0xce, 0x00, 0x50, 0xdf, 0x26, 0xc6, 0x05, 0x96, 0x1e, 0x84, 0xe0, 0xa7, 0x04, 0x09, 0x86, 0x03, 0x79, 0x00, 0x1f, 0x79, 0x00, 0x1e, 0x4a,
    0x26, 0xf7, 0x5a, 0x26, 0xeb, 0x39, 0xf6, 0xdf, 0x49, 0x25, 0xf3, 0x9f, 0xe7, 0x9f, 0x3e, 0xd9, 0xe7, 0xcf, 0xf7, 0xcf, 0x24, 0x9f, 0xf7, 0xdf, 0xe7, 0xdf, 0xb7, 0xdf, 0xd7, 0xdd, 0xf2, 0x4f, 0xd6, 0xdd, 0xf3, 0xcf, 0x93, 0x4f, 0xde, 0x26,
    0xc6, 0x64, 0x8d, 0x06, 0xc6, 0x0a, 0x8d, 0x02, 0xc6, 0x01, 0xd7, 0x0e, 0x5f, 0x91, 0x0e, 0x25, 0x05, 0x5c, 0x90, 0x0e, 0x20, 0xf7, 0xe7, 0x00, 0x08, 0x39, 0x0f, 0x9f, 0x12, 0x8e, 0x00, 0x2f, 0xde, 0x26, 0x20, 0x09, 0x0f, 0x9f, 0x12, 0x9e,
    0x26, 0x34, 0xce, 0x00, 0x30, 0xd6, 0x2b, 0xc4, 0x0f, 0x32, 0xa7, 0x00, 0x08, 0x7c, 0x00, 0x27, 0x5a, 0x2a, 0xf6, 0x9e, 0x12, 0x0e, 0x39, 0x16, 0x7f, 0x00, 0x3f, 0x01, 0xde, 0x26, 0x86, 0x01, 0x97, 0x1c, 0xc4, 0x0f, 0x26, 0x02, 0xc6, 0x10,
    0x37, 0xdf, 0x14, 0xa6, 0x00, 0x97, 0x1e, 0x7f, 0x00, 0x1f, 0xd6, 0x2e, 0xc4, 0x07, 0x27, 0x09, 0x74, 0x00, 0x1e, 0x76, 0x00, 0x1f, 0x5a, 0x26, 0xf5, 0xd6, 0x2e, 0x8d, 0x28, 0x96, 0x1e, 0x8d, 0x15, 0xd6, 0x2e, 0xcb, 0x08, 0x8d, 0x1e, 0x96,
    0x1f, 0x8d, 0x0b, 0x7c, 0x00, 0x2f, 0xde, 0x14, 0x08, 0x33, 0x5a, 0x26, 0xcb, 0x39, 0x16, 0xe8, 0x00, 0xaa, 0x00, 0xe7, 0x00, 0x11, 0x27, 0x04, 0x86, 0x01, 0x97, 0x3f, 0x39, 0x96, 0x2f, 0x84, 0x1f, 0x48, 0x48, 0x48, 0xc4, 0x3f, 0x54, 0x54,
    0x54, 0x1b, 0x97, 0x1d, 0xde, 0x1c, 0x39, 0xc6, 0xf0, 0xce, 0x80, 0x10, 0x6f, 0x01, 0xe7, 0x00, 0xc6, 0x06, 0xe7, 0x01, 0x6f, 0x00, 0x39, 0x8d, 0xee, 0x7f, 0x00, 0x18, 0x8d, 0x55, 0xe6, 0x00, 0x8d, 0x15, 0x97, 0x17, 0xc6, 0x0f, 0x8d, 0xe1,
    0xe6, 0x00, 0x54, 0x54, 0x54, 0x54, 0x8d, 0x07, 0x48, 0x48, 0x9b, 0x17, 0x97, 0x17, 0x39, 0xc1, 0x0f, 0x26, 0x02, 0xd7, 0x18, 0x86, 0xff, 0x4c, 0x54, 0x25, 0xfc, 0x39, 0xdf, 0x12, 0x8d, 0xbf, 0xa6, 0x01, 0x2b, 0x07, 0x48, 0x2a, 0xf9, 0x6d,
    0x00, 0x20, 0x07, 0x8d, 0xc2, 0x7d, 0x00, 0x18, 0x26, 0xec, 0x8d, 0x03, 0xde, 0x12, 0x39, 0xc6, 0x04, 0xd7, 0x21, 0xc6, 0x41, 0xf7, 0x80, 0x12, 0x7d, 0x00, 0x21, 0x26, 0xfb, 0xc6, 0x01, 0xf7, 0x80, 0x12, 0x39, 0x8d, 0x00, 0x37, 0xc6, 0xc8,
    0x5a, 0x01, 0x26, 0xfc, 0x33, 0x39, 0xce, 0x80, 0x12, 0xc6, 0x3b, 0xe7, 0x01, 0xc6, 0x7f, 0xe7, 0x00, 0xa7, 0x01, 0xc6, 0x01, 0xe7, 0x00, 0x39, 0x8d, 0x13, 0xa6, 0x00, 0x2b, 0xfc, 0x8d, 0xdd, 0xc6, 0x09, 0x0d, 0x69, 0x00, 0x46, 0x8d, 0xd3,
    0x5a, 0x26, 0xf7, 0x20, 0x17, 0xdf, 0x12, 0xce, 0x80, 0x12, 0x39, 0x8d, 0xf8, 0x36, 0x6a, 0x00, 0xc6, 0x0a, 0x8d, 0xbf, 0xa7, 0x00, 0x0d, 0x46, 0x5a, 0x26, 0xf7, 0x32, 0xde, 0x12, 0x39, 0x20, 0x83, 0x86, 0x37, 0x8d, 0xb9, 0xde, 0x02, 0x39,
    0x8d, 0xf7, 0xa6, 0x00, 0x8d, 0xdd, 0x08, 0x9c, 0x04, 0x26, 0xf7, 0x20, 0x0b, 0x8d, 0xea, 0x8d, 0xb7, 0xa7, 0x00, 0x08, 0x9c, 0x04, 0x26, 0xf7, 0x8e, 0x00, 0x7f, 0xce, 0xc3, 0xe9, 0xdf, 0x00, 0x86, 0x3f, 0x8d, 0x92, 0x8d, 0x43, 0x0e, 0x8d,
    0xce, 0x4d, 0x2a, 0x10, 0x8d, 0xc9, 0x84, 0x03, 0x27, 0x23, 0x4a, 0x27, 0xd8, 0x4a, 0x27, 0xc8, 0xde, 0x06, 0x6e, 0x00, 0x8d, 0x0c, 0x97, 0x06, 0x8d, 0x06, 0x97, 0x07, 0x8d, 0x23, 0x20, 0xdf, 0x8d, 0xad, 0x48, 0x48, 0x48, 0x48, 0x97, 0x0f,
    0x8d, 0xa5, 0x9b, 0x0f, 0x39, 0x8d, 0x12, 0xde, 0x06, 0x8d, 0x25, 0x8d, 0x9a, 0x4d, 0x2b, 0x04, 0x8d, 0xe8, 0xa7, 0x00, 0x08, 0xdf, 0x06, 0x20, 0xec, 0x86, 0x10, 0x8d, 0x2b, 0xce, 0x01, 0xc8, 0x86, 0xff, 0xbd, 0xc0, 0x7d, 0xce, 0x00, 0x06,
    0x8d, 0x06, 0x08, 0x8d, 0x03, 0x8d, 0x15, 0x39, 0xa6, 0x00, 0x36, 0x44, 0x44, 0x44, 0x44, 0x8d, 0x01, 0x32, 0xdf, 0x12, 0xbd, 0xc1, 0x93, 0xc6, 0x05, 0xbd, 0xc2, 0x24, 0x86, 0x04, 0x9b, 0x2e, 0x97, 0x2e, 0x86, 0x1a, 0x97, 0x2f, 0xde, 0x12,
    0x39, 0x7a, 0x00, 0x20, 0x7a, 0x00, 0x21, 0x7d, 0x80, 0x12, 0x3b, 0xde, 0x00, 0x6e, 0x00, 0x00, 0xc3, 0xf3, 0x00, 0x80, 0x00, 0x83, 0xc3, 0x60
};

Chip8Dream::Chip8Dream(Chip8EmulatorHost& host, Chip8EmulatorOptions& options, IChip8Emulator* other)
    : Chip8RealCoreBase(host, options)
    , _impl(new Private(host, *this, options))
{
    if(_options.advanced && _options.advanced->contains("kernel") && _options.advanced->at("kernel") == "chiposlo")
        std::memcpy(_impl->_rom.data(), dream6800ChipOslo, sizeof(dream6800ChipOslo));
    else
        std::memcpy(_impl->_rom.data(), dream6800Rom, sizeof(dream6800Rom));
    _impl->_pia.irqAOutputHandler = [this](bool level) {
        if(!level)
            _impl->_cpu.irq();
    };
    _impl->_pia.irqBOutputHandler = [this](bool level) {
        if(!level)
            _impl->_cpu.irq();
    };
    _impl->_pia.portAOutputHandler = [this](uint8_t data, uint8_t mask) {
        _impl->_keyMatrix.setCols(data & 0xF, mask & 0xF);
        _impl->_keyMatrix.setRows(data >> 4, mask >> 4);
    };
    _impl->_pia.portAInputHandler = [this](uint8_t mask) -> MC682x::InputWithConnection {
        if(mask & 0xF) {
            auto [value, conn] = _impl->_keyMatrix.getCols(mask & 0xF);
            return {uint8_t(value & mask), uint8_t(conn & mask)};
        }
        if(mask & 0xF0) {
            auto [value, conn] = _impl->_keyMatrix.getRows(mask >> 4);
            return {uint8_t((value<<4) & mask), uint8_t((conn<<4) & mask)};
        }
        return {0, 0};
    };
    _impl->_pia.pinCA1InputHandler = [this]() -> bool {
        auto [value, conn] = _impl->_keyMatrix.getCols(0xF);
        return 0xF == (((value & conn) | ~conn) & 0xF) ? false : true;
    };
    Chip8Dream::reset();
    if(other) {
        std::memcpy(_impl->_ram.data() + 0x200, other->memory() + 0x200, std::min(_impl->_ram.size() - 0x200, (size_t)other->memSize()));
        for(size_t i = 0; i < 16; ++i) {
            _state.v[i] = other->getV(i);
        }
        _state.i = other->getI();
        _state.pc = other->getPC();
        _state.sp = other->getSP();
        _state.dt = other->delayTimer();
        _state.st = other->soundTimer();
        std::memcpy(_state.s.data(), other->getStackElements(), stackSize() * sizeof(uint16_t));
        forceState();
    }
}

Chip8Dream::~Chip8Dream()
{

}

void Chip8Dream::reset()
{
    if(_options.optTraceLog)
        Logger::log(Logger::eBACKEND_EMU, _impl->_cpu.getCycles(), {_frames, frameCycle()}, fmt::format("--- RESET ---", _impl->_cpu.getCycles(), frameCycle()).c_str());
    std::memset(_impl->_ram.data(), 0, MAX_MEMORY_SIZE);
    std::memset(_impl->_screenBuffer.data(), 0, 256*192);
    _impl->_cpu.reset();
    _impl->_ram[0x006] = 0xC0;
    _impl->_ram[0x007] = 0x00;
    setExecMode(eRUNNING);
    while(!executeM6800() && (_impl->_cpu.getRegisterByName("SR").value & CadmiumM6800::I));
    flushScreen();
    M6800State state;
    _impl->_ram[0x026] = 0x00;
    _impl->_ram[0x027] = 0x00;
    std::memset(&_impl->_ram[0x30], 0, 16);
    _impl->_cpu.getState(state);
    state.pc = 0xC000;
    state.sp = 0x007f;
    _impl->_cpu.setState(state);
    _cycles = 0;
    _frames = 0;
    _impl->_nextFrame = 0;
    _cpuState = eNORMAL;
    while(!executeM6800() || getPC() != 0x200); // fast-forward to fetch/decode loop
    setExecMode(_impl->_host.isHeadless() ? eRUNNING : ePAUSED);
    if(_options.optTraceLog)
        Logger::log(Logger::eBACKEND_EMU, _impl->_cpu.getCycles(), {_frames, frameCycle()}, fmt::format("End of reset: {}/{}", _impl->_cpu.getCycles(), frameCycle()).c_str());
}

std::string Chip8Dream::name() const
{
    return "DREAM6800";
}

void Chip8Dream::fetchState()
{
    _state.cycles = _cycles;
    _state.frameCycle = frameCycle();
    std::memcpy(_state.v.data(), &_impl->_ram[0x30], 16);
    _state.i = (_impl->_ram[0x26]<<8) | _impl->_ram[0x27];
    _state.pc = (_impl->_ram[0x22]<<8) | _impl->_ram[0x23];
    _state.sp = (0x05F - ((_impl->_ram[0x24]<<8) | _impl->_ram[0x25])) >> 1;
    _state.dt = _impl->_ram[0x20];
    _state.st = _impl->_ram[0x21];
    for(int i = 0; i < stackSize() && i < _state.sp; ++i) {
        _state.s[i] = (_impl->_ram[0x05F - i*2 - 1] << 8) | _impl->_ram[0x05F - i*2];
    }
}

void Chip8Dream::forceState()
{
    _state.cycles = _cycles;
    _state.frameCycle = frameCycle();
    std::memcpy(&_impl->_ram[0x30], _state.v.data(), 16);
    _impl->_ram[0x26] = (_state.i >> 8); _impl->_ram[0x27] = _state.i & 0xFF;
    _impl->_ram[0x22] = (_state.pc >> 8); _impl->_ram[0x22] = _state.pc & 0xFF;
    auto sp = 0x5f - _state.sp * 2;
    _impl->_ram[0x24] = (sp >> 8); _impl->_ram[0x25] = sp & 0xFF;
    _impl->_ram[0x20] = _state.dt;
    _impl->_ram[0x21] = _state.st;
    for(int i = 0; i < stackSize() && i < _state.sp; ++i) {
        _impl->_ram[sp - i*2 - 1] = _state.s[i] >> 8;
        _impl->_ram[sp - i*2] = _state.s[i] & 0xFF;
    }
}

int Chip8Dream::executeVDG()
{
    static int lastFC = 312*64 + 1;
    auto fc = frameCycle();
    if(fc < lastFC) {
        flushScreen();
        // CPU is halted for 124*64 Cycles while video frame is generated
        _impl->_cpu.addCycles(128*64);
        ++_frames;
        // Trigger RTC/VSYNC on PIA (Will trigger IRQ on CPU)
        _impl->_pia.pinCB1(true);
        _impl->_pia.pinCB1(false);
        _impl->_keyMatrix.updateKeys(_host.getKeyStates());
    }
    lastFC = fc;
    return fc;
}

void Chip8Dream::flushScreen()
{
    for(int y = 0; y < 32*4; ++y) {
        for (int i = 0; i < 8; ++i) {
            auto* dest = &_impl->_screenBuffer[y * 256 + i * 8];
            auto data = _impl->_ram[0x100 + (y>>2)*8 + i];
            for (int j = 0; j < 8; ++j) {
                dest[j] = (data >> (7 - j)) & 1;
            }
        }
    }
}

bool Chip8Dream::executeM6800()
{
    static int lastFC = 0;
    auto fc = executeVDG();
    if(_options.optTraceLog  && _impl->_cpu.getCpuState() == CadmiumM6800::eNORMAL)
        Logger::log(Logger::eBACKEND_EMU, _impl->_cpu.getCycles(), {_frames, fc}, fmt::format("{:28} ; {}", _impl->_cpu.disassembleInstructionWithBytes(-1, nullptr), _impl->_cpu.dumpRegisterState()).c_str());
    if(_impl->_cpu.getPC() == Private::FETCH_LOOP_ENTRY) {
        if(_options.optTraceLog)
            Logger::log(Logger::eCHIP8, _cycles, {_frames, fc}, fmt::format("CHIP8: {:30} ; {}", disassembleInstructionWithBytes(-1, nullptr), dumpStateLine()).c_str());
    }
    _impl->_cpu.executeInstruction();

    if(_impl->_cpu.getPC() == Private::FETCH_LOOP_ENTRY) {
        fetchState();
        _cycles++;
        if(_impl->_cpu.getExecMode() == ePAUSED) {
            setExecMode(ePAUSED);
            _backendStopped = true;
        }
        else if (_execMode == eSTEP || (_execMode == eSTEPOVER && getSP() <= _stepOverSP)) {
            setExecMode(ePAUSED);
        }
        auto nextOp = opcode();
        bool newFrame = lastFC > fc;
        lastFC = fc;
        if(newFrame && (nextOp & 0xF000) == 0x1000 && (opcode() & 0xFFF) == getPC()) {
            setExecMode(ePAUSED);
        }
        if(hasBreakPoint(getPC())) {
            if(Chip8Dream::findBreakpoint(getPC()))
                setExecMode(ePAUSED);
        }
        return true;
    }
    else if(_impl->_cpu.getExecMode() == ePAUSED) {
        setExecMode(ePAUSED);
        _backendStopped = true;
    }
    return false;
}

void Chip8Dream::executeInstruction()
{
    if (_execMode == ePAUSED || _cpuState == eERROR) {
        setExecMode(ePAUSED);
        return;
    }
    //std::clog << "CHIP8: " << dumpStateLine() << std::endl;
    auto start = _impl->_cpu.getCycles();
    while(!executeM6800() && _execMode != ePAUSED && _impl->_cpu.getCycles() - start < 19968*0x30);
}

void Chip8Dream::executeInstructions(int numInstructions)
{
    for(int i = 0; i < numInstructions; ++i) {
        executeInstruction();
    }
}

//---------------------------------------------------------------------------
// For easier handling we shift the line/cycle counting to the start of the
// interrupt (if display is enabled)

inline int Chip8Dream::frameCycle() const
{
    return _impl->_cpu.getCycles() % 19968;
}

inline cycles_t Chip8Dream::nextFrame() const
{
    return ((_impl->_cpu.getCycles() + 19968) / 19968) * 19968;
}

void Chip8Dream::tick(int)
{
    if (_execMode == ePAUSED || _cpuState == eERROR) {
        setExecMode(ePAUSED);
        return;
    }

    auto nxtFrame = nextFrame();
    while(_execMode != ePAUSED && _impl->_cpu.getCycles() < nxtFrame) {
        executeM6800();
    }
}

bool Chip8Dream::isDisplayEnabled() const
{
    return true; //_impl->_video.isDisplayEnabled();
}

uint8_t* Chip8Dream::memory()
{
    return _impl->_ram.data();
}

int Chip8Dream::memSize() const
{
    return 4096;
}

uint8_t Chip8Dream::soundTimer() const
{
    return (_impl->_pia.portB() & 64) ? _state.st : 0;
}

float Chip8Dream::getAudioPhase() const
{
    return _impl->_wavePhase;
}

void Chip8Dream::setAudioPhase(float phase)
{
    _impl->_wavePhase = phase;
}

uint16_t Chip8Dream::getCurrentScreenWidth() const
{
    return 64;
}

uint16_t Chip8Dream::getCurrentScreenHeight() const
{
    return 128;
}

uint16_t Chip8Dream::getMaxScreenWidth() const
{
    return 64;
}

uint16_t Chip8Dream::getMaxScreenHeight() const
{
    return 128;
}

const uint8_t* Chip8Dream::getScreenBuffer() const
{
    return _impl->_screenBuffer.data();
}

GenericCpu& Chip8Dream::getBackendCpu()
{
    return _impl->_cpu;
}

uint8_t Chip8Dream::readByte(uint16_t addr) const
{
    if(addr == 0x1ff)
        return 5;
    else if(addr == 0x1fe)
        return 1;
    if(addr < _impl->_ram.size())
        return _impl->_ram[addr];
    if(addr >= 0x8010 && addr < 0x8020)
        return _impl->_pia.readByte(addr & 3);
    if(addr >= 0xC000)
        return _impl->_rom[addr & 0x3ff];
    _cpuState = eERROR;
    return 0;
}

uint8_t Chip8Dream::readDebugByte(uint16_t addr) const
{
    if(addr < _impl->_ram.size())
        return _impl->_ram[addr];
    if(addr >= 0x8010 && addr < 0x8020)
        return _impl->_pia.readByte(addr & 3);
    if(addr >= 0xC000)
        return _impl->_rom[addr & 0x3ff];
    return 0;
}

uint8_t Chip8Dream::getMemoryByte(uint32_t addr) const
{
    return Chip8Dream::readDebugByte(addr);
}

void Chip8Dream::writeByte(uint16_t addr, uint8_t val)
{
    if(addr < _impl->_ram.size())
        _impl->_ram[addr] = val;
    else if(addr >= 0x8010 && addr < 0x8020)
        _impl->_pia.writeByte(addr & 3, val);
    else {
        _cpuState = eERROR;
    }
}


}
