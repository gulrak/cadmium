//---------------------------------------------------------------------------------------
// src/emulation/hardware/hp48/nibblememory.hpp
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

#include <cstdint>
#include <vector>
#include <stdexcept>
#include <iterator>
#include <type_traits>
#include <utility>

namespace hp48 {
    class NibbleMemory {
    public:
        using address_type = std::uint32_t;
        using size_type = std::uint32_t;
        using value_type = std::uint8_t; // nibble value in [0, 15]

        // A "reference-like" proxy to a single nibble inside packed storage.
        class Proxy {
        public:
            Proxy() = default;

            // Read
            operator value_type() const noexcept {
                if (!_byte) return 0;
                const value_type b = *_byte;
                return _high ? static_cast<value_type>((b >> 4) & 0x0F) : static_cast<value_type>(b & 0x0F);
            }

            // Write from value
            Proxy &operator=(value_type v) noexcept {
                v &= 0x0F;
                if (!_byte) return *this;

                value_type b = *_byte;
                if (_high) {
                    b = value_type((b & 0x0F) | (v << 4));
                } else {
                    b = value_type((b & 0xF0) | v);
                }
                *_byte = b;
                return *this;
            }

            // Write from another proxy
            Proxy &operator=(const Proxy &rhs) noexcept {
                return (*this = value_type(rhs));
            }

            // Handy for generic code
            friend void swap(Proxy a, Proxy b) noexcept {
                const value_type tmp = value_type(a);
                a = value_type(b);
                b = tmp;
            }

        private:
            friend class NibbleMemory;

            Proxy(std::uint8_t *byte, bool high) noexcept : _byte(byte), _high(high) {
            }

            std::uint8_t *_byte = nullptr;
            bool _high = false;
        };

        using reference = Proxy;
        using const_reference = value_type;

        NibbleMemory(size_type size_nibbles)
            : _size(size_nibbles), _data((size_nibbles + 1u) / 2u, 0u) {
        }

        size_type size() const noexcept { return _size; } // number of nibbles
        size_type bytes() const noexcept { return (_size + 1u) / 2u; }

        void fill(value_type nibble) noexcept {
            nibble &= 0x0F;
            const std::uint8_t packed = std::uint8_t(nibble | (nibble << 4));
            std::fill(_data.begin(), _data.end(), packed);
            // If odd number of nibbles, the final high nibble is "unused" by address space;
            // leaving it set is fine (it is simply unreachable).
        }

        // Unchecked absolute-address access (like operator[] on a raw memory array)
        reference operator[](address_type addr) noexcept {
            const auto [byte, high] = locateUnchecked(addr);
            return Proxy(byte, high);
        }

        const_reference operator[](address_type addr) const noexcept {
            const auto [byte, high] = locateUnchecked(addr);
            const std::uint8_t b = *byte;
            return high ? value_type((b >> 4) & 0x0F) : value_type(b & 0x0F);
        }

        // Checked access
        reference at(address_type addr) {
            checkRange(addr);
            return (*this)[addr];
        }

        const_reference at(address_type addr) const {
            checkRange(addr);
            return (*this)[addr];
        }

        // ---- Iterators (random access) ----
        template<bool IsConst>
        class NibbleIterator {
        public:
            using difference_type = std::ptrdiff_t;
            using value_type = NibbleMemory::value_type;
            using iterator_category = std::random_access_iterator_tag;

            using memory_type = std::conditional_t<IsConst, const NibbleMemory, NibbleMemory>;
            using reference = std::conditional_t<IsConst, NibbleMemory::const_reference, NibbleMemory::reference>;

            NibbleIterator() = default;

            reference operator*() const noexcept {
                // We iterate over *indices* 0..size-1, mapped to absolute addresses base+pos.
                const address_type addr = _mem->base_ + address_type(_pos);
                if constexpr (IsConst) {
                    return (*_mem)[addr];
                } else {
                    return (*_mem)[addr]; // returns Proxy
                }
            }

            // Random access
            reference operator[](difference_type n) const noexcept {
                return *(*this + n);
            }

            NibbleIterator &operator++() noexcept {
                ++_pos;
                return *this;
            }

            NibbleIterator operator++(int) noexcept {
                auto t = *this;
                ++(*this);
                return t;
            }

            NibbleIterator &operator--() noexcept {
                --_pos;
                return *this;
            }

            NibbleIterator operator--(int) noexcept {
                auto t = *this;
                --(*this);
                return t;
            }

            NibbleIterator &operator+=(difference_type n) noexcept {
                _pos = size_type(_pos + n);
                return *this;
            }

            NibbleIterator &operator-=(difference_type n) noexcept {
                _pos = size_type(_pos - n);
                return *this;
            }

            friend NibbleIterator operator+(NibbleIterator it, difference_type n) noexcept {
                it += n;
                return it;
            }

            friend NibbleIterator operator+(difference_type n, NibbleIterator it) noexcept {
                it += n;
                return it;
            }

            friend NibbleIterator operator-(NibbleIterator it, difference_type n) noexcept {
                it -= n;
                return it;
            }

            friend difference_type operator-(const NibbleIterator &a, const NibbleIterator &b) noexcept {
                return difference_type(a._pos) - difference_type(b._pos);
            }

            friend bool operator==(const NibbleIterator &a, const NibbleIterator &b) noexcept {
                return a._mem == b._mem && a._pos == b._pos;
            }

            friend bool operator!=(const NibbleIterator &a, const NibbleIterator &b) noexcept { return !(a == b); }
            friend bool operator<(const NibbleIterator &a, const NibbleIterator &b) noexcept { return a._pos < b._pos; }
            friend bool operator>(const NibbleIterator &a, const NibbleIterator &b) noexcept { return b < a; }
            friend bool operator<=(const NibbleIterator &a, const NibbleIterator &b) noexcept { return !(b < a); }
            friend bool operator>=(const NibbleIterator &a, const NibbleIterator &b) noexcept { return !(a < b); }

        private:
            friend class NibbleMemory;

            NibbleIterator(memory_type *mem, size_type pos) noexcept : _mem(mem), _pos(pos) {
            }

            memory_type *_mem = nullptr;
            size_type _pos = 0;
        };

        using iterator = NibbleIterator<false>;
        using const_iterator = NibbleIterator<true>;

        iterator begin() noexcept { return iterator(this, 0); }
        iterator end() noexcept { return iterator(this, _size); }

        const_iterator begin() const noexcept { return const_iterator(this, 0); }
        const_iterator end() const noexcept { return const_iterator(this, _size); }
        const_iterator cbegin() const noexcept { return const_iterator(this, 0); }
        const_iterator cend() const noexcept { return const_iterator(this, _size); }

        // If you ever need the packed bytes (DMA, serialization, etc.)
        std::vector<std::uint8_t> &packedBytes() noexcept { return _data; }
        const std::vector<std::uint8_t> &packedBytes() const noexcept { return _data; }

    private:
        struct Location {
            std::uint8_t *byte;
            bool high;
        };

        struct ConstLocation {
            const std::uint8_t *byte;
            bool high;
        };

        void checkRange(address_type addr) const {
            if (addr >= _size) {
                throw std::out_of_range("NibbleMemory address out of range");
            }
        }

        Location locateUnchecked(address_type addr) noexcept {
            const size_type bi = addr >> 1; // /2
            const bool high = (addr & 1u) != 0u; // odd => high nibble
            return {&_data[bi], high};
        }

        ConstLocation locateUnchecked(address_type addr) const noexcept {
            const size_type bi = addr >> 1;
            const bool high = (addr & 1u) != 0u;
            return {&_data[bi], high};
        }

        size_type _size = 0; // number of nibbles
        std::vector<std::uint8_t> _data; // packed: low nibble = even index, high nibble = odd index
    };
}
