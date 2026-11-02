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

static std::string aboutText = R"about(           An emulator environment for CHIP-8 derivatives


Cadmium allows loading and running files for various CHIP-8 derivatives,
as well as developing software for them using Octo assembler language.
Assembling, disassembling and preprocessing are done by Chiplet, a
CHIP-8 assembler, disassembler and preprocessor that is also
available as a standalone command line tool:
  https://github.com/gulrak/chiplet

# Supported Presets

Generic CHIP-8 emulation:
  * CHIP-8
  * CHIP-10
  * CHIP-8E
  * CHIP-8X
  * CHIP-48
  * SCHIP-1.0-BETA
  * SCHIP-1.0
  * SCHIP-1.1
  * SCHIPC (SUPER-CHIP compatibility interpreter)
  * SCHIP-MODERN
  * MEGACHIP
  * XO-CHIP

Other interpreter emulation:
  * STRICT-CHIP-8 (cycle exact HLE VIP CHIP-8)
  * RS-MEGACHIP (original RS-M8001 MegaChip interpreter)

COSMAC VIP hardware emulation:
  * VIP (raw COSMAC VIP)
  * VIP-CHIP-8
  * VIP-CHIP-10 (128x64)
  * VIP-CHIP-8 RB (relative branching)
  * VIP-CHIP-8R (PC relative jumps, >4k, research)
  * VIP-CHIP-8 TPD (64x64)
  * VIP-CHIP-8 TPD TS (Tom Swan's 64x64 version)
  * VIP-CHIP-8 FPD (64x128)
  * VIP-CHIP-8X (with VP-590/VP-595 add-on boards)
  * VIP-CHIP-8X TPD (64x64)
  * VIP-CHIP-8X FPD (64x128)
  * VIP-CHIP-8E

DREAM6800 hardware emulation:
  * DREAM (raw DREAM6800)
  * DREAM-CHIP-8
  * DREAM-CHIP-8-LOP (with logical operators)

ETI-660 hardware emulation:
  * ETI (raw ETI-660)
  * ETI-CHIP-8 (64x48)

HP-48SX hardware emulation (in builds with the HP-48SX ROM):
  * HP48 (raw HP-48SX)
  * HP48-CHIP-48
  * HP48-SCHIP-1.0-BETA
  * HP48-SCHIP-1.0
  * HP48-SCHIP-1.1
  * HP48-SCHPC (SUPER-CHIP compatibility interpreter)

Besides selecting a CHIP-8 variant it is possible to configure various
quirks and behaviors to further tweak compatibility for a special
CHIP-8 rom.

# File Extensions Detected

If a loaded file is found in the program database, the settings known
for it are used. Files that are known to run on any CHIP-8 variant
keep the current preset. For all other files Cadmium switches to a
preset depending on the file extension:
  '*.ch8'     - CHIP-8
  '*.ch10'    - CHIP-10 (a 128x64-only video mode variant)
  '*.c8e'     - CHIP-8E
  '*.c8x'     - CHIP-8X (with some color and audio support hardware)
  '*.ch48', '*.c48'
              - CHIP-48
  '*.sc10b'   - SCHIP-1.0-BETA
  '*.sc10'    - SCHIP-1.0
  '*.sc8', '*.sc11'
              - SCHIP-1.1
  '*.scc'     - SCHIPC
  '*.scm'     - SCHIP-MODERN
  '*.mc8'     - MEGACHIP
  '*.xo8'     - XO-CHIP
  '*.c8vip'   - STRICT-CHIP-8
  '*.hc8'     - VIP-CHIP-8, Hybrid CHIP-8 (including 1802 code)
  '*.c10'     - VIP-CHIP-10
  '*.c8rb'    - VIP-CHIP-8 RB
  '*.c8r'     - VIP-CHIP-8R
  '*.c8tpd', '*.c8h'
              - VIP-CHIP-8 TPD (two page display, 64x64)
  '*.c8tpdts' - VIP-CHIP-8 TPD TS
  '*.c8fpd'   - VIP-CHIP-8 FPD (four page display, 64x128)
  '*.c8xtpd'  - VIP-CHIP-8X TPD
  '*.c8xfpd'  - VIP-CHIP-8X FPD
  '*.c8eti'   - ETI-CHIP-8
  '*.bin', '*.ram', '*.raw', '*.cos'
              - Binary file of a raw 1802 program that can be
                executed by a COSMAC VIP
  '*.et660'   - Binary file of a raw ETI-660 program
  '*.hp', '*.hp48'
              - Raw HP-48SX (in builds with the HP-48SX ROM)
  '*.hex'     - Intel HEX file, loaded to the addresses it contains,
                the current preset is not changed
  '*.8o'      - This is opened as Octo source file into the editor
                and instantly assembled to a binary if possible
  '*.gif'     - OctoCartridge format that combines programs with
                options and color settings in a GIF file

Where several presets share an extension, the first one in the list
above wins, so e.g. '*.c8x' selects the generic CHIP-8X and not the
VIP-CHIP-8X preset. The DREAM6800, RS-MEGACHIP and the HP-48SX
CHIP-8 presets are only selected manually or by the program database.

# Editor

Cadmium contains an editor to allow writing CHIP-8 programs or
modifying existing code. ROMs loaded into Cadmium are automatically
disassembled and made available in the editor. While the disassembler
uses speculative execution, it still might not lead to the optimal
result, e.g. in case of self modifying code or calculated branches
that speculative execution didn't have enough information to get.

The editor supports the typical key combinations with CTRL, so copy,
cut and paste are C, X and V, CTRL-A selects all text, CTRL-Z undoes
and SHIFT-CTRL-Z redoes the last change, CTRL-F opens the find bar
and CTRL-R the find and replace bar.
Note: On macOS CMD instead of CTRL is used!

Changes are automatically assembled by Chiplet, errors are currently
shown in the status line. So if "No errors." is shown, the code is
the one executed when starting emulation and changing the code resets
the emulator status.

                           - - -

# Special Credits

Cadmium started as a "Well I guess I should at least implement CHIP-8
once" project, after I already privately had fun writing emulators,
but the combined early reactions from the raylib Discord and the
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

* John Earnest (@JohnEarnest) - for his great Octo and for XO-Chip,
  while I still would have loved for some things to be different in
  Octo Assembly Syntax, I really appreciate that this new variant
  got CHIP-8 development to a more modern level, made it more
  accessible and together with the great web based IDE at
  https://johnearnest.github.io/Octo/
  gave CHIP-8 a new life and the OctoJam idea sure helped pushing
  it, thank you for that! His C-Octo implementation of the assembler
  was the base of the Octo assembler in Chiplet that drives the
  assembling capabilities of Cadmium, and parts of C-Octo are still
  embedded in Cadmium:
  https://github.com/JohnEarnest/c-octo

* Tim Franssen (@Timendus) - His "CHIP-8 Test Suite" helped me a
  great deal in finding issues and get a better understanding of the
  quirks that differentiate the CHIP-8 variants:
  https://github.com/Timendus/chip8-test-suite
  and for his work on a binary CHIP-8 container that could carry
  emulation parameters allowing easy set-up for roms, more info at:
  https://github.com/Timendus/chip8-binary-format

* @NinjaWeedle - for his great help and countless tests to get some
  flesh on the very skinny original MegaChip8 specs, to allow to
  actually support this exotic CHIP-8 variant and maybe help it to
  be more accepted. This work resulted in his documentation and
  tests on:
  https://github.com/NinjaWeedle/MegaChip8

* @Kouzeru - for pushing me into supporting XO-Audio and the
  creative discussions on where to go with future chip variants.
  https://github.com/Kouzeru/

* Joshua Moss (@Bandock) - for motivating me to implement a
  CDP1802 emulation core and getting "Support a real VIP mode"
  onto my todo list and for the interesting HyperChip64 ideas.
  https://github.com/Bandock/hyper_bandchip

* Tobias V. Langhoff (@tobiasvl) - for his excellent page on CHIP-8
  extensions:
    https://chip-8.github.io/extensions/
  and for his DREAM6800 emulator DRÖM that inspired me to look into
  the DREAM6800.
  https://github.com/tobiasvl/drom

* Michael J Bauer (@M-J-Bauer) - for developing the DREAM6800 computer
  and CHIPOS to extend the CHIP-8 world to the M6800 CPU, for making
  me learn M6800 assembler and for allowing me to use the CHIPOS data
  inside Cadmium's DREAM6800 emulator.
  https://www.mjbauer.biz/DREAM6800.htm

                           - - -

Cadmium uses the following open source libraries under their licenses:


)about" R"about(# raylib

A simple and easy-to-use library to enjoy videogames programming
https://www.raylib.com/

Copyright (c) 2013-2026 Ramon Santamaria (@raysan5)

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

# raygui (used by rlguipp)

A simple and easy-to-use immediate-mode gui library
https://github.com/raysan5/raygui

Copyright (c) 2014-2025 Ramon Santamaria (@raysan5)

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

# MIT licensed libraries

  * Chiplet, a CHIP-8 assembler, disassembler and preprocessor,
    containing an Octo assembler based on a completely reworked
    variant of C-Octo
    https://github.com/gulrak/chiplet
    Copyright (c) 2023, Steffen Schümann
  * C-Octo, a C rewrite of the Octo CHIP-8 IDE, the base of the Octo
    assembler in Chiplet, parts of it are also directly embedded in
    Cadmium
    https://github.com/JohnEarnest/c-octo
    Copyright (c) 2020, John Earnest
  * JSON for Modern C++
    https://github.com/nlohmann/json
    Copyright (c) 2013-2022 Niels Lohmann
  * fmtlib, a modern formatting library
    https://fmt.dev
    Copyright (c) 2012 - present, Victor Zverovich
  * Date, a date and time library based on the C++11/14/17 <chrono>
    header
    https://howardhinnant.github.io/date/date.html
    Copyright (c) 2015, 2016, 2017 Howard Hinnant
    Copyright (c) 2016 Adrian Colomitchi
    Copyright (c) 2017 Florian Dang
    Copyright (c) 2017 Paul Thompson
    Copyright (c) 2018, 2019 Tomasz Kamiński
    Copyright (c) 2019 Jiangang Zhuang
  * cpp-httplib, a C++ header-only HTTP/HTTPS library (desktop only)
    https://github.com/yhirose/cpp-httplib
    Copyright (c) 2017 yhirose
  * magic_enum, static reflection for enums
    https://github.com/Neargye/magic_enum
    Copyright (c) 2019 - 2022 Daniil Goncharov
  * fast_float, fast and exact number parsing
    https://github.com/fastfloat/fast_float
    Copyright (c) 2021 The fast_float authors
  * ghc::filesystem, an implementation of C++17 std::filesystem
    https://github.com/gulrak/filesystem
    Copyright (c) 2018, Steffen Schümann
  * ghc::sqlite, a small C++ wrapper for SQLite (desktop only)
    Copyright (c) 2025, Steffen Schümann

All of them are licensed under the following terms:

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

)about" R"about(# libresample

Real-time sample rate conversion library by Dominic Mazzoni, used
under the terms of the BSD License
https://github.com/minorninth/libresample

Copyright (c) 2003, Dominic Mazzoni
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the
  distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

                           - - -

# Zstandard (in builds with HP-48SX support)

Fast real-time compression algorithm
https://github.com/facebook/zstd

BSD License

For Zstandard software

Copyright (c) Meta Platforms, Inc. and affiliates. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in
   the documentation and/or other materials provided with the
   distribution.

 * Neither the name Facebook, nor Meta, nor the names of its
   contributors may be used to endorse or promote products derived
   from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

                           - - -

# LibreSSL (desktop only)

Free version of the SSL/TLS protocol forked from OpenSSL
https://www.libressl.org/

LibReSSL files are retained under the copyright of the authors. New
additions are ISC licensed as per OpenBSD's normal licensing policy,
or are placed in the public domain.

The OpenSSL code is distributed under the terms of the original
OpenSSL licenses which follow:

The OpenSSL toolkit stays under a dual license, i.e. both the
conditions of the OpenSSL License and the original SSLeay license
apply to the toolkit. See below for the actual license texts. In case
of any license issues related to OpenSSL please contact
openssl-core@openssl.org.

OpenSSL License
---------------

Copyright (c) 1998-2011 The OpenSSL Project.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in
   the documentation and/or other materials provided with the
   distribution.

3. All advertising materials mentioning features or use of this
   software must display the following acknowledgment:
   "This product includes software developed by the OpenSSL Project
   for use in the OpenSSL Toolkit. (http://www.openssl.org/)"

4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be
   used to endorse or promote products derived from this software
   without prior written permission. For written permission, please
   contact openssl-core@openssl.org.

5. Products derived from this software may not be called "OpenSSL"
   nor may "OpenSSL" appear in their names without prior written
   permission of the OpenSSL Project.

6. Redistributions of any form whatsoever must retain the following
   acknowledgment:
   "This product includes software developed by the OpenSSL Project
   for use in the OpenSSL Toolkit (http://www.openssl.org/)"

THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
OF THE POSSIBILITY OF SUCH DAMAGE.

This product includes cryptographic software written by Eric Young
(eay@cryptsoft.com).  This product includes software written by Tim
Hudson (tjh@cryptsoft.com).

Original SSLeay License
-----------------------

Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
All rights reserved.

This package is an SSL implementation written
by Eric Young (eay@cryptsoft.com).
The implementation was written so as to conform with Netscapes SSL.

This library is free for commercial and non-commercial use as long as
the following conditions are aheared to.  The following conditions
apply to all code found in this distribution, be it the RC4, RSA,
lhash, DES, etc., code; not just the SSL code.  The SSL documentation
included with this distribution is covered by the same copyright
terms except that the holder is Tim Hudson (tjh@cryptsoft.com).

Copyright remains Eric Young's, and as such any Copyright notices in
the code are not to be removed.
If this package is used in a product, Eric Young should be given
attribution as the author of the parts of the library used.
This can be in the form of a textual message at program startup or
in documentation (online or textual) provided with the package.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in
   the documentation and/or other materials provided with the
   distribution.
3. All advertising materials mentioning features or use of this
   software must display the following acknowledgement:
   "This product includes cryptographic software written by
    Eric Young (eay@cryptsoft.com)"
   The word 'cryptographic' can be left out if the rouines from the
   library being used are not cryptographic related :-).
4. If you include any Windows specific code (or a derivative thereof)
   from the apps directory (application code) you must include an
   acknowledgement:
   "This product includes software written by Tim Hudson
    (tjh@cryptsoft.com)"

THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The licence and distribution terms for any publically available
version or derivative of this code cannot be changed.  i.e. this code
cannot simply be copied and put under another distribution licence
[including the GNU Public Licence.]

                           - - -

# Public domain and similar

The following parts are in the public domain or under licenses that
do not require attribution, but they deserve a thank you nonetheless:

  * SQLite, the database engine (desktop only)
    https://sqlite.org/
  * stb_image, stb_image_write and stb_image_resize2 by Sean Barrett
    https://github.com/nothings/stb
  * tl::expected by Sy Brand (CC0)
    https://github.com/TartanLlama/expected
  * SHA-1 in C++ by Steve Reid, Bruce Guenter and Volker
    Diels-Grabsch
    https://github.com/vog/sha1
  * stdendian by Michael Clark
    https://github.com/michaeljclark/stdendian
)about";

