## v1.0.7 (wip)

* Preprocessor Support, this allows to use source files with includes, conditionals and
  sprite import from graphics files compatible to the Octopus syntax
* Editor now has a message panel to better show errors from different (included) files
* Editor shows current assembled size in the status bar
* New CHIP-8 variant 'SuperChip-Compatibility' (SCHIPC) that behaves similar to the
  SuperChip8 with the fixes from Chromatophore, but has a few additional quirks making
  it more in line with what Octo does as SuperChip and what a bunch of OctoJam entries for
  SuperChip expect as behavior
* Added new support for the configurable quirks: "Lores Dxy0 draws 8 pixel width", "Lores Dxy0 draws 16
  pixel width" (it is 0 if none of them is set, default in VIP), and "Dxyn uses SCHIP1.1 collision"
* Default speed of generic CHIP-8 was raised from 9ipf to 15ipf with the implementation of
  a new display wait mechanism that estimates if one or two frames need to be waited to have a more
  realistic timing
* Fix: The `clear` opcode (`00E0`) didn't respect the plane mask in XO-CHIP mode
* Fix: The scrolling was not respecting the plane mask in XO-CHIP mode

## v1.0.6

* Real-Dream support, this is a CHIP-8 mode that runs the CHIPOS CHIP-8 interpreter from
  Michael J Bauer on an emulated M6800 driven 2k RAM equipped DREAM 6800 computer. It is
  is able to execute hybrid roms that use M6800 code mixed with CHIP-8.
* Clipboard now should work on the Emscripten version in the expected way.
* On macOS instead of CTRL, the CMD key is now used for hotkeys and editor control.
* All CHIP-8 cores, the hardware emulating ones and the generic ones, are now based on a
  CPU interface that they also share with the real CPU cores (CDP1802 and M6800) and it
  allowed to move the debug ui out of the main source and handle all cores, backend or
  CHIP-8 generically, including debug controls.
* Breakpoints, and stepping now also works for the backend CPUs of the COSMAC VIP and
  the DREAM6800 emulations. The debugger controls are controlling the CPU that is selected
  in the disassembly panel.

## v1.0.3 (wip)

* Real-VIP support, this is a CHIP-8 mode that runs the actual CHIP-8 interpreter from
  Joseph Weisbecker on an emulated CDP1802 driven 4k RAM equipped COSMAC VIP. It is
  able to execute hybrid roms that use CDP1802 code mixed with CHIP-8.
* A new Trace-log view allows to see the last 1024 lines of log output and if "Trace-Log"
  is activated at the settings panel, the CHIP-8 emulation dumps state lines for every
  instruction there. The emulation needs significant more resources in that mode, so it
  is off by default but useful for debugging roms.

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
* In non-web targets `CTRL+S` in the editor saves the current source, if a name is set.
* Various bugfixes in the speculative executing disassembler
* The embedded Octo assembler now allows using planes with values up to 15 to allow up to four planes.
* The config is now stored to allow to remember the emulation settings and the file
  browser location. It is written to a JSON config file placed at:
    * Windows: `%localappdata%\net.gulrak.cadmium\config.json`
    * macOS: `~/Library/Application Support/net.gulrak.cadmium/config.json`
    * Linux: `~/.local/share/net.gulrak.cadmium/config.json`
* The option `-r`/`--run` is now starting a rom/source that is given as positional
  parameter
* Breakpoint-Support allows now to set/remove breakpoints by clicking into the
  instructions panel of the debugger, and also the Octo `:breakpoint <name>`
  directive allows to set breakpoints.
* Bugfix: Fixed scrolling in non-megachip8 modes after introducing issues through
  mc8 support.
* Bugfix: The XO-CHIP sound emulation was not frame driven but buffer driven, leading
  to it missing some faster updates, it now operates frame driven. 
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
