//---------------------------------------------------------------------------------------
// src/about.hpp
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
#include <string>

static std::string aboutText = R"(           An emulator environment for CHIP-8 derivates


Cadmium allows loading and running files for various CHIP-8 derivates,
as well as developing software for them using Octo assembler language.

# Supported Variants

Currently supported CHIP-8 variants are:
  * CHIP-8
  * CHIP-10
  * CHIP-48
  * SUPERCHIP 1.0
  * SUPERCHIP 1.1
  * MegaChip8
  * XO-CHIP

Besides selecting a CHIP-8 variant it is possible to configure various
quirks and behaviors to further tweak compatibility for a special
CHIP-8 rom.

# File Extensions Detected

Cadmium automatically switches to specific presets depending on the
file type of files loaded:
  '*.ch8' - This is the generic form and the current preset is not
            changed
  '*.sc8' - SUPERCHIP 1.1
  '*.mc8' - MegaChip8
  '*.xo8' - XO-CHIP
  '*.8o'  - This is opened as Octo source file into the editor and
            instantly assembled to a binary if possible
  '*.c8b' - Experimental support for CHIP-8 binary format files
            as described at:
            https://github.com/Timendus/chip8-binary-format

# Editor

Cadmium contains an editor to allow writing CHIP-8 programs or
modifying existing code. ROMs loaded into Cadmium are automatically
disassembled and made available in the editor. While the disassembler
uses speculative execution, it still might not lead to the optimal
result, e.g. in case of self modifying code or calculated branches
that speculative execution didn't have enough information to get.

The editor supports the typical key combinations with CTRL, so copy,
cut and paste are C, X and V, CTRL-A selects all text.

Changes are automatically compiled, errors are currently shown in the
status line. So if "No errors." is shown, the code is the one executed
when starting emulation and changing the code resets the emulator
status.

                           - - -

# Special Credits

Cadmium started as a "Well I guess I should at least implement CHIP-8
once" project, after I already privately had fun writing emulators
But the combined early reactions from the raylib Discord and the
Emulation Development Discord gave me so much fun that I started to
dig more into the subject. So first of all a big thank you to all
those who pushed me with their nice comments and inspirational
remarks, I can't name you all but it really did kick off this project.

I still want to give out some explicit thanks to:

* Ramon Santamaria (@raysan5) - I stumbled across raylib at the end
  of 2021 and since 2022 I started using it for some experiments and
  then took part in the raylib game jam and had so much fun. Without
  raylib I wouldn't have started this, it was originally a test bed
  for my raygui wrapper but instead became a life of its own.
  https://raylibtech.itch.io

* John Earnest (@JohnEarnest) - For his great Octo and for XO-Chip,
  while I still would have loved for some things to be different in
  Octo Assembly Syntax, I really appreciate that this new variant
  got CHIP-8 development to a more modern leve, made it more
  accessible and together with the great web based IDE at
  https://johnearnest.github.io/Octo/
  gave CHIP-8 a new live and the OctoJam idea sure helped pushing
  it, thank you for that! His C-Octo implementation of the assembler
  also is embedded in Cadmium and drives its assembling capabilities:
  https://github.com/JohnEarnest/c-octo

* Tim Franssen (@Timendus) - For his "CHIP-8 Test Suite" helped me a
  great deal in finding issues and get a better understanding of the
  quirks that differentiate the CHIP-8 variants:
  https://github.com/Timendus/chip8-test-suite
  and for his work on a binary CHIP-8 container that could carray
  emulation parameter allowing easy set-up for roms, more info at:
  https://github.com/Timendus/chip8-test-suite

* @NinjaWeedle - For his great help and countless tests to get some
  flesh on the very skinny original MegaChip8 specs to allow actually
  support this exotic CHIP-8 variant ant maybe help it to be more
  accepted. His work resulted in documentation and tests on:
  https://github.com/NinjaWeedle/MegaChip8

* @Kouzeru and for pushing me into supporting XO-Audio and the
  creative discussions on where to go with future chip
  https://github.com/Kouzeru/

* Joshua Moss (@Bandock) - for motivating me to implement and
  CDP1802 emulation core and getting "Support a real VIP mode"
  onto my todo list and for the interesting HyperChip64 ideas.
  https://github.com/Bandock/hyper_bandchip

                           - - -

Cadmium uses the following open source libraries under their licenses:


# raylib

A simple and easy-to-use library to enjoy videogames programming
https://www.raylib.com/

Copyright (c) 2013-2022 Ramon Santamaria (@raysan5)

This software is provided "as-is", without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute
it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must
   not claim that you wrote the original software. If you use this
   software in a product, an acknowledgment in the product
   documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must
   not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source
   distribution.

                           - - -

# Octo-Compiler taken from C-Octo

A C rewrite of the Octo CHIP-8 IDE
https://github.com/JohnEarnest/c-octo

The MIT License (MIT)

Copyright (c) 2020, John Earnest

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

                           - - -

# JSON parser

JSON for Modern C++
https://github.com/nlohmann/json

MIT License

Copyright (c) 2013-2022 Niels Lohmann

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

                           - - -

# fmtlib

A modern formatting library
https://fmt.dev

Copyright (c) 2012 - present, Victor Zverovich

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

                           - - -

# Date library

A date and time library based on the C++11/14/17 <chrono> header
https://howardhinnant.github.io/date/date.html

MIT License

Copyright (c) 2015, 2016, 2017 Howard Hinnant
Copyright (c) 2016 Adrian Colomitchi
Copyright (c) 2017 Florian Dang
Copyright (c) 2017 Paul Thompson
Copyright (c) 2018, 2019 Tomasz Kamiński
Copyright (c) 2019 Jiangang Zhuang

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
)";

