## Context

Today `default_envs = esp32-s3-host, esp32-c6-slave`. Host pins and SPI map live in `include/pins.h` under `BOARD_ROLE_HOST` for S3. Specs name S3 as the only host. The operator wants P4 Wi‑Fi 6 as the new default host while keeping S3 buildable, with future H/W (mic, speaker, camera, SD on second SPI, OTG) deferred. Scope for this change is **scaffold A** only.

## Goals / Non-Goals

**Goals:**
- P4 host PlatformIO env that builds the same host application sources as S3 (`BOARD_ROLE_HOST`).
- Default build includes P4 host + C6 slave; S3 host env remains selectable (`pio run -e esp32-s3-host`).
- Board-role / README language updated for dual host chips.
- Standing rule: dual-host feature parity unless a change explicitly scopes one board.
- Stub P4 pin map for slave SPI / LEDs / reset (TBD numbers marked clearly).
- Note future SD on a **different** SPI from the slave link (no driver yet).

**Non-Goals:**
- Mic, speaker, camera, SD filesystem, USB OTG drivers or UI.
- Changing slave C6 SPI protocol, pins, or env.
- Requiring a specific commercial PCB — first target is “generic Espressif P4 Wi‑Fi 6 kit” with TBD GPIOs until a kit board id is locked.

## Decisions

1. **Default envs:** `esp32-p4-host, esp32-c6-slave`. S3 stays as `[env:esp32-s3-host]` without being removed from the project.
2. **Shared host sources:** Same `build_src_filter` / lib set as S3 host where the Arduino-ESP32 P4 port allows; board-specific `#if` only for pins, flash/PSRAM, USB CDC, and unavoidable HAL differences — not for forking product features.
3. **Pins:** Split host pin maps by board macro (`BOARD_HOST_S3` / `BOARD_HOST_P4`) inside `BOARD_ROLE_HOST`. P4 product map: SPI 10–13 + RST 9 + IRQ 22, LEDs 2–7, boot 21; GPIO 14–19 reserved for ESP-Hosted Wi‑Fi SDIO; no LAN/LCD/touch; future SD on a separate SPI (e.g. 28–31).
4. **Platform package:** Use the project’s current `platform = espressif32` (or the minimum bump required for P4 Arduino support). If P4 needs a different platform URL/version than S3, isolate that on the P4 env only so S3 keep working.
5. **Joined OTA:** Still one host + one slave per package; docs MUST say which host SoC the package was built for (S3 vs P4). No dual-host joined package in this change.
6. **Future H/W ledger** (out of scope, recorded for later proposes): microphone; speaker; camera if P4 supports the needed interface; SD on SPI2 (or equivalent) separate from slave SPI1; USB OTG if the kit exposes it.
7. **Host version prefix:** One host `MAJOR.MINOR.BUILD` with SoC prefix only — `FIRMWARE_VERSION_HOST` is `S3-…` by default and `P4-…` under `BOARD_HOST_P4`. Stay within `SPI_STATUS_VERSION_MAX` (15). Joined OTA `host.version.txt` MUST match the running host string. `firmware_join.py` reads the first `FIRMWARE_VERSION_HOST` string in `Defines.h` (S3 default); P4 joins need `--host-version P4-…`.

## Risks / Trade-offs

- [Risk] Arduino-ESP32 / PlatformIO P4 support immature → Mitigation: P4 env may pin a known-good platform version; S3 env stays on the working baseline; fail the P4 env loudly in docs if the kit board id is missing.
- [Risk] TBD pins brick a real kit if flashed blindly → Mitigation: README MUST say P4 pins are stubs until filled; do not claim production pin parity.
- [Risk] Accidental S3-only feature later → Mitigation: dual-host parity requirement in `board-role` spec.

## Migration Plan

Developers: install PlatformIO packages that include P4; `pio run -e esp32-p4-host` (default) and `pio run -e esp32-s3-host` for legacy. Field units stay on S3 until a P4 joined package is published. No settings schema change.

## Open Questions

- Exact Espressif board id string for the “generic P4 Wi‑Fi 6 kit” once chosen (replace TBD in `platformio.ini` / pins).
- Whether P4 needs a distinct partition CSV vs reusing host S3 layout (decide during apply from flash size of the chosen kit).
