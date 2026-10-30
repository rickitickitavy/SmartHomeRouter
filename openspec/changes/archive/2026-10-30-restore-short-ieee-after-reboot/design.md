# Design

## Context

See proposal.md — Why. Today `resolveIeeeFromSource` can recover IEEE from the stack table, bound RAM slots, registered IEEEs via `esp_zb_address_short_by_ieee`, neighbors, or `fillIeeeFromUniqueUnresolved` (only when **exactly one** registered IEEE still has no short). After a coordinator reboot with many registered devices, almost all bound shorts are empty, so unique-orphan recovery fails. Sleepy end devices keep reporting on their old NWK short; the slave forwards `ieee=00:…` and the host drops the report as unmapped. `DeviceTopicEntry` has no persisted NWK field; SPI device sync ends at the type byte (`SPI_DEVICE_SYNC_ENTRY_LEN` ≈ 228, room for two more bytes under `SPI_MAX_PAYLOAD` 256). `FIRMWARE_VERSION` propose bump is `3.9.21`.

## Goals / Non-Goals

**Goals:**
- Survive host+slave reboot without re-pair when last-known shorts are still valid on air.
- Map short-only reports for **all** Zigbee device kinds on the shared attr/zone report path.
- Keep host as durable store; slave RAM map only.
- Extend SPI sync backward-compatibly (legacy frames without short → short `0`).

**Non-Goals:**
- Changing pan id / channel / network key, or forcing re-join for every device.
- New operator UI to edit NWK by hand.
- Fixing APS delivery to asleep end devices (status-read timeouts remain expected).
- Inferring IEEE when multiple unknowns share one short with no stored mapping.

## Decisions

1. **Persist last-known short on the host device record**  
   Add `uint16_t shortAddr` (or equivalent) on `DeviceTopicEntry`, serialize as `nwk` in `devices.json` / list JSON (hex or number; match existing found-list style if present).  
   **Why:** Stack tables clear on radio restart; host LittleFS does not.  
   **Alternative:** IEEE address discovery on every miss — fails while sleepy devices are asleep; slower; still need a durable cache.

2. **Extend SPI sync with a 2-byte little-endian trailer after type**  
   Bump `SPI_DEVICE_SYNC_ENTRY_LEN` by 2; pack/unpack accept shorter legacy lengths.  
   **Why:** Same path already pushes the full registry after `SLAVE_READY`; no new SPI command.  
   **Alternative:** Separate “seed shorts” command — more protocol surface for the same data.

3. **Seed `rememberShortIeee` in `upsertRegisteredDevice` (and on sync apply)**  
   When synced short is usable, bind short→IEEE before boot status reads. On short ownership change, clear the previous IEEE’s bound short for that value.  
   **Why:** Makes `findByShortAddr` succeed on the first post-reboot report.  
   **Alternative:** Only improve `fillIeeeFromUniqueUnresolved` — still wrong with many orphans.

4. **Host refreshes stored short from joins / identity / mapped attr reports**  
   When host sees usable ieee+short for a registered Zigbee device, update store and clear collisions; mark device persist due.  
   **Why:** Shorts can change after real rejoin (as Flower-sensor 2 did: new `0xa369`); stale shorts must not stick forever.  
   **Alternative:** Never update after first learn — wrong after address changes.

5. **Resolution order stays stack-first, then seeded map**  
   Keep existing stack/neighbor lookups; use seeded bound map when those miss (already partially true via `findByShortAddr` once seeded).  
   **Why:** Prefer live stack truth when present; fall back to durable last-known.

## Risks / Trade-offs

- **[Stale short after silent rejoin]** Device gets a new short; old short is reused by another node → wrong IEEE until a correct ieee+short report arrives. → Mitigation: clear colliding shorts on update; prefer stack IEEE when available; operator can re-pair the orphan.
- **[Cold install / empty nwk]** Devices never reported since feature shipped still unmapped until first identity. → Mitigation: first successful resolve/join populates `nwk`; document that one wake after upgrade is enough.
- **[SPI length drift]** Host/slave firmware skew during OTA. → Mitigation: unpack treats missing trailer as short `0`; joined OTA keeps both sides aligned when possible.
- **[Persist churn]** Frequent short updates rewrite `devices.json`. → Mitigation: persist only when short value changes.

## Migration Plan

1. Ship host+slave with propose/apply version bump; existing `devices.json` without `nwk` loads as short `0`.
2. After upgrade, one poll/identity per device (or any mapped report) fills `nwk`; subsequent reboots use the seed path.
3. Rollback: older firmware ignores extra sync bytes if length checks allow, or rejects oversize — keep trailer optional on unpack; pack only with matching paired builds via joined update when feasible.

## Open Questions

None that block implement: JSON `nwk` encoding (decimal vs `0x` hex) can follow existing found-list / bound JSON helpers already in tree.
