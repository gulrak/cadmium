# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- The source editor saves sources that were loaded from a file automatically (native builds): a second
  after the last change, when Cadmium loses the focus, when another view is shown and before another
  program is loaded. The undo history stays, closing a file makes its edits final. Disassemblies, new
  sources and cartridge sources still need a first save.
- Local history: the version a file had when it was opened is copied into the `history` folder of the
  configuration directory before its first save (10 versions per file, 30 days), the editor's context
  menu can revert to it
- Files changed by another program are reloaded as an undoable edit, with own unsaved changes the editor
  shows the conflict and offers *Reload from Disk* or saving over it
- The editor has a context menu (right-click, `Shift+F10` or the menu key) with the edit actions, find
  and replace, opening an included file, reload and revert
- Several files can be open in the editor: `Cmd/Ctrl+click` (or the context menu) on the file name of an
  `:include` line opens an included source, the panel title lists the open files, `Ctrl+Tab` switches to
  the previous one, `Cmd/Ctrl+W` closes the current one. All open files are saved before the main file is
  compiled, a change of an included file on disk compiles again, and a click on an error location opens
  the file it is in.
- Project folders: *Open Folder...* and *Open recent folder* in the main menu, Cadmium remembers the main
  file of each folder; loading a program from the project keeps its open files
- Projects with several programs (like a test suite): a shown project file that defines `: main` becomes
  the main file; opening a folder finds its main file also among several sources (the one no other
  includes, or named like the folder)
- Back and Forward in the editor (`Cmd+Opt+Left/Right` on macOS, `Alt+Left/Right` on Windows and Linux,
  the mouse side buttons, the panel title menu) return to the places left when another file was shown
- A file tree over the editor (`Cmd/Ctrl+1`) that marks the main file, the open files and the files of
  the build, with a context menu to open files, set the main file and create new files
- Quick open (`Cmd/Ctrl+P`) finds a file of the project by some letters of its name
- The web version keeps editing the one source of the loaded program, the file features above are for
  the native builds
- Window GIF recordings show the pressed keyboard shortcuts (with `Cmd`/`Ctrl`/`Alt`, or `Esc`, `F1`-`F12`)
  as a caption over the status bar, repetitions count up; a checkbox in the Misc settings turns it off

### Changed

- The editor is now the reusable `CodeEditor` of rlguipp (with a `FileTree` and a drawer popup), it edits
  like the text fields: `Delete` key, word movement and deletion, double click selects a word, triple click
  a line, `Shift`+click extends the selection, `Cmd/Ctrl` jumps to line and text ends
- Typing is undone word by word (and after a pause of a second) instead of whole runs of typing
- *Rep.all* in the replace bar is one undo step, the replace row lost its arrow button that had no function
- Requires Chiplet with `CompileResult::includedFiles` (newer than v2.0.0)

### Fixed

- A compile error in an included file showed the location of the `:include` line in the main file instead
  of the error
- The editor text and cursor were placed with a pixel offset
- The keyboard shortcut table in the README listed wrong keys for *New* and *Paste*
- A source whose program doesn't fit into the selected core (a MegaChip program with a CHIP-8 core e.g.)
  wasn't loaded into the editor at all, now it is, the editor says it doesn't fit, and it is loaded once
  another core is chosen

## [2.0.0] - 2026-11-10

### Added

- New test for CHIP-8E in `test-roms`
- New high-level variant `CHIP-8E`, the emulator will by default still use the real `VIP-CHIP-8E`
  due to more accurate timing, but the generic one will take up less resources for more constrained
  platforms
- New high-level variants `CHIP-8 TPD` (64x64) and `CHIP-8 FPD` (64x128), files with their extensions
  still default to the real `VIP-CHIP-8 TPD` and `VIP-CHIP-8 FPD`
- Programs that are loaded from source are now also checked for their assembled binary being known
  and options set accordingly
- COSMAC VIP now supports up to 32k RAM, the interpreters will still put their screen data at
  the end of the actual memory, theoretically expanding the CHIP-8 space (even if the max. 12
  bit operands limit usability quite a bit)
- New non-CHIP-8 mode: COSMAC VIP now supports running native VIP programs
- Memory panel now has a checkbox to detach it from auto-following the `I` register
- VIP settings allow now to individually select if video events should be added to the trace log
- Support for more fonts in generic CHIP-8 variants
- New debugger breakpoint tab under the video panel allowing configuration
- Breakpoints now can be disabled/enabled, first click in disassembly creates one, further clicks
  either enable or disable it, to delete a breakpoint a SHIFT click is now used.
- Breakpoints allow a condition, it can be an expression using any of the registers or access ram
  as if it is an array, e.g. `v0==23 && ram[0x234]>3` and only if that expression evaluates to
  a non-zero value the breakpoint stops, no condition makes it always stop
- Breakpoints now can have a log format entry (for `:breakpoint` directives it is the string parameter)
  that will be appended to to log window if the breakpoint triggers, an additional log condition
  can be set to make the log print only if that condition is met, independent of the breakpoint condition.
- Library/Research screen and database integration allows for easier research on existing roms
  and interpreters, all information about programs, and even the binaries themselves (not in web version)
  are now kept in a local SQLite3 file in the configuration directory (<10MB for all known programs combined)

#### Library
- A new library starts from built-in starter metadata with about 900 programs and 1100 ROMs,
  ROMs that are only known from that metadata are "missing" and hidden by default (eye button)
- The library search uses a small query language: plain words search name, description and
  authors, fields like `release < 1980`, `author ~ winter`, `tag = new`, `variant = xo-chip`,
  `ipf > 100`, `wrap = true`, `size < 1k`, partial dates and ranges (`197x`, `1990..1999`), the
  keywords `missing`, `present` and `analyzed`, and opcode search like `uses F?75`, combined with
  `and`, `or`, `not`/`-` and parentheses, a feedback line shows the number of matches or the error
- Tags filter the list (Ctrl/Cmd click for several), smart tags save a selection and search as
  a named query
- Programs can be edited (name, authors, release date, origin with reference, license as SPDX
  expression, copyright, URL, description), each ROM can have several configs (variant plus
  property changes) that can be loaded or run directly, ROMs can have their own tags, a ROM
  tagged "bad" is only used as default if nothing else is left
- ROMs can be moved between programs (also by dragging), detached, merged, removed or removed
  and ignored so future scans don't add them again, dropping files on the library adds them
- The library shows a live preview of the selected ROM and a scrollable info panel
- "Save settings to library" in the main menu stores the current settings (including the palette
  preset) as a config of the loaded ROM
- In the web version, the library works on the starter metadata, opening a ROM selects its entry,
  changes are lost on reload, library folders, scanning, the live preview and "Save settings to
  library" stay desktop only
- New command line options `--export-metadata-db` (library without ROMs and local data, for
  sharing), `--update-program-database` (compare with the chip-8-database `programs.json`) and
  `--opcode-stats`
- Searches can be started directly: `--search "uses 5xy2 or uses 5xy3"` opens the library with
  that search, also listing missing ROMs, and with `--list` the ROMs found are printed instead of
  opening a window. The web version takes it as URL parameter (`?search=uses+5xy2+or+uses+5xy3`),
  and a button next to the search field copies a link to the current search for sharing, the
  desktop version gives a link to the web version
- Generic cores have a new "Rotation" option (`--screen-rotation 0/90/180/270`) for programs
  played sideways, taken from the library, the chip-8-database and Octo cartridges
- [Desktop only] Cadmium now remembers window position and scaling and starts like it was closed
- Cadmium now has a `--version` command line option and shows the git commit hash in the about dialog

<!-- New systems and cores -->
- New system: HP-48SX hardware emulation, running the original CHIP-48, SUPER-CHIP 1.0 beta,
  SUPER-CHIP 1.0, SUPER-CHIP 1.1 and Chromatophore's SCHPC interpreters on an emulated HP-48SX
  (Saturn CPU), presets `hp-48-chip-48`, `hp-48-schip-1-0-beta`, `hp-48-schip-1-0`,
  `hp-48-schip-1-1`, `hp-48-schpc` and a raw
  `hp-48-none` mode. The interpreters are assembled from source, SUPER-CHIP 1.0 beta,
  SUPER-CHIP 1.1 and SCHPC are source reconstructions from the binaries, as the originals are lost
- The HP-48SX core comes with Cadmium's own clean-room replacement ROM, so it is available in
  every build, including the web version. It only runs the five interpreters (no calculator,
  so `hp-48-none` needs HP's revision E ROM) and shows a simple stack display while idle. Builds
  with HP's ROM compiled in can choose the ROM (`--rom`), saved states remember which ROM they
  belong to.
- New core: `strict-schip-1-1`, a cycle exact high-level emulation of SUPER-CHIP 1.1 as it runs
  on the HP-48SX, with optional HP-48SX display emulation, based on the measured interpreter
  timing.
- New system: RCA Studio II hardware emulation with the preset `studio-2-builtin`, running
  its built-in games (pad A on keys 1-9 and 0), this is still more of a preview mode
- HP-48SX display contrast is emulated from the calculator's contrast register, and as the
  calculator settings can't be reached without its keyboard, a "Fixed contrast" option
  (`--fixed-contrast`, `--contrast 0-31`) allows setting it directly, even while running
- New system: ETI-660 hardware emulation (CDP1802 with CDP1864 video and sound) with the
  presets `eti-chip-8` (64x48 CHIP-8) and the raw `eti-none` mode, selectable keyboard layouts
  (ETI-660 hex, ETI-660 two row, VIP hex)
- New core: `rs-megachip`, a faithful reimplementation of the original RS-M8001 MegaChip
  interpreter by Revival-Studios (2007), based on an analysis of the original binary,
  including its original bugs (but with checked memory bounds), with an optional
  "VIP mode" option for the original mode-1 behavior including double shifts
- New generic variant `SCHIP-1.0-BETA` (`schip-1-0-beta`, `.sc10b`), the SUPER-CHIP 1.0 beta
  with its preview behavior of `Fx29`
- New COSMAC VIP presets: `vip-chip-10` (CHIP-10 with the 128x64 hires hardware modification),
  `vip-chip-8-rb` (CHIP-8 with relative branching `BFnn`/`FBnn` by Wayne Smith, VIPER V2-I1),
  `vip-chip-8-tpd-ts` (Tom Swan's two page display version from "PIPS for VIPS"),
  `vip-chip-8-r` (CHIP-8R, a research variant with PC relative jumps `BBnn`/`BFnn`, calls through
  `I` with `BC00` and 16 bit `I` loads `F000 nnnn` for programs beyond 4k, also supported by
  Chiplet)
- New DREAM6800 raw mode `dream-none` to run native M6800 programs without CHIPOS, the DREAM6800
  can now be configured with 2k or 4k RAM
- Every hardware core (COSMAC VIP, strict CHIP-8, DREAM6800, ETI-660) now allows configuring the
  CPU clock rate

#### Emulation
- New generic quirks: "HP-48SX display emulation" (renders the generic SCHIP output like on the
  HP-48SX LCD), "HP SuperChip 1.0 'preview' Fx29", "CubeChip contextual tone" and "Clean RAM"
  (otherwise RAM starts with a pseudo random pattern, for SCHIP variants with a sparse pattern
  similar to a used HP-48)
- New generic options for RAM size (2k up to 16MB), load/start address and the 5px and 10px fonts
- COSMAC VIP monitor tape output (Q line) is now written as a `tape<timestamp>.wav` file when
  saving to tape from the VIP monitor
- COSMAC VIP: holding key `C` during reset boots into the monitor instead of the CHIP-8 interpreter,
  like on the real hardware
- Warm reset: a right click on the restart button resets the emulated system without reloading
  the program and without clearing the RAM, a left click still reloads and restarts the program
- Intel HEX files (`.hex`) can be loaded, the data is put to the addresses given in the file
- Octo cartridges (`.gif`) can be loaded via drag and drop, including their options, palette (all
  four planes, border and sound colors), font, screen rotation and touch mode
- Programs can be saved as Octo cartridge (`.gif`) from the save dialog, with a label, Octo's
  original or Cadmium's image and optionally as binary only (hex dump instead of source), options
  and palette are stored too, available for generic CHIP-8, CHIP-48, SCHIP and XO-CHIP presets
- New CRT effect option (`--crt-effect`, "CRT Effect") that shows the screen like a CRT with
  visible scan lines, for the COSMAC VIP, DREAM6800, ETI-660, RCA Studio II and the generic
  MegaChip preset
- New command line option `--custom-palette "#rrggbb,#rrggbb,..."` to set the palette colors
- New test ROMs in `test-roms`: a variant detection test that identifies which CHIP-8 variant
  it is running on, an XO-CHIP sound test, a font test, a DREAM6800 key test and the
  Chromatophore HP-48 draw test
- New test ROMs `superchip-test` (detects the SCHIP variant and tests the opcodes including the
  HP-48SX lores drawing details), `xochip-test` (XO-CHIP opcodes, also reports the number of
  planes) and `vipchip8-test` (classic CHIP-8 as on the VIP, including the display wait), all
  ending with a sound test

#### Debugger / UI
- Debugger memory view now shows 16 bytes per line and the disassembly is syntax highlighted
- The debugger supports the HP-48SX Saturn CPU: register panel with its 64 bit registers and a
  stable, syntax highlighted disassembly view of its variable length instructions
- Text fields now support selection, clipboard, undo/redo, word wise movement and double/triple
  click, multi-line fields wrap and scroll
- Disassembly scrolls by whole lines with smooth trackpad scrolling
- Breakpoint list shows the triggered breakpoint and the hit count of a breakpoint
- Octo `:breakpoint` directives with a valid expression as name now act as conditional breakpoints,
  `:breakpoint "v0 == 3 ? hit at i={i:04x}"` sets the condition and the log format separately, and
  a plain name still always breaks
- The log format and log condition of a breakpoint can be edited in the breakpoint tab, without a
  log condition a breakpoint logs whenever it breaks
- Breakpoint expressions support `!=`, `%`, `0x` hex and `0b` binary numbers, and the labels and
  constants of the loaded Octo source (`ram[data+1]`, `v0 == LIMIT`)
- Log formats can read memory like Octo monitor fields, `{data:%2i}` shows two bytes at `data` as a
  number, `{i:%4x}` four bytes at `I` in hex, besides the fmt style `{v0:02x}`
- New debugger Watch tab: the Octo `:monitor` entries of the source, shown like Octo does and
  updated while the program runs, and watches added in the tab as expressions or log formats;
  `:monitor` formats can use the `{...}` placeholders of log formats as a Cadmium extension
- Breakpoints now stay in sync across the emulation instances and survive resets
- New main menu with "Open recent" files (and "Clear recent files") and shown hotkeys
- Keyboard shortcuts: `Ctrl`/`Cmd` + `N`/`O`/`S`/`K`/`Q` for New, Open, Save, Key Map and Quit,
  `F5` run, `Shift+F5` pause, `F7` step into, `Shift+F7` step out, `F8` step over
- The game screen or the whole window can be recorded as an animated GIF from the main menu or with
  `Ctrl`/`Cmd` + `G` and `Ctrl`/`Cmd` + `Shift` + `G`, the file goes to the recording directory from
  the Misc settings, without one next to the current programs, and is downloaded in the web version,
  window recordings also show the mouse pointer and its clicks
- Toolbar buttons for opening the library, opening and saving a program
- Saving lets you choose between saving the ROM file or the source code and asks before
  overwriting an existing file
- Palette settings now also allow configuring background, border and signal colors, new palette
  presets "C64", "Intellivision" and "CGA"
- The Appearance settings tab allows tinting the UI colors and changing their saturation
- Settings show the emulated hardware configuration of a core (e.g. ROM and interpreter with
  their SHA1)
- The library view shows the SHA1 and the (estimated) type of a file
- About 390 more known ROMs (now about 1100, v1.1.8 had about 710), among them new ETI-660
  programs, and better palettes for known programs

#### Command line
- New headless mode: `--headless` runs a binary ROM deterministically without a window, with
  `--until-presentations`, `--max-instructions`, `--dump-screen` (export last frame as image) and
  `--dump-state` (export the machine state as JSON)
- All core options are now available on the command line, generated per core, with
  `--[no-]<option>` style switches (e.g. `--[no-]schip-11-collision`, `--clock-rate`, `--memory`,
  `--instructions-per-frame`, `--frame-rate`, `--trace-file`)

#### Web
- Web version: saving/downloading ROMs and sources, better clipboard support and the canvas now
  follows the browser window size and pixel ratio
- Web version: the library (see above) and the HP-48SX core (with the replacement ROM) are
  available, the page no longer busy-waits between frames, "unlimited" instructions per frame runs
  at 1000000 instructions per frame there, and a new deploy is no longer hidden by cached files

### Changed

- Enhanced memory compatibility in strict CHIP-8 core to have the same ram state as CHIP-8 on a VIP
- Changed hardly visible memory change highlight color on darker columns
- When running the program and the view is on settings or editor, the view will now switch to the
  last used run-view (those are full video or debugger)
- When loading source that has a compile error, the editor is shown.
- Due to the overhead of scissor mode for scroll panels, GuiDrawText and GuiDrawRectangle where
  changed to use new clipping helpers that clip rectangles and glyph drawing and make scissor
  mode unnecessary.

- Settings were reworked: each emulation core has its own set of options, presets are grouped
  by core (generic HLE, strict, RS-M8001, COSMAC VIP, HP-48SX, DREAM6800, ETI-660) and the
  options of hardware cores are no longer limited to CHIP-8 quirks
- Command line quirk options have been renamed to the per core option names
  (e.g. `--lores-dxy0-width-8` is now `--lores-dxy-0-is-8-x-16`), `-s/--exec-speed` is replaced by
  `--instructions-per-frame`, `--trace-log` is now per core and `--random-gen`/`--random-seed`
  were removed
- Preset names are now lowercase and dash separated (`schip-1-1`, `vip-chip-8-x`, ...), see
  `--help` or the README for the full list
- Changed preset defaults:
  - `SCHIPC` now behaves closer to Chromatophore's SCHPC: it waits for vblank on `Dxyn`, lores
    `Dxy0` draws 8 pixels wide, it uses half pixel scrolling and the HP-48SX lores drawing and
    it runs at 64 fps
  - `SCHIP-1.0` now runs at 30 instead of 15 instructions per frame
  - `SCHIP-MODERN` now runs at 64 fps
  - `MEGACHIP` now runs at 50 fps with 5000 instructions per frame and waits for vblank on `Dxyn`
  - `CHIP-8`, `CHIP-10` and `CHIP-8E` use the extended display wait emulation by default
  - The extended display wait now depends on the time left in the frame instead of only on the sprite size
    and position, this is much closer to the draw rates of the real interpreters
- Generic SUPER-CHIP 1.x presets (all but `SCHIP-MODERN`) now only save/load eight flag
  registers with `Fx75`/`Fx85` like the HP-48SX interpreters (`x` is clamped to 7)
- The HP-48SX lores drawing of the generic SCHIP presets now matches the interpreters: a lores
  row is drawn on the even hires row and copied to the odd row below, collisions only look at
  the even row
- Generic cores start with a pseudo random RAM content instead of zeroed RAM unless "Clean RAM"
  is set
- Generic cores now emulate the short key feedback beep while `Fx0A` waits for a key
- CHIP-8X based VIP presets now run with the 1.789773 MHz clock that comes with the VP-590
  color board
- COSMAC VIP buzzer sound is now a more realistic enveloped frequency changing sound instead
  of a plain square wave
- Editor syntax highlighting now handles constructs spanning multiple lines
- Backend updated to raylib 6.0
- [Desktop only] macOS builds now need macOS 13.3 or newer

### Fixed

- FPS calculation got confused by pausing or stepping (thanks @JanitorRaus for reporting)
- Memory view "Follow" showed wrong position
- Disassembly sometimes only showed hex bytes instead of opcodes
- The VIP-CHIP-8E interpreter had a typo leading to `BBnn` not working ([#12](https://github.com/gulrak/cadmium/issues/12))
- Disassembling 0x7C, 0x7D or 0x7F generated single byte opcodes instead two byte ones ([#13](https://github.com/gulrak/cadmium/issues/12))
- Octo-Assembler would hang on macro definitions without name or parameter
- High-level emulation didn't finish a frame with the right amount of cycles if part of it was
  single stepped or breakpoints involved
- Added wrapping behavior for ram and rom, including the unconnected second 2k of a 2k
  VIP in strict CHIP-8 and VIP emulation
- Wrong initial cycles after a reset for the COSMAC VIP and the CHIP-8 strict timing core, the
  time in ROM was not counted in
- COSMAC VIP could make Cadmium hang on opcode 0x68
- Step-Over handling in hardware based emulation was not working correctly
- Memory panel modification highlight was messed up
- Memory panel `I` following was delayed by an instruction cycle when stepping
- DREAM6800 was wrongly buzzing all the time, correctly controlled by PB6 now
- DREAM6800 had not finished the last frame when auto-pausing on self-loop, potentially leaving
  change undisplayed
- Due to a change in the gui library the toggle buttons in the find bar of the editor didn't work
- Loading of Octo source that didn't compile without error was not possible
- `.c8b` files (CHIP-8 binary format) were loaded as raw data instead of their bytecode, they load
  again with their palette and speed, and a speed above 65535 instructions per frame is no longer cut
- MegaChip audio suppressed the last sample of a non looping sound
- VIP finally shows Q in the trace log
- SUPER-CHIP v1.1 collision handling was broken
- Syzygy was wrongly classified as CHIP-48, it's author declares it as CHIP-48 and it is working
  with CHIP-48 or SCHIP, switched to CHIP-48 (thanks to `@juan_tigre` for the find).

- The DREAM6800 CHIPOSLO kernel had a bug in `Ex9E` key handling, fixed with a size neutral
  patch ([tobiasvl/chiposlo#3](https://github.com/tobiasvl/chiposlo/issues/3))
- Stepping or breakpoints could trigger while the generic core was waiting for vblank or a key
- Keyboard shortcuts with `Ctrl`/`Cmd` no longer also trigger the CHIP-8 key mapped to that key
- The big digit `6` of the SUPER-CHIP 1.0 font had a wrong pixel in its third row
- Horsey Jump was wrongly classified as `SCHIP-1.1`, now `SCHIP-MODERN`, Raycaster got a better
  instructions per frame setting

## [1.1.8] - 2024-01-01

### Added

- MegaChip emulation now supports wrapping quirk and Mega8 scroll blending
- Support for non-60 fps variants (CHIP-48 and SCHIP1.x are now running at 64fps,
  Dream6800 is now running at 50fps)
- Web version now can load programs from new `url` parameter but the source needs
  to support the CORS mechanism (e.g. `raw.githubusercontent.com`) and the url
  needs to be url-encoded, for programs it knows am url for, the SHA1 number works too
- New variant `Modern-SuperChip` that is in line with Octos interpretation of SCHIP,
  and the one from the CHIP-8 test suite v4.1, the SCHIPC is now more oriented on the
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

## [1.0.6] - 2023-02-17

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
