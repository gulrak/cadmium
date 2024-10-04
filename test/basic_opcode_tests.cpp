//---------------------------------------------------------------------------------------
// tests/basic_opcode_tests.cpp
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

#include <doctest/doctest.h>

#include "chip8testhelper.hpp"

#define TIMER_DEFAULT 0

#define QUIRK_VF_NOT_RESET 1
#define QUIRK_SHIFT_VX 2
#define QUIRK_JUMP_VX 4
#define QUIRK_NO_JUMP 8
#define QUIRK_LOAD_X 16
#define QUIRK_LOAD_0 32
#define QUIRK_NO_SHIFT 64



#define C8CORE "chip-8"
#define QUIRKS 0
#include "basic_opcode_tests.hpp"
#undef QUIRKS
#undef C8CORE

#define C8CORE "chip-10"
#define QUIRKS 0
#include "basic_opcode_tests.hpp"
#undef QUIRKS
#undef C8CORE

#define C8CORE "chip-8e"
#define QUIRKS 0
#include "basic_opcode_tests.hpp"
#undef QUIRKS
#undef C8CORE

#define C8CORE "chip-8x"
#define QUIRKS QUIRK_NO_JUMP
#include "basic_opcode_tests.hpp"
#undef QUIRKS
#undef C8CORE

#define C8CORE "chip-48"
#define QUIRKS (QUIRK_VF_NOT_RESET|QUIRK_LOAD_X|QUIRK_JUMP_VX|QUIRK_SHIFT_VX)
#include "basic_opcode_tests.hpp"
#undef QUIRKS
#undef C8CORE

#define C8CORE "schip-1-0"
#define QUIRKS (QUIRK_VF_NOT_RESET|QUIRK_LOAD_X|QUIRK_JUMP_VX|QUIRK_SHIFT_VX)
#include "basic_opcode_tests.hpp"
#undef QUIRKS
#undef C8CORE

#define C8CORE "schip-1-1"
#define QUIRKS (QUIRK_VF_NOT_RESET|QUIRK_LOAD_0|QUIRK_JUMP_VX|QUIRK_SHIFT_VX)
#include "basic_opcode_tests.hpp"
#undef QUIRKS
#undef C8CORE

#define C8CORE "schipc"
#define QUIRKS QUIRK_VF_NOT_RESET
#include "basic_opcode_tests.hpp"
#undef QUIRKS
#undef C8CORE

#define C8CORE "schip-modern"
#define QUIRKS (QUIRK_VF_NOT_RESET|QUIRK_LOAD_0|QUIRK_SHIFT_VX)
#include "basic_opcode_tests.hpp"
#undef QUIRKS
#undef C8CORE

#define C8CORE "megachip"
#define QUIRKS (QUIRK_VF_NOT_RESET|QUIRK_LOAD_0|QUIRK_JUMP_VX|QUIRK_SHIFT_VX)
#include "basic_opcode_tests.hpp"
#undef QUIRKS
#undef C8CORE

#define C8CORE "xo-chip"
#define QUIRKS QUIRK_VF_NOT_RESET
#include "basic_opcode_tests.hpp"
#undef QUIRKS
#undef C8CORE

#define C8CORE "strict-chip-8"
#define QUIRKS 0
#include "basic_opcode_tests.hpp"
#undef QUIRKS
#undef C8CORE

#define C8CORE "vip-chip-8"
#define QUIRKS 0
#include "basic_opcode_tests.hpp"
#undef QUIRKS
#undef C8CORE

#define C8CORE "vip-chip-10"
#define QUIRKS 0
#include "basic_opcode_tests.hpp"
#undef QUIRKS
#undef C8CORE

#define C8CORE "vip-chip-8-rb"
#define QUIRKS QUIRK_NO_JUMP
#include "basic_opcode_tests.hpp"
#undef QUIRKS
#undef C8CORE

#define C8CORE "vip-chip-8-tpd"
#define QUIRKS QUIRK_NO_JUMP
#include "basic_opcode_tests.hpp"
#undef QUIRKS
#undef C8CORE

#define C8CORE "vip-chip-8-fpd"
#define QUIRKS QUIRK_NO_JUMP
#include "basic_opcode_tests.hpp"
#undef QUIRKS
#undef C8CORE

#define C8CORE "vip-chip-8x"
#define QUIRKS QUIRK_NO_JUMP
#include "basic_opcode_tests.hpp"
#undef QUIRKS
#undef C8CORE

#define C8CORE "vip-chip-8x-tpd"
#define QUIRKS QUIRK_NO_JUMP
#include "basic_opcode_tests.hpp"
#undef QUIRKS
#undef C8CORE

#define C8CORE "vip-chip-8x-fpd"
#define QUIRKS QUIRK_NO_JUMP
#include "basic_opcode_tests.hpp"
#undef QUIRKS
#undef C8CORE

#define C8CORE "vip-chip-8e"
#define QUIRKS QUIRK_NO_JUMP
#include "basic_opcode_tests.hpp"
#undef QUIRKS
#undef C8CORE
