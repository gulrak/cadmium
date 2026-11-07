//---------------------------------------------------------------------------------------
// src/emulation/hardware/hp48/saturndecoder.cpp
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

#include "saturndecoder.hpp"
#include <algorithm>
#include <cmath>

namespace hp48 {

// Lookup tables from your existing code
const std::array<std::string_view, 16> field{"p", "wp", "xs", "x", "s", "m", "b", "w"};
const std::array<std::string_view, 16> regpair4{"a,b", "b,c", "c,a", "d,c"};
const std::array<std::string_view, 16> regpair4rev{"b,a", "c,b", "a,c", "c,d"};
const std::array<std::string_view, 16> regpair8{"b,a", "c,b", "a,c", "c,d", "a,b", "b,c", "c,a", "d,c"};
const std::array<std::string_view, 16> regpair8split{"b,a", "c,b", "a,c", "c,d", {}, {}, {}, {}, "a,b", "b,c", "c,a", "d,c"};
const std::array<std::string_view, 16> regpair12{"b,a", "c,b", "a,c", "c,d", "a,a", "b,b", "c,c", "d,d", "a,b", "b,c", "c,a", "d,c"};
const std::array<std::string_view, 16> reg{"a", "b", "c", "d"};
const std::array<std::string_view, 16> mrpair{"a,@d0", "a,@d1", "@d0,a", "@d1,a", "c,@d0", "c,@d1", "@d0,c", "@d1,c"};
const std::array<std::string_view, 16> daregpair{"a,d0", "a,d1", {}, {}, "c,d0", "c,d1"};
const std::array<std::string_view, 16> daregpairrev{"d0,a", "d1,a", {}, {}, "d0,c", "d1,c"};
const std::array<std::string_view, 16> tempreg{"r0", "r1", "r2", "r3", "r4"};

const std::array<const std::array<std::string_view, 16>, 11> varTables{field, regpair4, regpair4rev, regpair8, regpair8split, regpair12, reg, mrpair, daregpair, daregpairrev, tempreg};

// Include your instruction table from saturnclarke.cpp
// For now, I'll provide a placeholder - you'll need to include the actual table
extern const InstructionInfo instructions[];
extern const size_t instructionCount;


inline std::string makeOpcodePattern(std::uint64_t opcode, std::uint8_t opcodeSize, std::uint8_t numVars, const std::uint8_t varSizes[3], const std::uint8_t varOffsets[3], const std::uint8_t varTypes[3])
{
    static constexpr char hexDigits[] = "0123456789abcdef";

    numVars = std::min<std::uint8_t>(numVars, 3);
    opcodeSize = std::min<std::uint8_t>(opcodeSize, 16);

    std::string result;

    // varOffsets are interpreted from the most-significant used nibble:
    // 0 = first nibble in the printed opcode pattern,
    // 1 = second nibble, etc.
    const std::uint8_t fixedCount = (numVars > 0) ? std::min<std::uint8_t>(varOffsets[0], opcodeSize) : opcodeSize;

    // Emit the fixed opcode prefix.
    for (std::uint8_t i = 0; i < fixedCount; ++i) {
        const std::uint8_t nibbleIndexFromLsb = opcodeSize - 1 - i;
        const auto value = static_cast<std::uint8_t>((opcode >> (nibbleIndexFromLsb * 4)) & 0xF);
        result += hexDigits[value];
    }

    // Emit variables in order.
    for (std::uint8_t i = 0; i < numVars; ++i) {
        const char ch = (varTypes[i] == 19) ? '0' : static_cast<char>('G' + varTypes[i]);

        result.append(varSizes[i], ch);
    }

    while (result.size() < opcodeSize) {
        result += '0';
    }

    return result;
}

inline std::string replaceVarPlaceholders(const std::string& input, std::uint8_t numVars, const std::uint8_t varTypes[3])
{
    numVars = std::min<std::uint8_t>(numVars, 3);

    std::string result;
    result.reserve(input.size());

    for (std::size_t i = 0; i < input.size(); ++i) {
        if (i + 2 < input.size() && input[i] == '{' && input[i + 2] == '}' && input[i + 1] >= '0' && input[i + 1] <= '2') {
            const std::uint8_t index = static_cast<std::uint8_t>(input[i + 1] - '0');

            if (index < numVars)
                result += static_cast<char>('G' + varTypes[index]);
            else
                result.append(input, i, 3);  // leave unknown/out-of-range placeholder unchanged

            i += 2;
        }
        else {
            result += input[i];
        }
    }

    return result;
}

inline std::string sanitizeCppIdentifier(std::string_view input)
{
    std::string result;
    result.reserve(input.size());

    bool lastWasUnderscore = false;

    for (unsigned char ch : input) {
        const bool allowed = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || (ch == '_');

        if (allowed) {
            if (ch == '_') {
                if (!result.empty() && !lastWasUnderscore) {
                    result += '_';
                    lastWasUnderscore = true;
                }
            }
            else {
                result += static_cast<char>(ch);
                lastWasUnderscore = false;
            }
        }
        else {
            if (!result.empty() && !lastWasUnderscore) {
                result += '_';
                lastWasUnderscore = true;
            }
        }
    }

    if (!result.empty() && result.back() == '_')
        result.pop_back();

    return result;
}

std::string InstructionInfo::uniqueIdentifier() const
{
    auto pattern = makeOpcodePattern(opcode, abs(opcodeSize), numVars, varSizes, varOffsets, reinterpret_cast<const uint8_t*>(varTypes));
    auto mnemonic = replaceVarPlaceholders(fmt, numVars, reinterpret_cast<const uint8_t*>(varTypes));
    auto identifier = sanitizeCppIdentifier(mnemonic + "_" + pattern);
    return identifier;
}

SaturnDecoder::SaturnDecoder()
{
    buildTrie();
}

std::span<const InstructionInfo> SaturnDecoder::instructionInfos()
{
    return {instructions, instructionCount};
}

unsigned SaturnDecoder::getNibbleFromOpcode(uint32_t opcode, uint8_t position, uint8_t totalSize)
{
    // Extract nibble at position from opcode
    // position 0 is the most significant nibble
    int shift = 4 * (totalSize - 1 - position);
    return (opcode >> shift) & 0xF;
}

DecodedInstruction SaturnDecoder::decodeRecursive(TrieNode* node, uint32_t baseAddr, uint32_t depth)
{
    DecodedInstruction result{nullptr, 0, {0, 0, 0}, 0};

    if (!node) {
        return result;
    }

    // DEPTH-FIRST: Try to go deeper first to find most specific match
    // This ensures nop3 (0x420) is tried before brcs (0x4XX)
    if (node->candidateIndices.empty() || instructions[node->candidateIndices[0]].opcodeSize > depth) {
        unsigned nibble = getNibble(baseAddr + depth);
        if (node->children[nibble]) {
            result = decodeRecursive(node->children[nibble].get(), baseAddr, depth + 1);
            if (result.info) {
                // Found a match at a deeper level - this is more specific, return it
                return result;
            }
        }
    }

    // No match deeper, try candidates at this level
    // Try each candidate in order (table order matters for disambiguation)
    for (size_t idx : node->candidateIndices) {
        if (matchesInstruction(instructions[idx], baseAddr, result)) {
            // Found a match at this level
            return result;
        }
    }

    // No match at this level or deeper
    return {nullptr, 0, {0, 0, 0}, 0};
}

uint8_t SaturnDecoder::getFixedPrefixLength(const InstructionInfo& instr) const
{
    if (instr.opcodeSize < 0) {
        // Variable length instruction - check for varconst
        // The fixed prefix goes up to the first varconst field
        uint8_t prefixLen = std::abs(instr.opcodeSize);
        for (uint8_t i = 0; i < instr.numVars; ++i) {
            if (instr.varTypes[i] == vt_varconst) {
                prefixLen = instr.varOffsets[i];
                break;
            }
        }
        return prefixLen;
    }

    // For fixed-length instructions, find the first variable position
    uint8_t firstVarPos = instr.opcodeSize;
    for (uint8_t i = 0; i < instr.numVars; ++i) {
        firstVarPos = std::min(firstVarPos, instr.varOffsets[i]);
    }
    return firstVarPos;
}

void SaturnDecoder::buildTrie()
{
    _root = std::make_unique<TrieNode>();

    for (size_t i = 0; i < instructionCount; ++i) {
        insertInstruction(i);
    }
}

void SaturnDecoder::insertInstruction(size_t instrIndex)
{
    const auto& instr = instructions[instrIndex];
    TrieNode* node = _root.get();

    // Build a map of which nibble positions are fixed vs variable
    uint8_t totalSize = std::abs(instr.opcodeSize);
    std::vector<bool> isFixed(totalSize, true);

    for (uint8_t v = 0; v < instr.numVars; ++v) {
        uint8_t offset = instr.varOffsets[v];
        uint8_t size = instr.varSizes[v];
        for (uint8_t n = 0; n < size; ++n) {
            if (offset + n < totalSize) {
                isFixed[offset + n] = false;
            }
        }
    }

    // Walk through fixed prefix, building trie
    uint8_t pos = 0;
    while (pos < totalSize && isFixed[pos]) {
        unsigned nibble = getNibbleFromOpcode(instr.opcode, pos, totalSize);

        if (!node->children[nibble]) {
            node->children[nibble] = std::make_unique<TrieNode>();
        }
        node = node->children[nibble].get();
        ++pos;
    }

    // Add this instruction as a candidate at this node
    node->candidateIndices.push_back(instrIndex);
}

/*
template <typename T>
T SaturnDecoder::getParameter(const InstructionInfo& info, const DecodedInstruction& decoded, uint32_t startAddress, unsigned index)
{
    T result;
    if (info.varTypes[index] == vt_none) {
        return {};
    }
    if (info.opcodeSize < 0) {
        auto varSize = (decoded.opcode & 0xF) + 1;
        return index ? decoded.varArg : varSize;
    }
    auto varSize = info.varSizes[index]*4;
    auto val = varSize ? (decoded.opcode >> (info.opcodeSize*4 - (info.varOffsets[index]*4 + varSize))) & ((1ULL << varSize) - 1) : 0;
    val = reverseNibbles(val, info.varSizes[index]);
    if (info.varTypes[index] < vt_maxtables) {
        return val - info.varArgs[index];
    }
    switch (info.varTypes[index]) {
        case vt_const:
            return val;
        case vt_nzconst:
            return val+1;
        case vt_pcofs: {
            auto distance = twosComplement(val, varSize);
            return startAddress + distance + info.varArgs[index];
        }
        case vt_varconst:
            return decoded.varArg;
        case vt_hwflags:
            return val;
        default:
            return 0;
    }
}

template uint64_t SaturnDecoder::getParameter<uint64_t>(const InstructionInfo& instr, const DecodedInstruction& decoded, uint32_t startAddress, unsigned index);
template unsigned SaturnDecoder::getParameter<unsigned>(const InstructionInfo& instr, const DecodedInstruction& decoded, uint32_t startAddress, unsigned index);
*/
//uint32_t SaturnDecoder::extractVariable(VariableType type, uint32_t address, uint8_t offset, uint8_t size) const
uint32_t SaturnDecoder::extractVariable(const InstructionInfo& instr, const DecodedInstruction& decoded, uint32_t address, uint8_t index)
{
    uint32_t value = 0;
    auto type = instr.varTypes[index];
    auto size = instr.varSizes[index];
    if (type == vt_varconst) {
        if (index == 0) {
            throw std::runtime_error("First variable must be a constant");
        }
        size = decoded.operands[index - 1];
    }
    for (uint8_t i = 0; i < size; ++i) {
        unsigned nibble = getNibble(address + instr.varOffsets[index] + i);
        value |= (nibble << (i * 4));
    }

    if (instr.varTypes[index] < vt_maxtables) {
        return value - instr.varArgs[index];
    }
    switch (instr.varTypes[index]) {
        case vt_const:
            return value;
        case vt_nzconst:
            return value+1;
        case vt_pcofs: {
            auto distance = twosComplement(value, size*4);
            return (distance + instr.varArgs[index]);// & 0xFFFFF;
        }
        case vt_varconst:
            return value;
        case vt_hwflags:
            return value;
        default:
            return 0;
    }
    
    
    if (type == vt_nzconst) {
        value += 1;
    }
    else if (type == vt_pcofs) {
        auto distance = twosComplement(value, instr.varSizes[index]);
        return distance + instr.varArgs[index];
    }
    return value;
}

bool SaturnDecoder::isValidVariable(uint32_t value, VariableType type, uint8_t arg) const
{
    switch (type) {
        case vt_nzconst:
            // Non-zero constant
            return value != 0;

        case vt_const:
            return true;

        case vt_pcofs:
            return value != arg;

        case vt_varconst:
            // Variable length constant - value determines length
            // For now, accept any value; length calculation happens elsewhere
            return true;

        case vt_hwflags:
            // Hardware flags - specific values only
            // Would need more detail on valid values
            return true;

        case vt_field:
        case vt_regpair4:
        case vt_regpair4rev:
        case vt_regpair8:
        case vt_regpair8split:
        case vt_regpair12:
        case vt_reg:
        case vt_mrpair:
        case vt_daregpair:
        case vt_daregpairrev:
        case vt_tempreg: {
            // Table-based validation
            if (type >= vt_maxtables) {
                return false;
            }

            // Check if value+arg is valid in the corresponding table
            const auto& table = varTables[type];
            if (value >= table.size()) {
                return false;
            }

            // Check if entry is empty (some tables have gaps)
            return !table[value].empty();
        }

        case vt_none:
            return true;
    }

    return true;
}

uint8_t SaturnDecoder::calculateLength(const InstructionInfo& instr, const uint32_t operands[3]) const
{
    if (instr.opcodeSize >= 0) {
        return instr.opcodeSize;
    }
    return std::abs(instr.opcodeSize) + operands[0];
}

bool SaturnDecoder::matchesInstruction(const InstructionInfo& instr, uint32_t address, DecodedInstruction& decoded)
{
    static size_t cnt = 0;
    if (instr.opcode == 0x8086000 && instr.fmt[0] == 'r') {
        cnt++;
    }
    uint8_t totalSize = std::abs(instr.opcodeSize);
    auto start = instr.numVars ? instr.varOffsets[0] : 0;
    // Build map of fixed vs variable nibbles
    std::vector<bool> isFixed(totalSize, true);
    for (uint8_t v = 0; v < instr.numVars; ++v) {
        uint8_t offset = instr.varOffsets[v];
        uint8_t size = instr.varSizes[v];
        for (uint8_t n = 0; n < size; ++n) {
            if (offset + n < totalSize) {
                isFixed[offset + n] = false;
            }
        }
    }

    // Check all fixed nibbles match
    for (uint8_t pos = start; pos < totalSize; ++pos) {
        if (!isFixed[pos]) {
            continue;
        }

        unsigned expectedNibble = getNibbleFromOpcode(instr.opcode, pos, totalSize);
        unsigned actualNibble = getNibble(address + pos);

        if (actualNibble != expectedNibble) {
            return false;
        }
    }

    // Extract and validate all variables
    for (uint8_t v = 0; v < instr.numVars; ++v) {
        auto value = extractVariable(instr, decoded, address, v);
        if (!isValidVariable(value, instr.varTypes[v], instr.varArgs[v])) {
            return false;
        }
        decoded.operands[v] = value;
    }

    // All checks passed
    decoded.opcode = instr.opcode;
    decoded.info = &instr;
    decoded.length = calculateLength(instr, decoded.operands);

    return true;
}

DecodedInstruction SaturnDecoder::decode(uint32_t& address)
{
    _startAddress = address;
    _length = 0;
    DecodedInstruction result = decodeRecursive(_root.get(), address, 0);
    if (result.info) {
        address += result.length;
    }
    return result;
#if 0
    DecodedInstruction result{nullptr, 0, {0, 0, 0}, 0};
    _startAddress = address;
    _length = 0;

    // Walk the trie to find candidate instructions
    TrieNode* node = _root.get();
    uint32_t currentAddr = address;

    // Follow the trie as far as we can
    while (true) {
        // If we have candidates at this node, try them
        if (!node->candidateIndices.empty()) {
            // Try each candidate in order (order matters for disambiguation)
            for (size_t idx : node->candidateIndices) {
                if (matchesInstruction(instructions[idx], address, result)) {
                    // Found a match!
                    address += result.length;
                    return result;
                }
            }

            // No candidates matched, return failure
            if (node->children.empty()) {
                return result;
            }
        }

        // Try to continue in trie
        unsigned nibble = getNibble(currentAddr);
        if (node->children[nibble]) {
            node = node->children[nibble].get();
            ++currentAddr;
        }
        else {
            // Dead end in trie with no candidates
            break;
        }
    }

    // No instruction found
    return result;
#endif
}

size_t SaturnDecoder::getTrieInstructionCount() const
{
    return countTrieInstructions(_root.get());
}

size_t SaturnDecoder::countTrieInstructions(const TrieNode* node) const
{
    if (!node) {
        return 0;
    }

    // Count instruction candidates at this node
    size_t count = node->candidateIndices.size();

    // Recursively count all children
    for (unsigned nibble = 0; nibble < 16; ++nibble) {
        if (node->children[nibble]) {
            count += countTrieInstructions(node->children[nibble].get());
        }
    }

    return count;
}

}  // namespace hp48
