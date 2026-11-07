//---------------------------------------------------------------------------------------
// src/emulation/hardware/hp48/saturndecoder.hpp
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

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>
#include <fmt/format.h>

namespace hp48 {

enum VariableType : uint8_t {
    vt_field,           // opcode pattern G
    vt_regpair4,        // opcode pattern H
    vt_regpair4rev,     // opcode pattern I
    vt_regpair8,        // opcode pattern J
    vt_regpair8split,   // opcode pattern K
    vt_regpair12,       // opcode pattern L
    vt_reg,             // opcode pattern M
    vt_mrpair,          // opcode pattern N
    vt_daregpair,       // opcode pattern O
    vt_daregpairrev,    // opcode pattern P
    vt_tempreg,         // opcode pattern Q
    vt_maxtables,
    vt_const = vt_maxtables, // opcode pattern R
    vt_nzconst,         // opcode pattern S
    vt_varconst,        // opcode pattern T
    vt_pcofs,           // opcode pattern U
    vt_hwflags,         // opcode pattern V
    vt_none
};

struct InstructionInfo
{
    const char* fmt;
    const char* fmt2;
    uint32_t opcode;
    int8_t opcodeSize;
    uint8_t numVars;
    uint8_t varSizes[3];
    uint8_t varOffsets[3];
    VariableType varTypes[3];
    uint8_t varArgs[3];
    std::string uniqueIdentifier() const;
};

struct DecodedInstruction
{
    const InstructionInfo* info;
    uint32_t opcode;
    uint32_t operands[3];
    uint8_t length;  // actual length in nibbles
};

class SaturnDecoder
{
public:
    SaturnDecoder();
    SaturnDecoder(const SaturnDecoder&) = delete;
    SaturnDecoder& operator=(const SaturnDecoder&) = delete;
    SaturnDecoder(SaturnDecoder&&) noexcept = default;
    SaturnDecoder& operator=(SaturnDecoder&&) noexcept = default;

    virtual ~SaturnDecoder() = default;

    // Decode instruction at address
    // Returns pointer to InstructionInfo, or nullptr if invalid
    // Updates address to point past the instruction
    DecodedInstruction decode(uint32_t& address);
    size_t getTrieInstructionCount() const;

    template <typename Formatter>
    void dumpTrie(Formatter&& formatter)
    {
        printTrie(*_root, formatter);
    }

    void dumpTrie()
    {
        dumpTrie([](size_t index) { return std::to_string(index); });
    }

    static std::span<const InstructionInfo> instructionInfos();

protected:
    // Derived class must implement this to provide memory access
    virtual uint8_t readNibble(uint32_t address) = 0;
    virtual void writeNibble(uint32_t address, uint8_t value) = 0;

private:
    unsigned getNibble(uint32_t address)
    {
        if (address < _startAddress) {
            throw std::out_of_range("Address is before start address");
        }

        uint32_t offset = address - _startAddress;

        if (offset < _length) {
            // Address is already in cache
            return _nibbleCache[offset];
        }

        if (offset > _length) {
            throw std::out_of_range("Address gap detected in sequential read");
        }

        if (_length >= 24) {
            throw std::out_of_range("Nibble cache is full");
        }

        // Read new nibble and add to cache
        unsigned nibble = readNibble(address);
        _nibbleCache[_length] = static_cast<uint8_t>(nibble);
        _length++;

        return nibble;
    }

    struct TrieNode
    {
        std::array<std::unique_ptr<TrieNode>, 16> children;
        std::vector<size_t> candidateIndices;  // indices into instruction table
    };
    size_t countTrieInstructions(const TrieNode* node) const;

    std::unique_ptr<TrieNode> _root;
    // Nibble cache info
    mutable uint32_t _startAddress;
    mutable uint32_t _length;
    mutable std::array<uint8_t, 24> _nibbleCache;

    // Build the instruction decode trie
    void buildTrie();

    // Insert single instruction into trie
    void insertInstruction(size_t instrIndex);

    // Get the fixed prefix length for an instruction
    uint8_t getFixedPrefixLength(const InstructionInfo& instr) const;

    // Check if instruction matches at given address
    bool matchesInstruction(const InstructionInfo& instr, uint32_t address, DecodedInstruction& decoded);

    // Extract a variable field value
    uint32_t extractVariable(const InstructionInfo& instr, const DecodedInstruction& decoded, uint32_t address, uint8_t index);
    //template <typename T>
    //T getParameter(const InstructionInfo& instr, const DecodedInstruction& decoded, uint32_t startAddress, unsigned index);

    // Validate variable value based on type
    bool isValidVariable(uint32_t value, VariableType type, uint8_t arg) const;

    // Calculate actual instruction length (handles varconst)
    uint8_t calculateLength(const InstructionInfo& instr, const uint32_t operands[3]) const;

    // Get nibble from opcode pattern at position
    static unsigned getNibbleFromOpcode(uint32_t opcode, uint8_t position, uint8_t totalSize);

    // Recursive depth-first decoder helper
    DecodedInstruction decodeRecursive(TrieNode* node, uint32_t baseAddr, uint32_t depth);

    static int64_t twosComplement(const uint64_t value, const unsigned bitSize)
    {
        if (value & (1ULL << (bitSize - 1))) {
            return static_cast<int64_t>(value) - (1LL << bitSize);
        }
        return static_cast<int64_t>(value);
    }
    static uint64_t reverseNibbles(uint64_t value, int n)
    {
        uint64_t result = 0;
        for (int i = 0; i < n; i++) {
            uint8_t nibble = (value >> (i * 4)) & 0xF;
            result |= (uint64_t)nibble << ((n - 1 - i) * 4);
        }
        return result;
    }

    char hexDigit(int v) { return (v < 10) ? static_cast<char>('0' + v) : static_cast<char>('A' + (v - 10)); }

    template <typename Formatter>
    std::string formatCandidates(const std::vector<size_t>& candidates, Formatter&& formatter)
    {
        std::string s = "[";
        for (size_t i = 0; i < candidates.size(); ++i) {
            if (i != 0)
                s += ", ";
            s += formatter(candidates[i]);
        }
        s += "]";
        return s;
    }

    template <typename Formatter>
    void printTrieNode(const TrieNode& node, const std::string& prefix, bool isLast, const std::string& path, Formatter&& formatter)
    {
        fmt::print("{}{}{}", prefix, isLast ? "└── " : "├── ", path);

        if (!node.candidateIndices.empty())
            fmt::print(" {}", formatCandidates(node.candidateIndices, formatter));

        fmt::print("\n");

        std::vector<int> childIndices;
        childIndices.reserve(16);
        for (int i = 0; i < 16; ++i) {
            if (node.children[i])
                childIndices.push_back(i);
        }

        const std::string childPrefix = prefix + (isLast ? "    " : "│   ");

        for (size_t i = 0; i < childIndices.size(); ++i) {
            const int idx = childIndices[i];
            const bool childIsLast = (i + 1 == childIndices.size());
            printTrieNode(*node.children[idx], childPrefix, childIsLast, path + hexDigit(idx), formatter);
        }
    }

    template <typename Formatter>
    void printTrie(const TrieNode& root, Formatter&& formatter)
    {
        fmt::print(".");

        if (!root.candidateIndices.empty())
            fmt::print(" {}", formatCandidates(root.candidateIndices, formatter));

        fmt::print("\n");

        std::vector<int> childIndices;
        childIndices.reserve(16);
        for (int i = 0; i < 16; ++i) {
            if (root.children[i])
                childIndices.push_back(i);
        }

        for (size_t i = 0; i < childIndices.size(); ++i) {
            const int idx = childIndices[i];
            const bool isLast = (i + 1 == childIndices.size());
            printTrieNode(*root.children[idx], "", isLast, std::string(1, hexDigit(idx)), formatter);
        }
    }
};

// Lookup tables for operand decoding
extern const std::array<std::string_view, 16> field;
extern const std::array<std::string_view, 16> regpair4;
extern const std::array<std::string_view, 16> regpair4rev;
extern const std::array<std::string_view, 16> regpair8;
extern const std::array<std::string_view, 16> regpair8split;
extern const std::array<std::string_view, 16> regpair12;
extern const std::array<std::string_view, 16> reg;
extern const std::array<std::string_view, 16> mrpair;
extern const std::array<std::string_view, 16> daregpair;
extern const std::array<std::string_view, 16> daregpairrev;
extern const std::array<std::string_view, 16> tempreg;

extern const std::array<const std::array<std::string_view, 16>, 11> varTables;

// Instruction table
extern const InstructionInfo instructions[];
extern const size_t instructionCount;

}  // namespace hp48
