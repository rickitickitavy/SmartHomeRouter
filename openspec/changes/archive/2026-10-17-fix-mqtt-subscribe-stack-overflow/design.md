# Design

## Context

See proposal.md — Why. Observed panic: `Stack canary watchpoint triggered (IDLE1)` with a corrupted backtrace shortly after remote MQTT connect logs a dense burst of `MQTT subscribe topic=…` lines, then slave status-read timeouts and (when born fires) Zigbee status collect.

Today, after `mqttConnect` succeeds, `remoteSessionAttachPending` causes `finishRemoteSessionAttach()` on the Arduino **loop** task to call `subscribeBridge()` → `subscribeDeviceCommands()`, which walks the entire device map and calls PubSubClient `subscribe` / tracking-list updates / `LOGGER.info(String + topic)` with no yield. Server-born handling can then request a full Zigbee status collect on a nearby loop tick. Adjacent FreeRTOS stacks (including IDLE) get corrupted when the overflowing task’s frame blows past its limit—hence IDLE1 in the exception text.

Constraints: PubSubClient remote path; keep final subscription membership identical; fragile SPI OTA zones are out of scope; console usability during MQTT work already exists as a requirement and must cover attach pacing.

## Goals / Non-Goals

**Goals:**
- Eliminate stack-canary reboots on remote MQTT connect with a large device map.
- Keep subscription semantics (which topics) unchanged after a short settle window.
- Keep loop / SPI / HTTP progressing while attach subscriptions install.

**Non-Goals:**
- Changing MQTT topic naming, `publishToMqtt` rules, or born settings UI.
- Rewriting PubSubClient or moving the whole MQTT client off Arduino.
- Fixing unrelated slave “Status read timeout” noise except where it couples to attach stack depth.

## Decisions

1. **Incremental subscribe attach on the loop (preferred over one bigger stack)**  
   Replace the single-pass `subscribeDeviceCommands()` burst used from `finishRemoteSessionAttach` with a state machine: mark attach in progress, each `dispatch`/loop tick process a bounded number of subscribe/unsubscribe ops (and yield), then clear pending when the wanted set matches.  
   *Alternatives considered:* only increasing `CONFIG_ARDUINO_LOOP_STACK_SIZE` — helps margin but does not bound work; moving all subscribe work into `mqttConnect` task — shares PubSubClient affinity risks with loop’s `client->loop()`; leave burst and trim logs only — insufficient if PubSubClient itself is deep.

2. **Defer Zigbee status collect off the attach call stack**  
   If born / status-collect is requested while subscription attach is active (or immediately from the same handler that just finished a burst), queue “collect wanted” and run it on a later loop iteration after attach has yielded.  
   *Alternatives:* run collect immediately — recreates nested stack pressure; drop born during attach — wrong functionally.

3. **Cheap logging / fewer temporaries on the subscribe hot path**  
   Prefer fixed buffers or debug-level per-topic logs during attach; keep a single summary info line (counts) so serial does not allocate dozens of `String` concatenations in one tick.  
   *Alternatives:* keep every info line — useful for field debug but contributes pressure during the failure mode.

4. **Optional loop-stack headroom**  
   If PlatformIO/Arduino for this core exposes a loop stack size flag, raise it modestly as defense in depth after pacing is in place—not as the sole fix.

## Risks / Trade-offs

- **[Risk] Brief window where some device topics are not yet subscribed after connect** → Mitigation: small per-tick batch so settle stays within a few seconds; document as expected; born collect waits until attach settles or runs after yield.
- **[Risk] PubSubClient not re-entrant across tasks** → Mitigation: keep subscribe work on the same task that calls `client->loop()` (Arduino loop), only slice it.
- **[Risk] IDLE1 blame is misleading after corruption** → Mitigation: treat as stack smash; verify with high watermark / deliberate large-map soak after fix.

## Migration Plan

- Ship in host firmware only; no settings migration.
- Rollback: previous firmware image if soak still panics (unexpected).

## Open Questions

- Exact Arduino loop stack size for the current pioarduino ESP32-S3 build (confirm when applying whether a build flag is available/needed after pacing).
