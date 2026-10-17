#!/usr/bin/env python3
"""Inject no-unilateral-behavior-change policy on every user prompt."""

from __future__ import annotations

import json
import sys

CONTEXT = (
    "HARD RULE (all projects): NEVER change behavior the human has described "
    "without their explicit choice. If a fix or OpenSpec plan would redefine "
    "product rules (triggers, conditions, value-must-change vs rising-edge, "
    "re-fire semantics, etc.), STOP. Propose options, describe trade-offs, "
    "ask which behavior they want, and wait. Do not implement or write "
    "planning that picks a side until they answer."
)


def main() -> int:
    try:
        json.load(sys.stdin)
    except Exception:
        pass
    print(
        json.dumps(
            {
                "continue": True,
                "additional_context": CONTEXT,
            }
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
