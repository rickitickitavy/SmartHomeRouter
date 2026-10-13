# Design

## Context

See proposal.md for motivation. Console gauges already exist (`viewStyle: gauge`, arc/linear, min/max/ranges) in `data/index.html` and `data/user.html`. Eligibility is gated by `consoleBindingParamIsNumeric`, which returns false for `status` before Measurement types are considered. New bindings default to `param: "status"`, and `fillAutomationParamSelect` injects an unmatched previous value as a selected option—so Measurement devices keep a fake Status selection and never show Gauge.

Measurement live state is already published as scaled engineering text (e.g. `22.50` for temperature). Display helpers `placeParseNumericState` / `parseNumericState` and `placeFormatMeasurement` / `formatMeasurementDisplay` exist; gauge needle parsing currently uses a thinner `parseBindingNumericValue`.

## Goals / Non-Goals

**Goals:**
- Make Measurement Sensor value bindings eligible for Gauge in the Consoles dialog.
- Ensure new/switched Measurement bindings land on `sensorValue` (catalog), not orphan `status`.
- Align gauge numeric parsing with the measurement display parse path (and unit conversion when the binding unit changes the needle value).
- Gauge readout is digits only (no unit suffix); arc center value font is 15% smaller (16px → 13.6px).

**Non-Goals:**
- Changing host console JSON schema or SPI/device report formatting.
- Expanding which Zigbee types are classified as Measurement beyond the existing `isMeasurementDeviceType` set (unless a clear gap blocks temperature/pressure).
- Automation condition GREATER/LESS semantics beyond what is needed for console gauges.
- Fragile-zone OTA / InterChip work.
- Changing linear gauge value font size.

## Decisions

### 1. Fix param default / orphan Status before expanding eligibility
- **Choice:** When filling the param select, if `selectedParam` is not in `automationKnownParamsForDevice(device)`, select the first catalog id (do not append the unmatched value as selected). Default new binding drafts to empty/`sensorValue`-friendly param so Measurement gets Sensor value.
- **Why:** Eligibility already treats `sensorValue` as numeric; the user-visible failure is mostly the stuck `status` selection.
- **Alternative:** Also treat Measurement + `status` as numeric. Rejected as primary fix—Status is the wrong catalog label for sensors and would confuse On/Off vs Measurement. May still treat Measurement + `status` as numeric only as a safety net for already-saved bad bindings.

### 2. Keep Measurement type list centralized
- **Choice:** Reuse existing `isMeasurementDeviceType` in index/user pages for gauge eligibility and default-param behavior.
- **Why:** Same set already drives measurement text formatting.
- **Alternative:** Drive eligibility only from param id `sensorValue`. Insufficient alone if orphan `status` remains; combine with Decision 1.

### 3. Shared parse for gauge needle; unitless readout
- **Choice:** Gauge render paths call the same numeric parse (and unit conversion) used for measurement text so min/max stay in operator units (°C/°F/hPa/…). Pass a unitless formatted number as `displayText` (fixed decimals, no suffix).
- **Why:** Spec requires engineering-unit consistency for the needle and forbids unit suffixes on the visible value.
- **Alternative:** Keep `parseBindingNumericValue` only. Works for plain `22.50` but diverges when unit conversion or `val=` raw forms appear. Stripping units from `formatMeasurementDisplay` output is fragile vs formatting digits directly.

### 4. Arc value font via CSS
- **Choice:** Set `.console-gauge-value-svg { font-size: 13.6px; }` in `data/css/all.css` (was 16px). Leave `.console-gauge-value` (linear) unchanged.
- **Why:** Single shared stylesheet for Place and user pages.
- **Alternative:** Inline SVG `font-size` attribute per render. Rejected; CSS already owns the class.

### 5. UI-only change surface
- **Choice:** Implement in `data/index.html`, `data/user.html`, and `data/css/all.css` (FIRMWARE_VERSION bumped on apply). No C++ console-store changes.
- **Why:** Bug and eligibility are console JS; host already stores arbitrary viewStyle/gauge JSON.

## Risks / Trade-offs

- [Saved bindings still have `param: "status"` on Measurement devices] → On load, remap unmatched params to first catalog option when opening the dialog; optional render-time: if Measurement and param is `status`, treat like `sensorValue` for gauge eligibility and value resolve.
- [Occupancy / unknown types also get `sensorValue` in the broad else branch] → Out of scope; do not widen Measurement set unless needed for temperature/pressure.
- [Unit conversion changes needle vs prior raw parse] → Desired; document that min/max are in the binding’s selected unit.

## Migration Plan

1. Flash firmware / refresh LittleFS web assets with 3.3.29+.
2. Existing Measurement bindings with correct `sensorValue` + gauge keep working.
3. Bindings stuck on orphan `status`: reopen binding dialog (param snaps to Sensor value) or re-save; optional soft treat-as-sensorValue on render.
4. Rollback: previous firmware assets; no store migration required.

## Open Questions

None — Measurement set and Sensor value catalog behavior are already established; this change corrects eligibility and defaults around them.
