//---------------------------------------------------------------------------------------
// src/emulation/hardware/hp48/binary.hpp
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

// Hp48Binary.hpp - HP-48 binary file loader
//
// Loads HP binary files with the "HPHP48-" header format.
// These files contain nibble data where each byte stores two nibbles,
// with the low nibble first (LSN ordering).
//
// File structure:
//   8 bytes:  "HPHP48-X" where X is ROM revision (A-J for SX, K-R for G/GX)
//   5 nibbles: Object prolog (identifies object type)
//   5 nibbles: Object length in nibbles (body length, interpretation varies by type)
//   N nibbles: Object body (the actual content)
//
// Data is stored in compact form (2 nibbles per byte). A custom iterator
// provides transparent access to individual nibbles.

#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace hp48 {

using Nibble = uint8_t;

// Known HP-48 object prologs (5-nibble values)
namespace ObjectProlog {
    constexpr uint32_t program = 0x02D9D;  // Program (DOCOL)
    constexpr uint32_t string  = 0x02A2C;  // String object
    constexpr uint32_t grob    = 0x02B1E;  // Graphics object
    constexpr uint32_t library = 0x02B40;  // Library
    constexpr uint32_t backup  = 0x02B62;  // Backup object
    constexpr uint32_t list    = 0x02A74;  // List
    constexpr uint32_t dir     = 0x02A96;  // Directory
    constexpr uint32_t symb    = 0x02AB8;  // Algebraic/symbolic
    constexpr uint32_t unit    = 0x02ADA;  // Unit object
    constexpr uint32_t tagged  = 0x02AFC;  // Tagged object
    constexpr uint32_t ext1    = 0x02BAA;  // External 1
    constexpr uint32_t code    = 0x02DCC;  // Code object (machine language)
}

class Binary {
public:
    // Magic header for HP-48 binary files
    static constexpr std::string_view magicHeader = "HPHP48-";
    static constexpr size_t headerSize = 8; // "HPHP48-" + ROM version char
    static constexpr size_t prologNibbles = 5;      // Object prolog size
    static constexpr size_t lengthNibbles = 5;      // Object length field size
    static constexpr size_t objectHeaderNibbles = prologNibbles + lengthNibbles; // 10 nibbles

    // Forward declaration of iterator
    class NibbleIterator;

    Binary() = default;

    // Load from file path
    explicit Binary(std::filesystem::path const& path) {
        load(path);
    }

    // Load from raw bytes
    explicit Binary(std::span<uint8_t const> data) {
        load(data);
    }

    // Load from file
    void load(std::filesystem::path const& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Failed to open file: " + path.string());
        }

        file.seekg(0, std::ios::end);
        auto size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> buffer(static_cast<size_t>(size));
        file.read(reinterpret_cast<char*>(buffer.data()), size);

        if (!file) {
            throw std::runtime_error("Failed to read file: " + path.string());
        }

        load(buffer);
    }

    // Load from raw bytes
    void load(std::span<uint8_t const> data) {
        if (data.size() < headerSize) {
            throw std::runtime_error("File too small to contain HP-48 header");
        }

        // Verify magic header
        if (!std::equal(magicHeader.begin(), magicHeader.end(), data.begin())) {
            throw std::runtime_error("Invalid HP-48 binary: missing HPHP48- header");
        }

        // Extract ROM version character (byte after "HPHP48-")
        _romVersion = static_cast<char>(data[magicHeader.size()]);

        // Copy content bytes (after header) - stored in compact form
        _data.assign(data.begin() + headerSize, data.end());

        // Parse object header if we have enough data
        if (rawNibbleCount() >= objectHeaderNibbles) {
            _prolog = static_cast<uint32_t>(readRawNibbles(0, prologNibbles));
            _objectLength = static_cast<uint32_t>(readRawNibbles(prologNibbles, lengthNibbles));
        }
    }

    // Check if data has been loaded
    [[nodiscard]] bool isLoaded() const noexcept {
        return !_data.empty();
    }

    // Get ROM version character (e.g., 'A', 'B', 'C', 'D', 'E', 'J' for SX; 'R' for GX)
    [[nodiscard]] char romVersion() const noexcept {
        return _romVersion;
    }

    // Get ROM version as descriptive string
    [[nodiscard]] std::string romVersionString() const {
        switch (_romVersion) {
            case 'A': return "Rev A (HP-48SX)";
            case 'B': return "Rev B (HP-48SX)";
            case 'C': return "Rev C (HP-48SX)";
            case 'D': return "Rev D (HP-48SX)";
            case 'E': return "Rev E (HP-48SX)";
            case 'J': return "Rev J (HP-48SX)";
            case 'K': return "Rev K (HP-48G)";
            case 'L': return "Rev L (HP-48G)";
            case 'P': return "Rev P (HP-48GX)";
            case 'R': return "Rev R (HP-48GX)";
            default:  return std::string("Rev ") + _romVersion + " (Unknown)";
        }
    }

    // === Object metadata ===

    // Get object prolog (5 nibbles identifying object type)
    [[nodiscard]] uint32_t prolog() const noexcept {
        return _prolog;
    }

    // Get object length field (interpretation varies by object type)
    [[nodiscard]] uint32_t objectLength() const noexcept {
        return _objectLength;
    }

    // Get object type as string
    [[nodiscard]] std::string objectTypeString() const {
        switch (_prolog) {
            case ObjectProlog::code:    return "Code";
            case ObjectProlog::string:  return "String";
            case ObjectProlog::grob:    return "GROB";
            case ObjectProlog::library: return "Library";
            case ObjectProlog::backup:  return "Backup";
            case ObjectProlog::list:    return "List";
            case ObjectProlog::dir:     return "Directory";
            case ObjectProlog::symb:    return "Symbolic";
            case ObjectProlog::unit:    return "Unit";
            case ObjectProlog::tagged:  return "Tagged";
            case ObjectProlog::program: return "Program";
            default: return "Unknown (0x" + toHex(_prolog, 5) + ")";
        }
    }

    // Check if this is a code/program object
    [[nodiscard]] bool isCode() const noexcept {
        return _prolog == ObjectProlog::code;
    }

    // Check if this is a string object
    [[nodiscard]] bool isString() const noexcept {
        return _prolog == ObjectProlog::string;
    }

    // Get number of nibbles in content body (excluding prolog and length)
    [[nodiscard]] size_t nibbleCount() const noexcept {
        size_t raw = rawNibbleCount();
        return (raw > objectHeaderNibbles) ? (raw - objectHeaderNibbles) : 0;
    }

    // Get number of bytes in content (original byte count)
    [[nodiscard]] size_t byteCount() const noexcept {
        return nibbleCount() / 2;
    }

    // Access individual nibble by index (relative to body start)
    [[nodiscard]] Nibble nibble(size_t index) const {
        if (index >= nibbleCount()) {
            throw std::out_of_range("Nibble index out of range");
        }
        return nibbleUnchecked(index);
    }

    // Access nibble with bounds checking (returns nullopt if out of range)
    [[nodiscard]] std::optional<Nibble> nibbleAt(size_t index) const noexcept {
        if (index >= nibbleCount()) {
            return std::nullopt;
        }
        return nibbleUnchecked(index);
    }

    // Subscript operator for nibble access (no bounds checking, relative to body)
    [[nodiscard]] Nibble operator[](size_t index) const noexcept {
        return nibbleUnchecked(index);
    }

    // Get span of all compact data bytes (including header)
    [[nodiscard]] std::span<uint8_t const> rawBytes() const noexcept {
        return _data;
    }

    // Read multiple nibbles as a value from body (LSN first, up to 16 nibbles)
    [[nodiscard]] uint64_t readNibbles(size_t offset, size_t count) const {
        if (count > 16) {
            throw std::invalid_argument("Cannot read more than 16 nibbles at once");
        }
        if (offset + count > nibbleCount()) {
            throw std::out_of_range("Read extends past end of data");
        }

        uint64_t result = 0;
        for (size_t i = 0; i < count; ++i) {
            result |= static_cast<uint64_t>(nibbleUnchecked(offset + i)) << (i * 4);
        }
        return result;
    }

    // Read a byte (2 nibbles) at given nibble offset in body
    [[nodiscard]] uint8_t readByte(size_t nibbleOffset) const {
        return static_cast<uint8_t>(readNibbles(nibbleOffset, 2));
    }

    // Read an address (5 nibbles) at given nibble offset in body
    [[nodiscard]] uint32_t readAddress(size_t nibbleOffset) const {
        return static_cast<uint32_t>(readNibbles(nibbleOffset, 5));
    }

    // Get total number of nibbles including object header
    [[nodiscard]] size_t rawNibbleCount() const noexcept {
        return _data.size() * 2;
    }

    // Read nibbles from raw data (including header)
    [[nodiscard]] uint64_t readRawNibbles(size_t offset, size_t count) const {
        if (count > 16) {
            throw std::invalid_argument("Cannot read more than 16 nibbles at once");
        }
        if (offset + count > rawNibbleCount()) {
            throw std::out_of_range("Read extends past end of data");
        }

        uint64_t result = 0;
        for (size_t i = 0; i < count; ++i) {
            result |= static_cast<uint64_t>(rawNibbleUnchecked(offset + i)) << (i * 4);
        }
        return result;
    }

    // Iterator support for range-based for loops over body nibbles
    [[nodiscard]] NibbleIterator begin() const noexcept;
    [[nodiscard]] NibbleIterator end() const noexcept;
    [[nodiscard]] NibbleIterator cbegin() const noexcept;
    [[nodiscard]] NibbleIterator cend() const noexcept;

    // Random access iterator for nibbles over compact storage
    class NibbleIterator {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = Nibble;
        using difference_type = std::ptrdiff_t;
        using pointer = void;  // No pointer type - we return by value
        using reference = Nibble;  // Return by value, not reference

        NibbleIterator() noexcept = default;

        NibbleIterator(uint8_t const* data, size_t nibbleIndex) noexcept
            : _data(data), _nibbleIndex(nibbleIndex) {}

        [[nodiscard]] Nibble operator*() const noexcept {
            size_t byteIndex = _nibbleIndex / 2;
            bool highNibble = (_nibbleIndex % 2) != 0;
            uint8_t byte = _data[byteIndex];
            return highNibble ? ((byte >> 4) & 0x0F) : (byte & 0x0F);
        }

        [[nodiscard]] Nibble operator[](difference_type n) const noexcept {
            return *(*this + n);
        }

        NibbleIterator& operator++() noexcept {
            ++_nibbleIndex;
            return *this;
        }

        NibbleIterator operator++(int) noexcept {
            NibbleIterator tmp = *this;
            ++_nibbleIndex;
            return tmp;
        }

        NibbleIterator& operator--() noexcept {
            --_nibbleIndex;
            return *this;
        }

        NibbleIterator operator--(int) noexcept {
            NibbleIterator tmp = *this;
            --_nibbleIndex;
            return tmp;
        }

        NibbleIterator& operator+=(difference_type n) noexcept {
            _nibbleIndex = static_cast<size_t>(static_cast<difference_type>(_nibbleIndex) + n);
            return *this;
        }

        NibbleIterator& operator-=(difference_type n) noexcept {
            _nibbleIndex = static_cast<size_t>(static_cast<difference_type>(_nibbleIndex) - n);
            return *this;
        }

        [[nodiscard]] friend NibbleIterator operator+(NibbleIterator it, difference_type n) noexcept {
            it += n;
            return it;
        }

        [[nodiscard]] friend NibbleIterator operator+(difference_type n, NibbleIterator it) noexcept {
            it += n;
            return it;
        }

        [[nodiscard]] friend NibbleIterator operator-(NibbleIterator it, difference_type n) noexcept {
            it -= n;
            return it;
        }

        [[nodiscard]] friend difference_type operator-(NibbleIterator const& a, NibbleIterator const& b) noexcept {
            return static_cast<difference_type>(a._nibbleIndex) - static_cast<difference_type>(b._nibbleIndex);
        }

        [[nodiscard]] friend bool operator==(NibbleIterator const& a, NibbleIterator const& b) noexcept {
            return a._nibbleIndex == b._nibbleIndex;
        }

        [[nodiscard]] friend bool operator!=(NibbleIterator const& a, NibbleIterator const& b) noexcept {
            return a._nibbleIndex != b._nibbleIndex;
        }

        [[nodiscard]] friend bool operator<(NibbleIterator const& a, NibbleIterator const& b) noexcept {
            return a._nibbleIndex < b._nibbleIndex;
        }

        [[nodiscard]] friend bool operator<=(NibbleIterator const& a, NibbleIterator const& b) noexcept {
            return a._nibbleIndex <= b._nibbleIndex;
        }

        [[nodiscard]] friend bool operator>(NibbleIterator const& a, NibbleIterator const& b) noexcept {
            return a._nibbleIndex > b._nibbleIndex;
        }

        [[nodiscard]] friend bool operator>=(NibbleIterator const& a, NibbleIterator const& b) noexcept {
            return a._nibbleIndex >= b._nibbleIndex;
        }

        // Access to underlying index (for debugging)
        [[nodiscard]] size_t index() const noexcept { return _nibbleIndex; }

    private:
        uint8_t const* _data = nullptr;
        size_t _nibbleIndex = 0;
    };

private:
    std::vector<uint8_t> _data;  // Compact storage: 2 nibbles per byte
    char _romVersion = '\0';
    uint32_t _prolog = 0;        // Object prolog (5 nibbles)
    uint32_t _objectLength = 0;  // Object length field (5 nibbles)

    // Internal unchecked nibble access (relative to body, skipping object header)
    [[nodiscard]] Nibble nibbleUnchecked(size_t index) const noexcept {
        return rawNibbleUnchecked(index + objectHeaderNibbles);
    }

    // Internal unchecked raw nibble access (includes header)
    [[nodiscard]] Nibble rawNibbleUnchecked(size_t index) const noexcept {
        size_t byteIndex = index / 2;
        bool highNibble = (index % 2) != 0;
        uint8_t byte = _data[byteIndex];
        return highNibble ? ((byte >> 4) & 0x0F) : (byte & 0x0F);
    }

    // Helper to format hex values
    [[nodiscard]] static std::string toHex(uint32_t value, int nibbles) {
        static constexpr char hexChars[] = "0123456789ABCDEF";
        std::string result;
        result.reserve(nibbles);
        for (int i = nibbles - 1; i >= 0; --i) {
            result += hexChars[(value >> (i * 4)) & 0xF];
        }
        return result;
    }
};

// Iterator method implementations
inline Binary::NibbleIterator Binary::begin() const noexcept {
    return NibbleIterator(_data.data(), 0);
}

inline Binary::NibbleIterator Binary::end() const noexcept {
    return NibbleIterator(_data.data(), nibbleCount());
}

inline Binary::NibbleIterator Binary::cbegin() const noexcept {
    return begin();
}

inline Binary::NibbleIterator Binary::cend() const noexcept {
    return end();
}

// Utility function to check if a file appears to be an HP-48 binary
[[nodiscard]] inline bool isHp48Binary(std::span<uint8_t const> data) noexcept {
    if (data.size() < Binary::headerSize) {
        return false;
    }
    return std::equal(
        Binary::magicHeader.begin(),
        Binary::magicHeader.end(),
        data.begin()
    );
}

[[nodiscard]] inline bool isHp48Binary(std::filesystem::path const& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }

    std::array<uint8_t, Binary::headerSize> header{};
    file.read(reinterpret_cast<char*>(header.data()), header.size());

    if (!file) {
        return false;
    }

    return isHp48Binary(header);
}

} // namespace Hp48
