#!/usr/bin/env python3
"""Deny git commit/push that targets master or develop — no user override."""

from __future__ import annotations

import json
import os
import re
import subprocess
import sys


PROTECTED = frozenset({"master", "develop", "main"})


def current_branch(cwd: str | None) -> str | None:
    try:
        result = subprocess.run(
            ["git", "rev-parse", "--abbrev-ref", "HEAD"],
            cwd=cwd or None,
            capture_output=True,
            text=True,
            timeout=5,
            check=False,
        )
    except Exception:
        return None
    if result.returncode != 0:
        return None
    name = (result.stdout or "").strip()
    if not name or name == "HEAD":
        return None
    return name


def is_git_commit(command: str) -> bool:
    return bool(
        re.search(r"(?:^|[;&|]\s*|&&\s*)git(?:\s+-C\s+\S+)?\s+commit\b", command)
    )


def is_git_push(command: str) -> bool:
    return bool(
        re.search(r"(?:^|[;&|]\s*|&&\s*)git(?:\s+-C\s+\S+)?\s+push\b", command)
    )


def push_targets_protected(command: str) -> bool:
    # Explicit refspecs / branch names in the push line.
    for name in PROTECTED:
        if re.search(rf"(?:^|\s)(?:origin/)?{name}(?:\s|:|$)", command):
            return True
        if re.search(rf":(?:refs/heads/)?{name}\b", command):
            return True
    return False


def deny(message: str) -> int:
    print(
        json.dumps(
            {
                "permission": "deny",
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
    cwd = payload.get("cwd") or payload.get("working_directory") or os.getcwd()
    if isinstance(cwd, str) and cwd.strip():
        cwd = cwd.strip()
    else:
        cwd = os.getcwd()

    if not command.strip():
        print(json.dumps({"permission": "allow"}))
        return 0

    branch = current_branch(cwd)

    if is_git_commit(command):
        if branch in PROTECTED:
            return deny(
                f"Hard rule: never commit on `{branch}`. "
                "Create/switch to a feature branch first. This cannot be overridden."
            )

    if is_git_push(command):
        if branch in PROTECTED:
            return deny(
                f"Hard rule: never push while on `{branch}`. "
                "Push from a feature branch only. This cannot be overridden."
            )
        if push_targets_protected(command):
            return deny(
                "Hard rule: never push to master/develop/main. "
                "Push the feature branch and open a PR. This cannot be overridden."
            )

    print(json.dumps({"permission": "allow"}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
