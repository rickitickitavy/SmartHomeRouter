# Design

## Context

See proposal.md — Why. Today, join/bind for lights goes through Arduino `ZigbeeSwitch::findEndpoint` → ZDO match-desc → `esp_zb_zdo_device_bind_req` for On/Off, then `storeBoundDevice` into the slave `BoundZigbeeDevice` table. Search (`SpiCmdPermitJoin` + `startPairingWindow`) clears the whole switch bind cache so ANNCE is not skipped. After settings restore on another router, the host registry is restored and synced (`SpiCmdSetDevice`), but the coordinator bind table / switch `_bound_devices` list is empty, so devices look registered yet unbound. Operators delete + Search + Add to force bind again. Propose versions: host `4.2.56`, slave `4.2.05`.

## Goals / Non-Goals

**Goals:**
- One Rebind action on the Zigbee edit dialog that preserves the registry entry.
- Immediate ZDO bind when short/endpoint are known; pairing-window fallback otherwise.
- Same entry point for **all** Zigbee kinds on the shared join/bind path.
- SPI + API results the UI can show (success / pairing fallback / failure).

**Non-Goals:**
- Auto-rebind of every device on settings restore (operator still chooses Rebind per device).
- Changing settings export/restore shape or Zigbee pan/key migration.
- Replacing Search/Add for brand-new devices.
- Bulk “rebind all” in this change.
- Fixing sleepy-device reachability beyond opening permit-join and clearing the bind-skip cache.

## Decisions

1. **API: `POST /api/devices/rebind` with IEEE**  
   Body/query carries `ieee`. Gate with `editDevices` (same class as editing a registered device). Reject MQTT and unknown IEEE.  
   **Why:** Mirrors `/api/devices/configure` and `/api/devices/command` style; one clear action.  
   **Alternative:** Reuse Search — would still feel like pairing-only and does not try immediate bind.

2. **SPI: new host→slave rebind command + slave→host result**  
   Payload: IEEE (8 bytes). Result: IEEE + outcome enum (`immediateOk`, `pairingFallback`, `failed`). Host waits for the result with a bounded timeout before answering HTTP (or returns pairing fallback if the slave already opened the window).  
   **Why:** Bind runs on the slave; host must not invent radio success.  
   **Alternative:** Host only sends `SpiCmdPermitJoin` — skips immediate bind the user asked for first.

3. **Immediate bind implementation on slave**  
   Resolve usable short from `BoundZigbeeDevice` / registered last-known short / stack `esp_zb_address_short_by_ieee`. Use known endpoints (slot known-EP list, else registered `channels` 1..N). Issue coordinator ZDO bind for the clusters that matter for control/reporting on that path (at minimum the same On/Off bind the switch EP already uses for lights; for other kinds, bind the type-relevant clusters the coordinator already consumes—On/Off, IAS Zone, Window Covering, measurement clusters as applicable—or run match-desc against the short like `findEndpoint` then bind). Call `storeBoundDevice` / add to switch bound list on success so refresh and control paths see the device.  
   **Why:** After restore the device may still be on-network with a known short; delete+re-add is unnecessary.  
   **Alternative:** Only open Search — worse UX when the device is already reachable.

4. **Failure → pairing fallback**  
   Triggers: no usable short; no usable endpoint; ZDO bind status failure; immediate-path timeout. Actions: remove **that** IEEE from `zigbeeSwitch` bound-device list (not necessarily clear-all like Search), `permitJoin` for a finite window (reuse Search duration / permit-join setting), keep registered map entry, report `pairingFallback`.  
   **Why:** Matches today’s working recovery (re-announce bind) without deleting the host record; clearing only the target IEEE avoids disrupting other already-bound devices.  
   **Alternative:** Always clear the entire switch bind cache — broader than needed for one-device Rebind.

5. **UI: Rebind button on parameter dialog footer for Zigbee edit**  
   Visible with Export/Delete when editing a registered Zigbee device; hidden for MQTT and unsaved Add. Show status/error text in the dialog; keep dialog open; busy state on the button while waiting. Enabled styling per enabled-controls rule.  
   **Why:** User asked for the device dialog; that is the edit parameter dialog.  
   **Alternative:** Row overflow menu only — less discoverable next to Delete.

6. **No confirm dialog required for Rebind**  
   Rebind is non-destructive to the registry; show outcome text instead.  
   **Why:** Faster recovery after restore; Delete already has confirm.  
   **Alternative:** Confirm before pairing fallback — optional later if noisy.

## Risks / Trade-offs

- **[Sleepy / offline device]** Immediate bind fails → pairing fallback still needs the end device to rejoin/announce. → Mitigation: clear UI copy that pairing is open; operator wakes/resets the device as today.
- **[Wrong short after restore]** Stale `nwk` may target the wrong node. → Mitigation: prefer stack resolve by IEEE when available; bind failure falls back to pairing; short collision clearing already exists elsewhere.
- **[On/Off-centric Arduino bind]** Stock `ZigbeeSwitch::findEndpoint` only matches On/Off. → Mitigation: rebind path must not stop at On/Off-only; cover all kinds on the shared path (explicit cluster binds or match-desc per kind).
- **[HTTP wait vs AsyncWebServer]** Long ZDO wait can stress the HTTP task. → Mitigation: bounded slave timeout; prefer result event + short host wait; fail over to pairing fallback rather than hanging.
- **[SPI enum growth]** New command/event ids must stay unused. → Mitigation: allocate next free `SpiCmd` / event codes in `SpiProtocol.h` with pack/unpack length checks.

## Migration Plan

1. Ship joined host+slave with propose/apply version bumps.
2. After restore on another router, operators open each Zigbee device → Rebind (no Delete).
3. Rollback: older firmware ignores unknown SPI cmd; UI button absent on old host builds.

## Open Questions

None that block implement: exact HTTP path (`/api/devices/rebind` vs nested under ieee) and SPI numeric ids can follow existing naming/next-id conventions during apply.
