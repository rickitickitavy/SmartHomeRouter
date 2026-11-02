# Proposal

## Why

Operators need ONVIF cameras in the same console and automation surface as Zigbee devices: a global registry, console cells that show a live or refreshing picture, and optional snapshot attachments on email actions. Today the gateway has no camera entity, no Cameras UI, and email sends are text-only.

## What Changes

- Add a global operator sidebar section **Cameras** with a registered-cameras table (Active, Name, Type, URL, Users, Status) and card footer **Add** / **Export** / **Import**.
- Add a Camera config dialog: Name, Active, Type (dropdown; **ONVIF** only for now), and ONVIF settings sufficient to show images in the console via browser-direct URLs (`<img src="http://cam-ip/...">` — approach **A**: media does not proxy through the ESP) plus two snapshot channels (two resolutions).
- Extend composite (and equivalent) widget cell content with **Camera** (alongside Empty / Text / Device). Console view offers **JPEG with refresh** or **live video**, with channel selection. Rendering uses the stored HTTP URLs in the browser; the host MUST NOT buffer console images.
- Extend automation **e-mail** actions to optionally attach a camera snapshot with channel selection. Host fetch of that JPEG (if any) MUST use **PSRAM only** — **never** internal RAM for capturing or holding image bytes.
- Persist the full camera registry in **PSRAM** (rare-path data), with durable backup on LittleFS (or equivalent) so settings survive reboot without living on the scarce internal heap.
- Propose bump: `FIRMWARE_VERSION` `3.9.30` → `3.9.31` (apply bumps again when source lands).

## Capabilities

### New Capabilities

- `onvif-cameras`: Host camera registry (Active, Name, Type, URLs, users who may view, status), ONVIF-type settings with two channels (snapshot + browser-playable live URL), PSRAM-backed runtime store, reachability status without holding images in internal RAM, import/export, and APIs used by console and email.

### Modified Capabilities

- `web-console`: Cameras sidebar section, table, Add/Export/Import, camera dialog; console editor camera cell; Place / simple-user camera rendering (JPEG refresh vs live + channel).
- `control-consoles`: Persist and round-trip camera cell content (camera id, display mode, channel) on composite (and related) widgets; view-mode render rules; ACL so only allowed users see a camera.
- `automations`: E-mail action gains optional camera snapshot attachment (camera + channel); fields round-trip through save/list/import/export.
- `outbound-email`: Async send MAY include one JPEG attachment whose bytes were allocated only in PSRAM and freed after send.
- `runtime-heap-allocation`: Camera registry and any host-held image bytes MUST NOT use internal RAM; PSRAM (or fail closed) only.

## Impact

- Host: new camera store/API, WebConsole routes, `data/index.html` + `data/user.html` (sync rule), automation email UI/JSON, `EmailSender` attachment path (**fragile zone `smtp-send`** — apply requires explicit zone approval).
- Console media path: browser → camera HTTP URLs directly (LAN router/AP as usual; not cloud; not ESP proxy). SoftAP-only browsers may be unable to reach STA-side cameras (documented limitation of A).
- Live video in the browser requires a **browser-playable HTTP** URL (typically MJPEG); raw RTSP is stored if discovered but not embedded as a native `<video>`/`<img>` source in v1 unless an HTTP live URL is configured.
- Slave / Zigbee / OTA fragile zones unchanged.
- `FIRMWARE_VERSION` in `include/Defines.h` bumped at propose to `3.9.31`.
