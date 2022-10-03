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

![Cadmium debug view](screenshots/screenshot01.png?raw=true "A screenshot of the debug view")

An Emscripten build is available here for testing: https://games.gulrak.net/cadmium -
simply drag rom files (`.ch8`, `.sc8`, `.xo8`) or Octo sources (`.8o`) onto the
window to load them.

## Features

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
    * `Dxyn` dosn't wait for vsync
    * 128x64 hires support
    * only 128x64 mode
    * multicolor support
    * xo-chip sound engine
* Presets allow easy selection of quirks sets. Currently, the emulator supports
  the following presets:
    * chip-8
    * chip-10
    * chip-48
    * superchip-1.0
    * superchip-1.1
    * xo-chip
* Traditional 1400Hz buzzer or XO-CHIP audio emulation
* When multicolor support is active, 16 colors in four planes are supported

### Compiling from Source

Cadmium is written in C++17 and uses CMake as a build solution. To build it,
checkout or download the source.

#### Linux / macOS

Open a terminal, enter the directory where the code was extracted and run:

```
cmake -S . -B build
```

to configure the project, and

```
cmake --build build
```

to compile it.

#### Windows

Currently Cadmium is not been able to compile with MSVC++ so the recommended
toolchain is W64devkit.

```
export PATH="$PATH;C:/Program Files/CMake/bin"
cmake -G"Unix Makefiles" -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DGRAPHICS=GRAPHICS_API_OPENGL_21 -S . -B build-w64dev
```

#### Build for Web

```
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<path-to-emsdk>/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake
cmake --build build
```

### Used Resources

* [raylib](https://www.raylib.com) - the fun game programming library that
  inspired me to write this.
* [c-octo assembler](https://github.com/JohnEarnest/c-octo) - the octo
  assembler for support of generating binaries from [octo assembler syntax](http://johnearnest.github.io/Octo/docs/Manual.html).
* [fmtlib](https://github.com/fmtlib/fmt) - a modern formatting library in
  the style of C++20 std::format
* [date](https://howardhinnant.github.io/date/date.html) - A date and time library based on the C++11/14/17 &lt;chrono&gt; header

