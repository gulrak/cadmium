
## v1.0.1 (wip)

* MegaChip8 support (Shoutout to @NinjaWeedle to his help on figuring out details of MegaChip8!)
    * able to run `*.mc8` roms
    * show the additional opcodes in instruction
    * 256 color 256x192 pixel video mode
    * blend modes for sprites (normal, 25%, 50%, 76%, add, multiply)
    * play digisound
    * disassemble MegaChip8 to Octo source with macros prepended the result to allow using 
      MegaChip opcodes
* Dynamic GUI layout (to support other CHIP-8 variant video resolutions)
* _Frame boost_ now disabled when _instructions per frame_ is set to 0 (unlimited)
* Memory view is now scrollable when debugger is paused
* The Editor now supports find and replace with `CTRL+F`/`CTRL+R`
* Various bugfixes in the speculative executing disassembler
* The embedded Octo assembler now allows using planes with values up to 15 to allow up to four planes.
* The config is now stored to allow to remember the emulation settings and the file
  browser location. It is written to a JSON config file placed at:
    * Windows: `%localappdata%\net.gulrak.cadmium\config.json`
    * macOS: `~/Library/Application Support/net.gulrak.cadmium/config.json`
    * Linux: `~/.local/share/net.gulrak.cadmium/config.json`
* Bugfix: `F000 nnnn` (`i := long nnnn`) was not skipped correctly in skip opcodes
* Bugfix: two-word opcodes where messing with disassembly, new implementation is better
  in handling backwards disassembly upwards from the PC
* Bugfix: Dxy0 was not behaving correct in lores on SCHIP

## v1.0.0 (2022-10-03) Initial public release

* Execute CHIP-8, CHIP-10, CHIP-48, SCHIP 1.0, SCHIP 1.1 and XO-CHIP
* Support for buzzer sound or XO-CHIP 1-bit-sample sound patterns
* Editor with Octo syntax highlighting
* Integrated Octo assembler automatically translating code in background
* Any ROM loaded will be available in the editor as decompiled compilable source
* Export either rom file or `.8o` Octo source
* Lots of things I forgot ;-)
