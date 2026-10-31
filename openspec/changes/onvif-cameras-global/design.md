# Design

## Context

See proposal.md — Why. Consoles already persist composite cells (`empty` / `text` / `device`) and automations already send async text email via `EmailSender` (PSRAM job queue; fragile zone `smtp-send`). There is no camera entity. Operator chose console media path **A**: browser loads camera HTTP URLs directly (LAN path may use the home AP/router; media is not proxied by the ESP and is not cloud-relayed).

Hard memory rule (operator): **always PSRAM** for capturing/holding image bytes; **never internal RAM**. Camera settings also live in PSRAM (rare-path), with LittleFS durability like automations/consoles.

`FIRMWARE_VERSION` propose bump is `3.9.31`.

## Goals / Non-Goals

**Goals:**
- Global Cameras CRUD UI + JSON import/export.
- ONVIF-typed records with two channels (snapshot + live HTTP URLs) for console `<img>` / MJPEG-style display.
- Composite camera cells on Place and `user.html` with JPEG-refresh vs live + channel.
- Optional email JPEG attach (host HTTP GET → PSRAM → SMTP), text still sent if attach fails.
- Viewer ACL per camera (user name list).

**Non-Goals:**
- ESP media proxy / RTSP demux / H.264 decode on the host.
- Additional camera types beyond ONVIF in this change (dropdown is enum-ready).
- New RBAC role `editCameras` (Cameras section is `isAdmin` only).
- SoftAP↔STA bridging so SoftAP clients can reach STA cameras.
- Full ONVIF device discovery UX as a hard dependency of v1 (manual/ONVIF-filled URLs are enough; optional Resolve helper may be added if cheap).

## Decisions

1. **Console media = browser-direct URLs (A)**  
   Store snapshot/live HTTP URLs per channel; UI sets `<img src>` (JPEG refresh with cache-buster) or embeds the live HTTP URL the same way.  
   *Alternatives:* ESP JPEG proxy (rejected — contradicts A and burns PSRAM continuously); WebRTC (out of scope).

2. **Live = browser-playable HTTP (typically MJPEG)**  
   ONVIF often yields RTSP. Spec requires an HTTP live URL for live mode. RTSP MAY be stored for future use but live UI MUST NOT claim success without an HTTP URL.  
   *Alternatives:* Transcode on ESP (rejected); third-party player (rejected for embedded console size).

3. **Registry in PSRAM + LittleFS file**  
   Mirror AutomationStore pattern: parse/load into PSRAM structures; persist JSON on LittleFS; APIs list/get/put/delete/import/export.  
   *Alternatives:* EEPROM slots (too small / wrong lifetime); internal heap (rejected by hard rule).

4. **Status without image buffers**  
   Inactive → Offline. Active → TCP/HTTP HEAD or short GET of status line only; never keep a full JPEG on internal RAM. Optional periodic probe.  
   *Alternatives:* Full JPEG fetch for status (rejected — wasteful; if ever used, PSRAM-only and discard).

5. **Email attach path**  
   On email action run: resolve camera channel snapshot URL → HTTP GET into PSRAM buffer → extend `EmailSender` enqueue to carry optional attachment → MIME multipart → free PSRAM.  
   **Apply gate:** changes under `src/EmailSender.cpp` / `include/EmailSender.h` require explicit fragile-zone approval `smtp-send`.

6. **No pre-add**  
   Add opens dialog draft only; first successful Save creates the registry row (project no-preadd rule).

7. **UI sync**  
   Camera cell rendering behavioral changes land in both `data/index.html` (Place) and `data/user.html`.

8. **Users column**  
   Per-camera users ACL was removed; any signed-in user who can open a console with a camera cell may receive media URLs.

## Risks / Trade-offs

- [SoftAP client cannot reach STA camera] → Document; operator must use a browser on the same LAN as the camera (A).
- [RTSP-only cameras cannot use Live mode] → Require HTTP live URL; JPEG refresh still works if snapshot HTTP exists.
- [Large JPEG exhausts PSRAM on email] → Cap max attachment size; skip attach + warn; never fall back to internal RAM.
- [smtp-send fragile zone] → Plan attach behind zone approval; if denied, defer attach implementation and keep UI/fields gated or no-op with warning until approved.
- [Camera credentials in browser URLs] → Prefer cams that allow local snapshot without putting secrets in the page; if basic-auth embeds in URL, treat as operator-accepted LAN risk (same as A).
- [CORS] → `<img>`/`src` usually OK cross-origin; if a cam blocks, operator must fix cam/network (not ESP proxy).

## Migration Plan

- Older consoles without `camera` cells: unchanged load path.
- Older email actions without attach fields: treat as attach disabled.
- Empty camera registry on first boot; no seed cameras.
- Rollback: firmware without cameras ignores new JSON keys; LittleFS camera file unused.

## Open Questions

- Optional ONVIF SOAP “Resolve URLs” button vs URLs-only dialog — implement URLs-first; add Resolve only if it fits without blocking table/dialog/API.
- Exact Import semantics (replace-all vs merge-by-id) — default **replace-all with confirm**, matching Consoles import tone unless Automations-style merge is preferred at apply time.
