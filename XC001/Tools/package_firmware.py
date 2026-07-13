from __future__ import annotations

import argparse
import re
import shutil
from pathlib import Path


VERSION_PATTERN = re.compile(
    r'^\s*#define\s+XC001_SOFTWARE_VERSION\s+"([^"]+)"', re.MULTILINE
)


def read_version(project_root: Path) -> str:
    config = project_root / "Core" / "Inc" / "xc001_config.h"
    match = VERSION_PATTERN.search(config.read_text(encoding="utf-8"))
    if match is None:
        raise RuntimeError(f"XC001_SOFTWARE_VERSION not found in {config}")

    version = match.group(1).strip()
    if not version or re.search(r'[<>:"/\\|?*]', version):
        raise RuntimeError(f"invalid firmware version for a file name: {version!r}")
    return version


def copy_versioned(source: Path, version: str) -> Path | None:
    if not source.is_file():
        return None
    destination = source.with_name(f"{source.stem}_{version}{source.suffix}")
    shutil.copy2(source, destination)
    print(destination)
    return destination


def read_ihex_payload(source: Path) -> bytes:
    payload = bytearray()
    upper_address = 0
    for line_number, raw_line in enumerate(source.read_text(encoding="ascii").splitlines(), 1):
        line = raw_line.strip()
        if not line:
            continue
        if not line.startswith(":"):
            raise RuntimeError(f"invalid Intel HEX record in {source}:{line_number}")

        try:
            byte_count = int(line[1:3], 16)
            record_type = int(line[7:9], 16)
            data = bytes.fromhex(line[9 : 9 + byte_count * 2])
        except ValueError as exc:
            raise RuntimeError(f"invalid Intel HEX record in {source}:{line_number}") from exc

        if record_type == 0x00:
            payload.extend(data)
        elif record_type == 0x04:
            upper_address = int.from_bytes(data, "big") << 16
            _ = upper_address
        elif record_type == 0x01:
            break

    return bytes(payload)


def read_artifact_payload(source: Path) -> bytes:
    if source.suffix.lower() == ".hex":
        return read_ihex_payload(source)
    return source.read_bytes()


def ensure_artifact_contains_version(source: Path, version: str) -> None:
    payload = read_artifact_payload(source)
    if version.encode("ascii") not in payload:
        raise RuntimeError(
            f"{source} does not contain firmware version {version}. "
            "Clean/Rebuild the application before packaging versioned firmware."
        )


def copy_versioned_checked(source: Path, version: str) -> Path | None:
    if not source.is_file():
        return None
    ensure_artifact_contains_version(source, version)
    return copy_versioned(source, version)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Create versioned XC001 firmware build artifacts"
    )
    parser.add_argument(
        "build_dir", type=Path, nargs="?", help="CubeIDE Debug/Release directory"
    )
    parser.add_argument("--factory", type=Path, help="factory HEX to copy with a version")
    parser.add_argument("--bootloader", type=Path, help="bootloader HEX to copy with a version")
    parser.add_argument("--print-version", action="store_true")
    args = parser.parse_args()

    project_root = Path(__file__).resolve().parents[1]
    version = read_version(project_root)
    if args.print_version:
        print(version)
        return

    created: list[Path | None] = []
    if args.build_dir is not None:
        build_dir = args.build_dir.resolve()
        created.extend(
            [
                copy_versioned_checked(build_dir / "XC001.hex", version),
                copy_versioned_checked(build_dir / "XC001.bin", version),
            ]
        )
    if args.factory is not None:
        created.append(copy_versioned_checked(args.factory.resolve(), version))
    if args.bootloader is not None:
        created.append(copy_versioned(args.bootloader.resolve(), version))
    if not any(created):
        raise FileNotFoundError("no firmware artifact was found to package")


if __name__ == "__main__":
    main()
