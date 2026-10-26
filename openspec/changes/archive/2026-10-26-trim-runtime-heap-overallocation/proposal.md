# Proposal

## Why

Host RAM still over-reserves for runtime caches and console bindings: every `EndpointStatus` embeds a 384-byte `state` buffer (userData max) plus covering fields, every `CachedDevice` keeps empty `manufacturer`/`model` arrays, and every `ConsoleBinding` pays for gauge/icon/logical fields even when unused. On a real settings dump this wastes tens of kilobytes that the gateway needs for Wi-Fi/MQTT/heap. The standing rule is: reserve only what is actually needed.

## What Changes

- **EndpointStatus**: stop embedding `char state[384]` and fixed covering buffers. Allocate channel slots to the device’s configured channel count only (already the intent; keep that exact). Store status text as a heap string sized to the value (clamped to the existing userData max when copying). Allocate covering/discrete extras only when that endpoint actually uses them.
- **CachedDevice `manufacturer` / `model`**: they **are** used today — SPI join fill, `devicesJson` / bound-device table via `BoundDeviceJson`. They are **not** needed as fixed 32+32 bytes on every slot (MQTT devices never set them). Replace with heap pointers allocated only when non-empty; free on slot release. Do **not** drop the fields from the product/API surface.
- **ConsoleBinding**: move view-style-specific and optional blocks behind pointers allocated only when needed — at least gauge config (`type`/`min`/`max`/`ranges`) only when `viewStyle == gauge`; icon config only when `viewStyle == icon`; logical-threshold block only when `asLogical`. JSON load/save and UI behavior stay the same.
- **`FIRMWARE_VERSION`**: propose bump `3.5.69` → `3.5.70` (apply bumps again when source lands).

### Heap estimate for `settings (11).json` (host structs in scope)

Measured with host ABI sizes (`sizeof`): `EndpointStatus` 425, `CachedDevice` 112, `ConsoleBinding` 152.

Config: **25** devices, **28** channels (24×1 + 1×4), **42** bindings (19 default / 16 gauge / 6 icon / 1 spacer), **6** Zigbee / **19** MQTT.

| Bucket | Before (reserved) | After (est. after typical status fill) |
|--------|-------------------|----------------------------------------|
| EndpointStatus channel arrays | 28 × 425 = **11 900** | 28 × ~24 structs + state strings (~400) + cover extras (~80) ≈ **1 160** |
| CachedDevice slots | 25 × 112 = **2 800** | 25 × ~64 = **1 600** + manu/model heap only when set (≈0–200) |
| ConsoleBinding structs (+ side records) | 42 × 152 = **6 384** | 42 × ~128 + 16×gauge + 6×icon + 1×logical ≈ **5 860** |
| **Focused total** | **≈ 21 084 B** | **≈ 8 6xx–8 8xx B** |
| **Saved** | | **≈ 12 kB (~58%)** |

At boot with empty status strings the EndpointStatus win is larger still (~11.9 kB → ~0.7 kB for the channel table alone), because today the 384-byte buffers are reserved even when unused.

String payloads already on the heap (titles, IEEE text, icon names, gauge colors, click-action JSON) are unchanged and excluded from the table.

## Capabilities

### New Capabilities

- `runtime-heap-allocation`: Host runtime caches and console binding storage MUST reserve only the memory required for the configured channel count and for optional blocks that are actually present (status text, covering extras, manufacturer/model, gauge/icon/logical config).

### Modified Capabilities

- (none — external console JSON, device registry, and SPI join payload shapes stay the same; this is allocation policy inside existing host structures.)

## Impact

- Host: `include/ZigbeeSpiProxy.h`, `src/ZigbeeSpiProxy.cpp` (`EndpointStatus`, `CachedDevice`, status update/read paths, `devicesJson`).
- Host: `include/ConsoleStore.h`, `src/ConsoleStore.cpp` (parse/free/serialize bindings; gauge/icon/logical side records).
- Call sites that touch `state[]` / covering buffers / `manufacturer`/`model` arrays / inline gauge fields must use the pointer forms.
- No settings schema or web API JSON shape change expected.
- `FIRMWARE_VERSION` in `include/Defines.h` bumped at propose to `3.5.70`.
