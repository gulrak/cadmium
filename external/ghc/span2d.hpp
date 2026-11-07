//---------------------------------------------------------------------------------------
// ghc/span2d.hpp
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
// span2d is a lightweight, non-owning view over a 2D region of contiguous memory.
// It supports row-major iteration with optional row padding (stride), making it ideal
// for working with image buffers, framebuffers, or any 2D grid data.
//
// Usage example:
//   std::vector<uint32_t> pixels(640 * 480);
//   ghc::span2d<uint32_t> view(pixels.data(), 640, 480);
//
//   // Access individual pixels
//   view(10, 20) = 0xFF0000FF;  // Set pixel at row 10, column 20
//
//   // Iterate over all pixels (row-major order)
//   for (auto& pixel : view) {
//       pixel = 0x00000000;  // Clear to black
//   }
//
//   // Access a specific row
//   auto row = view.row(5);
//   std::fill(row.begin(), row.end(), 0xFFFFFFFF);
//
//   // Create a sub-region view
//   auto subView = view.subspan(100, 50, 200, 100);  // x=100, y=50, w=200, h=100
//---------------------------------------------------------------------------------------

#pragma once

#include <cassert>
#include <cstddef>
#include <span>
#include <type_traits>

namespace ghc {

template <class T>
class span2d
{
public:
    using element_type = T;
    using value_type = std::remove_cv_t<T>;
    using size_type = std::size_t;
    using pointer = T*;
    using reference = T&;
    using span_type = std::span<T>;
    using const_span_type = std::span<const value_type>;

    // Iterates row-major over all width*height elements, skipping padding via rowStride.
    template <class U>
    class basic_iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::remove_cv_t<U>;
        using difference_type = std::ptrdiff_t;
        using pointer = U*;
        using reference = U&;

        constexpr basic_iterator() noexcept = default;

        constexpr size_type x() const noexcept { return _x; }
        constexpr size_type y() const noexcept { return _y; }

        constexpr reference operator*() const noexcept
        {
            assert(_base != nullptr || (_width == 0 && _height == 0));
            assert(_y < _height);
            assert(_x < _width);
            return _base[_y * _rowStride + _x];
        }

        constexpr pointer operator->() const noexcept { return std::addressof(**this); }

        constexpr basic_iterator& operator++() noexcept
        {
            // Advance within row, then jump to next row respecting stride.
            if (_width == 0 || _height == 0)
                return *this;

            ++_x;
            if (_x >= _width) {
                _x = 0;
                ++_y;
            }
            return *this;
        }

        constexpr basic_iterator operator++(int) noexcept
        {
            basic_iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        friend constexpr bool operator==(const basic_iterator& a, const basic_iterator& b) noexcept
        {
            // Same "view" and same position.
            return a._base == b._base && a._width == b._width && a._height == b._height && a._rowStride == b._rowStride && a._x == b._x && a._y == b._y;
        }

        friend constexpr bool operator!=(const basic_iterator& a, const basic_iterator& b) noexcept { return !(a == b); }

    private:
        friend class span2d;

        constexpr basic_iterator(pointer base, size_type width, size_type height, size_type rowStride, size_type x, size_type y) noexcept
            : _base(base)
            , _width(width)
            , _height(height)
            , _rowStride(rowStride)
            , _x(x)
            , _y(y)
        {
        }

        pointer _base = nullptr;
        size_type _width = 0;
        size_type _height = 0;
        size_type _rowStride = 0;
        size_type _x = 0;
        size_type _y = 0;  // end() represented as (x=0, y=height)
    };

    using iterator = basic_iterator<T>;
    using const_iterator = basic_iterator<const value_type>;

    constexpr span2d() noexcept = default;

    /// rowStride is in *elements*, not bytes.
    constexpr span2d(pointer data, size_type width, size_type height, size_type rowStride) noexcept
        : _data(data)
        , _width(width)
        , _height(height)
        , _stride(rowStride)
    {
        // Basic sanity: allow null only for empty views
        assert((_data != nullptr) || (_width == 0 && _height == 0));
        // Most common expectation for row-strided buffers
        assert(_stride >= _width || _height == 0);
    }
    /// rowStride defaults to width.
    constexpr span2d(pointer data, size_type width, size_type height) noexcept
        : span2d(data, width, height, width)
    {
    }

    [[nodiscard]] constexpr pointer data() const noexcept { return _data; }
    [[nodiscard]] constexpr size_type width() const noexcept { return _width; }
    [[nodiscard]] constexpr size_type height() const noexcept { return _height; }
    [[nodiscard]] constexpr size_type stride() const noexcept { return _stride; }

    [[nodiscard]] constexpr bool empty() const noexcept { return _width == 0 || _height == 0; }

    /// Range-based for support (row-major, respects stride)
    [[nodiscard]] constexpr iterator begin() noexcept
    {
        if (empty())
            return end();
        return iterator(_data, _width, _height, _stride, 0, 0);
    }
    [[nodiscard]] constexpr iterator end() noexcept
    {
        // End = "one past last": y == height, x == 0
        return iterator(_data, _width, _height, _stride, 0, _height);
    }

    [[nodiscard]] constexpr const_iterator begin() const noexcept { return cbegin(); }
    [[nodiscard]] constexpr const_iterator end() const noexcept { return cend(); }
    [[nodiscard]] constexpr const_iterator cbegin() const noexcept
    {
        if (empty())
            return cend();
        return const_iterator(_data, _width, _height, _stride, 0, 0);
    }
    [[nodiscard]] constexpr const_iterator cend() const noexcept { return const_iterator(_data, _width, _height, _stride, 0, _height); }

    /// 2D indexing: (y, x)
    [[nodiscard]] constexpr reference operator()(size_type y, size_type x) const noexcept
    {
        assert(y < _height);
        assert(x < _width);
        return _data[y * _stride + x];
    }

    /// Returns a span over a row (width elements)
    [[nodiscard]] constexpr span_type row(size_type y) const noexcept
    {
        assert(y < _height);
        return span_type(_data + y * _stride, _width);
    }

    /// Pointer to the first element in row y
    [[nodiscard]] constexpr pointer rowPtr(size_type y) const noexcept
    {
        assert(y < _height);
        return _data + y * _stride;
    }

    /// Create a sub-rectangle view (still controlled by the same backing stride).
    [[nodiscard]] constexpr span2d subspan(size_type x, size_type y, size_type width, size_type height) const noexcept
    {
        assert(x <= _width);
        assert(y <= _height);
        assert(x + width <= _width);
        assert(y + height <= _height);

        return span2d(_data + y * _stride + x, width, height, _stride);
    }

    /// Convert to const view
    template <class U = T, class = std::enable_if_t<!std::is_const_v<U>>>
    [[nodiscard]] constexpr span2d<const value_type> as_const() const noexcept
    {
        return span2d<const value_type>(_data, _width, _height, _stride);
    }

private:
    pointer _data = nullptr;
    size_type _width = 0;
    size_type _height = 0;
    size_type _stride = 0;  // in elements
};

}  // namespace ghc
