# Design

## Context

See proposal.md — Why. Grounding from current code:

- `historyPersistFlashField` is force-hidden when `isRepeater` in `data/index.html`.
- `InterconnectPeer.deviceCount` is serialized but never recomputed from the device map.
- Paired peer dialog still fills Online and Devices.
- Interconnection tab shows `Queue: used / depth`; Status Interconnection does not.
- Operator and simple-user Status still say **Historical storage**, **Values stored**, **on-SD history size**, and **Slave**.
- Search devices dialog is too narrow for IEEE / Type / Router headers (screenshot).
- History retention, SD/flash persist, and related fields live under System → Hardware.
- `InterconnectStore::importJson(..., keepExistingMacAuthoritative=true)` is used by Maintenance import and settings restore.
- `INTERCONNECT_QUEUE_DEPTH` is already **1024** in `Defines.h` (main router-interconnect spec still said 128).

## Goals / Non-Goals

**Goals:**

- Align Status labels and Interconnection Status content on both pages.
- Correct Repeater device counts and restore flash-persist visibility rules (role + SD).
- Move history storage settings to System → Internal DB after Hardware.
- Make interconnect import/restore a full paired-table replace.

**Non-Goals:**

- Changing interconnect AEAD/protocol, queue depth value (already 1024), or rebind/re-pair flows.
- Slave firmware changes.
- Redesigning Search beyond dialog/table width so headers/cells fit one line.

## Decisions

### 1. Recompute `deviceCount` from DeviceTopicMap by `router_id`

Whenever peers are serialized for HTTP/WS/status, count registered Zigbee devices whose `router_id` equals the peer’s router id. Prefer a single helper on the store or interconnect layer called from list/status refresh paths so table, dialog (even if Devices is removed), and Status stay consistent.

**Alternatives:** Increment on bind/unbind only — rejected as easier to desync after restore/import.

### 2. Flash persist: SD presence wins; Repeater may show control

Boot/reconcile path already forces persist off when SD history is active; extend so “SD present/usable” always clears and persists `historyPersistFlash=false` and the UI hides the control. Remove the Repeater-only hide of that field; after the Internal DB move, Repeater sees Internal DB history settings including flash persist when SD is absent.

**Alternatives:** Keep hiding on Repeater — rejected; user asked for the setting to appear again on Repeater.

### 3. Full override import via `keepExistingMacAuthoritative=false`

Maintenance interconnect import and global settings restore SHALL call `importJson(..., false)` (or equivalent replace API). Export already includes peers; no schema change required beyond ensuring the slice remains in export.

**Alternatives:** Soft merge with operator toggle — rejected; user asked for FULL override.

### 4. Status queue + version in Interconnection widget

Move the queue used/depth rendering from the Interconnection settings tab into Status Interconnection (both `index.html` and `user.html`). Include each peer’s `hostVersion` in the Status rows. Keep `/api/status` or existing interconnect status JSON fields as the source; add queue fields to the Status poll payload if not already present there.

### 5. Internal DB tab owns history storage settings

Cut history retention, entry size, max size, SD backup interval, flash persist, and flash interval from Hardware markup into a new System nested tab **Internal DB** after Hardware. Reuse the same `/api/hardware` keys for save/load to avoid a second API in this change. Hardware Repeater allow-list unchanged for SPI/LED/queue/low-memory; Internal DB remains visible on Repeater.

### 6. Search dialog width

Increase `#searchDialog` / found-table min-width (CSS) so Router header and long IEEE/Type/Manufacturer cells stay one line on typical desktop widths. No column removal.

## Risks / Trade-offs

- [Full override deletes local peers absent from file] → Mitigation: confirm dialog already on import; document BREAKING in proposal.
- [Internal DB still posts `/api/hardware`] → Acceptable short-term; rename endpoint later only if needed.
- [Status parity drift] → Apply the same label/string changes in `index.html` and `user.html` in one pass.

## Migration Plan

1. Deploy host firmware + LittleFS UI together.
2. Existing flash-persist ON with SD present becomes OFF at first boot reconcile.
3. Operators who relied on MAC-authoritative interconnect import must re-export after pairing; restore/import now replaces the table.

## Open Questions

None — all fourteen items are specified by the user request.
