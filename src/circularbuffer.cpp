//---------------------------------------------------------------------------------------
// src/circularbuffer.cpp
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
#include <circularbuffer.hpp>

#include <external/miniaudio.h>
#include <cstring>

class CircularBufferBase::Private {
public:
    ma_rb _marb;
    size_t _size;
};

CircularBufferBase::CircularBufferBase(size_t size)
: _impl(new Private)
{
    _impl->_size = size;
    auto result = ma_rb_init(size, nullptr, nullptr, &_impl->_marb);
    if (result == MA_SUCCESS) {
        ma_rb_reset(&_impl->_marb);
    }
    else {
        _impl.reset();
    }
}

CircularBufferBase::~CircularBufferBase()
{
    if(_impl) {
        ma_rb_uninit(&_impl->_marb);
    }
}

void CircularBufferBase::resetBuffer()
{
    if(_impl)
        ma_rb_reset(&_impl->_marb);
}

size_t CircularBufferBase::readAvailable() const
{
    if(!_impl) return 0;
    return ma_rb_available_read(&_impl->_marb);
}

size_t CircularBufferBase::writeAvailable() const
{
    if(!_impl) return 0;
    return ma_rb_available_write(&_impl->_marb);
}

size_t CircularBufferBase::readSome(void* destination, size_t size)
{
    void *src;
    if(!_impl) return 0;
    ma_rb_acquire_read(&_impl->_marb, &size, &src);
    std::memcpy(destination, src, size);
    ma_rb_commit_read(&_impl->_marb, size);
    return size;
}

size_t CircularBufferBase::readInto(void* destination, size_t size)
{
    auto availableData = readAvailable();
    if (availableData == 0)
        return 0;
    auto len = readSome(destination, size);
    size -= len;
    availableData -= len;
    if (size > 0 && availableData) {
        len += readSome(reinterpret_cast<uint8_t*>(destination) + len, size);
    }
    return len;
}

size_t CircularBufferBase::writeSome(const void* source, size_t size)
{
    void *dest;
    if(!_impl) return 0;
    ma_rb_acquire_write(&_impl->_marb, &size, &dest);
    std::memcpy(dest, source, size);
    ma_rb_commit_write(&_impl->_marb, size);
    return size;
}

size_t CircularBufferBase::writeInto(const void* source, size_t size)
{
    auto availableSpace = writeAvailable();
    if (availableSpace == 0)
        return 0;
    auto len = writeSome(source, size);
    size -= len;
    availableSpace -= len;
    if (size > 0 && availableSpace) {
        len += writeSome(reinterpret_cast<uint8_t const*>(source) + len, size);
    }
    return len;
}
