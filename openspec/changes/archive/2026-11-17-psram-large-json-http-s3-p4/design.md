# Design

## Context

See proposal.md for why. Today `PsramJsonBuffer` prefers PSRAM but falls back to `malloc`, and `toString()` copies the whole document into an Arduino `String`. HTTP JSON bodies are `HttpRequestBody::text` (`String`), grown with `reserve`/`concat`. Both 16 KiB and 256 KiB caps can exceed 4096 bytes. `realloc` of an internal block does not migrate to PSRAM. Settings export also prints through `AsyncResponseStream` after building large sections as `String`. File loads for devices, consoles, automations, cameras, and scenes already use `PsramJsonBuffer::assignFromFile`. Joined firmware upload does not use this collector.

## Goals / Non-Goals

**Goals:**

- S3 and P4: large JSON text and HTTP JSON bodies live in PSRAM through generate, parse, and send.
- Failed large PSRAM allocation fails the operation (HTTP 507) with no internal-heap copy of that buffer.
- Existing JSON shapes, 413 overflow, and per-request body isolation stay.

**Non-Goals:**

- Changing `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL` or moving every `malloc` above 4 KiB.
- Firmware image upload, slave firmware, log ring, history slab, camera image buffers.
- Changing route body caps (16 KiB and 256 KiB) or JSON field sets.

## Decisions

### 1. Large means above 4096 bytes; HTTP collectors always use PSRAM

The cutoff matches `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL`. Collectors whose max is 16 KiB or 256 KiB allocate in PSRAM from the first chunk, not after crossing 4 KiB. A buffer that starts on internal heap cannot be `realloc`’d into PSRAM.

Alternative: switch to PSRAM only after 4 KiB. Rejected because the first block would stay internal.

### 2. Extend `PsramJsonBuffer`; no internal fallback above 4096 when PSRAM exists

On `BOARD_HAS_PSRAM` (S3 and P4 hosts), `allocate` for more than 4096 bytes uses `heap_caps_malloc(..., MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)` and then `ps_malloc`. If both fail, return null. Do not call `malloc`. Allocations of 4096 bytes or less keep the current internal fallback. Hosts without PSRAM are unchanged.

`HttpRequestBody` stores a `PsramJsonBuffer` instead of `String`. `takeRequestBody` moves that buffer to the handler. Trim in place (`PsramJsonBuffer::trim`). Parsers keep `const char *` and MUST NOT assign the whole document to a `String`. Bounded field extracts into short `String` or fixed arrays stay allowed.

### 3. Send large JSON from the PSRAM buffer

Store list/export/full-document builders that today `return json.toString()` return a `PsramJsonBuffer` (move). The console sends it with a response that writes those bytes and frees them when the response finishes. Do not pass the document through `beginResponse(..., String)` or `AsyncResponseStream` for a full large document. Settings export is assembled into one `PsramJsonBuffer` (MQTT, email, devices, consoles, automations, cameras, scenes, and the other existing groups) and sent the same way.

A document of 4096 bytes or less MAY still be sent as a `String`. Anything built as `PsramJsonBuffer` for a list or export is sent from that buffer even when it happens to be small, so there is one path and no size-check copy.

If reserve/append fails, respond **507** `Out of memory` (already used for scene/camera export) and free the partial buffer.

### 4. Ownership

The HTTP response owns the buffer after the handler queues the send. The body collector is freed when the handler finishes parsing, as today, but the bytes stay one PSRAM allocation for the parse. Camera ONVIF worker input is already a PSRAM copy; its JSON result uses the same PSRAM generate/send path when the handler returns it to HTTP.

## Risks / Trade-offs

- [AsyncWebServer copies the body into an internal buffer] → During apply, confirm the chosen response writes from the PSRAM pointer (chunked or raw) and does not `memcpy` the full document to internal heap. If the stock `String`/`uint8_t*` helpers copy, add a small response type that holds the pointer until ack.
- [507 where a tight internal heap used to succeed] → Accepted. Spec requires failing closed so internal RAM stays for the TCP stack and the low-memory fail-safe.
- [Double free or leak on client abort] → One owner: the response frees on complete and on abort.
- [PSRAM bandwidth] → Acceptable for JSON text. Not used for DMA-only paths.
- [Missed `toString()` call site] → Apply searches host `toString()` and `String json` on list/export/restore handlers and switches those documents. Short status and settings fragments stay on `String`.

## Migration Plan

Host firmware only (`S3-4.3.26` / `P4-4.3.26` for this propose; apply bumps host build again when source changes). No stored-data migration. Rollback is the previous host image. Slave image stays `4.2.11`.

## Open Questions

None.
