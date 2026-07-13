from __future__ import annotations

import argparse
import struct
import zlib
from pathlib import Path

from intelhex import IntelHex


STAGE0_RANGE = (0x08000000, 0x08020000)
APP_RANGE = (0x08020000, 0x080C0000)
RECOVERY_RANGE = (0x081E0000, 0x08200000)
UPDATE_META_ADDRESS = 0x081A0000
UPDATE_MAGIC = 0x58555044
UPDATE_RECORD_VERSION = 1
UPDATE_STATE_APPLIED = 2


def load_checked(path: Path, allowed: tuple[int, int], label: str) -> IntelHex:
    image = IntelHex(str(path))
    segments = image.segments()
    if not segments:
        raise ValueError(f"{label} image is empty: {path}")
    for start, end in segments:
        if start < allowed[0] or end > allowed[1]:
            raise ValueError(
                f"{label} segment 0x{start:08X}-0x{end:08X} is outside "
                f"0x{allowed[0]:08X}-0x{allowed[1]:08X}"
            )
    image.start_addr = None
    return image


def applied_record(application: IntelHex) -> bytes:
    image_end = max(end for _, end in application.segments())
    image_size = image_end - APP_RANGE[0]
    application.padding = 0xFF
    image = bytes(
        application.tobinarray(start=APP_RANGE[0], end=image_end - 1)
    )
    image_crc = zlib.crc32(image) & 0xFFFFFFFF
    body = struct.pack(
        "<IHHIIIII",
        UPDATE_MAGIC,
        UPDATE_RECORD_VERSION,
        UPDATE_STATE_APPLIED,
        1,
        image_size,
        image_crc,
        0,
        0,
    )
    return body + struct.pack("<I", zlib.crc32(body) & 0xFFFFFFFF)


def main() -> None:
    parser = argparse.ArgumentParser(description="Merge XC001 factory programming HEX")
    parser.add_argument("--stage0", type=Path, required=True)
    parser.add_argument("--app", type=Path, required=True)
    parser.add_argument("--recovery", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    stage0 = load_checked(args.stage0, STAGE0_RANGE, "Stage0")
    application = load_checked(args.app, APP_RANGE, "Application")
    recovery = load_checked(args.recovery, RECOVERY_RANGE, "Recovery")
    merged = IntelHex()
    merged.merge(stage0, overlap="error")
    merged.merge(application, overlap="error")
    merged.merge(recovery, overlap="error")
    merged.puts(UPDATE_META_ADDRESS, applied_record(application))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    merged.write_hex_file(str(args.output))
    print(args.output)


if __name__ == "__main__":
    main()
