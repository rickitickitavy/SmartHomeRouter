## MODIFIED Requirements

### Requirement: Binding dialog View style for binary sensors and On/Off
When the selected target is a binary sensor type (Binary Input / Output / Value or BACnet binary), an On/Off device, an automation, or a `userData` device parameter whose schema field type is `bool`, the binding dialog MUST show View style with Default and Icon (not Gauge). When Icon is selected, the dialog MUST show an icon picker of Material Design Icons (Home Assistant / Pictogrammers) that includes **all previously offered icons** and the **operator-approved additional MDI names** recorded for this change (garden/outdoor, lighting, water, gates, appliances, and related ids such as `flower-tulip`, `sprinkler`, `ceiling-light`, `curtains`, `boom-gate`, `water-boiler`, etc.). Icons in the picker and on the console MUST remain **CSS-recolorable** (MDI webfont / `color`, including existing Icon on/off color controls). The picker MUST NOT rely on fixed-color raster images and MUST NOT require loading the full public MDI meta catalog. Measurement numeric bindings MUST keep Default/Gauge and MUST NOT use this Icon View style path. `userData` int/float/string params MUST NOT offer Icon.

#### Scenario: Approved lighting icon available
- **WHEN** the operator selects Icon for an On/Off or binary / bool binding
- **THEN** the icon picker includes `mdi:ceiling-light` (and the other approved additions present in the linked MDI font)

#### Scenario: Prior icons still available
- **WHEN** the operator opens the Icon picker
- **THEN** previously offered icons such as `mdi:door` and `mdi:water-pump` remain selectable

#### Scenario: Icon color still follows operator colors
- **WHEN** the operator picks an approved new icon and sets Icon on/off colors
- **THEN** the live and preview icon glyphs use those colors (not a baked multi-color bitmap)

#### Scenario: Bool userData still offers Icon
- **WHEN** the binding target is a `userData` device and the selected parameter’s schema type is `bool`
- **THEN** View style shows Default and Icon and the icon picker is available when Icon is selected
