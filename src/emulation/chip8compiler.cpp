#include <emulation/chip8compiler.hpp>
#include <emulation/utility.hpp>

#include <sha1/sha1.hpp>
#include <iostream>

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wnarrowing"
#pragma GCC diagnostic ignored "-Wc++11-narrowing"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wimplicit-int-float-conversion"
#pragma GCC diagnostic ignored "-Wenum-compare"
#pragma GCC diagnostic ignored "-Wwritable-strings"
#pragma GCC diagnostic ignored "-Wwrite-strings"
#if __clang__
#pragma GCC diagnostic ignored "-Wenum-compare-conditional"
#endif
#endif  // __GNUC__

extern "C" {
#include <octo_compiler.h>
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

namespace emu {

class Chip8Compiler::Private
{
public:
    octo_program* _program{nullptr};
    std::string _sha1hex;
    std::string _errorMessage;
    std::vector<std::pair<uint32_t, uint32_t>> _lineCoverage;
};

Chip8Compiler::Chip8Compiler()
    : _impl(new Private)
{
    _impl->_program = nullptr;
}

Chip8Compiler::~Chip8Compiler()
{
    if (_impl->_program) {
        octo_free_program(_impl->_program);
        _impl->_program = nullptr;
    }
}

bool Chip8Compiler::compile(std::string str)
{
    if (_impl->_program) {
        octo_free_program(_impl->_program);
        _impl->_program = nullptr;
    }
    char* source = (char*)malloc(str.length() + 1);
    memcpy(source, str.data(), str.length() + 1);
    _impl->_program = octo_compile_str(source);
    if (!_impl->_program) {
        _impl->_errorMessage = "ERROR: unknown error, no binary generated";
    }
    else if (_impl->_program->is_error) {
        _impl->_errorMessage = "ERROR (" + std::to_string(_impl->_program->error_line + 1) + ":" + std::to_string(_impl->_program->error_pos + 1) + "): " + _impl->_program->error;
        //std::cerr << _impl->_errorMessage << std::endl;
    }
    else {
        _impl->_sha1hex = calculateSha1Hex(code(), codeSize());
        _impl->_errorMessage = "No errors.";
        //std::clog << "compiled successfully." << std::endl;
    }
    return !_impl->_program->is_error;
}

bool Chip8Compiler::isError() const
{
    return !_impl->_program || _impl->_program->is_error;
}

const std::string& Chip8Compiler::errorMessage() const
{
    return _impl->_errorMessage;
}

uint16_t Chip8Compiler::codeSize() const
{
    return _impl->_program && !_impl->_program->is_error ? _impl->_program->length - 0x200 : 0;
}

const uint8_t* Chip8Compiler::code() const
{
    return reinterpret_cast<const uint8_t*>(_impl->_program->rom + 0x200);
}

const std::string& Chip8Compiler::sha1Hex() const
{
    return _impl->_sha1hex;
}

std::pair<uint32_t, uint32_t> Chip8Compiler::addrForLine(uint32_t line) const
{
    return line < _impl->_lineCoverage.size() && !isError() ? _impl->_lineCoverage[line] : std::make_pair(0xFFFFFFFFu, 0xFFFFFFFFu);
}

uint32_t Chip8Compiler::lineForAddr(uint32_t addr) const
{
    return addr < OCTO_RAM_MAX && !isError() ? _impl->_program->romLineMap[addr] : 0xFFFFFFFF;
}

void Chip8Compiler::updateLineCoverage()
{
    _impl->_lineCoverage.clear();
    _impl->_lineCoverage.resize(_impl->_program->source_line);
    if (!_impl->_program)
        return;
    for (size_t addr = 0; addr < OCTO_RAM_MAX; ++addr) {
        auto line = _impl->_program->romLineMap[addr];
        if (line < _impl->_lineCoverage.size()) {
            auto& range = _impl->_lineCoverage.at(line);
            if (range.first > addr || range.first == 0xffffffff)
                range.first = addr;
            if (range.second < addr || range.second == 0xffffffff)
                range.second = addr;
        }
    }
}

}
