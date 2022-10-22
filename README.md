<!-- TOC -->
* [Cadmium](#cadmium)
  * [Introduction](#introduction)
  * [Features](#features)
  * [Compiling from Source](#compiling-from-source)
    * [Linux / macOS](#linux--macos)
    * [Windows](#windows)
    * [Build for Web](#build-for-web)
  * [Used Resources](#used-resources)
    * [Information](#information)
    * [Other Emulators used for Verification](#other-emulators-used-for-verification)
    * [Libraries](#libraries)
  * [FAQ](#faq)
    * [Why the name "Cadmium"?](#why-the-name--cadmium--)
    * [Why the pixel look?](#why-the-pixel-look)
    * [Which font is that?](#which-font-is-that)
<!-- TOC -->

# Cadmium

A CHIP-8 emulation environment written in C++ with a raylib backend.

## Introduction

CHIP-8 is maybe the first breed of what is today called a "fantasy console".
The first opcode interpreter  was written for the RCA COSMAC VIP in 1977.
CHIP-8 is today seen as the "Hello world" of emulators, so the  first
suggestion if someone wants to get into emulator coding is to write a CHIP-8
the get the feeling for it. I already had done some work on emulators and
wrote my own implementation of a Commodore 64 emulator in C++, so one could
say that I had no reason to actually make a CHIP-8 one, but on a longer
weekend off, I stumbled across the topic, I knew the system from my trusty
old HP48GX, during my study times in the 1990s so I felt it would be fun to
try to cobble an implementation together in a few hours.
It worked quite well, and as I was using raylib for some tools and took part
in a raylib game jam, I saw this as a good fit.

![Cadmium debug view](media/screenshot01.png?raw=true "A screenshot of the debug view")

An Emscripten build is available here for testing: https://games.gulrak.net/cadmium -
simply drag rom files (`.ch8`, `.sc8`, `.mc8`, `.xo8`) or Octo sources (`.8o`) onto the
window to load them.

## Features

The emulation behavior used in Cadmium is based on opcode information documented
in [the wiki](https://github.com/gulrak/cadmium/wiki/Instruction-Overview).

* Emulation uses a number of configurable "quirks" or options, to allow a wide
  range of roms to work with it. Contrary to some other sources, Cadmium sees 
  the initial original VIP behavior as the reference, so disabling all
  following options gives a basic VIP CHIP-8:
    * `8xy6`/`8xyE` just shift Vx
    * `8xy1`/`8xy2`/`8xy3` don't reset VF
    * `Fx55`/`Fx65` increment I only by x
    * `Fx55`/`Fx65` don't increment I
    * `Bxnn`/`jump0` uses Vx
    * wrap sprite pixels
    * `Dxyn` doesn't wait for vsync
    * 128x64 hires support
    * only 128x64 mode
    * multicolor support
    * xo-chip sound engine
* Presets allow easy selection of quirks sets. Currently, the emulator supports
  the following presets that can be used in any case-insensitive form, also
  with removed dashes, with the commandline option `-p` to preselect the active
  CHIP-8 variant:
    * chip-8
    * chip-10
    * chip-48
    * superchip-1.0
    * superchip-1.1
    * megachip8
    * xo-chip
* Traditional 1400Hz buzzer, MegaChip8 sample playback and XO-CHIP audio emulation
* When multicolor support is active, 16 colors in four planes are supported

## Compiling from Source

Cadmium is written in C++17 and uses CMake as a build solution. To build it,
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

Currently, Cadmium is not being able to compile with MSVC++ so the recommended
toolchain is W64devkit. I might try to make it compile on MSVC++, but I am not
working on that in the near future. That being said, I would not per-se reject
PRs helping with this.

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
  the one supported by Cadmium.
* [Silicon8](https://github.com/Timendus/silicon8) - While experimenting with
  more than 4 colors in XO-Chip mode this emulator helped to compare the results
  and test the very advanced and resource hungry roms its author has written.

### Libraries

* [raylib](https://www.raylib.com) - the fun game programming library that
  inspired me to write this.
* [c-octo assembler](https://github.com/JohnEarnest/c-octo) - the octo
  assembler for support of generating binaries from [Octo assembler syntax](http://johnearnest.github.io/Octo/docs/Manual.html).
* [fmtlib](https://github.com/fmtlib/fmt) - a modern formatting library in
  the style of C++20 std::format
* [date](https://howardhinnant.github.io/date/date.html) - A date and time
  library based on the C++11/14/17 &lt;chrono&gt; header

## FAQ

### Why the name "Cadmium"?
The first time I got into contact with CHIP-8 was with Super-Chip on an
HP-48 calculator during my CS studies. The base of that was the CHIP-48
variant, named after the calculator. I think the reliving of CHIP-8 on
these calculators was one of the things that helped CHIP-8 survive the
time from the seventies until today. Cadmium is an element with the
atomic number of 48 in the periodic table of elements, so that made me
chose it as the name reference for this project.

### Why the pixel look?
Cadmium is about emulating and developing for CHIP-8 and its variants,
a platform that has a resolution of traditionally 64x32 pixel, the
"hires" mode has 128x64 pixels and even MegaChip8 only 256x192 pixels,
so if you hate seeing pixels, this might not be the platform for you. :wink:
Jokes aside, I wanted Cadmium to have an original and recognizable look
and while many emulators use the absolutely great _Dear ImGui_ for the
UI and the fact that a ready-to-use memory editor exists for it helps
a lot, getting something professionally looking done fast. While there
is nothing wrong with that, and [Dorito](https://github.com/lesharris/dorito)
another CHIP-8 IDE that I only came to know after I already had editor and
assembler working, is a really great example of what can be done that way, I
still think there is not much individually recognizable between the look
of these emulators. One can argue that standardization is a good thing,
and I can see that, I just didn't feel like adding one to the list.
So I guess Cadmium is for people that can appreciate this individuality
and raygui, the raylib extension that is behind many widgets of Cadmiun
is the way to get this pixel look.

### Which font is that?
Cadmiums UI uses a handmade font heavily inspired by the 5x8 pixel
font integrated in the [Thomson EF936x](https://en.wikipedia.org/wiki/Thomson_EF936x)
video chip that was used in an extension card of the [NDR-Klein-Computer](https://en.wikipedia.org/wiki/NDR-Klein-Computer),
a DIY computer that could be build following a german educational tv
program in the eighties. The font is build in an ASCII file in the
`tools` folder through a commandline tool and the generated array is
embedded into the `src/cadmium.cpp` source file. The chip originally only
has 95 characters and I created the basic set and filled up the latin-1
codepage characters and some additional unicode code points. I think
its historic source and its look add to the unique look and feel of
the Cadmium UI, but I can agree that it is a matter of taste. :wink:
