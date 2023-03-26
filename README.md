# Cadmium

A CHIP-8 emulation environment written in C++ with a raylib backend.

![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)
![Supported Platforms](https://img.shields.io/badge/platform-macOS%20%7C%20Linux%20%7C%20Windows%20%7C%20Web-blue.svg)
[![CMake Build Matrix](https://github.com/gulrak/cadmium/actions/workflows/build_cmake.yml/badge.svg)](https://github.com/gulrak/cadmium/actions/workflows/build_cmake.yml)
[![Latest Release Tag](https://img.shields.io/github/tag/gulrak/cadmium.svg)](https://github.com/gulrak/cadmium/tree/v1.0.6)

<!-- TOC -->
* [Cadmium](#cadmium)
  * [Introduction](#introduction)
  * [Features](#features)
    * [Supported CHIP-8 variants](#supported-chip-8-variants)
    * [Quirks](#quirks)
  * [Command-line](#command-line)
  * [Versioning](#versioning)
  * [Compiling from Source](#compiling-from-source)
    * [Linux / macOS](#linux--macos)
    * [Windows](#windows)
    * [Build for Web](#build-for-web)
  * [Used Resources](#used-resources)
    * [Information](#information)
    * [Other Emulators used for Verification](#other-emulators-used-for-verification)
    * [Libraries](#libraries)
  * [Special Credits](#special-credits)
  * [FAQ](#faq)
    * [Why the name Cadmium?](#why-the-name-cadmium)
    * [Why the pixel look?](#why-the-pixel-look)
    * [Which font is that?](#which-font-is-that)
<!-- TOC -->

## Introduction

CHIP-8 is maybe the first breed of what is today called a "fantasy console".
The first opcode interpreter was written by Joseph Weisbecker for the
"COSMAC" CDP1802 cpu that he developed at RCA and was used in machines like 
e.g. the COSMAC VIP and the Telmac 1800. 

CHIP-8 is today seen as the "Hello world" of emulators, so the  first
suggestion if someone wants to get into emulator coding is to write a CHIP-8
the get the feeling for it. I already had done some work on emulators and
wrote my own implementation of a Commodore 64 emulator in C++, so one could
say that I had no reason to actually make a CHIP-8 one, but on a longer
weekend off, I stumbled across the topic, I knew the system from my trusty
old HP48GX, during my study times in the 1990s, so I felt it would be fun to
try to cobble an implementation together in a few hours.
It worked quite well, and as I was using raylib for some tools and took part
in a raylib game jam, I saw this as a good fit.

![Cadmium debug view](media/cadmium_01.png?raw=true "A screenshot of the debug view")

An Emscripten build is available here for testing: https://games.gulrak.net/cadmium -
simply drag rom files (`.ch8`, `.hc8`, `.ch10`, `.sc8`, `.mc8`, `.xo8`) or
Octo sources (`.8o`) onto the window to load them.

## Features

The emulation behavior used in Cadmium is based on opcode information documented
in [the wiki](https://github.com/gulrak/cadmium/wiki/Instruction-Overview).

### Supported CHIP-8 variants

The emulation is based on different emulation cores and, depending on the core,
on a set of "quirks" named options that go along with the core. To lessen the
burden to remember the combinations, Cadmium uses "Behavior Presets" to combine
a core, some settings and set of quirks to easily switch between CHIP-8 variants.
The naming conventions adapted are based on the list put together by
_Tobias V. Langhoff_ at https://chip-8.github.io/extensions/ and for the
classic COSMAC VIP based variants it is mainly based on the VIPER magazine where
those  interpreter variants where published.

The Supported presets are:

* CHIP-8
* CHIP-10
* CHIP-48
* SUPER-CHIP 1.0
* SUPER-CHIP 1.1
* SUPER-CHIP COMP
* MegaChip 8
* XO-CHIP
* VIP-CHIP-8
* CHIP-8-DREAM

The `SUPER-CHIP COMP` or `SCHIPC` is a more generic variant that is similar
to Chromatophores SCHPC/GCHPC variants of SCHIP1.1 to allow more modern games
(often developed on Octo) that target SuperCHIP to run without tweeking some
quirks that, while correct for the original SCHIP1.1, are not common in modern
programs.

The `VIP-CHIP-8` preset activates a core that is emulating a COSMAC
VIP driven by a CDP1802 CPU with 4k RAM, to execute original CHIP-8
interpreter variants to allow more accurate emulation of classic CHIP-8 and even
allow hybrid roms (`.hc8`) that contain CDP1802 parts  to execute on Cadmium.

The `CHIP-8-DREAM` preset activates a core that is emulating a DREAM6800
driven by an M6800 CPU with 4k RAM, to execute the original CHIP-8 CHIPOS
kernel and run an accurate emulation of DREAM6800 CHIP-8. This core also
allows hybrid roms that use machine code parts.

### Multi Architecture Debugging

Cadmium allows for the presets that emulate actual hardware to debug
seamlessly on the CHIP-8 opcode level or the backend CPU assembly level:

![Cadmium emulating DREAM6800](media/cadmium_mdbg_01.png?raw=true "Debugging on CHIP8 level") ![Cadmium emulating DREAM6800](media/cadmium_mdbg_02.png?raw=true "Debugging on M6800 level")

Breakpoints and single stepping works in both views, the selected instruction
tab defines the logical entity the debug buttons work on, the register view
shows the registers of that entity. Breakpoints of the hidden entity still
work and the tab switches to the entity that hit the breakpoint.

### Quirks

Emulation uses a number of configurable "quirks" or options, to allow a wide
range of roms to work with it. Contrary to some other sources, _Cadmium_ sees 
the initial original VIP behavior as the reference, so disabling all
following options gives a basic VIP CHIP-8:
* `8xy6`/`8xyE` just shift Vx
* `8xy1`/`8xy2`/`8xy3` don't reset VF
* `Fx55`/`Fx65` increment I only by x
* `Fx55`/`Fx65` don't increment I
* `Bxnn`/`jump0` uses Vx
* wrap sprite pixels
* `Dxyn` doesn't wait for vsync
* Lores `Dxy0` draws with 0(classic CHIP-8)/8/16 pixel width
* `Dxyn` uses SCHIP1.1 collision logic
* 128x64 hires support
* only 128x64 mode
* multicolor support
* XO-CHIP sound engine


## Command-line

```
USAGE: cadmium [options] file

OPTIONS:

--opcode-table
    Dump an opcode table to stdout

--random-gen <arg>
    Select a predictable random generator used for trace log mode (rand-lgc or counting)

--random-seed <arg>
    Select a random seed for use in combination with --random-gen, default: 12345

--screen-dump
    When in trace mode, dump the final screen content to the console

-b, --benchmark
    Run benchmark against octo-c

-c, --compare
    Run and compare with reference engine, trace until diff

-h, --help
    Show this help text

-p <arg>, --preset <arg>
    Select CHIP-8 preset to use: chip-8, chip-10, chip-48, schip1.0, schip1.1, megachip8, xo-chip of vip-chip-8

-r, --run
    if a ROM is given (positional) start it

-s <arg>, --exec-speed <arg>
    Set execution speed in instructions per frame (0-500000, 0: unlimited)

-t <arg>, --trace <arg>
    Run headless and dump given number of trace lines

file
ROM file ('.ch8', '.ch10', '.hc8', '.sc8', '.mc8', '.xo8') or Octo ('.8o') source to load
```

## Versioning

Cadmium uses version numbers to communicate the difference between releases and
work-in-progress builds. Only even patch versions will be used for releases
and odd patch version will only be used for commits while working on  the next
version. So a version like `v1.0.1` can vary in features and behavior as it is
the changing `main` branch head between `v1.0.0` and `v1.0.2`.

## Compiling from Source

_Cadmium_ is written in C++17 and uses CMake as a build solution. To build it,
checkout or download the source.

### Linux / macOS

Open a terminal, enter the directory where the code was extracted and run:

```
cmake -S . -B build
```

to configure the project, and

```
cmake --build build
```

to compile it.

### Windows

Currently, _Cadmium_ is not being able to compile with MSVC++ so the recommended
toolchain is [W64devkit](https://github.com/skeeto/w64devkit). I might try to make
it compile on MSVC++, but I am not  working on that in the near future. That being
said, I would not per-se reject PRs helping with this.

```
export PATH="$PATH;C:/Program Files/CMake/bin"
cmake -G"Unix Makefiles" -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DGRAPHICS=GRAPHICS_API_OPENGL_21 -S . -B build-w64dev
```

### Build for Web

This has only been tried on macOS yet, you need emsdk and cmake installed, and
I expect it to work on Linux the same way.

```
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<path-to-emsdk>/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake
cmake --build build
```

## Used Resources

### Information

* [Mastering CHIP-8](https://github.com/mattmikolay/chip-8/wiki/Mastering-CHIP%E2%80%908) -
  A great document to start the CHIP-8 emulation journey with (not about how emulators
  work in general)
* [CHIP-8 extensions and compatibility](https://chip-8.github.io/extensions/) - 
  A comprehensive overview of different  CHIP-8 variants that was the initial
  motivator to support many of them, a great overview.
* [HP48-Superchip](https://github.com/Chromatophore/HP48-Superchip) - 
  This repository contains valuable information on the specifics of
  CHIP-48/Super-Chip that go beyond the specification of Super-Chip.


### Other Emulators used for Verification

* [Octo](https://johnearnest.github.io/Octo/) - This has to be the first one named,
  this very user-friendly web based emulation environment sets the standard any
  CHIP-8 emulator that tries to help in development of CHIP-8 programs needs to be
  measured by, and I used it to try out a lot of stuff, it's assembly language is
  the one supported by _Cadmium_.
* [Silicon8](https://github.com/Timendus/silicon8) - While experimenting with
  more than 4 colors in XO-Chip mode this emulator helped to compare the results
  and test the very advanced and resource hungry roms its author has written.


### Libraries

* [raylib](https://www.raylib.com) - the fun game programming library that
  inspired me to write this _(automatically fetched during configuration from a fork)_
* [raygui](https://github.com/raysan5/raygui) - the lightweight immediate mode
  gui library for raylib _(contained in my wrapper rlguipp)_
* [c-octo assembler](https://github.com/JohnEarnest/c-octo) - the octo
  assembler for support of generating binaries from [Octo assembler syntax](http://johnearnest.github.io/Octo/docs/Manual.html)
  _(containied in `external/c-octo`)_
* [fmtlib](https://github.com/fmtlib/fmt) - a modern formatting library in
  the style of C++20 std::format _(contained in `external/fmt`)_
* [date](https://howardhinnant.github.io/date/date.html) - A date and time
  library based on the C++11/14/17 &lt;chrono&gt; header _(contained in `external/date`)_
* [json for modern c++](https://github.com/nlohmann/json) - A flexible and
  nice to use JSON implementation _(contained in `external/nlohmann`)_
* [ghc::filesystem](https://github.com/gulrak/filesystem) - An implementation
  of std::filesystem for C++11/14/17/20 that also works on MingW variants
  that don't have a working std::filesystem in C++17 and with better utf8
  support _(automatically fetched during configuration)_


## Special Credits

Cadmium started as a "Well I guess I should at least implement CHIP-8
once" project, after I already privately had fun writing emulators
But the combined early reactions from the raylib Discord and the
Emulation Development Discord gave me so much fun that I started to
dig more into the subject. So **first of all a big thank you to all
those who pushed me with their nice comments and inspirational
remarks, I can't name you all, but it really did kick off this project**.

I still want to give out some explicit thanks to:

* _Ramon Santamaria (@raysan5)_ - I stumbled across [raylib](https://www.raylib.com/)
  at the end of 2021 and since 2022 I started using it for some experiments and
  then took part in the raylib game jam and had so much fun. Without
  raylib I wouldn't have started this, it was originally a test bed
  for my wrapper of Ramons [raygui](https://github.com/raysan5/raygui) but
  instead became a life of its own.

* _John Earnest (@JohnEarnest)_ - for his great Octo and for XO-Chip,
  while I still would have loved for some things to be different in
  Octo Assembly Syntax, I really appreciate that this new variant
  got CHIP-8 development to a more modern level, made it more
  accessible and together with the great [web based IDE](https://johnearnest.github.io/Octo/)
  gave CHIP-8 a new live and the idea of a yearly [OctoJam](https://itch.io/jam/octojam-9)
  sure helped, pushing it. Thank you for that!\
  His [C-Octo implementation](https://github.com/JohnEarnest/c-octo) of the assembler
  also is embedded in Cadmium and drives its assembling capabilities:

* _Tim Franssen (@Timendus)_ - for his
  [CHIP-8 Test Suite](https://github.com/Timendus/chip8-test-suite)
  helped me a great deal in finding issues and get a better understanding of the
  quirks that differentiate the CHIP-8 variants, and for his work
  on a [binary CHIP-8 container](https://github.com/Timendus/chip8-binary-format)
  that could carry emulation parameter allowing easy set-up for roms.

* _@NinjaWeedle_ - for his great help and countless tests to get some
  flesh on the very skinny original MegaChip8 specs to allow actually
  support this exotic CHIP-8 variant ant maybe help it to be more
  accepted. This work resulted in his extended
  [MegaChip8 documentation](https://github.com/NinjaWeedle/MegaChip8).

* _@Kouzeru_ - for pushing me into supporting
  [XO-Audio](http://johnearnest.github.io/Octo/docs/XO-ChipSpecification.html#audio)
  and the creative discussions on where to go with future chip variants
  and the cool music he creates on XO-Audio. 

* _Joshua Moss (@Bandock)_ - for motivating me to implement and
  CDP1802 emulation core and getting "Support a real VIP mode"
  onto my todo list and for the interesting
  [HyperChip64](https://github.com/Bandock/hyper_bandchip) ideas.

* _Tobais V. Langhoff (@tobiasvl)_ - for his excellent page on [CHIP-8 extensions
  and compatibility](https://chip-8.github.io/extensions/),
  and for his DREAM6800 emulator [DRÖM](https://github.com/tobiasvl/drom)
  that inspired me to look into the DREAM6800.

* _Michael J Bauer (@M-J-Bauer)_ - for developing the [DREAM6800
  computer](https://www.mjbauer.biz/DREAM6800.htm)
  and CHIPOS, to extend the CHIP-8 world to the [M6800 CPU](https://en.wikipedia.org/wiki/Motorola_6800),
  and for making me learn M6800 assembler and giving his okay to use the CHIPOS rom
  data inside Cadmiums DREAM6800 emulator.


## FAQ

### Why the name Cadmium?
The first time I got into contact with CHIP-8 was with Super-Chip on an
HP-48 calculator during my CS studies. The base of that was the CHIP-48
variant, named after the calculator. I think the reliving of CHIP-8 on
these calculators was one of the things that helped CHIP-8 survive the
time from the seventies until today. _Cadmium_ is an element with the
atomic number of 48 in the periodic table of elements, so that made me
chose it as the name reference for this project.

### Why the pixel look?
_Cadmium_ is about emulating and developing for CHIP-8 and its variants,
a platform that has a resolution of traditionally 64x32 pixel, the
"hires" mode has 128x64 pixels and even MegaChip8 only 256x192 pixels,
so if you hate seeing pixels, this might not be the platform for you. :wink:
Jokes aside, I wanted _Cadmium_ to have an original and recognizable look.
Many emulators use the absolutely great _Dear ImGui_ for the
UI, and the fact that a ready-to-use memory editor exists for it helps
a lot, getting something professionally looking done fast. While there
is nothing wrong with that, and [Dorito](https://github.com/lesharris/dorito)
another CHIP-8 IDE that I only came to know after I already had editor and
assembler working, is a really great example of what can be done that way, I
still think there is not much individually recognizable between the look
of these emulators. One can argue that standardization is a good thing,
and I can see that, I just didn't feel like adding one to the list.
So I guess _Cadmium_ is for people that can appreciate this individuality
and raygui, the raylib extension that is behind many widgets of _Cadmium_
is the way to get this pixel look.

### Which font is that?
_Cadmium_'s UI uses a handmade font heavily inspired by the 5x8 pixel
font integrated in the [Thomson EF936x](https://en.wikipedia.org/wiki/Thomson_EF936x)
video chip that was used in an extension card of the [NDR-Klein-Computer](https://en.wikipedia.org/wiki/NDR-Klein-Computer),
a DIY computer that could be build following a german educational tv
program in the eighties. The font is build in an ASCII file in the
`tools` folder through a commandline tool and the generated array is
embedded into the `src/cadmium.cpp` source file. The chip originally only
has 95 characters and I created the basic set and filled up the latin-1
codepage characters and some additional unicode code points. I think
its historic source and its look add to the unique look and feel of
the _Cadmium_ UI, but I can agree that it is a matter of taste. :wink:
