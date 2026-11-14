#!/usr/bin/env python3
"""Deny git commands that roll back / discard / restore working-tree or history."""

from __future__ import annotations

import json
import re
import sys


# git [-C path] <subcommand>
GIT_PREFIX = r"(?:^|[;&|]\s*|&&\s*)git(?:\s+-C\s+\S+)?"


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


def allow() -> int:
    print(json.dumps({"permission": "allow"}))
    return 0


def is_rollback(command: str) -> str | None:
    """Return a short reason if this is a forbidden git rollback command."""
    # Explicit restore / reset / revert / clean (destructive discard).
    if re.search(rf"{GIT_PREFIX}\s+restore\b", command):
        return "git restore"
    if re.search(rf"{GIT_PREFIX}\s+reset\b", command):
        return "git reset"
    if re.search(rf"{GIT_PREFIX}\s+revert\b", command):
        return "git revert"
    if re.search(rf"{GIT_PREFIX}\s+clean\b", command):
        return "git clean"

    # checkout / switch that discard local changes (-f / --force / --discard-changes)
    if re.search(
        rf"{GIT_PREFIX}\s+(?:checkout|switch)\b[^\n;|&]*?(?:--force|-f|--discard-changes)\b",
        command,
    ):
        return "git checkout/switch -f (discards changes)"

    # Path restore forms: git checkout [--] <paths>  OR  git checkout <tree-ish> -- <paths>
    # Allow plain branch/ref switch: git checkout <branch>  (no -- paths, no -f).
    if re.search(rf"{GIT_PREFIX}\s+checkout\b", command):
        # `git checkout -- <paths>` or `git checkout <rev> -- <paths>`
        if re.search(rf"{GIT_PREFIX}\s+checkout\b[^\n;|&]*?\s--\s+\S+", command):
            return "git checkout -- <paths> (path restore)"
        # `git checkout -p` / `--patch` restores hunks
        if re.search(
            rf"{GIT_PREFIX}\s+checkout\b[^\n;|&]*?(?:--patch|-p)\b",
            command,
        ):
            return "git checkout -p (hunk restore)"

    if re.search(rf"{GIT_PREFIX}\s+switch\b", command):
        if re.search(
            rf"{GIT_PREFIX}\s+switch\b[^\n;|&]*?(?:--discard-changes|-f|--force)\b",
            command,
        ):
            return "git switch -f (discards changes)"

    return None


def main() -> int:
    try:
        payload = json.load(sys.stdin)
    except Exception:
        return allow()

    command = str(payload.get("command") or "")
    if not command.strip():
        return allow()

    reason = is_rollback(command)
    if reason:
        return deny(
            "Hard rule: NEVER use git for rollback "
            f"({reason}). Edit files forward and commit; "
            "do not restore/reset/revert/clean or path-checkout. "
            "This cannot be overridden."
        )

    return allow()


if __name__ == "__main__":
    raise SystemExit(main())
