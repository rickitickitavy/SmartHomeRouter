# Tasks

## 1. Param picker and numeric eligibility

- [x] 1.1 In `data/index.html`, change `fillAutomationParamSelect` so an unmatched `selectedParam` does not become the selected option; select the first catalog param instead, and verify choosing a temperature device no longer leaves Status selected
- [x] 1.2 Default new console device binding drafts to an empty/`sensorValue`-friendly param (not hard-coded `status`) and verify a new Measurement binding opens with Sensor value selected
- [x] 1.3 Update `consoleBindingParamIsNumeric` / gauge eligibility so Measurement-class `sensorValue` (and Measurement + orphan `status` as a safety net) allows Gauge, while On/Off Status still hides it; verify View style appears for temperature/pressure Sensor value

## 2. Gauge value parse alignment and presentation

- [x] 2.1 In Place preview (`data/index.html`) and simple-user page (`data/user.html`), drive gauge needle numeric values through the same measurement parse/unit path as the text display; verify a temperature gauge moves with the Celsius (or selected unit) reading within min/max
- [x] 2.2 Pass unitless digit text into gauge display (no °C/hPa/%/lx); verify temperature arc shows e.g. `22.5` not `22.5 °C`
- [x] 2.3 Set `.console-gauge-value-svg` font-size to 13.6px (15% below 16px) in `data/css/all.css`; verify arc value is smaller while linear `.console-gauge-value` is unchanged
- [x] 2.4 Confirm battery/position/custom gauge paths still parse their numeric values after the shared helper change

## 3. Integration check

- [x] 3.1 Manually: Consoles → widget → add temperature and pressure bindings → Sensor value → Gauge → save; Place and user pages show arc/linear gauges with live readings
- [x] 3.2 Manually: switch binding device from On/Off to temperature and confirm param snaps to Sensor value and Gauge becomes available without a stuck Status option

## Workflow follow-up

- After implementation and review, sync specs / archive this change with the project’s usual OpenSpec workflow.
- Commit and push on a feature branch (never on `master` / `main` / `develop`).
