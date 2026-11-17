# Proposal

## Why

On the ESP32-S3 and ESP32-P4 hosts, large JSON documents and HTTP bodies still land on internal heap. Request bodies are collected in an Arduino `String`, and documents built in PSRAM are copied back into a `String` (or an internal response buffer) before they are parsed or sent. Internal `realloc` cannot move a growing buffer into PSRAM, so a large console save, restore, or list can exhaust internal RAM and trip the low-memory fail-safe even when PSRAM is free.

## What Changes

- On the **S3 and P4 hosts**, generate large JSON in PSRAM and keep that text in PSRAM until the HTTP response is sent. Do not copy the full document onto internal heap.
- On the **S3 and P4 hosts**, collect HTTP JSON bodies in PSRAM and parse them in place from that buffer. Do not copy the whole body into an Arduino `String` to parse it.
- A buffer is **large** when it is above **4096 bytes** (the existing `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL` cutoff). HTTP collectors whose cap is 16 KiB or 256 KiB always use PSRAM, because they are allowed to grow past 4 KiB.
- If PSRAM cannot hold that large buffer, fail that request with the existing **507 Out of memory** response. Do not fall back to a large internal-heap copy.
- JSON field sets, success responses, and the existing **413** oversized-body rejection stay as they are.
- Firmware image upload and the joined OTA path are unchanged.
- Slave firmware is unchanged.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `runtime-heap-allocation`: Large JSON generation buffers, large JSON parse working text, and HTTP JSON bodies on S3 and P4 hosts are PSRAM-only. No internal-heap fallback for those large buffers.
- `web-console`: HTTP JSON POST collection and large JSON HTTP responses use that PSRAM path. Oversized bodies stay 413. PSRAM exhaustion on a large body or large generated document returns 507 and does not apply the body.

## Impact

- Host web console body collector (`HttpRequestBody` / `appendRequestBody` / `takeRequestBody`), JSON list/export/save responses, and store methods that currently return a full document via `PsramJsonBuffer::toString()`.
- `PsramJsonBuffer` large allocations on S3 and P4 no longer fall back to internal `malloc`.
- Settings export/restore, device list, consoles, automations, cameras, and scenes JSON over HTTP.
- No slave sources. Host firmware identity for this propose is `S3-4.3.26` / `P4-4.3.26`.
- Joined firmware upload (`joined-firmware-ota`) is out of scope.
