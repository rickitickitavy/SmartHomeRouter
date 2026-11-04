# Design

## Context

See proposal.md for motivation. Device reports already land on the host S3 (`ZigbeeSpiProxy` live cache / MQTT ingest). Host has 8 MB PSRAM shared with the log ring, cameras, OTA buffers, and action/email queues. There is no existing time-series store or chart UI. `ReportableDataDef` today is `{cluster, attribute, type}` only; device main state is live-only.

## Goals / Non-Goals

**Goals:**
- Predictable PSRAM use via a global 4 KiB page slab sized by Hardware Maximum history size (kB).
- Opt-in history via epsilon on device and each reportable row; UI flag + epsilon with grayed disabled epsilon when off.
- Change-only + deadband sampling; HA-like step charts; binding-scoped chart open on operator and simple-user UIs.
- Age-based page purge and reclaim-on-allocate within the slab.
- Shrink log ring target to 128 KiB to free headroom.

**Non-Goals:**
- Persisting history across reboot (PSRAM wipe is intentional).
- Slave-side history or SPI sync of history bytes / epsilon (host settings only for epsilon).
- Periodic heartbeat samples when value is unchanged.
- Multi-series overlay charts in v1 (one binding → one series).
- Live resize of the slab or entry size without reboot when Maximum history size or Entry size changes (persist + apply at boot).

## Decisions

### D1 — Global 4 KiB slab + free-list (not per-page malloc/free)
- **Choice:** At boot, `heap_caps_malloc` one contiguous region `pageCount * 4096` where `pageCount = floor(maxHistoryKiB * 1024 / 4096)`. Track free slots with a bitmap/free-list in PSRAM (or a small internal index). All history data pages are slab slots.
- **Why:** Matches anti-fragmentation intent under competing odd-sized PSRAM users; Status “pages” maps cleanly to slab used/free.
- **Alternatives:** Per-page `malloc`/`free` (simpler, fragments); one huge ring for all series (harder per-series purge/UI).

### D2 — Entry metadata in PSRAM, size from Hardware Entry size
- **Choice:** Per-series entry in PSRAM sized by Hardware **Entry size** ∈ {256, 512, 1024, 2048, 4096} (default **1024**). Layout: overall `startDate`/`lastDate`, `oldestPageIndex` / `newestPageIndex` (uint16; Tail=oldest purge side, Head=newest append side), then a page-record array `{startDate, lastDate, pageSlotOrAddress}` (12 B each). Max page records = `(EntrySize - 12) / 12` → 20 / 41 / 84 / 169 / 340 for those sizes.
- **Why:** Lets operators trade entry PSRAM vs pages-per-series depth; hard enum avoids odd sizes.
- **Alternatives:** Fixed 1024 only; free-form size field.

### D3 — Epoch seconds since 2010-01-01 UTC
- **Choice:** Store `uint32` seconds since 2010-01-01 00:00:00 UTC. Convert at edges from/to Unix (`unix - 1262304000`). Skip append when wall clock is not sane (same class of check as existing NTP-gated logic).
- **Why:** User-selected; fits uint32 well past current product life.
- **Alternatives:** Raw Unix uint32 (simpler debug); millis-based relative (breaks after reboot/NTP).

### D4 — Epsilon is the persisted Hist flag
- **Choice:** No separate bool in storage. `epsilon < 0` ⇒ off (default `-1`); `>= 0` ⇒ on with deadband. String: `0` on / `< 0` off. Bool: sign only. Device main-state uses the same epsilon field. UI: Hist checkbox + epsilon; unchecked ⇒ gray disabled empty epsilon on load when `< 0`; save unchecked ⇒ `-1`; save checked ⇒ entered `>= 0`.
- **Why:** User decision; one field for enable + deadband.
- **Alternatives:** Separate Hist bool + epsilon (original sketch).

### D5 — Boot preallocate entry + first page; lazy only for later pages
- **Choice:** After settings load, for each series with epsilon `>= 0`, allocate entry + first empty data page. Further pages allocate on demand when the current page is full.
- **Why:** User preference for boot allocation of the first page.
- **Alternatives:** Fully lazy first page (saves empty pages until first change).

### D6 — Per-channel main-state series
- **Choice:** Device epsilon on ⇒ one series per channel (on/off state or covering position only).
- **Why:** User decision; matches multi-channel controls.

### D7 — Sampling and chart semantics
- **Choice:** Append only when changed beyond deadband (string: inequality; bool/state: any change). Charts step-hold last value to “now” / range end (HA style). Numeric = line; binary/actuator state = colored bands.
- **Why:** User chose change-only + deadband; no heartbeat in v1.

### D8 — Hardware settings
- **Fields:** Max History store days (1–20, default 7); History sweep interval minutes (1–120, default 10); Maximum history size kB (default **3072** → 768×4 KiB pages); **Entry size** hard choice 256 / 512 / 1024 / 2048 / 4096 (default 1024).
- **Apply:** Days/sweep take effect on next sweep tick; slab size and entry size applied at boot.

### D9 — Purge and allocate
- Sweep all series on interval; purge pages with `lastDate` older than now − max days, starting at oldest.
- On need-new-page: purge outdated for that series; if still no slot or ring full (`(EntrySize-12)/12`), purge oldest pages until space; then take free slab page, write sample, append page record at newest, update entry dates/indices.

### D10 — UI / API
- Status: Historical storage group (entries, pages, history PSRAM KiB used, values stored) on operator + simple-user Status.
- History query API: series id (device IEEE + kind: main-state channel | reportable cluster/attr) + from/to + maxPoints.
- Binding click → that binding’s series only; sync `index.html` + `user.html`.

### D11 — Log ring 128 KiB
- Change `Logger::kRingCapacityTarget` from `512 * 1024` to `128 * 1024`.

### D12 — Version
- `FIRMWARE_VERSION` = `4.0.01`.

## Risks / Trade-offs

- [Risk] Empty first pages for many enabled series waste slab until first change → Mitigation: document; operators enable Hist sparingly; optional later lazy-first-page.
- [Risk] Large Entry size × many series burns PSRAM on directories alone (e.g. 4096 × N) → Mitigation: default 1024; Status shows history usage; smaller enum values available.
- [Risk] Slab too large starves OTA/camera peaks → Mitigation: conservative factory max-history kB; Status shows usage; fail closed if slab alloc fails.
- [Risk] Custom 2010 epoch bugs at API/UI boundary → Mitigation: single conversion helper; tests for known Unix↔epoch pairs.
- [Risk] Float noise still chatty with epsilon `0` → Mitigation: UI default off; operators set deadband; bool/string unaffected.
- [Risk] Chart payloads large → Mitigation: maxPoints + downsample in query API.
- [Risk] Enabled-controls-not-gray: epsilon gray only when disabled → Mitigation: disable/readonly when Hist unchecked (allowed).

## Migration Plan

1. Ship firmware 4.0.01 with new settings fields defaulting history off and log ring 128 KiB.
2. Existing `devices.json` without epsilon ⇒ treat as `-1`.
3. Old Hardware clients omitting history fields ⇒ keep defaults / prior stored values.
4. Rollback: flash previous image; history is volatile anyway; epsilon fields ignored by older firmware if present in JSON (older builds must ignore unknown keys — verify parse paths tolerate extras).

## Open Questions

- Precise HTTP path and JSON shape for history query (implement to match existing `/api/...` auth style).
