# Proposal

## Why

Console widget gauges are offered only when the binding parameter is treated as numeric, but Measurement-class devices (temperature, pressure, humidity, illuminance, flow, metering, electrical measurement) often end up with param `status` (new-binding default or an orphan option after device select). That path is hard-coded as non-numeric, so View style Gauge never appears and operators cannot show those readings as gauges.

## What Changes

- Treat Measurement-class sensor readings as numeric for console binding View style / gauge eligibility (parameter `sensorValue`, and the device’s main reading when that is what the catalog offers).
- Stop leaving an unmatched default `status` selected on Measurement (and other non-status) devices when filling the param picker; fall back to the first catalog parameter (`sensorValue`).
- Keep gauge needle using the same engineering-unit parse path as the existing measurement text display (so temperature/pressure gauges track the scaled value, not a mis-parsed raw token).
- Gauge readout text MUST show the numeric value only — no measure unit suffix (°C, hPa, %, lx, etc.).
- Arc gauge center value font size MUST be 15% smaller than the previous default (16px → 13.6px).
- No host persistence schema change; existing saved gauge bindings keep working. No **BREAKING** API changes.

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `control-consoles`: Clarify that Measurement-class `sensorValue` (and the device’s main numeric reading) counts as a numeric binding for view style `gauge`.
- `web-console`: Require the Consoles device-binding dialog to offer Gauge for Measurement devices, and to select a catalog param instead of an orphan `status` when the device does not list Status.

## Impact

- Web UI: `data/index.html` (Consoles binding dialog + Place preview), `data/user.html` (simple-user gauge render).
- Firmware version **3.3.30** (propose 3.3.29, apply bump).
- No InterChip / SPI OTA / fragile-zone changes. Host console store JSON shape unchanged.
