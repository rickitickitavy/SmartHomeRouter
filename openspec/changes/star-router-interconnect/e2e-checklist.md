# STAR interconnect E2E checklist (task 5.1)

Two hosts on the same LAN, both on firmware **4.4.05+**. Call one **Main**, one **Repeater**.

## Prep

1. Flash both hosts; open admin console on each.
2. System → Interconnection: set distinct **router names**, same **port** (default `10241`), Save.
3. Set Main role = **Main**, Repeater role = **Repeater** (double-confirm on role change; device reboots after role change).
4. Network → ZigBee: note Main channel; leave Repeater channel alone (becomes read-only after pair).

## A. Pair

### Direct IP

1. On Repeater: **Pair to host** → countdown + LED6 2 Hz blink.
2. On Main: **Pair** → enter Repeater IP + port → **Start**.
3. Expect: Main peer table row; Repeater shows Main name/IP/MAC; blink/modal stop.
4. Refuse path: with 7 peers already, 8th pair must fail.

### Subnet scan (optional)

1. Repeater: Pair to host again (or use a fresh unpaired Repeater).
2. Main: Pair → leave IP empty → Start → confirm found candidate → Yes.
3. Expect same as successful direct pair.

## B. Boot hello + variables

1. Reboot Repeater only.
2. Expect: after STA up, Main peer goes online; Repeater applies `zigbeeChannel` / `checkAliveSec` from Main (`variable`); Interconnection In/Out move.

## C. Search + save remote device

1. On Main: Devices → Add → Search (Multi-router).
2. Join a Zigbee device near the **Repeater** radio.
3. Expect found row **Router** = `#<routerId>` (not `local`).
4. Add/Save on Main. Expect device `routerId` persisted after reload.

## D. Report forward

1. Trigger a report on the remote device.
2. Expect Main Devices live/status update; peer **In** increases; no local SPI enqueue for that IEEE on Main.

## E. Remote command (all kinds you use)

1. From Main, send On/Off, covering/cluster, and/or write-attr as applicable to the remote device.
2. Expect device reacts; peer **Out** increases.
3. Take Repeater offline (unplug/Wi-Fi down): device shows offline/gray; command fails with offline-router reason.

## F. Remote rebind

1. Rebind the remote IEEE from Main.
2. Expect success, pairing-fallback, or timeout failure within ~30 s; registry row **kept**.

## G. checkAlive

1. Leave both online idle past 2× checkAlive interval.
2. Expect Main peer online stays green; after Repeater power-off, online goes gray within ~2 intervals.

## Pass / fail

| Step | Pass? | Notes |
|------|-------|-------|
| A Pair | | |
| B Hello/vars | | |
| C Search/save | | |
| D Forward | | |
| E Command | | |
| F Rebind | | |
| G checkAlive | | |

When all Pass, mark OpenSpec task **5.1** and archive with `/opsx-archive`.
