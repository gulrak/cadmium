#!/usr/bin/env python3
"""Generate the preset family tree SVG used in the README.

The tree is defined in TREE and BARE_MACHINES below. Each Variant is a CHIP-8
variant with the year it appeared, the presets implementing it as high level
emulation (hle) or as the original interpreter on emulated hardware (hw), and
the variants derived from it (children). Edit those and run:

    python3 tools/gen_preset_tree.py

This writes media/cadmium_preset_tree.svg (use -o to write elsewhere). With
--cadmium <binary> the presets of the tree are compared to the ones listed by
"<binary> --help", so a new or renamed preset is not forgotten.

All text of the SVG sits on opaque mid-tone boxes, so it is readable on GitHub's
light and dark theme alike. Chip widths are estimated from the name length, as
the SVG uses the system font stack and has no text measuring.
"""

import argparse
import re
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path


@dataclass
class Variant:
    name: str
    year: str = ""
    hle: list = field(default_factory=list)
    hw: list = field(default_factory=list)
    children: list = field(default_factory=list)


# --- The family tree, edit here --------------------------------------------

TREE = Variant("CHIP-8", "1977", hle=["chip-8", "strict-chip-8"], hw=["vip-chip-8"], children=[
    Variant("CHIP-10", "1979", hle=["chip-10"], hw=["vip-chip-10"]),
    Variant("CHIP-8 TPD", "1979", hle=["chip-8-tpd"], hw=["vip-chip-8-tpd"], children=[
        Variant("TPD (Tom Swan)", "1979", hw=["vip-chip-8-tpd-ts"]),
    ]),
    Variant("CHIP-8 FPD", "1980", hle=["chip-8-fpd"], hw=["vip-chip-8-fpd"]),
    Variant("CHIP-8 RB", "1979", hw=["vip-chip-8-rb"]),
    Variant("CHIP-8E", "1979", hle=["chip-8-e"], hw=["vip-chip-8-e"]),
    Variant("CHIP-8X", "1980", hle=["chip-8-x"], hw=["vip-chip-8-x"], children=[
        Variant("CHIP-8X TPD", "1982", hw=["vip-chip-8-x-tpd"]),
        Variant("CHIP-8X FPD", "1982", hw=["vip-chip-8-x-fpd"]),
    ]),
    Variant("CHIP-8-R", "2026", hw=["vip-chip-8-r"]),
    Variant("CHIPOS", "1979", hw=["dream-chip-8"], children=[
        Variant("CHIPOSLO", "2020", hw=["dream-chip-8-lop"]),
    ]),
    Variant("ETI-660 CHIP-8", "1982", hw=["eti-chip-8"]),
    Variant("CHIP-48", "1990", hle=["chip-48"], hw=["hp-48-chip-48"], children=[
        Variant("SCHIP 1.0 beta", "1991", hle=["schip-1-0-beta"],
                hw=["hp-48-schip-1-0-beta"], children=[
            Variant("SCHIP 1.0", "1991", hle=["schip-1-0"], hw=["hp-48-schip-1-0"], children=[
                Variant("SCHIP 1.1", "1991", hle=["schip-1-1", "strict-schip-1-1"],
                        hw=["hp-48-schip-1-1"], children=[
                    Variant("SCHIPC", "2017", hle=["schipc"], hw=["hp-48-schpc"]),
                    Variant("MegaChip", "2007", hle=["megachip", "rs-megachip"]),
                    Variant("Octo SCHIP", "2014", hle=["schip-modern"], children=[
                        Variant("XO-CHIP", "2014", hle=["xo-chip"]),
                    ]),
                ]),
            ]),
        ]),
    ]),
])

# Presets without a CHIP-8 interpreter, shown below the tree (all hardware).
BARE_MACHINES = ["vip-none", "hp-48-none", "dream-none", "eti-none", "studio-2-builtin"]

# --- Layout ------------------------------------------------------------------

WIDTH = 680
LEFT = 20
RIGHT = 660
INDENT = 18            # horizontal step per tree level
ROW_HEIGHT = 30        # distance between variant rows
CHIP_LINE_HEIGHT = 26  # extra height when a row needs more than one chip line
LABEL_RIGHT = 266      # right end of the variant boxes
CHIPS_X = 290          # column where the preset chips start
CHIPS_MAX_X = 660      # chips wrap to a new line beyond this
CHIP_HEIGHT = 20
LABEL_HEIGHT = 22
CHIP_GAP = 6
LABEL_CHAR_WIDTH = 7.8   # estimated width per char of a 13px bold label
CHIP_CHAR_WIDTH = 6.6    # estimated width per char of a 12px chip text
CHIP_PADDING = 18

# All text sits on an opaque box with mid-tone colors, and the lines are a gray
# visible on white and on dark backgrounds. That way one SVG works in light and
# dark mode: GitHub has no reliable way to switch README images with its theme
# (prefers-color-scheme, also in <picture>, follows the OS setting instead).
STYLE = """<style>
text{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Helvetica,Arial,sans-serif;
font-size:12px;fill:#FFFFFF}
.n{font-size:13px;font-weight:600}.y{fill:#D3D1C7}
.l{fill:none;stroke:#888780;stroke-width:1}
.sep{stroke:#888780;stroke-width:0.75;stroke-dasharray:3 3}
.g{fill:#5F5E5A}.h{fill:#534AB7}.v{fill:#0F6E56}
</style>"""

# Chip kind -> (css class, corner radius); hle chips are boxes, hw chips pills,
# so the kinds also differ without color.
CHIP_KINDS = {"hle": ("h", 4), "hw": ("v", 10)}


def chip_width(name):
    return round(len(name) * CHIP_CHAR_WIDTH + CHIP_PADDING)


def chip_lines(chips):
    """Split (name, kind) chips into lines between CHIPS_X and CHIPS_MAX_X."""
    lines = [[]]
    x = CHIPS_X
    for chip in chips:
        width = chip_width(chip[0])
        if lines[-1] and x + width > CHIPS_MAX_X:
            lines.append([])
            x = CHIPS_X
        lines[-1].append(chip)
        x += width + CHIP_GAP
    return lines


class SvgWriter:
    def __init__(self):
        self.out = []

    def chip(self, kind, x, y, text):
        """Draw a chip with its left edge at x and its vertical center at y."""
        css, radius = CHIP_KINDS[kind]
        width = chip_width(text)
        self.out.append(
            f'<rect class="{css}" x="{x}" y="{y - CHIP_HEIGHT // 2}" width="{width}" '
            f'height="{CHIP_HEIGHT}" rx="{radius}"/>'
            f'<text x="{x + width / 2:.0f}" y="{y + 4}" text-anchor="middle">{text}</text>')
        return width

    def chips(self, y, chips):
        lines = chip_lines(chips)
        for line in lines:
            x = CHIPS_X
            for name, kind in line:
                x += self.chip(kind, x, y, name) + CHIP_GAP
            y += CHIP_LINE_HEIGHT
        return len(lines)

    def label(self, x, y, name, year=""):
        """Draw a gray variant box from x to LABEL_RIGHT, name left and year right."""
        if x + 8 + len(name) * LABEL_CHAR_WIDTH > LABEL_RIGHT - (44 if year else 8):
            raise SystemExit(f"error: label '{name}' is too long for its box")
        self.out.append(
            f'<rect class="g" x="{x}" y="{y - LABEL_HEIGHT // 2}" width="{LABEL_RIGHT - x}" '
            f'height="{LABEL_HEIGHT}" rx="4"/>'
            f'<text class="n" x="{x + 8}" y="{y + 5}">{name}</text>')
        if year:
            self.out.append(f'<text class="y" x="{LABEL_RIGHT - 8}" y="{y + 4}" '
                            f'text-anchor="end">{year}</text>')

    def separator(self, y):
        self.out.append(f'<line class="sep" x1="{LEFT}" y1="{y}" x2="{RIGHT}" y2="{y}"/>')

    def legend(self):
        width = self.chip("hle", LEFT, 18, "High level emulation")
        self.chip("hw", LEFT + width + CHIP_GAP, 18, "Original interpreter on emulated hardware")

    def variant(self, variant, depth, y, parent_y=None):
        """Draw a variant and its children starting at row center y, return the next y."""
        x = LEFT + depth * INDENT
        if parent_y is not None:
            line_x = x - INDENT + 6
            self.out.append(f'<path class="l" d="M{line_x} {parent_y + LABEL_HEIGHT // 2} '
                            f'L{line_x} {y} L{x} {y}"/>')
        self.label(x, y, variant.name, variant.year)
        chips = [(name, "hle") for name in variant.hle] + [(name, "hw") for name in variant.hw]
        lines = self.chips(y, chips)
        next_y = y + ROW_HEIGHT + (lines - 1) * CHIP_LINE_HEIGHT
        for child in variant.children:
            next_y = self.variant(child, depth + 1, next_y, y)
        return next_y


def render():
    svg = SvgWriter()
    svg.legend()
    svg.separator(42)
    y = svg.variant(TREE, 0, 64)
    bottom = y + 6
    svg.separator(bottom)
    y = bottom + 30
    svg.label(LEFT, y, "Bare machines, no CHIP-8")
    lines = svg.chips(y, [(name, "hw") for name in BARE_MACHINES])
    height = y + (lines - 1) * CHIP_LINE_HEIGHT + 24
    head = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{WIDTH}" height="{height}" '
        f'viewBox="0 0 {WIDTH} {height}" role="img">',
        "<title>Cadmium preset family tree</title>",
        f"<desc>The {len(tree_presets())} Cadmium presets grouped under the CHIP-8 variant they "
        "implement, from CHIP-8 (1977) to XO-CHIP, plus the bare machine presets.</desc>",
        STYLE,
    ]
    return "\n".join(head + svg.out + ["</svg>"]) + "\n"


def tree_presets():
    presets = []

    def collect(variant):
        presets.extend(variant.hle + variant.hw)
        for child in variant.children:
            collect(child)

    collect(TREE)
    return presets + BARE_MACHINES


def cadmium_presets(binary):
    """Return the preset names listed by '<binary> --help'."""
    help_text = subprocess.run([binary, "--help"], capture_output=True, text=True).stdout
    presets = []
    in_presets = False
    for line in help_text.splitlines():
        if "--preset" in line:
            in_presets = True
        elif in_presets and re.match(r"^  -", line):
            break
        elif in_presets:
            match = re.match(r"^ {12}([a-z0-9-]+) - ", line)
            if match:
                presets.append(match.group(1))
    return presets


def check_presets(binary):
    known = cadmium_presets(binary)
    if not known:
        print(f"error: no presets found in the --help output of {binary}", file=sys.stderr)
        return False
    drawn = tree_presets()
    ok = True
    for name in sorted({p for p in drawn if drawn.count(p) > 1}):
        print(f"error: preset '{name}' is in the tree more than once", file=sys.stderr)
        ok = False
    for name in known:
        if name not in drawn:
            print(f"error: preset '{name}' is missing in the tree", file=sys.stderr)
            ok = False
    for name in drawn:
        if name not in known:
            print(f"error: preset '{name}' is not known to {binary}", file=sys.stderr)
            ok = False
    if ok:
        print(f"all {len(known)} presets of {binary} are in the tree")
    return ok


def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description="Generate the Cadmium preset family tree SVG.")
    parser.add_argument("-o", "--output", type=Path,
                        default=root / "media" / "cadmium_preset_tree.svg",
                        help="SVG file to write (default: media/cadmium_preset_tree.svg)")
    parser.add_argument("--cadmium", metavar="BINARY",
                        help="check the tree against the presets listed by this cadmium binary")
    args = parser.parse_args()
    if args.cadmium and not check_presets(args.cadmium):
        return 1
    args.output.write_text(render())
    print(f"wrote {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
