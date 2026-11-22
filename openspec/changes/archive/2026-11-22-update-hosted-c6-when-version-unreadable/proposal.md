## Why

On P4 joined Update, if the host cannot read the running ESP-Hosted C6 version, the session currently **fails** the Hosted step and does not program `hosted-c6.bin`. Operators then cannot recover a C6 whose version API is broken or returns 0.0.0 even when the package carries a valid Hosted image.

## What Changes

- When `hosted-c6.bin` is present and `hosted-c6.version.txt` is valid: if **getting the running Hosted C6 version fails**, the host SHALL **still require and run** the Hosted C6 SDIO update (treat as “needed”), not fail closed.
- Unchanged: absent image → skip; readable running version equals package → skip; readable and differs → apply; missing/empty package version text → still fail (cannot decide skip vs apply by match).
- S3 host path unchanged (no Hosted C6 step).
- Slave firmware unchanged.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `joined-firmware-ota`: Hosted C6 skip/apply decision when running version is unreadable → apply.
- `p4-hosted-c6-ota`: Same decision for the P4 Hosted apply path.

## Impact

- Touches P4 joined Update prepare path in `FirmwareOta` (fragile zone `firmware-update-process`). SPI slave framing unchanged.
- May re-flash Hosted C6 more often when version query fails (including false “unknown” cases).
- Host propose bump **4.6.03**; slave unchanged.
