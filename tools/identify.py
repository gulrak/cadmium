#!/usr/bin/env python3
import sys
import os
import hashlib
import pathlib
import json

root_dir = sys.argv[1]
progs_file = sys.argv[2]

with open(progs_file) as f:
    progs = json.load(f)

def getSHA1(file):
    sha1 = hashlib.sha1()
    with open(file, 'rb') as f:
        while True:
            data = f.read(65535)
            if not data:
                break
            sha1.update(data)
    return sha1.hexdigest()

roms = {}

for root, dirs, files in os.walk(root_dir):
    path = root.split(os.sep)
    # print((len(path) - 1) * '---', os.path.basename(root))
    for file in files:
        extension = os.path.splitext(file)[1]
        if extension in [".c8", ".c8h", ".c8x", ".ch10", ".ch8", ".hc8", ".mc8", ".sc8", ".xo8"]:
            filepath = os.path.join(root, file)
            chksum = getSHA1(filepath)
            if chksum in roms:
                roms[chksum]["names"].append(filepath)
            else:
                roms[chksum] = {"names": [filepath], "type": []}
            if extension != ".ch8":
                roms[chksum]["type"].append(extension)

typed_files = 0
for chksum, info in roms.items():
    print(chksum, repr(info["type"]))
    if info["type"]:
        typed_files += 1
    for file in info["names"]:
        print("    ", file)

print(f"Found {len(roms)} unique files with {typed_files} of them typed")
print("Scanning for database entries...")
for prg in progs:
    for chksum, info in roms.items():
        for name in info["names"]:
            if pathlib.Path(name).stem.casefold() == pathlib.Path(prg["file"]).stem.casefold():
                prg["hash"] = chksum

with open('programs_hashed.json', 'w') as f:
    json.dump(progs, f)