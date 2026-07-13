from __future__ import annotations

import argparse
from pathlib import Path

from intelhex import IntelHex


STAGE0_RANGE = (0x08000000, 0x08020000)
RECOVERY_RANGE = (0x081E0000, 0x08200000)


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


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Merge XC001 Stage0 and Recovery into one bootloader HEX"
    )
    parser.add_argument("--stage0", type=Path, required=True)
    parser.add_argument("--recovery", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    stage0 = load_checked(args.stage0, STAGE0_RANGE, "Stage0")
    recovery = load_checked(args.recovery, RECOVERY_RANGE, "Recovery")
    merged = IntelHex()
    merged.merge(stage0, overlap="error")
    merged.merge(recovery, overlap="error")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    merged.write_hex_file(str(args.output))
    print(args.output)


if __name__ == "__main__":
    main()