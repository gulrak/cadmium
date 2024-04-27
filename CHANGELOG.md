# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.1.9-1.1.11] (wip)

### Added

- New test for CHIP-8E in `test-roms`
- New high-level variant `CHIP-8E`, the emulator will by default still use the real `VIP-CHIP-8E`
  due to more accurate timing, but the generic one will take up less resources for more constrained
  platforms
- Programs that are loaded from source are now also checked for their assembled binary being known
  and options set accordingly
- COSMAC VIP now supports up to 32k RAM, the interpreters will still put their screen data at
  the end of the actual memory, theoretically expanding the CHIP-8 space (even if the max. 12
  bit operands limit usability quite a bit)
- New non-CHIP-8 mode: COSMAC VIP now supports running native VIP programs
- Memory panel now has a checkbox to detach it from auto-following the `I` register

### Changed

- Changed hardly visible memory change highlight color on darker columns
- When running the program and the view is on settings or editor, the view will now switch to the
  last used run-view (those are full video or debugger)
- When loading source that has a compile error, the editor is shown

### Fixed

- The VIP-CHIP-8E interpreter had a typo leading to `BBnn` not working ([#12](https://github.com/gulrak/cadmium/issues/12))
- Disassembling 0x7C, 0x7D or 0x7F generated single byte opcodes instead two byte ones ([#13](https://github.com/gulrak/cadmium/issues/12))
- Octo-Assembler would hang on macro definitions without name or parameter
- High-level emulation didn't finish a frame with the right amount of cycles if part of it was
  single stepped or breakpoints involved
- Wrong initial cycles after a reset for the COSMAC VIP and the CHIP-8 strict timing core, the
  time in ROM was not counted in
- COSMAC VIP could make Cadmium hang on opcode 0x68
- Step-Over handling in hardware based emulation was not working correctly
- Memory panel modification highlight was messed up
- Memory panel `I` following was delayed by an instruction cycle when stepping
- DREAM6800 was wrongly buzzing all the time, correctly controlled by PB6 now
- DREAM6800 had not finished the last frame when auto-pausing on self-loop, potentially leaving
  change undisplayed
- Due to an change in the gui library the toggle buttons in the find bar of the editor didn't work
- Loading of Octo source that didn't compile without error was not possible

## [1.1.8] - 2024-01-01

### Added

- MegaChip emulation now supports wrapping quirk and Mega8 scroll blending
- Support for non-60 fps variants (CHIP-48 and SCHIP1.x are now running at 64fps,
  Dream6800 is now running at 50fps)
- Web version now can load programs from new `url` parameter but the source needs
  to support the CORS mechanism (e.g. `raw.githubusercontent.com`) and the url
  needs to be url-encoded, for programs it knows am url for, the SHA1 number works too
- New variant `Modern-SuperChip` that is in line with Octos interpretation of SCHIP,
  and the one from the CHIP-8 test suite v4.1, the SCHIPC is now moreoriented on the
  behavior of Chromatophores SCHIPC
- Added support for CHIP-8E on the VIP core
- Realistic buzzer sound from HP48 based variants (CHIP-48, SCHIP1.x, SCHIPC)

### Changed

- Cadmium editor allows multi megabyte sources
- Embedded Chiplet assembler now supports MegaChip with up to 16MB address space
- Reworked audio rendering for less risk of artifacts
- Backend updated to stock raylib 5.0
- Lots of small fixes I missed to note

### Fixed

- SCHIP1.x/SCHIPC now have display wait on lores as does the original SCHIP on the
  HP-48 calculators
- SCHIP Dxy0 didn't draw 16x16 in hires due to some refactoring errors
- CHIP-10 did accidentally draw on Dxy0
- The new vblank system caused sounds to be one timer interval shorter than expected


## [1.0.9] - web preview only

* Cadmium now as an embedded list of known programs that it detects by SHA1 hash and
  automatically configures platform, quirks and colors for, if any of that is known;
  This data is based on the combined efforts of John Earnest, Tobias Langhoff,
  Tim Franssen and other users of the _Emulation Development Discord_
* The settings dialog now has an option to remember program specific settings for
  a loaded program, these take precedence over settings from the known meta data
  (not available in the web version)
* The desktop builds of Cadmium now show a preview of the program when selecting
  files while browsing, by executing the emulation for them a brief moment and
  capturing a screenshot
* Support for "Two page display CHIP-8" in the VIP emulation, based on the TDP interpreter
  version from VIPER vol. 1, issue 3
* All supported quirks can now be configured via the commandline
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

## [1.0.6] - 2923-02-17

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

## [1.0.3] - web preview only

* Real-VIP support, this is a CHIP-8 mode that runs the actual CHIP-8 interpreter from
  Joseph Weisbecker on an emulated CDP1802 driven 4k RAM equipped COSMAC VIP. It is
  able to execute hybrid roms that use CDP1802 code mixed with CHIP-8.
* A new Trace-log view allows to see the last 1024 lines of log output and if "Trace-Log"
  is activated at the settings panel, the CHIP-8 emulation dumps state lines for every
  instruction there. The emulation needs significant more resources in that mode, so it
  is off by default but useful for debugging roms.

## [1.0.1] - web preview only

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

## [1.0.0] - 2022-10-03 - Initial public release

* Execute CHIP-8, CHIP-10, CHIP-48, SCHIP 1.0, SCHIP 1.1 and XO-CHIP
* Support for buzzer sound or XO-CHIP 1-bit-sample sound patterns
* Editor with Octo syntax highlighting
* Integrated Octo assembler automatically translating code in background
* Any ROM loaded will be available in the editor as decompiled compilable source
* Export either rom file or `.8o` Octo source
* Lots of things I forgot ;-)
