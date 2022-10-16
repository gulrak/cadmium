
## v1.0.1 (wip)

* MegaChip support (Shoutout to @NinjaWeedle to his help on figuring out details of MegaChip!)
    * Able to run `*.mc8` roms
    * show the additional opcodes in instruction
    * 256 color 256x192 pixel video mode
    * play digisound
    * disassemble MegaChip to Octo source with prefixed macros to allow MegaChip opcodes
* Dynamic GUI layout (to support other CHIP-8 variant video resolutions)
* _Frame boost_ now disabled when _instructions per frame_ is set to 0 (unlimited)
* Memory view is now scrollable when debugger is paused
* Various bugfixes in the speculative executing disassembler
* Bugfix: `F000 nnnn` (`i := long nnnn`) was not skipped correctly in skip opcodes

## v1.0.0 (2022-10-03) Initial public release

* Execute CHIP-8, CHIP-10, CHIP-48, SCHIP 1.0, SCHIP 1.1 and XO-CHIP
* Support for buzzer sound or XO-CHIP 1-bit-sample sound patterns
* Editor with Octo syntax highlighting
* Integrated Octo assembler automatically translating code in background
* Any ROM loaded will be available in the editor as decompiled compilable source
* Export either rom file or `.8o` Octo source
* Lots of things I forgot ;-)