//---------------------------------------------------------------------------------------
// src/circularbuffer.hpp
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
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

class CircularBufferBase
{
public:
    CircularBufferBase(size_t size);
    virtual ~CircularBufferBase();

protected:
    void resetBuffer();
    size_t readAvailable() const;
    size_t writeAvailable() const;
    size_t readInto(void* destination, size_t size);
    size_t writeInto(const void* source, size_t size);
private:
    size_t readSome(void* destination, size_t size);
    size_t writeSome(const void* source, size_t size);
    class Private;
    std::unique_ptr<Private> _impl;
};

template<typename T, int channels = 1>
class CircularBuffer : public CircularBufferBase
{
public:
    static constexpr auto FRAME_SIZE = sizeof(T) * channels;
    CircularBuffer(size_t size) : CircularBufferBase(size * FRAME_SIZE) {}

    void reset() { resetBuffer(); }

    size_t dataAvailable() const
    {
        return readAvailable() / FRAME_SIZE;
    }

    size_t spaceAvailable() const
    {
        return writeAvailable() / FRAME_SIZE;
    }

    size_t read(T* destination, size_t frames)
    {
        return readInto(destination, frames * FRAME_SIZE) / FRAME_SIZE;
    }

    size_t write(const T* source, size_t frames)
    {
        return writeInto(source, frames * FRAME_SIZE) / FRAME_SIZE;
    }
};
