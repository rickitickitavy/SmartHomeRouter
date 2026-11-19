#!/usr/bin/env python3
"""Ask the human before any firmware/device flash upload. Never auto-allow."""

from __future__ import annotations

import json
import re
import sys


def is_firmware_upload(command: str) -> bool:
    cmd = command or ""
    if not cmd.strip():
        return False

    # PlatformIO / pio: -t upload, --target upload, uploadfs
    if re.search(r"\b(?:pio|platformio)\b", cmd):
        if re.search(r"(?:^|[;&|]\s*|&&\s*)(?:\S+\s+)*(?:-t|--target)\s+upload(?:fs)?\b", cmd):
            return True
        # e.g. pio run -e x upload (legacy positional)
        if re.search(r"\brun\b.*\bupload(?:fs)?\b", cmd):
            return True

    # esptool write_flash / erase+write
    if re.search(r"\besptool(?:\.py)?\b", cmd) and re.search(
        r"\b(?:write_flash|erase_flash|erase_region)\b", cmd
    ):
        return True

    # ESP-IDF flash
    if re.search(r"\bidf\.py\b", cmd) and re.search(r"\bflash\b", cmd):
        return True

    # arduino-cli upload
    if re.search(r"\barduino-cli\b", cmd) and re.search(r"\bupload\b", cmd):
        return True

    return False


def ask(message: str) -> int:
    print(
        json.dumps(
            {
                "permission": "ask",
                "user_message": message,
                "agent_message": message,
            }
        )
    )
    return 0


def main() -> int:
    try:
        payload = json.load(sys.stdin)
    except Exception:
        print(json.dumps({"permission": "allow"}))
        return 0

    command = str(payload.get("command") or "")
    if not is_firmware_upload(command):
        print(json.dumps({"permission": "allow"}))
        return 0

    return ask(
        "Hard rule: ask before firmware upload. "
        "Approve only if you want this flash/upload to run on the device now.\n"
        f"Command: {command}"
    )


if __name__ == "__main__":
    raise SystemExit(main())
