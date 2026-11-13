#!/usr/bin/env python3
"""Join host.bin and/or slave.bin into a firmware ZIP for System → Update."""

from __future__ import annotations

import argparse
import zipfile
from pathlib import Path


def read_define_string(defines_path: Path, macro_name: str) -> str:
    text = defines_path.read_text(encoding="utf-8")
    needle = f"#define {macro_name}"
    for line in text.splitlines():
        stripped = line.strip()
        if stripped.startswith("//"):
            continue
        if needle not in line or '"' not in line:
            continue
        start = line.find('"')
        end = line.find('"', start + 1)
        if start >= 0 and end > start:
            return line[start + 1 : end]
    raise SystemExit(f"{macro_name} not found in {defines_path}")


def read_firmware_versions(
    defines_path: Path, *, host_board: str = "s3"
) -> tuple[str, str]:
    """Resolve host identity as S3-/P4- + FIRMWARE_VERSION_HOST."""
    host_num = read_define_string(defines_path, "FIRMWARE_VERSION_HOST")
    if host_num.startswith("S3-") or host_num.startswith("P4-"):
        host = host_num
    else:
        board = (host_board or "s3").strip().lower()
        if board in ("p4", "esp32-p4", "esp32p4"):
            host = "P4-" + host_num
        else:
            host = "S3-" + host_num
    slave = read_define_string(defines_path, "FIRMWARE_VERSION_SLAVE")
    return host, slave


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--host",
        type=Path,
        default=None,
        help="Host application .bin (optional if --slave is set)",
    )
    parser.add_argument(
        "--slave",
        type=Path,
        default=None,
        help="Slave application .bin (optional if --host is set)",
    )
    parser.add_argument(
        "--hosted-c6",
        type=Path,
        default=None,
        help="Optional ESP-Hosted Wi-Fi C6 .bin (P4: apply only if present and needed)",
    )
    parser.add_argument(
        "--hosted-c6-version",
        default=None,
        help="Hosted C6 version for hosted-c6.version.txt (required with --hosted-c6)",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=Path("firmware-joined.zip"),
        help="Output ZIP path (default: firmware-joined.zip)",
    )
    parser.add_argument(
        "--deflate",
        action="store_true",
        help="Use ZIP DEFLATE (default is STORE for fast on-device extract)",
    )
    parser.add_argument(
        "--host-version",
        default=None,
        help="Host version string for host.version.txt (default: S3-/P4- + FIRMWARE_VERSION_HOST)",
    )
    parser.add_argument(
        "--host-board",
        default="s3",
        choices=("s3", "p4", "esp32-p4", "esp32p4"),
        help="SoC prefix for default host version: S3- or P4- (default: s3)",
    )
    parser.add_argument(
        "--slave-version",
        default=None,
        help="Slave version string for slave.version.txt (default: FIRMWARE_VERSION_SLAVE)",
    )
    parser.add_argument(
        "--version",
        default=None,
        help="Deprecated: sets both host and slave versions when specific flags are omitted",
    )
    parser.add_argument(
        "--defines",
        type=Path,
        default=Path("include/Defines.h"),
        help="Path to Defines.h when versions are omitted",
    )
    args = parser.parse_args()

    if args.host is None and args.slave is None:
        raise SystemExit("Provide at least one of --host or --slave")

    members: list[str] = []
    if args.slave is not None:
        if not args.slave.is_file() or args.slave.stat().st_size == 0:
            raise SystemExit(f"slave binary missing or empty: {args.slave}")
        members.append("slave.bin")
    if args.host is not None:
        if not args.host.is_file() or args.host.stat().st_size == 0:
            raise SystemExit(f"host binary missing or empty: {args.host}")
        members.append("host.bin")
    hosted_version = ""
    if args.hosted_c6 is not None:
        if not args.hosted_c6.is_file() or args.hosted_c6.stat().st_size == 0:
            raise SystemExit(f"hosted-c6 binary missing or empty: {args.hosted_c6}")
        hosted_version = (args.hosted_c6_version or "").strip()
        if not hosted_version or len(hosted_version) > 15:
            raise SystemExit(
                f"hosted-c6 version must be 1-15 chars, got {hosted_version!r}"
            )
        members.append("hosted-c6.bin")

    default_host, default_slave = read_firmware_versions(
        args.defines, host_board=args.host_board
    )
    if args.version:
        default_host = args.version.strip()
        default_slave = args.version.strip()
    host_version = (args.host_version or default_host).strip()
    slave_version = (args.slave_version or default_slave).strip()
    for label, version in (("host", host_version), ("slave", slave_version)):
        if not version or len(version) > 15:
            raise SystemExit(f"{label} version must be 1-15 chars, got {version!r}")

    compression = zipfile.ZIP_DEFLATED if args.deflate else zipfile.ZIP_STORED
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.output, "w", compression=compression) as archive:
        if args.slave is not None:
            archive.write(args.slave, arcname="slave.bin")
        if args.host is not None:
            archive.write(args.host, arcname="host.bin")
        if args.hosted_c6 is not None:
            archive.write(args.hosted_c6, arcname="hosted-c6.bin")
            archive.writestr("hosted-c6.version.txt", hosted_version + "\n")
        archive.writestr("host.version.txt", host_version + "\n")
        archive.writestr("slave.version.txt", slave_version + "\n")
        archive.writestr("version.txt", slave_version + "\n")

    extras = (
        f" + hosted-c6.version.txt={hosted_version}" if args.hosted_c6 is not None else ""
    )
    print(
        f"Wrote {args.output} ({' + '.join(members)}"
        f" + host.version.txt={host_version}"
        f" + slave.version.txt={slave_version}{extras})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
