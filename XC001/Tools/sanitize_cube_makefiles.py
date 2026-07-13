from __future__ import annotations

import argparse
from pathlib import Path


UNSUPPORTED_FLAGS = (
    "-fcyclomatic-complexity",
)


def sanitize_file(path: Path, unsupported_flags: tuple[str, ...]) -> bool:
    text = path.read_text(encoding="utf-8")
    sanitized = text
    for flag in unsupported_flags:
        sanitized = sanitized.replace(f" {flag}", "")
        sanitized = sanitized.replace(f"\t{flag}", "\t")

    if sanitized == text:
        return False

    path.write_text(sanitized, encoding="utf-8", newline="")
    return True


def sanitize_tree(build_dir: Path, unsupported_flags: tuple[str, ...]) -> list[Path]:
    if not build_dir.is_dir():
        return []

    changed: list[Path] = []
    for makefile in build_dir.rglob("*.mk"):
        if sanitize_file(makefile, unsupported_flags):
            changed.append(makefile)
    return changed


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Remove CubeIDE-generated GCC options that are not accepted by the "
            "command-line arm-none-eabi-gcc toolchain."
        )
    )
    parser.add_argument(
        "build_dir",
        nargs="?",
        default="Debug",
        help="CubeIDE build output directory that contains generated *.mk files.",
    )
    parser.add_argument(
        "--quiet",
        action="store_true",
        help="Do not print changed files.",
    )
    args = parser.parse_args()

    build_dir = Path(args.build_dir)
    changed = sanitize_tree(build_dir, UNSUPPORTED_FLAGS)

    if not args.quiet and changed:
        print("Sanitized CubeIDE generated makefiles:")
        for path in changed:
            print(path)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
