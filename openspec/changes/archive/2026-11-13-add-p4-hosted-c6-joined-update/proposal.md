## Why

P4 Wi‑Fi uses ESP-Hosted to an on-board C6 coprocessor. When a joined Update package carries a Hosted C6 image, the P4 host must apply it only if that file is present and the running coprocessor version does not already match.

## What Changes

- Joined ZIP MAY include optional `hosted-c6.bin` + `hosted-c6.version.txt`.
- After the package is fully staged on a P4 host:
  - no `hosted-c6.bin` → skip Hosted update
  - present + running version equals package version → skip
  - present + versions differ → SDIO-apply that image
- Join tool can optionally add those two members.
- Host LED3 (blue) indicates web activity on S3 and P4: ON for any HTTP/WebSocket request, OFF when that response finishes.
- No Status polling of Hosted version, no embed in host partition, no SPI protocol changes.

## Capabilities

### New Capabilities

- `p4-hosted-c6-ota`: Present-and-needed Hosted C6 apply from joined ZIP on P4.

### Modified Capabilities

- `joined-firmware-ota`: Optional Hosted C6 members; skip/apply rules above.
- `status-rgb-led`: Host LED3 blue web-request indicator (S3 and P4).

## Impact

- Touches joined Update / join tooling (fragile zone `firmware-update-process`); SPI slave OTA framing unchanged.
- Operator still uploads one ZIP.
- Host status LED3 / web console request path.
