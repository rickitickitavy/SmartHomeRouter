# Design

## Context

See proposal.md for motivation. Observed: `InterChipSlave::kMaxDeferredDeviceCommands` is **32** in `include/InterChipSlave.h`. Slots are heap-allocated (`calloc`) when a new IEEE+endpoint appears; same destination reuses the slot. `applyDeferredRadioCommands` drains into ZigbeeCoordinator with existing in-flight gating. SPI slave→host outbound (`kQueue = 32`) is a different queue and is out of scope.

`FIRMWARE_VERSION` propose bump: `3.5.68`.

## Goals / Non-Goals

**Goals:**

- Raise deferred Zigbee device-command capacity to **128**.
- Preserve replace-by-IEEE+endpoint and in-flight semantics.

**Non-Goals:**

- Changing host `InterChipHost::kOutQueue`, slave SPI `kQueue`, or status-read queue sizes.
- Changing radio in-flight concurrency or APS stack tuning.

## Decisions

1. **Constant only** — Set `kMaxDeferredDeviceCommands` from `32` to `128`. No data-structure redesign; linked list + count already supports N slots.  
   *Alt considered:* fixed static array of 128 — more RAM always reserved; keep heap-on-demand.

2. **Memory** — Worst case ~128 × `sizeof(DeferredDeviceCommand)` on heap when full. Acceptable on slave for operator-requested headroom. Monitor if OOM appears under extreme load; not expected for typical device counts.

3. **Logging** — Keep existing `"Deferred device command table full"` warning at the new limit.

## Risks / Trade-offs

- [Risk] More deferred commands increase peak latency before radio drain → Mitigation: in-flight rule still serializes per destination; capacity only absorbs bursts.
- [Risk] Heap fragmentation from many calloc/free cycles → Mitigation: slots freed when applied; same as today at larger N.

## Migration Plan

- Rebuild/flash slave (and joined firmware as usual). No persisted schema change.
- Rollback: restore constant to 32.

## Open Questions

- None.
