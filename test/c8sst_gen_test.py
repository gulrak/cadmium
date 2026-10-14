"""Exercise SST generation, independent decoding, and preset replay."""

import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile


def apply_ranges(base, ranges):
    result = bytearray(base)
    end = 0
    for offset, data in ranges:
        values = bytes.fromhex(data)
        assert values and offset >= end
        end = offset + len(values)
        assert end <= len(result)
        result[offset:end] = values
    return result


def decode_display(value, initial=None):
    if value is None:
        assert initial is not None
        return bytearray(initial)
    if value.get("base") == "initial":
        assert initial is not None
        ranges = []
        for plane, offset, data in value["replace"]:
            assert plane == 0
            ranges.append([offset, data])
        return apply_ranges(initial, ranges)
    assert (value["width"], value["height"], value["mode"]) == (64, 32, "low")
    assert len(value["planes"]) == 1
    plane = value["planes"][0]
    result = (bytearray([plane["fill"]] * 256) if "fill" in plane
              else bytearray.fromhex(plane["data"]))
    assert len(result) == 256
    return result


def validate(corpus):
    profile = json.loads((corpus / "profile.json").read_text())
    assert profile["format"] == "chip8-sst" and profile["version"] == 1
    assert profile["profile"] == "chip8-vip-4k-v1"
    defaults = profile["defaults"]
    ram_default = apply_ranges(
        bytearray([defaults["ram"]["fill"]] * 4096),
        defaults["ram"]["replace"],
    )
    display_default = decode_display(defaults["display"])
    ids = set()
    counts = {"clear": 0, "draw": 0, "immediate": 0, "arithmetic": 0}
    for file in profile["files"]:
        tests = json.loads((corpus / file["path"]).read_text())
        assert len(tests) == file["count"]
        for test in tests:
            assert test["id"] not in ids
            ids.add(test["id"])
            assert "cycles" not in test
            initial, final = test["initial"], test["final"]
            ram = apply_ranges(ram_default, initial.get("ram", []))
            expected_ram = apply_ranges(ram, final.get("ram", []))
            display = decode_display(initial.get("display"), display_default)
            expected_display = decode_display(final.get("display"), display)
            assert ram[0xF00:] == display
            assert expected_ram[0xF00:] == expected_display
            for state in (initial, final):
                assert len(state["v"]) == 16
                assert all(0 <= v <= 255 for v in state["v"])
                assert len(state["stack"]) <= 12
                assert state["dt"] == state["st"] == 0
            opcode = int(test["opcode"], 16)
            pc = initial["pc"]
            assert int.from_bytes(ram[pc:pc + 2], "big") == opcode
            prefix, x, y, n = (
                opcode >> 12, (opcode >> 8) & 15,
                (opcode >> 4) & 15, opcode & 15,
            )
            if prefix in (6, 7):
                registers = initial["v"].copy()
                registers[x] = ((opcode & 255) if prefix == 6
                                else (registers[x] + (opcode & 255)) & 255)
                assert final["v"] == registers
                assert final["pc"] == initial["pc"] + 2
                assert "display" not in final
                counts["immediate"] += 1
            if prefix == 8 and x != 15 and y != 15:
                registers = initial["v"].copy()
                a, b = registers[x], registers[y]
                if n == 0:
                    registers[x] = b
                elif n in (1, 2, 3):
                    registers[x] = {1: a | b, 2: a & b, 3: a ^ b}[n]
                    registers[15] = 0
                elif n == 4:
                    registers[x] = (a + b) & 255
                    registers[15] = int(a + b > 255)
                elif n in (5, 7):
                    lhs, rhs = (a, b) if n == 5 else (b, a)
                    registers[x] = (lhs - rhs) & 255
                    registers[15] = int(lhs >= rhs)
                elif n == 6:
                    registers[x], registers[15] = b >> 1, b & 1
                elif n == 14:
                    registers[x], registers[15] = (b << 1) & 255, b >> 7
                assert final["v"] == registers, test["id"]
                assert "display" not in final
                counts["arithmetic"] += 1
            if opcode == 0x00E0:
                assert expected_display == bytearray(256)
                counts["clear"] += 1
            if prefix == 13 and n > 0 and x != 15 and y != 15:
                # Original VIP: origin modulo dimensions, then clip the sprite.
                px, py = initial["v"][x] % 64, initial["v"][y] % 32
                drawn = bytearray(display)
                collision = 0
                for row in range(n):
                    sprite = ram[initial["i"] + row]
                    for bit in range(8):
                        if px + bit >= 64 or py + row >= 32:
                            continue
                        if sprite & (0x80 >> bit):
                            offset = (py + row) * 8 + (px + bit) // 8
                            mask = 0x80 >> ((px + bit) % 8)
                            collision |= int(bool(drawn[offset] & mask))
                            drawn[offset] ^= mask
                assert expected_display == drawn, test["id"]
                assert final["v"][15] == collision, test["id"]
                counts["draw"] += 1
    assert len(ids) == profile["test_count"]
    assert all(counts.values()), counts
    return profile, counts


def exercise_runner(executable, corpus, root):
    manifest = json.loads((corpus / "profile.json").read_text())

    def run(path, preset="strict-chip-8", extra=(), expected=0):
        result = subprocess.run(
            [executable, "--tests", str(path), "--preset", preset,
             "--max-failures", "0", *extra],
            capture_output=True, text=True, timeout=180,
        )
        assert result.returncode == expected, result.stdout + result.stderr
        return result

    reports = {}
    presets = (("vip-chip-8", 0), ("strict-chip-8", 0), ("chip-8", 1))
    for preset, code in presets:
        report_path = root / f"{preset}.json"
        run(corpus, preset, ["--report", str(report_path)], code)
        reports[preset] = json.loads(report_path.read_text())
        report = reports[preset]
        assert report["complete"]
        assert report["tested"] == manifest["test_count"]
        assert report["passed"] + report["failed"] == report["tested"]
    assert reports["vip-chip-8"]["failed"] == 0
    assert reports["strict-chip-8"]["failed"] == 0
    alias = next(f for f in reports["chip-8"]["failures"]
                 if f["id"] == "display/store-framebuffer")
    assert any(d["field"] == "display" for d in alias["differences"])
    assert reports["chip-8"]["families"]["font"]["failed"] > 0
    run(corpus / "profile.json", extra=["--case", "control/1222"])
    run(corpus, extra=["--family", "missing"], expected=2)
    run(corpus, preset="missing", expected=2)
    run(corpus, extra=["--report", str(root / "vip-chip-8.json")], expected=2)

    original = json.loads((corpus / "immediate.json").read_text())[0]

    def single(name, test, mutate_profile=None, second=None):
        path = root / name
        path.mkdir()
        profile = copy.deepcopy(manifest)
        tests = [test] if second is None else [test, second]
        profile["files"] = [{"path": "immediate.json", "count": len(tests)}]
        profile["test_count"] = len(tests)
        if mutate_profile is not None:
            mutate_profile(profile)
        # Object key order must have no effect on the public JSON format.
        (path / "profile.json").write_text(json.dumps(profile, sort_keys=True))
        (path / "immediate.json").write_text(json.dumps(tests, sort_keys=True))
        return path

    run(single("reordered", original))
    wrong = copy.deepcopy(original)
    wrong["final"]["v"][5] ^= 1
    report_path = root / "wrong-register.json"
    run(single("wrong-register", wrong),
        extra=["--report", str(report_path)], expected=1)
    report = json.loads(report_path.read_text())
    differences = report["failures"][0]["differences"]
    assert any(d["field"] == "v5" for d in differences)

    clear = copy.deepcopy(original)
    defaults = manifest["defaults"]["ram"]
    baseline = apply_ranges(
        bytearray([defaults["fill"]] * 4096), defaults["replace"],
    )
    ram = apply_ranges(baseline, clear["initial"]["ram"])
    pc = clear["initial"]["pc"]
    ram[pc:pc + 2] = bytes.fromhex("00e0")
    clear["opcode"] = "00e0"
    clear["initial"]["ram"] = [
        [0, ram[:0xE90].hex()], [0xF00, ram[0xF00:].hex()],
    ]
    assert "display" not in clear["final"] and "ram" not in clear["final"]
    report_path = root / "unexpected-clear.json"
    run(single("unexpected-clear", clear),
        extra=["--report", str(report_path)], expected=1)
    report = json.loads(report_path.read_text())
    fields = {d["field"] for d in report["failures"][0]["differences"]}
    assert fields >= {"ram", "display"}

    wrong_again = copy.deepcopy(wrong)
    wrong_again["id"] += "-second"
    report_path = root / "fail-fast.json"
    run(single("fail-fast", wrong, second=wrong_again),
        extra=["--fail-fast", "--report", str(report_path)], expected=1)
    report = json.loads(report_path.read_text())
    assert report["selected"] == 2 and report["tested"] == 1
    assert not report["complete"]

    invalid = copy.deepcopy(original)
    invalid["initial"]["display"] = {
        "width": 64, "height": 32, "mode": "low",
        "planes": [{"fill": 0}],
    }
    run(single("contradictory-alias", invalid), expected=2)
    invalid = copy.deepcopy(original)
    invalid["initial"]["ram"] = [[0, "zz"]]
    run(single("invalid-hex", invalid), expected=2)
    invalid = copy.deepcopy(original)
    invalid["opcode"] = "60ff"
    run(single("invalid-opcode", invalid), expected=2)
    run(single("invalid-version", original,
               mutate_profile=lambda p: p.update(version=2)), expected=2)
    run(single("invalid-count", original,
               mutate_profile=lambda p: p.update(test_count=2)), expected=2)
    run(single("duplicate-id", original, second=original), expected=2)
    print("Preset replay, mismatch detection, filters, "
          "and malformed-corpus checks passed")


def main():
    executable = str(Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory(prefix="c8sst-test-") as temporary:
        root = Path(temporary)
        first, second = root / "first", root / "second"
        args = [executable, "--random", "2", "--seed", "123", "-o"]
        subprocess.run(args + [str(first)], check=True, timeout=180)
        profile, counts = validate(first)
        if len(sys.argv) > 2:
            exercise_runner(str(Path(sys.argv[2]).resolve()), first, root)
        subprocess.run(args + [str(second)], check=True, timeout=180)
        for path in first.iterdir():
            assert path.read_bytes() == (second / path.name).read_bytes()
        # Invalid requests and overwrite attempts must fail without output.
        for options in (["--preset", "xo-chip"], ["--family", "missing"],
                        ["--random", "-1"], ["--seed", "-1"]):
            output = root / "invalid"
            result = subprocess.run([executable, *options, "-o", str(output)],
                                    capture_output=True, timeout=20)
            assert result.returncode != 0 and not output.exists()
        result = subprocess.run(args + [str(first)], capture_output=True)
        assert result.returncode != 0
        print(f"Validated {profile['test_count']} tests; "
              f"independent checks: {counts}")


if __name__ == "__main__":
    main()
