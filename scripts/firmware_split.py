#!/usr/bin/env python3
"""Split a joined firmware ZIP back into host.bin, slave.bin, and optional hosted-c6.bin."""

from __future__ import annotations

import argparse
import zipfile
from pathlib import Path


CORE_ANY = ("slave.bin", "host.bin")
OPTIONAL = ("hosted-c6.bin",)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path, help="Joined firmware ZIP")
    parser.add_argument(
        "-o",
        "--output-dir",
        type=Path,
        default=Path("."),
        help="Directory for extracted binaries",
    )
    args = parser.parse_args()

    if not args.package.is_file():
        raise SystemExit(f"package not found: {args.package}")

    args.output_dir.mkdir(parents=True, exist_ok=True)
    written: list[str] = []
    with zipfile.ZipFile(args.package, "r") as archive:
        names = set(archive.namelist())
        if not any(name in names for name in CORE_ANY):
            raise SystemExit("missing ZIP members: need slave.bin and/or host.bin")
        for name in CORE_ANY + OPTIONAL:
            if name not in names:
                continue
            target = args.output_dir / name
            target.write_bytes(archive.read(name))
            if target.stat().st_size == 0:
                raise SystemExit(f"extracted empty member: {name}")
            written.append(name)

    print(f"Wrote {', '.join(str(args.output_dir / name) for name in written)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
