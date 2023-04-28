#!/usr/bin/env python3
import sys
import json
import re
from color_constants import colors


def color_value(col):
    if col[0] == '#':
        return col
    if col not in colors:
        print("unknown color: " + col)
        raise KeyError
    return colors[col].hex_format()


progs_file = sys.argv[1]

with open(progs_file) as f:
    progs = json.load(f)

for prog_info in progs:
    if 'sha1' in prog_info:
        name = prog_info['title'].title().replace('"', "'")
        meta = ""
        if 'authors' in prog_info and len(prog_info['authors']) :
            authors = ", ".join(prog_info['authors'])
            if 'release' in prog_info:
                meta = f" ({authors}, {prog_info['release']})"
            else:
                meta = f" ({authors})"
        else:
            m = re.search(r"\[([^,]+)\s*,\s*(\d\w*)]", prog_info['file'])
            if m:
                meta = f" ({m.group(1)}, {m.group(2)})"
            else:
                m = re.search(r"\[([^,]+)\s*]", prog_info['file'])
                if m:
                    meta = f" ({m.group(1)})"
                else:
                    m = re.search(r"\(([^)]+)\)", prog_info['file'])
                    if m:
                        meta = f" ({m.group(1)})"
        platform = "CHIP_8"
        if 'platform' in prog_info:
            if prog_info['platform'] == "xochip":
                platform = "XO_CHIP"
            elif prog_info['platform'] == "schip":
                platform = "SCHIPC"
                if 'options' in prog_info:
                    opts = prog_info['options']
                    if 'shiftQuirks' in opts and opts['shiftQuirks']:
                        platform = "SCHIP_1_1"
            elif prog_info['platform'] != "chip8":
                print(prog_info['platform'])
                raise KeyError
        options = {}
        advanced = {}
        if 'options' in prog_info:
            for key, value in prog_info['options'].items():
                if key == 'shiftQuirks':
                    if ((platform == 'CHIP_8' or platform == 'XO_CHIP' or platform == 'SCHIPC') and value) or (platform == 'SCHIP_1_1' and not value):
                        options['optJustShiftVx'] = value
                elif key == 'loadStoreQuirks':
                    if ((platform == 'CHIP_8' or platform == 'XO_CHIP' or platform == 'SCHIPC') and value) or (platform == 'SCHIP_1_1' and not value):
                        options['optLoadStoreDontIncI'] = value
                elif key == 'jumpQuirks':
                    if ((platform == 'CHIP_8' or platform == 'XO_CHIP' or platform == 'SCHIPC') and value) or (platform == 'SCHIP_1_1' and not value):
                        options['optJump0Bxnn'] = value
                elif key == 'vBlankQuirks':
                    if (platform == 'CHIP_8' and not value) or (platform != 'CHIP_8' and value):
                        options['optInstantDxyn'] = not value
                elif key == 'logicQuirks':
                    if (platform == 'CHIP_8' and not value) or (platform != 'CHIP_8' and value):
                        options['optDontResetVf'] = not value
                elif key == 'clipQuirks':
                    if (platform == 'XO_CHIP' and value) or (platform != 'XO_CHIP' and not value):
                        options['optWrapSprites'] = not value
                elif key == 'tickrate':
                    options['instructionsPerFrame'] = value
                elif key == 'fontStyle' or key == 'screenRotation':
                    advanced[key] = value
                elif key == 'backgroundColor':
                    advanced['col0'] = color_value(value)
                elif key == 'fillColor':
                    advanced['col1'] = color_value(value)
                elif key == 'fillColor2':
                    advanced['col2'] = color_value(value)
                elif key == 'blendColor':
                    advanced['col3'] = color_value(value)
                elif key == 'buzzColor' or key == 'quietColor':
                    advanced[key] = color_value(value)
                elif key == 'enableXO':
                    pass
                    #if (platform != 'XO_CHIP' and value) or (platform == 'XO_CHIP' and not value):
                    #    print('weird enableXO: ' + name)
                    #    raise KeyError
                elif key != 'touchInputMode' and key != 'vfOrderQuirks':
                    print(f"{key}: {value}")
                    raise KeyError
        comment = f"\t\t\t// {prog_info['file']}" if not meta else ""
        if advanced:
            options['advanced'] = advanced
        options = json.dumps(options) if options else ""
        if options:
            print(f"{{\"{prog_info['sha1']}\", {{emu::chip8::Variant::{platform}, \"{name}{meta}\", R\"({options})\"}}}},{comment}")
        else:
            print(f"{{\"{prog_info['sha1']}\", {{emu::chip8::Variant::{platform}, \"{name}{meta}\"}}}},{comment}")
