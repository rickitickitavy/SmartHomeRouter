## Context

Symptom (operator, **P4 only**, ~1–2 day cadence): HTTP console dead; ping works; MQTT connected and RX LED blinks; Zigbee works. That is the classic “AsyncWebServer listen PCB stale while outbound MQTT / ICMP survive” failure already fixed for on-chip STA on S3 via `2026-09-15-rebind-web-console-after-sta-ip`.

Current rebind machine (shared S3/P4):

- `WiFi.onEvent` → `notifyStaDisconnected` / `notifyStaGotIp` → `requestStaWebRebind`.
- Polled `WL_CONNECTED` drop edge in `WiFiController::update()` also arms the flag.
- When STA is connected again, `runPendingStaWebRebind` calls `WebConsole::rebind()` (`server.end()` / `begin()`).
- SoftAP recovery does **not** arm STA rebind (`apActive` gate); SoftAP is out of this reported STA symptom.
- First boot `begin()` must not be torn down by the boot GOT_IP (handler armed after `begin()`, flag cleared).

P4 difference: STA is **ESP-Hosted → C6** (SDIO). Hosted can auto-restart transport / restore the data plane (`CONFIG_ESP_HOSTED_TRANSPORT_RESTART_ON_FAILURE` and similar) while the P4 host never sees DISCONNECTED / LOST_IP / GOT_IP and never flaps `WiFi.status()` off `WL_CONNECTED`. Then `staNeedsWebRebind` stays false and the listen socket stays dead.

Observed code today: **no** product path maps “Hosted transport / C6 Wi‑Fi reinit” → `requestStaWebRebind`. The host only “knows” Hosted recovery if Arduino STA events or the status poll fire.

## Goals / Non-Goals

**Goals:**

- On P4 STA, after Hosted/C6 recovery that leaves ICMP/MQTT up, rebind the console listen socket via the existing pipeline so HTTP answers on the current STA IP.
- Prefer bridging real Hosted recovery signals into `requestStaWebRebind`; keep rebind execution in `update()` / loop.
- Confirm with logs whether field failures miss STA events (hypothesis check).
- Leave S3 behavior unchanged.

**Non-Goals:**

- SoftAP listen rebind (different mode; not the reported failure).
- New host reboot as the primary recovery.
- Changing MQTT, Zigbee, SPI, or auth.
- Periodic HTTP self-probe as the first-line product behavior.
- Fixing AsyncTCP slot exhaustion from stuck browser tabs.
- Slave firmware changes.

## Decisions

1. **Scope: P4 host only (`BOARD_HOST_P4`)**  
   S3 already has working on-chip STA event → rebind. Do not change S3 gates unless a shared bug is proven.

2. **Reuse the existing rebind pipeline**  
   Hosted recovery MUST call `requestStaWebRebind()` (or equivalent that sets `staNeedsWebRebind` under the same STA/armed/`!apActive` gates). `runPendingStaWebRebind` + `WebConsole::rebind()` stay the only listen end/begin path. Do not call `server.end()`/`begin()` from a Hosted callback / Wi‑Fi event task.

3. **Primary signal: Hosted recovery the host can observe**  
   Investigate and wire the strongest available Hosted/ESP-Hosted / wifi-remote signal that means transport or remote Wi‑Fi stack reinited while the host process kept running (transport restart done, remote Wi‑Fi re-init complete, or documented Hosted event). When that fires and STA mode is active and later `isStaConnected()`, arm rebind the same way GOT_IP does after a blip.  
   *Why:* Matches the operator hypothesis; closes the silent-recovery gap without inventing a new product “health” service.

4. **Backup: P4-only connected-but-stale detect only if Hosted events are insufficient**  
   If apply finds no reliable Hosted hook, use a **narrow** P4 backup that still maps into `requestStaWebRebind` (e.g. detect Hosted link/transport flap via existing Hosted status APIs, or a one-shot arm when Hosted logs/transport counters show restart). Do **not** default to “HTTP GET localhost every N minutes” unless Hosted hooks and STA events are both proven useless; if that last resort is needed, document it in apply notes and keep it P4-only with a long period (≥ hours, not seconds).

5. **Logging for field proof**  
   On P4, log distinctly when rebind is armed from Hosted recovery vs STA disconnect/GOT_IP vs polled disconnect. Operators can then match “console dead” windows to missing vs present rebind arms.

6. **Keep boot / first-bind rules**  
   Do not rebind away the first successful post-boot `begin()`. Hosted hooks must not arm until `staWebRebindArmed` (handler registered after `begin()`), same as today.

## Risks / Trade-offs

- **[Risk] Hosted API surface is thin / version-specific** → Mitigation: probe current ESP-Hosted headers used by this PlatformIO env first; fall back to decision 4 only with evidence.
- **[Risk] Extra end/begin flaps drop in-flight console uploads** → Mitigation: rare Hosted recoveries; same risk already accepted for GOT_IP rebind.
- **[Risk] False Hosted “restart” while listen is fine** → Mitigation: still only rebind when STA connected; harmless end/begin; avoid high-frequency arming.
- **[Risk] Symptom is actually AsyncTCP slot exhaustion, not Hosted** → Mitigation: field logs (no rebind arm + Hosted quiet) redirect diagnosis; do not pretend Hosted hook fixed that.

## Migration Plan

- Flash **P4 host** image with build **4.5.20** (joined package as usual). Slave image unchanged for this change.
- No settings wipe.
- After flash: on Hosted/C6 recovery, expect log arm + `Rebinding web console…` and HTTP on STA IP again without host reboot.

## Open Questions

None blocking. SoftAP rebind stays out unless a later P4 SoftAP-only console-dead report appears.

## Apply notes (field logs)

Hosted signals used (`esp_hosted_event.h`): `ESP_HOSTED_EVENT_TRANSPORT_UP` and `ESP_HOSTED_EVENT_CP_INIT` → `requestStaWebRebind` (P4 only). Rebind still runs only from `WiFiController::update()` → `WebConsole::rebind()`.

First arm log (when the flag rises):

- `Web rebind armed: Hosted TRANSPORT_UP` / `Hosted CP_INIT` — Hosted recovery path (P4)
- `Web rebind armed: STA disconnect/lost IP` — Arduino STA event
- `Web rebind armed: STA got IP` — Arduino GOT_IP
- `Web rebind armed: polled STA disconnect` — `WL_CONNECTED` drop edge in `update()`

Then expect `Rebinding web console after Wi-Fi change` / `Web console rebound on port 80`. If the console is dead with live ping/MQTT and none of the arm lines appear, Hosted events were not posted (different failure mode).
