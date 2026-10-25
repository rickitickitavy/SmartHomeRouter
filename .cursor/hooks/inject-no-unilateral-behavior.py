#!/usr/bin/env python3
"""Inject no-unilateral-behavior-change policy on every user prompt."""

from __future__ import annotations

import json
import sys

CONTEXT = (
    "HARD RULE (all projects): Do NOT invent or replace product behavior "
    "the human did not ask for. If YOU would redefine product rules "
    "(triggers, conditions, value-must-change vs rising-edge, re-fire, etc.) "
    "without the human requesting that change, STOP — propose options, "
    "describe trade-offs, ask, and wait. "
    "If the human already directed the behavior (including OpenSpec propose/"
    "write proposal/add fix/do this in current change), implement or plan "
    "that directed change — do NOT demand a separate approval for their own request."
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
