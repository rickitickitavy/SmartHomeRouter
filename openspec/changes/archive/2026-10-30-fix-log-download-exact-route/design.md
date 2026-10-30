# Design

## Context

See proposal.md — Why. Product already requires full-ring download vs a 64 KiB on-screen tail (`GET /api/log` uses `snapshotRingTail`; `GET /api/log/download` uses `snapshotRing`). ESP32Async string routes default to BackwardCompatible matching (`/path` and `/path/...`); first match wins. `/api/log` was registered before `/api/log/download` without `exact()`, so download always hit the view-tail handler. The same trap was already guarded for `/api/users`, `/api/consoles`, `/api/automations`, and `/api/devices`.

## Goals / Non-Goals

**Goals:**
- Make `/api/log/download` reach the full-ring handler every time.
- Keep `/api/log` as the bounded view-tail API.
- Leave a durable route-registration pattern so sibling log paths cannot collapse again.

**Non-Goals:**
- Changing ring capacity, PSRAM allocation, or on-screen view cap.
- Reworking client download mechanics (`<a href="/api/log/download">` already correct).
- Broad rewrite of all non-exact routes that have no child paths.

## Decisions

1. **`AsyncURIMatcher::exact` for both log routes; register download first**  
   Exact match stops prefix steal. Ordering is belt-and-suspenders if exact is ever dropped on the parent.  
   *Alternatives:* Only reorder download before `/api/log` — rejected; BackwardCompatible on `/api/log` would still steal if registration order flips later. Rename download off the `/api/log/` prefix — rejected; unnecessary API churn.

2. **Keep Content-Length callback streaming in `handleLogDownloadGet`**  
   Handler body is already correct; the bug was routing. No LittleFS staging required for this fix.

3. **Document exact-matcher rule in the AsyncWebServer skill**  
   Prevents repeating the users/consoles/log class of bugs on the next parent/child API pair.

## Risks / Trade-offs

- **[Risk] Other parent routes still use BackwardCompatible with undeclared children** → Mitigation: this change only hardens log; skill calls out the pattern; existing exact() parents stay as models.
- **[Risk] Operators on old firmware still see 64 KiB downloads** → Mitigation: ship host build `3.9.14+`; verify downloaded size vs `X-Log-Ring-Used` / ring used length after flash.

## Migration Plan

- Host firmware + LittleFS assets if packaged together; route change is host C++ only.
- Rollback: previous image restores prefix steal (broken download).
