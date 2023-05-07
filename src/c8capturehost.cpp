//---------------------------------------------------------------------------------------
// src/emulation/c8capturehost.cpp
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

#include <c8capturehost.hpp>
#include <emulation/ichip8.hpp>

C8CaptureHost::C8CaptureHost()
{
    _snapshot = GenImageColor(128, 64, BLACK);
    _nineSnapshot = GenImageColor(128*3, 64*3, BLACK);
}

C8CaptureHost::~C8CaptureHost()
{
    UnloadImage(_nineSnapshot);
    UnloadImage(_snapshot);
}

void C8CaptureHost::preClear()
{
    if(_snapNum < 2) {
        grabImage((uint32_t*)_snapshot.data, _snapshot.width, _snapshot.height, _snapshot.width);
    }
    if(_snapNum < 9) {
        grabImage((uint32_t*)_snapshot.data + (_snapNum/3)*64*_nineSnapshot.width + (_snapNum%3)*128, 128, 64, _nineSnapshot.width);
    }
    _snapNum++;
}

void C8CaptureHost::grabImage(uint32_t* destination, int destWidth, int destHeight, int destStride)
{
    if (_chipEmu->getScreen()) {
        auto screen = *_chipEmu->getScreen();
        float srcWidth = _chipEmu->getCurrentScreenWidth();
        float srcHeight = _chipEmu->getCurrentScreenHeight();
        for (int y = 0; y < destHeight; ++y) {
            for (int x = 0; x < destWidth; ++x) {
                destination[y * destStride + x] = screen.getPixel(x * srcWidth / destWidth, y * srcHeight / destHeight);
            }
        }
    }
    else if (_chipEmu->getScreenRGBA()) {
        auto screen = *_chipEmu->getScreenRGBA();
        float srcWidth = _chipEmu->getCurrentScreenWidth();
        float srcHeight = _chipEmu->getCurrentScreenHeight();
        for (int y = 0; y < destHeight; ++y) {
            for (int x = 0; x < destWidth; ++x) {
                destination[y * destStride + x] = screen.getPixel(x * srcWidth / destWidth, y * srcHeight / destHeight);
            }
        }
    }
}
