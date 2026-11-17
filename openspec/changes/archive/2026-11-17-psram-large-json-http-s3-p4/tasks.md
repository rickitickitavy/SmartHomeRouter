# Tasks

## 1. PSRAM buffer rules

- [x] 1.1 In `PsramJsonBuffer::allocate`, when `BOARD_HAS_PSRAM` is set and the request is above 4096 bytes, allocate only with SPIRAM caps (`heap_caps_malloc` then `ps_malloc`) and return null on failure with no `malloc` fallback. Keep the existing internal fallback for 4096 bytes or less, and when PSRAM is not compiled in. Verify by reading the function: the `> 4096` PSRAM branch has no `malloc` call.

## 2. HTTP JSON bodies

- [x] 2.1 Store `HttpRequestBody` in a `PsramJsonBuffer` (not `String`). `appendRequestBody` reserves and appends chunks in that buffer for both the 16 KiB and 256 KiB caps, still sets overflow above the cap, and still keeps one collector per request. `takeRequestBody` moves the buffer out and still answers HTTP 413 on overflow. On PSRAM reserve failure, send HTTP 507 and do not apply the body. Verify by reading the collector: it has no `String` member, 413 text is unchanged, and a failed reserve path sends 507 without `malloc` of the body.
- [x] 2.2 Point JSON POST handlers at `const char *` from that buffer (in-place trim). Remove whole-body `String` copies (`String body`, `String trimmed = body`) on those handlers. Leave bounded field extracts as they are. Do not change the firmware upload handler. Verify with a search: JSON POST handlers no longer assign the full body to a `String`, and the OTA upload function is untouched.

## 3. Generated JSON responses

- [x] 3.1 Add a response path that writes a `PsramJsonBuffer` and frees it on complete and on abort, without copying the full document into a `String` or an internal response buffer. If stock `beginResponse` copies, use a small response type that holds the pointer. Verify by inspection that the send path’s only full-document allocation is the PSRAM buffer, and that abort frees it once.
- [x] 3.2 Return list, export, and full-document JSON from device, console, automation, camera, and scene stores as `PsramJsonBuffer` (move) instead of `toString()`. Send those responses, and settings export assembled into one `PsramJsonBuffer`, through the path from 3.1. On buffer failure send HTTP 507. Keep short settings and status fragments on `String`. JSON fields stay the same. Verify with a search that those store `*Json` methods no longer `return json.toString()`, and that settings export no longer builds the full file through `AsyncResponseStream` plus section `String`s.

## 4. Host build

- [x] 4.1 Bump `FIRMWARE_VERSION_HOST` build by 1 in both the S3 and P4 strings. Leave `FIRMWARE_VERSION_SLAVE` unchanged. Verify the two host strings share the new build and the slave string is still `4.2.11`.
- [x] 4.2 Build `esp32-s3-host` and `esp32-p4-host` and verify both finish with success. Do not build or edit the slave image for this change.
