#!/usr/bin/env python3
"""Extract an Electron .asar container.

asar is a Chromium pickle (uint32 length-prefixed buffer) holding a JSON tree,
followed by the concatenated file data at the offsets the JSON names. No
external tooling required.

    unpack-asar.py <input.asar> <output-dir>
"""
import json
import os
import shutil
import struct
import sys


def read_header(f):
    # Offset 0..7 is a pickle whose payload is a single uint32 = headerSize.
    # File data therefore starts at 8 + headerSize. The header pickle is
    # [uint32 payload][uint32 strLen][JSON], i.e. the JSON begins at +8.
    size_pickle = f.read(8)
    if len(size_pickle) < 8:
        raise SystemExit("file too small to be an asar")
    (header_size,) = struct.unpack("<I", size_pickle[4:8])
    if header_size < 12 or header_size > 64 * 1024 * 1024:
        raise SystemExit(f"implausible headerSize {header_size}")
    header_buf = f.read(header_size)
    (str_size,) = struct.unpack("<I", header_buf[4:8])
    if str_size > len(header_buf) - 8:
        raise SystemExit(f"header string claims {str_size}B, buffer holds {len(header_buf) - 8}B")
    header = json.loads(header_buf[8 : 8 + str_size].decode("utf-8"))
    return header, 8 + header_size


def walk(f, node, base, data_start, out_root, unpacked_root):
    for name, meta in node.get("files", {}).items():
        path = os.path.join(base, name)
        if "files" in meta:
            os.makedirs(path, exist_ok=True)
            walk(f, meta, path, data_start, out_root, unpacked_root)
            continue
        rel = os.path.relpath(path, out_root)
        if meta.get("unpacked"):
            # The flag is a bool; the bytes live beside the archive as
            # <asar>.unpacked/<relpath>. They are outside the header, so copy
            # them across rather than seeking.
            src = os.path.join(unpacked_root, rel)
            if os.path.exists(src):
                os.makedirs(os.path.dirname(path), exist_ok=True)
                shutil.copyfile(src, path)
                os.chmod(path, 0o755 if os.access(src, os.X_OK) else 0o644)
            continue
        offset = int(meta["offset"])
        size = int(meta["size"])
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as wf:
            f.seek(data_start + offset)
            remaining = size
            while remaining:
                chunk = f.read(min(1 << 20, remaining))
                if not chunk:
                    raise SystemExit(f"truncated: {rel}")
                wf.write(chunk)
                remaining -= len(chunk)
        mode = meta.get("executable")
        os.chmod(path, 0o755 if mode else 0o644)


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    src, dst = sys.argv[1], sys.argv[2]
    os.makedirs(dst, exist_ok=True)
    with open(src, "rb") as f:
        header, data_start = read_header(f)
        print(f"{src}: data starts @{data_start}")
        walk(f, header, dst, data_start, dst, src + ".unpacked")


if __name__ == "__main__":
    main()