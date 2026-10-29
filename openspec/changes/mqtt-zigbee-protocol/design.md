# Design

## Context

See proposal.md for motivation. Today:

- `mqtt` devices: host **subscribes** to STATE, **publishes** to COMMAND; bodies are mostly plain text; channel `2`–`16` use topic suffixes on state ingest; `publishDeviceCommand` for `mqtt` already uses the unsuffixed command topic but **drops** the endpoint from the published body.
- `zigbee` devices: radio path uses `cl`/`attr`/`val` (and `cl`/`cmd`); MQTT bridge uses channels mapping + optional FULL CONTROL parsing of the same vocabulary toward the radio.
- Status cache (`noteReportTelemetry`): any inbound `cl=0x…,attr=…` that is **not** the user-data cluster returns early and **does not** store endpoint status. That is fine when Zigbee on/off arrives as `ON`/`OFF` via a different SPI path, but it **breaks** Zigbee-protocol MQTT status if left unchanged.
- Multi-metric sensors share one endpoint status string; temperature and humidity overwrite each other with no `(cl, attr)` store.

UI already has readonly **DEVICE SOURCE** then class/name/channels/MQTT topics; checkbox goes immediately after DEVICE SOURCE for `mqtt` only.

## Goals / Non-Goals

**Goals:**

- One opt-in flag so MQTT devices share Zigbee payload vocabulary and exact topics.
- Encode console/automation shortcuts into that vocabulary on outbound COMMAND, including `ep`.
- Parse inbound STATE for `ep=` and update status/automations.
- Persist the flag through the normal device life cycle (save/list/reboot/export).
- Persist editable `reportableData[]` (max 16, dynamic alloc); runtime last values; widget metric picker additional to channel.
- Manufacturer `Zbeacon` with seed catalog; create auto-fill = manufacturer seed ∪ discovered class/clusters.
- Unknown `(cl, attr)` vs list: always log (ignore device DEBUG).

**Non-Goals:**

- Renaming UI label STATE → STATUS (STATE remains the inbound status topic).
- Changing Zigbee radio FULL CONTROL semantics.
- Syncing `zigbeeProtocol` / `reportableData` to the slave (host-only, like `userDataFields`).
- Changing bare brace-list MQTT when `zigbeeProtocol` is off.
- Auto-converting existing MQTT devices; default remains off.
- Auto-appending unknown reports into `reportableData`.

## Decisions

1. **JSON field `zigbeeProtocol` (bool), UI “Zigbee protocol”**  
   Scoped to `mqtt` transport only; omitted/false = today’s behavior.  
   *Alternative:* overload FULL CONTROL — rejected; FULL CONTROL means “parse MQTT → Zigbee write on radio.”

2. **STATE = status topic, COMMAND = command/outgoing topic**  
   Keep existing fields; protocol docs call STATE the status topic. No third topic.

3. **Exact topics always when flag on**  
   Ignore `usesTopicSuffix` / `usesPayloadParse` for subscribe and publish. `channels` still drives UI channel count and which `ep` values Manual command / … / automations offer.

4. **Outbound encoding**  
   Shared helper: if body already contains recognized Zigbee keys (`cl=` / `cmd=`), publish as-is (ensure `ep` present from chosen channel when missing). Else map shortcuts the same way Zigbee command path does (on/off → cluster `0x0006` attr `0x0000` val; covering OPEN/CLOSE/STOP → `cl`/`cmd`; etc.) and append `ep=<channel>`.  
   *Alternative:* pass plain `ON` through unchanged — rejected; operator asked for Zigbee-style send **and** receive.

5. **Inbound path**  
   Match exact STATE topic only. Parse `ep=` (default 1). Feed status cache with a path that **stores** non-userData `cl=`/`attr=` reports for these devices (fix or bypass the current early-return). Prefer interpreting known on/off/covering reports into the same console vocabulary automations already use where the Zigbee path would have published `ON`/`OFF` / move strings; keep raw body available when needed.

6. **Cluster from packet**  
   Inbound Zigbee-protocol MQTT is dispatched by `cl=`/`attr=` in the body. `0xFC00`/`0x0001` → user-data brace schema (`userDataFields` in settings). Power Config `0x0001`/`0x0021` → battery. Other known clusters keep On/Off / IAS / covering maps. Device class alone MUST NOT force every packet to `0xFC00`. Outbound: only brace-list bodies are wrapped as `0xFC00`; already-shaped `cl=` bodies pass through.

7. **Custom commands**  
   When flag on, use Zigbee execute shape (`cmd=` / `cmd=,val=`) plus `ep`, on the exact command topic.

8. **FULL CONTROL checkbox**  
   Remains for Zigbee radio devices; for `mqtt` + flag it has no radio effect. Leave the control as today (no new semantics); do not require it for Zigbee-protocol MQTT.

9. **Host-only persistence**  
   `devices.json` / APIs / export-restore only. Do not extend SPI device-sync flag bits for this field.

10. **`reportableData[]`**  
    Packed row: `uint16_t cluster`, `uint16_t attribute`, `uint8_t type` (`int`/`float`/`bool`/`string`). Max 16; `malloc` exactly `count * sizeof(row)`. Create seed: manufacturer catalog (Zbeacon → temp `0x0402/0x0000` float, humidity `0x0405/0x0000` float, battery `0x0001/0x0021` int) **merged** with discovered class/cluster hints (e.g. temperatureMeasurement / relativeHumidity / power). Always editable. Empty omit on older firmware load → count 0.

11. **ZCL → typed value**  
    Built-in conversion by `(cluster, attr)` when storing: temp/humidity hundredths → float display units; battery percentage remaining → int percent (`raw/2`). Unknown pairs: coerce raw `val` to the row’s type with no scale.

12. **Widget binding**  
    Keep Channel = endpoint. Add **Reported data** select when the device has reportable rows; binding stores the chosen `(cl, attr)` (stable if rows reorder). Display reads runtime telemetry for that ep+cl+attr.

13. **Unknown reports**  
    When the device has a non-empty reportable list and inbound `cl`/`attr` is not listed (and is not handled by existing special paths that still apply): `LOGGER.info` the report **even if** `debug` is off. Do not mutate the list.

14. **Measurement SPI identity**  
    Slave measurement reports that previously sent a bare scaled number SHALL include `cl`/`attr`/`val` (raw ZCL) so the host can match reportable rows and convert. Endpoint status may still receive a converted display string for legacy widgets.

15. **Manufacturer Zbeacon**  
    Enum next after Develco; json id `zbeacon`; join match fold contains `"zbeacon"`.

## Risks / Trade-offs

- **[Status cache drops `cl=`/`attr=`]** → Must change ingest so Zigbee-protocol MQTT reports update endpoint status and automation listeners; without this the feature is useless for consoles.
- **[Multi-channel MQTT already drops `ep` on command publish]** → Encoding `ep=` fixes that for flag-on devices; flag-off devices stay as today (known limitation).
- **[channels=0 with flag on]** → Parse mode becomes meaningless; documented as ignored. Operators may still pick “Multichannel with parsing” by mistake → mitigations: ignore mapping; optional later UI hint.
- **[External device dialect mismatch]** → Opt-in only; wrong flag yields unread/unparsed traffic until corrected.
- **[Optional cluster-name prefix]** → Accept inbound `On/Off cl=…`; outbound may omit the name prefix for smaller payloads (name is optional sugar).
- **[Automation triggers on ON vs cl= body]** → Prefer normalizing known clusters into existing status vocabulary on ingest so existing ON/OFF listeners keep working.
- **[CUSTOM / FULL CONTROL confusion]** → Document that Zigbee-protocol MQTT is about the MQTT wire format, not radio writes.
- **[Bare measurement numbers]** → Changing slave to shaped bodies requires host conversion for legacy gauge widgets; keep converted endpoint status when possible.

## Migration Plan

- Bump `FIRMWARE_VERSION` build on propose and again on each source apply.
- Existing MQTT devices load with `zigbeeProtocol` false → no behavior change.
- Existing devices load with empty `reportableData` → no change until create/edit seeds or operator adds rows.
- Operators enable Zigbee protocol only when the remote device speaks the Zigbee-shaped dialect on the configured STATE/COMMAND topics.
- Rollback: set flag false or flash previous build; store remains backward compatible (unknown keys ignored by older firmware).

## Open Questions

- None blocking: product choices locked (types + ZCL conversion, metric additional to channel, always-log unknown, max 16 dynamic, Zbeacon like Tuya, seed = manufacturer + discovered, fold into this change).
