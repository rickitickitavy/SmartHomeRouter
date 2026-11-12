## 1. PlatformIO dual host

- [x] 1.1 Add `[env:esp32-p4-host]` for a generic Espressif P4 Wi‑Fi 6 kit board id (or documented placeholder), mirroring S3 host src filter / host libs where possible
- [x] 1.2 Set `default_envs` to `esp32-p4-host, esp32-c6-slave`; keep `[env:esp32-s3-host]` buildable
- [x] 1.3 Isolate any P4-only platform/package/sdkconfig needs on the P4 env so S3 host still builds on the current baseline

## 2. Pins and board flags

- [x] 2.1 Split host pin maps (`BOARD_HOST_S3` / `BOARD_HOST_P4` or equivalent) under `BOARD_ROLE_HOST`; keep S3 numbers; lock P4 slave-SPI / LED / RST / boot from product schematic discussion
- [x] 2.2 Add build flags so each host env defines the correct board macro and `BOARD_ROLE_HOST`
- [x] 2.3 Comment or short doc note: future SD uses a separate SPI from the slave link (no SD driver in this change)

## 3. Docs, versions, and parity rule

- [x] 3.1 Update `README.md` for S3 host, P4 host, C6 slave; how to build default vs `esp32-s3-host`; document locked P4 pins
- [x] 3.2 Note joined OTA packages are one host SoC + slave (label S3 vs P4); `host.version.txt` must match `S3-…` or `P4-…`
- [x] 3.3 Host version macros: `FIRMWARE_VERSION_HOST` as `S3-…` / `P4-…` (same build number, prefix only); `BOARD_HOST_S3` / `BOARD_HOST_P4` build flags; leave `FIRMWARE_VERSION_SLAVE` unprefixed
- [x] 3.4 Update `scripts/firmware_join.py` to compose `S3-`/`P4-` from CORE via `--host-board` (fragile zone `firmware-update-process` approved)

## 4. Verify

- [x] 4.1 `pio run -e esp32-s3-host` succeeds
- [x] 4.2 `pio run -e esp32-p4-host` succeeds (or fails only with a documented missing board/platform prerequisite)
- [x] 4.3 `pio run -e esp32-c6-slave` unchanged success
