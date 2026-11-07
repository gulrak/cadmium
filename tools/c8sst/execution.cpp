//---------------------------------------------------------------------------------------
// tools/c8sst/execution.cpp
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
#include "execution.hpp"
#include <emulation/chip8realcorebase.hpp>
#include <fmt/format.h>
#include <stdexcept>

namespace c8sst {
void executeHardwareInstruction(emu::IChip8Emulator& core)
{
    auto* hardware = dynamic_cast<emu::Chip8RealCoreBase*>(&core);
    if (!hardware) throw std::runtime_error("VIP profile requires a hardware interpreter oracle");
    auto& backend = hardware->getBackendCpu();
    // A breakpoint at the interpreter's current fetch boundary stops only after
    // it is reached again. This distinguishes completion from the core's timeout.
    auto boundary = backend.getPC();
    backend.setBreakpoint(boundary, {"", "c8sst instruction boundary", "", emu::GenericCpu::BreakpointInfo::eTRANSIENT});
    core.setExecMode(emu::GenericCpu::eRUNNING);
    // The hardware core yields after a bounded amount of native work. Clear
    // and draw can take longer; resume the same instruction until the boundary
    // breakpoint fires, with an additional bound for malformed cases.
    for (int attempt = 0; attempt < 16 && core.execMode() != emu::GenericCpu::ePAUSED; ++attempt)
        core.executeInstruction();
    const bool completed = backend.getPC() == boundary && backend.execMode() == emu::GenericCpu::ePAUSED;
    backend.removeBreakpoint(boundary);
    if (!completed || core.cpuState() == emu::GenericCpu::eERROR) {
        std::string native;
        for (size_t r = 0; r < backend.numRegisters(); ++r) native += fmt::format(" {}={:04x}", backend.registerNames()[r], backend.registerbyIndex(r).value);
        throw std::runtime_error(fmt::format("Oracle failed at native boundary {:04x}/{:04x}, mode {}: {}", boundary, backend.getPC(), static_cast<int>(backend.execMode()), core.dumpStateLine() + " " + backend.disassembleInstructionWithBytes(-1, nullptr) + native));
    }
}

} // namespace c8sst
