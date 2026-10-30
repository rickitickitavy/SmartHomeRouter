# Design

## Context

See proposal.md for motivation. Today, signed-in `data/index.html` and `data/user.html` recover live WebSockets via `recoverWantedLiveSocket` / `onclose` timers and poll `/api/auth/me` in `startSessionWatch`, but there is no operator-visible disconnect UI (aside from firmware reboot wait). Console binding units are filled in `updateConsoleDeviceUnitField` / `measurementUnitOptions(typeId)` from the device select’s `data-type`, so every param on a Measurement-class device gets that class’s units. Linear gauges use a single growing fill bar; arc gauges already draw full range-colored bands plus a needle.

## Goals / Non-Goals

**Goals:**
- One shared connection-overlay UX on operator and simple-user pages: Disconnected picture + title, then RECONNECTING picture + “reconnecting to router” text while retrying.
- Drive overlay state from HTTP reachability failures and required live WebSocket loss/reconnect, without breaking existing reboot-wait or login flows.
- Key binding unit (and decimals default when tied to measure kind) off the selected param’s measure kind, reusing existing reportable→type mapping (`reportableFormatTypeId`) and battery as percent.
- One linear-gauge style field with four states (`bar` | `filled` | `vertical` | `verticalFilled`); vertical styles consume max container height after the binding label.

**Non-Goals:**
- New host HTTP API routes (persist via existing console JSON only).
- Changing how live values are converted/stored on the device (only editor options + consistent display kind).
- Per-socket dual modals (one overlay for “link unhealthy,” not separate dialogs per socket).
- Redesigning the firmware reboot wait UI.
- Two separate boolean flags for filled/vertical.

## Decisions

1. **Single overlay, two visual states**  
   One blocking dialog/overlay element per page (`#connectionOverlay` or equivalent). States: hidden | disconnected | reconnecting. Same shell; swap icon/image and body text.  
   *Alternatives:* toast/banner (too easy to miss); separate dialogs (noisy).

2. **What counts as “main connection”**  
   Treat failed signed-in HTTP probes (session watch / auth me network error, and similarly failed critical fetches while signed in) plus `navigator.onLine === false` as HTTP-side down. Treat required live sockets (`/ws/devices`, `/ws/consoles`, and any other wanted live socket on that page) as down when wanted and not `OPEN`. Show overlay if **either** side is unhealthy while signed in. Reconnecting when a retry/timer/`CONNECTING` is in progress. Clear only when HTTP OK and every wanted socket is `OPEN` (or none wanted).  
   *Alternatives:* WebSocket-only (misses SoftAP/STA drop with stale tabs); HTTP-only (misses silent WS death).

3. **Icons / pictures**  
   Inline SVG or compact static assets in `data/` (or CSS background) for disconnected vs reconnecting—no external CDN. Match existing dialog-overlay stacking (`dialog-overlay-front` / high z-index) so the overlay sits above normal dialogs while signed in. Non-dismissible (no outside-click dismiss); no Cancel that pretends the link is fine.

4. **Exclusions**  
   Gate overlay on `signedInUser` and `!firmwareRebootWaitActive`. On sign-out, force-hide. Do not fight the existing reboot wait block.

5. **Unit measure kind from param**  
   Introduce a small resolver used by the binding dialog (and decimals defaults):  
   - `battery` → humidity-like percent options (`%`)  
   - reportable param → `reportableFormatTypeId(param)` when known  
   - `sensorValue` (and legacy measurement status) → device `type` if Measurement-class  
   - otherwise → no engineering unit field  
   Call this from `updateConsoleDeviceUnitField` / decimals refresh whenever device or param changes (`refreshConsoleDeviceParamFields` path). Keep persisted `binding.unit` string as today; if the saved unit is not in the new option list after a param change, reset to the first option (same pattern as other dependent selects).

6. **Display path**  
   Place/user formatters already take `binding.unit` and often a type id; prefer the same measure-kind resolver when formatting so humidity reportables never convert as °C. Prefer minimal shared helper duplicated or factored carefully in both HTML pages only as needed for editor (user page has no binding editor).

7. **One linear style enum (four states)**  
   Persist `gauge.linearStyle` as one of: `bar`, `filled`, `vertical`, `verticalFilled`. Missing/invalid → `bar` (today’s horizontal growing fill). Editor: single select, visible only when type is Linear.  
   *Rejected:* two booleans `filled` + `vertical` (user asked for one option with four states).

8. **Filled drawing**  
   Reuse arc range-color segmentation on a straight thick stroke/band; place a small arrow (straightened needle) at the value position. Do not use the growing single-color fill for `filled` / `verticalFilled`.

9. **Vertical height vs label**  
   For `vertical` / `verticalFilled`, layout the binding so the gauge track flexes to the remaining height of the cell/row after the title (showTitle / titlePosition). Top/bottom titles reduce height; left/right titles leave height for the track beside the label. Fit scaling must not leave a short stub gauge when the container is tall.

10. **Host store**  
    Extend `ConsoleGaugeConfig` + parse/serialize in `ConsoleStore` so reboot round-trips `linearStyle`. Arc bindings may omit the field.

## Risks / Trade-offs

- [Brief network blips flash the modal] → Mitigation: optional short grace (e.g. show disconnected after ~300–500 ms still down; show reconnecting immediately on known retry) so single packet loss does not flicker; keep grace small so real outages stay obvious.
- [Existing bindings saved with wrong unit for param] → Mitigation: display still applies stored unit string; editor corrects on next open/param change; no mandatory migration rewrite.
- [Overlay vs nested dialogs] → Mitigation: high z-index front overlay; document that connection loss interrupts editing until restored.
- [Vertical + composite/grid fit interaction] → Mitigation: reuse existing viewport fit hooks; verify title positions top/bottom/left/right explicitly in tasks.

## Migration Plan

- Ship with firmware `3.9.10` LittleFS web assets and host console JSON that accepts the new optional field.
- Older consoles without `linearStyle` keep `bar` behavior.
- Rollback: previous firmware ignores unknown JSON keys / restores old HTML.

## Open Questions

None material; exact SVG arrow geometry can match the arc needle tip style during apply.
