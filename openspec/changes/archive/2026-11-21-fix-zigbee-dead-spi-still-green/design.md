## Context

Base problem: Zigbee radio path dead while SPI/green OK; host does not reboot. Search with no blue is proof deferred apply did not run (blue arms only in `startPairingWindow` after real `permitJoin`).

Today:

- Server-born (MQTT message or host publish cadence) already calls Zigbee status collect (enqueue reads).
- Collect does not verify slave apply; Ping lock+buffer can still pass.
- `SpiCmdPermitJoin` returns ok before apply/blink.
- `BORN_INTERVAL_MIN` lives under MQTT; empty server-born topic disables born listen/publish and, with current wiring, starves the Zigbee collect trigger if nothing else fires born.

Operator choices (this update):

- **THE SLAVE KNOWS RF IS DOWN** when radio apply fails; recovery **B** = immediate slave reboot; host still resets when Ping clears Zigbee-started.
- Periodic trigger = **server-born interval only** (independent of MQTT topic); MQTT topic additive when set.
- **Rejected:** per-minute slave Zigbee self-test without real permit-join (can corrupt/contend with real inbound packets).
- Move interval UI to System/Hardware.

## Goals / Non-Goals

**Goals:**

- When the slave knows RF/apply is down, reboot immediately (B); clear Ping Zigbee-started so host resets too.
- Run Zigbee status collect on `BORN_INTERVAL_MIN` even with empty MQTT server-born topic (only periodic collect cadence).
- Keep MQTT born topic behavior when configured.
- Honest permit-join/Search applied success (same apply-failure knowledge; operator truth).
- Interval setting editable under System/Hardware.

**Non-Goals:**

- Per-minute (or other standing) Zigbee self-test / fake probe on the slave.
- New host keepalive / second checkAlive.
- Fail born collect because sleepy devices did not answer (device replies ≠ slave health).
- Option A (host requires applied success for the whole read batch as the primary recovery).
- SPI OTA fragile path edits.
- Quiet-inbound-only death.

## Decisions

1. **B — Slave trips lost immediately when Zigbee apply fails**  
   When deferred radio work is pending (permit-join, reads, writes, cluster cmds, rebind, etc.) and apply cannot proceed (coordinator not started when required, Zigbee lock miss for a required stack call, inability to arm Search blue / open join, or equivalent hard apply failure), the slave MUST `markZigbeeLost` and **reboot itself immediately** via the existing lost-Zigbee path, and clear Zigbee-started for Ping. Zero devices found / no end-device replies is **not** Zigbee failure — the slave already knows real Zigbee failure (e.g. Search would not start blue).  
   *Why:* Operator note; slave-side certainty; no wait on host Ping cadence for the reboot decision.

2. **No second alive service and no minute Zigbee self-test**  
   Keep existing SPI Ping + lock/buffer health. Do not add app-heartbeat Ping rules. Do **not** add a periodic slave Zigbee self-test (operator rejected: can contend with / corrupt real inbound packets). The periodic host action is **server-born interval collect only**; detection of RF down remains **slave apply failure (B)**.  
   *Why:* Slave already knows; fake probes are dangerous and redundant.

3. **Zigbee born interval independent of MQTT topic**  
   Host MUST run the same Zigbee status-collect path used after server-born on every `BORN_INTERVAL_MIN` tick while the gateway is in normal operation, **whether or not** `serverBornTopic` is set. That collect supplies deferred radio work so apply failure can surface; it is not “devices must answer.”  
   *Why:* Only agreed periodic cadence; mesh health must not depend on MQTT configuration.

4. **MQTT topic remains additive**  
   If `serverBornTopic` is non-empty: keep subscribe-on-message → Zigbee collect, and keep periodic MQTT born publish on the interval, as today. Those MUST NOT replace or disable the independent Zigbee interval collect.  
   *Why:* Operator: “If MQTT topic specified then MQTT way will pass too.”

5. **Born collect is not “all devices answered”**  
   Interval/born collect still requests status from known registered Zigbee devices. An empty result set is fine. Pass/fail for slave health is **Zigbee apply capability (B)**, not count of end-device replies.  
   *Why:* Operator: no problem if slave finds no devices; sleepy/offline must not force resets.

6. **Honest deferred CmdResult (especially permit-join)**  
   Success only after apply; failure result when apply cannot run (and that failure engages B). Host Search SHOULD wait for applied result and must not claim started on enqueue-only; on failure/timeout host MAY also drive existing reset (complements B when Ping is slow).  
   *Why:* Matches no-blue evidence; closes false “Search started.”

7. **Lock open/close network**  
   Deferred permit-join open/close under timed Zigbee lock; lock miss → apply failure → B.  

8. **UI: interval under System/Hardware**  
   Show/edit `BORN_INTERVAL_MIN` on Hardware; keep `SERVER_BORN_TOPIC` on MQTT. Same persisted field unless apply finds a cleaner split—default is same `bornIntervalMin` storage, new UI placement.  

9. **OTA quiet**  
   While firmware-transfer quiet pauses deferred radio, do not trip B solely for that intentional pause.

## Risks / Trade-offs

- **[Risk] Lock-miss flapping under load → slave reboot loop** → Mitigation: finite lock timeout; clear logs; only trip when work was pending and apply failed (not on idle).
- **[Risk] Double collect** (interval tick + MQTT born message close together) → Mitigation: accept overlap or coalesce with a short in-flight guard (apply detail).
- **[Risk] Operators expect MQTT-only born** → Mitigation: Hardware help text that interval also drives Zigbee collect without topic.

## Migration Plan

- Joined flash host **4.5.18** + slave **4.2.17**.
- No forced settings wipe; existing `bornIntervalMin` / topic values remain.
- After flash: empty topic still gets Zigbee collect every interval; apply death → slave reboot + host Ping lost reset.

## Open Questions

None blocking. Search wait 3–5 s remains an apply assumption for host honesty. Minute self-test is rejected.
