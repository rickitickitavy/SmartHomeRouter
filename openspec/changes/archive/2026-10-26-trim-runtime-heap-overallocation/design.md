# Design

## Context

See proposal.md for motivation and the measured heap table against `settings (11).json`.

Today on the host:

- `ZigbeeSpiProxy::EndpointStatus` embeds `char state[kUserDataValuesMaxBytes]` (384) plus `coveringMove[16]`, `coveringPosition[8]`, `discreteStatus[16]` on **every** channel slot. Channel **count** is already grown via `ensureChannelCapacity` / `channelCapacityForIeee` from the registry.
- `CachedDevice` embeds `char manufacturer[32]` and `char model[32]` always; filled on SPI join and read by `devicesJson` → `appendBoundRadioDeviceJson`.
- `ConsoleBinding` inlines gauge (`gaugeType`/`gaugeMin`/`gaugeMax`/`hasGauge`/`gaugeRanges`), icon (`icon`/`iconScale`/`iconAlign`), and logical (`asLogical`/`logicalThreshold`) fields on every binding; parse already clears unused gauge/icon content but the struct bytes remain.

Slave `ZigbeeCoordinator` / `FoundDeviceList` already use heap `char *` for manufacturer/model; this change is host-side over-reservation.

`FIRMWARE_VERSION` propose bump is already `3.5.70`.

## Goals / Non-Goals

**Goals:**

- Exact channel-table capacity = configured channel count (keep; fix any path that still jumps to `DEVICE_CHANNEL_COUNT_MAX` when a concrete count is known).
- Replace fixed status/covering/manu/model/optional-binding reserves with allocate-on-use heap.
- Preserve JSON schemas and runtime control/automation behavior.

**Non-Goals:**

- Shrinking `kUserDataValuesMaxBytes` / userData field limits.
- Changing SPI join frame layout (still carries 31+31 manufacturer/model bytes on the wire).
- Reworking composite cell layout fields that every binding may need (`fontScale`, `alignH`/`alignV`, `showTitle`, `titlePosition`).
- Slave coordinator device maps (already pointer-based for manu/model).
- Broader heap audits outside these three structures.

## Decisions

### 1. EndpointStatus: slim slot + heap strings + optional cover block

**Choice:** Change to roughly:

```text
struct EndpointStatus {
  uint8_t endpoint;
  char *state;                 // strdup of current value; nullptr if unset
  CoveringStatus *covering;    // nullptr until move/position/discrete needed
};
struct CoveringStatus {
  char move[16];
  char position[8];
  char discrete[16];
};
```

Helpers: `setStatusString(slot, text)` (free + strdup, clamp length to `kUserDataValuesMaxBytes`), `clearStatusString`, `ensureCovering(slot)` (malloc once), free covering + state in `releaseSlot` / when shrinking or clearing a channel.

**Why:** Removes 384 + 40 bytes of dead weight per channel. Covering block (~40 B) only for window-covering / IAS-style paths that write those fields.

**Alternatives:** Keep covering fields as three `char*` (more malloc churn for short tokens); keep fixed covering arrays in the slim struct (~40 B always) — still a large win vs 425 but violates “only if needed” for on/off majority. Rejected fixed covering arrays.

**Empty-slot detection:** Replace `state[0] != '\0'` with `state != nullptr && state[0] != '\0'` (and covering presence checks via pointer).

### 2. Channel capacity stays registry-exact

**Choice:** Keep `channelCapacityForIeee` returning `entry->channelCount` when ≥ 2, else 1. Only `DEVICE_CHANNEL_PARSE` may use `DEVICE_CHANNEL_COUNT_MAX`. Do not grow capacity “just in case” beyond configured count except the existing endpoint>capacity clamp when a higher endpoint is observed (document as last-resort grow).

**Why:** Matches the user’s channel-count rule; PARSE devices are rare and already special-cased.

### 3. CachedDevice manufacturer/model → `char *`

**Choice:** Replace arrays with `char *manufacturer` / `char *model` (nullptr when empty). On join, `duplicateBoundedString(..., 32)` like FoundDeviceList. Free in `releaseSlot`. `devicesJson` passes pointers (nullable) unchanged to `BoundDeviceJson`.

**Why:** Fields **are** used for the bound-device table and join cache; removing them would blank the Devices UI column. Fixed 64 B × N devices is pure waste for MQTT-only slots.

**Alternatives:** Drop from `CachedDevice` and read only from `FoundDeviceList` — found list is pairing-ephemeral and cleared after register; bound list would lose manu/model after reboot until re-join. Rejected.

### 4. ConsoleBinding optional side records

**Choice:**

```text
ConsoleGaugeConfig  { type, min, max, ranges* }  // only if viewStyle == gauge
ConsoleIconConfig   { icon*, scale, align }      // only if viewStyle == icon
ConsoleLogicalConfig { threshold }               // only if asLogical
```

Core `ConsoleBinding` keeps shared fields + nullable pointers to the three configs. Parse: allocate gauge/icon/logical only when applicable (same rules as today for clearing unused). Free helpers extended. Serialize: emit JSON only when pointer non-null (same as today’s conditional emit).

**Why:** Directly implements “gauge fields only if gauge; and so on.”

**Alternatives:** Tagged union / variant by viewStyle — heavier C++ on Arduino; inheritance — worse. Pointer side records match existing `gaugeRanges` / click-action JSON style.

Keep `asLogical` bool on the core struct (1 byte) so readers do not chase a pointer for a flag; allocate `ConsoleLogicalConfig` only for the float. Alternatively put the flag only on the side record — either is fine; prefer side-record-only if it shrinks core without awkward null checks at every call site.

### 5. Allocation discipline

**Choice:** Use existing `ConsoleStore::heapAllowsAllocation` / `duplicateBoundedString` patterns; for ZigbeeSpiProxy add small helpers and fail soft (keep previous value / skip update) on OOM, consistent with current channel alloc warnings.

## Risks / Trade-offs

- **[Risk] More malloc/free churn on frequent status updates** → Mitigation: if new text equals old (`strcmp`), do not reallocate; only replace when changed (already have `valueChanged`).
- **[Risk] Missed call site still assumes fixed arrays** → Mitigation: compile will fail on `sizeof(state)` / array decay; grep `coveringMove` / `.manufacturer` / `hasGauge` / `gaugeMin` during apply.
- **[Risk] Fragmentation from many small strings** → Mitigation: acceptable vs reserving 384×channels; lengths are tiny for non-userData.
- **[Trade-off] Pointer side records add 4–12 B of pointers on core binding** → Still net win vs inlining floats for every binding; measured ~24 B/struct shrink plus avoided gauge/icon payload on non-matching rows.

## Migration Plan

- Firmware-only; no settings migration. Existing console JSON loads unchanged.
- Rollback: revert the structural change; settings files remain compatible.

## Open Questions

None that block apply. (Whether `titlePosition` stays on core vs gauge-only can follow usage: today default bindings may omit it; keep on core unless apply shows it is gauge-only in UI.)
