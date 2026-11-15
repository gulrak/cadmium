"""End-to-end checks for bounded headless execution and exported artifacts."""
import json
import struct
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path


BINARY = Path(sys.argv[1]).resolve()


def run(rom, directory, *options, expected_code=0):
    result = subprocess.run(
        [str(BINARY), str(rom), '--preset', 'megachip', '--headless',
         *map(str, options)],
        cwd=directory, capture_output=True, text=True, timeout=20,
    )
    assert result.returncode == expected_code, result.stdout + result.stderr
    return result


def write_rom(path, instructions, data=None):
    rom = bytearray(struct.pack('>' + 'H' * len(instructions), *instructions))
    for address, values in (data or {}).items():
        offset = address - 512
        if len(rom) < offset + len(values):
            rom.extend(bytes(offset + len(values) - len(rom)))
        rom[offset:offset + len(values)] = bytes(values)
    path.write_bytes(rom)
    return path


def write_hex(path, chunks):
    records = []
    for address, values in chunks:
        for offset in range(0, len(values), 16):
            data = values[offset:offset + 16]
            current = address + offset
            record = bytes([len(data), current >> 8, current & 255, 0]) + data
            records.append(':' + record.hex().upper()
                           + f'{(-sum(record)) & 255:02X}')
    path.write_text('\n'.join(records) + '\n:00000001FF\n')
    return path


def read_png(path):
    """Decode Raylib's RGB/RGBA PNG without a third-party image dependency."""
    data = path.read_bytes()
    assert data[:8] == b'\x89PNG\r\n\x1a\n'
    offset, compressed = 8, bytearray()
    while offset < len(data):
        size = struct.unpack('>I', data[offset:offset + 4])[0]
        kind = data[offset + 4:offset + 8]
        payload = data[offset + 8:offset + 8 + size]
        if kind == b'IHDR':
            width, height, depth, color, _, _, interlace = struct.unpack(
                '>IIBBBBB', payload,
            )
            assert depth == 8 and color in (2, 6) and interlace == 0
        if kind == b'IDAT':
            compressed.extend(payload)
        offset += size + 12
    channels = 4 if color == 6 else 3
    stride = width * channels
    raw = zlib.decompress(compressed)
    rows, previous = [], bytearray(stride)
    for y in range(height):
        start = y * (stride + 1)
        filter_type = raw[start]
        row = bytearray(raw[start + 1:start + stride + 1])
        for x in range(stride):
            left = row[x - channels] if x >= channels else 0
            up = previous[x]
            corner = previous[x - channels] if x >= channels else 0
            prediction = left + up - corner
            distances = [abs(prediction - value)
                         for value in (left, up, corner)]
            paeth = (left, up, corner)[distances.index(min(distances))]
            predictors = (0, left, up, (left + up) // 2, paeth)
            row[x] = (row[x] + predictors[filter_type]) & 255
        rows.append(row)
        previous = row
    return width, height, channels, rows


def check_frames(directory, mono):
    """Frame-driven runs (--max-frames) on the generic and other cores."""
    # A frame is the unit: 3 frames at 10 instructions each, no matter how
    # many instructions the limit would allow.
    counter = write_rom(directory / 'counter.ch8', [0x7001, 0x1200])
    run(counter, directory, '--preset', 'chip-8',
        '--instructions-per-frame', 10, '--max-frames', 3,
        '--dump-state', 'counter.json')
    state = json.loads((directory / 'counter.json').read_text())
    assert state['mode'] == 'frames' and state['stopReason'] == 'frame-limit'
    assert state['executedFrames'] == 3 and state['frames'] == 3
    assert state['instructions'] == 30 and state['v'][0] == 15
    assert state['executionUnits'][0]['pc'] == 0x200
    run(counter, directory, '--preset', 'chip-8', '--max-frames', 3,
        '--max-instructions', 100, expected_code=1)
    run(counter, directory, '--preset', 'chip-8',
        '--instructions-per-frame', 0, '--max-frames', 3, expected_code=1)
    # The hardware VIP runs the original interpreter, the screen is the raw
    # CDP1861 picture with four lines per CHIP-8 row. The interpreter core
    # stops on the jump to itself at the end.
    run(mono, directory, '--preset', 'vip-chip-8', '--max-frames', 120,
        '--dump-screen', 'vip.png', '--dump-state', 'vip.json')
    state = json.loads((directory / 'vip.json').read_text())
    assert state['mode'] == 'frames' and state['stopReason'] == 'endless-loop'
    assert state['pc'] == 0x208 and state['executedFrames'] < 120
    assert 'CDP1802' in [unit['name'] for unit in state['executionUnits']]
    width, height, channels, rows = read_png(directory / 'vip.png')
    assert (width, height) == (64, 128)
    assert any(rows[120][60 * channels:60 * channels + 3])
    assert not any(rows[119][60 * channels:60 * channels + 3])
    previous_png = (directory / 'vip.png').read_bytes()
    run(mono, directory, '--preset', 'vip-chip-8', '--max-frames', 120,
        '--dump-screen', 'vip.png', '--dump-state', 'vip2.json')
    assert (directory / 'vip.png').read_bytes() == previous_png
    assert json.loads((directory / 'vip2.json').read_text()) == state
    run(mono, directory, '--preset', 'vip-chip-8', '--max-instructions', 100,
        expected_code=1)
    check_dump_scale(directory, mono)


def lit(rows, channels, x, y):
    return any(rows[y][x * channels:x * channels + 3])


def check_dump_scale(directory, mono):
    """--dump-scale writes the screen as shown: aspect ratio and rotation."""
    # The VIP's four lines per CHIP-8 row become one shown row.
    run(mono, directory, '--preset', 'vip-chip-8', '--max-frames', 120,
        '--dump-screen', 'shown.png', '--dump-scale', 1)
    width, height, channels, rows = read_png(directory / 'shown.png')
    assert (width, height) == (64, 32)
    assert lit(rows, channels, 60, 30) and not lit(rows, channels, 60, 29)
    # Every shown pixel becomes 3x3 pixels.
    run(mono, directory, '--preset', 'vip-chip-8', '--max-frames', 120,
        '--dump-screen', 'shown.png', '--dump-scale', 3)
    width, height, channels, rows = read_png(directory / 'shown.png')
    assert (width, height) == (192, 96)
    assert all(lit(rows, channels, x, y)
               for x in range(180, 183) for y in range(90, 93))
    assert not lit(rows, channels, 180, 89) and not lit(rows, channels, 179, 90)
    # A clockwise rotation by 90 degrees puts pixel (60, 30) at (1, 60).
    run(mono, directory, '--preset', 'chip-8', '--screen-rotation', 90,
        '--instructions-per-frame', 1, '--until-presentations', 1,
        '--dump-screen', 'shown.png', '--dump-scale', 1)
    width, height, channels, rows = read_png(directory / 'shown.png')
    assert (width, height) == (32, 64)
    assert lit(rows, channels, 1, 60) and not lit(rows, channels, 30, 60)
    run(mono, directory, '--preset', 'chip-8', '--dump-scale', 2,
        expected_code=1)
    check_keys(directory)


def run_keys(rom, directory, preset, keys, frames, expected_code=0):
    options = ['--preset', preset, '--max-frames', frames,
               '--dump-state', 'keys.json']
    if preset == 'chip-8':
        options += ['--instructions-per-frame', 10]
    if keys is not None:
        options += ['--keys', keys]
    result = run(rom, directory, *options, expected_code=expected_code)
    return json.loads((directory / 'keys.json').read_text()), result


def check_keys(directory):
    """--keys presses CHIP-8 keys at given frames, on every core."""
    # Ex9E: v1 := 1 once key 5 is down.
    skip = write_rom(directory / 'skip.ch8', [
        0x6005, 0xE09E, 0x1202, 0x6101, 0x1208,
    ])
    for preset in ('chip-8', 'vip-chip-8'):
        state, _ = run_keys(skip, directory, preset, None, 60)
        assert state['v'][1] == 0 and state['stopReason'] == 'frame-limit'
        assert 'keys' not in state
        state, _ = run_keys(skip, directory, preset, '20:5', 60)
        assert state['v'][1] == 1 and state['stopReason'] == 'endless-loop'
        # The program ends in its loop while key 5 is still down.
        assert state['executedFrames'] > 20 and state['keys'] == '20:5'
    # Several keys at once: only "16" holds 1 and 6 together.
    both = write_rom(directory / 'both.ch8', [
        0x6001, 0x6106, 0xE09E, 0x1204, 0xE19E, 0x1204, 0x6201, 0x120E,
    ])
    state, _ = run_keys(both, directory, 'chip-8', '10:16', 60)
    assert state['v'][2] == 1
    state, _ = run_keys(both, directory, 'chip-8', '10:1,+5:6', 60)
    assert state['v'][2] == 0 and state['keysDone']
    # Fx0A takes a held key with its release, the + start counts from the
    # previous release.
    wait = write_rom(directory / 'wait.ch8', [0xF30A, 0xF40A, 0x1204])
    for preset in ('chip-8', 'strict-chip-8'):
        state, _ = run_keys(wait, directory, preset, '5:A+3 +2:B', 60)
        assert state['v'][3] == 0xA and state['v'][4] == 0xB
        assert state['pc'] == 0x204
        # Key A is still down at the end: the second Fx0A still waits.
        state, result = run_keys(wait, directory, preset, '5:1 +2:A+50', 30)
        assert state['v'][3] == 1 and state['v'][4] == 0
        assert state['pc'] == 0x202
        assert not state['keysDone'] and 'key script' in result.stderr
    for bad in ('x', '60', '60:', '60:G', '60:1+0', '-5:1', '60:1+x'):
        run_keys(wait, directory, 'chip-8', bad, 10, expected_code=1)
    result = subprocess.run(
        [str(BINARY), '--preset', 'chip-8', '--keys', '60:1'],
        capture_output=True, text=True, timeout=20,
    )
    assert result.returncode == 1 and 'require --headless' in result.stderr


def main():
    with tempfile.TemporaryDirectory() as temporary:
        directory = Path(temporary)
        rom = write_rom(
            directory / 'pixels.mc8',
            [0x0011, 0xA300, 0x0201, 0x0302, 0x0401, 0xA310,
             0x6003, 0x6104, 0xD010, 0x00E0, 0x60FF, 0x1214],
            {0x300: [255, 18, 52, 86], 0x310: [1, 1]},
        )
        options = ['--until-presentations', 1, '--max-instructions', 50,
                   '--dump-screen', 'frame.png', '--dump-state', 'state.json']
        run(rom, directory, *options)
        state = json.loads((directory / 'state.json').read_text())
        assert state['presentations'] == 1 and state['instructions'] == 10
        assert state['pc'] == 0x214 and state['v'][0] == 3
        assert state['lastOpcode'] == 0x00E0
        assert state['frameRate'] == 50
        assert state['stopReason'] == 'presentations'
        # Sparse HEX uses recorded addresses, with the explicit CLI preset.
        image = rom.read_bytes()
        hex_rom = write_hex(directory / 'pixels.hex', [
            (0x310, image[0x110:0x112]),
            (0x200, image[:24]),
            (0x300, image[0x100:0x104]),
        ])
        run(hex_rom, directory, *options)
        assert json.loads((directory / 'state.json').read_text()) == state
        bad_hex = directory / 'invalid.hex'
        bad_hex.write_text(':020200006000100\n:00000001FF\n')
        run(bad_hex, directory, expected_code=1)
        # Restore the baseline before continuing the binary ROM checks.
        run(rom, directory, *options)
        width, height, channels, rows = read_png(directory / 'frame.png')
        assert (width, height) == (256, 192)
        for x in (3, 4):
            assert tuple(rows[4][x * channels:x * channels + 3]) == (
                17, 51, 85,  # Existing display conversion uses /256 alpha.
            )
        # Frame padding is separate from actual instruction attempts.
        run(rom, directory, *options, '--instructions-per-frame', 1000000)
        fast = json.loads((directory / 'state.json').read_text())
        assert fast['instructions'] == 10 and fast['cycles'] == 1000000
        assert fast['presentations'] == 1
        # Capture outputs are repeatable with the same ROM/settings.
        previous_png = (directory / 'frame.png').read_bytes()
        run(rom, directory, *options, '--instructions-per-frame', 1000000)
        assert json.loads((directory / 'state.json').read_text()) == fast
        assert (directory / 'frame.png').read_bytes() == previous_png

        loop = write_rom(directory / 'loop.mc8', [0x6000, 0x1200])
        run(loop, directory, '--until-presentations', 1,
            '--max-instructions', 17, '--dump-state', 'limit.json',
            expected_code=2)
        limit = json.loads((directory / 'limit.json').read_text())
        assert limit['instructions'] == 17 and limit['presentations'] == 0
        assert limit['stopReason'] == 'instruction-limit'
        run(loop, directory, '--max-instructions', 17)

        invalid = write_rom(directory / 'invalid.mc8', [0xFFFF])
        run(invalid, directory, '--dump-state', 'error.json', expected_code=1)
        error = json.loads((directory / 'error.json').read_text())
        assert error['stopReason'] == 'emulator-error'
        assert error['lastInstructionPc'] == 512
        assert error['lastOpcode'] == 0xFFFF and error['error']

        exited = write_rom(directory / 'exit.mc8', [0x00FD])
        run(exited, directory, '--dump-state', 'exit.json')
        assert json.loads((directory / 'exit.json').read_text())[
            'stopReason'
        ] == 'program-exit'
        run(exited, directory, '--until-presentations', 1, expected_code=2)

        wait = write_rom(directory / 'wait.mc8', [0xF00A])
        run(wait, directory, '--until-presentations', 1,
            '--max-instructions', 11, expected_code=2)
        timer = write_rom(directory / 'timer.mc8', [
            0x0011, 0x6002, 0xF015, 0xF007, 0x3000, 0x1206, 0x00E0,
        ])
        run(timer, directory, '--instructions-per-frame', 4,
            '--until-presentations', 1, '--dump-state', 'timer.json')
        assert json.loads((directory / 'timer.json').read_text())[
            'delayTimer'
        ] == 0
        # Active stack entries and the full 24-bit index are exported.
        registers = write_rom(directory / 'registers.mc8', [
            0x2204, 0x00FD, 0x0102, 0x3456, 0x00FD,
        ])
        run(registers, directory, '--dump-state', 'registers.json')
        state = json.loads((directory / 'registers.json').read_text())
        assert state['i'] == 0x023456 and state['stack'] == [0x202]
        assert state['sp'] == 1
        mono = write_rom(directory / 'mono.ch8', [
            0x603C, 0x611E, 0xA300, 0xD011, 0x1208,
        ], {0x300: [128]})
        run(mono, directory, '--preset', 'chip-8',
            '--instructions-per-frame', 1, '--until-presentations', 1,
            '--dump-screen', 'mono.png')
        width, height, channels, rows = read_png(directory / 'mono.png')
        assert (width, height) == (64, 32)
        assert any(rows[30][60 * channels:60 * channels + 3])
        # Cores without instructions per frame run frame-driven, the strict
        # core rejects the MegaChip opcode as an emulator error.
        run(rom, directory, '--preset', 'strict-chip-8',
            '--dump-state', 'strict.json', expected_code=1)
        strict = json.loads((directory / 'strict.json').read_text())
        assert strict['mode'] == 'frames'
        assert strict['stopReason'] == 'emulator-error' and strict['error']
        check_frames(directory, mono)
        run(directory / 'missing.mc8', directory, expected_code=1)
        empty = write_rom(directory / 'empty.mc8', [])
        run(empty, directory, expected_code=1)
        run(rom, directory, '--max-instructions', 0, expected_code=1)
        run(rom, directory, '--until-presentations', 0, expected_code=1)
        run(rom, directory, '--instructions-per-frame', 0, expected_code=1)
        run(rom, directory, '--dump-screen', directory / 'missing/frame.png',
            '--until-presentations', 1, expected_code=1)
        run(rom, directory, '--dump-state', directory / 'missing/state.json',
            '--until-presentations', 1, expected_code=1)
        run(rom, directory, '--benchmark', 10, expected_code=1)
        result = subprocess.run(
            [str(BINARY), '--preset', 'megachip',
             '--max-instructions', '3000000'],
            capture_output=True, text=True, timeout=20,
        )
        assert result.returncode == 1
        assert 'require --headless' in result.stderr
    print('Headless CLI checks passed.')


if __name__ == '__main__':
    main()
