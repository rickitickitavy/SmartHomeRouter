# Tasks

## 1. Connection overlay UI shell

- [x] 1.1 Add shared connection-overlay markup and CSS (Disconnected / Reconnecting states, non-dismissible front overlay, disconnected and RECONNECTING pictures/icons) in `data/css/all.css` plus both `data/index.html` and `data/user.html`, and verify both pages render the two states when forced visible in the browser
- [x] 1.2 Wire overlay show/hide helpers that ignore unsigned-in and `firmwareRebootWaitActive`, force-hide on sign-out, and verify reboot-wait and login flows never leave the connection overlay stuck visible

## 2. Connection state detection

- [x] 2.1 Track HTTP reachability from signed-in session/main fetch failures and `navigator.onLine`, drive disconnected vs reconnecting text/icon, and verify cutting network or going offline shows Disconnected then Reconnecting while retries run
- [x] 2.2 Hook required live WebSocket `onclose` / `CONNECTING` / `onopen` (and existing reconnect timers) on operator and user pages so a closed wanted socket shows the overlay and a restored open socket clears it when HTTP is OK; verify by killing the socket or stopping the host briefly then restoring
- [x] 2.3 Confirm intentional sign-out and firmware reboot wait still use login / reboot UI only (no Disconnected modal), observable in those two flows

## 3. Binding units from selected parameter

- [x] 3.1 Add a measure-kind resolver from selected binding param (sensorValue → device Measurement type, reportable → `reportableFormatTypeId`, battery → percent, else none) and use it in `updateConsoleDeviceUnitField` / decimals defaults in `data/index.html`; verify temperature Sensor value still lists °C/°F/K
- [x] 3.2 Refresh unit options when the param select changes; verify Battery level on a temperature device does not offer Celsius/Fahrenheit/Kelvin, and a humidity reportable offers % not temperature units
- [x] 3.3 Align Place/live formatting type id with the same measure kind for reportable/battery bindings where display still keys off device type alone, and verify a humidity reportable binding does not display as °C

## 4. Linear gauge four-state style

- [x] 4.1 Persist `linearStyle` (`bar` | `filled` | `vertical` | `verticalFilled`, default `bar`) on `ConsoleGaugeConfig` through `ConsoleStore` parse/serialize; verify save/reload/reboot keeps `verticalFilled` and missing field loads as `bar`
- [x] 4.2 Add the binding-dialog Style select (four labels) shown only when Gauge type is Linear in `data/index.html`; verify Arc hides it and Save stores the chosen style
- [x] 4.3 Render `filled` / `verticalFilled` as a straight range-colored band with small value arrow (same range colors as arc) on Place and user pages; verify it is not the growing single-color fill bar
- [x] 4.4 Render `vertical` / `verticalFilled` oriented vertically using maximum remaining container height after the binding label; verify top/bottom titles shrink the track and left/right titles still allow full height beside the label

## 5. Integration check

- [x] 5.1 Smoke both pages signed-in: disconnect/reconnect overlay; multi-param units; each of the four linear styles; bump `FIRMWARE_VERSION` build on apply source edit and confirm version string matches
