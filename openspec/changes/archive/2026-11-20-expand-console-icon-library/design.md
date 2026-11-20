## Context

- Picker uses `BINARY_SENSOR_ICON_CHOICES` (~42 `mdi:…` names) and MDI webfont + CSS on/off colors.
- Operator rejected “add as many as possible / full catalog” and supplied an explicit add list.

## Goals / Non-Goals

**Goals:**

- Union current icons with the approved list below.
- Keep CSS-recolorable MDI webfont rendering.

**Non-Goals:**

- CDN `meta.json` full-catalog fetch.
- PNG/JPG/multi-color SVG assets.
- Changing Icon vs Gauge eligibility.
- Slave changes.

## Decisions

1. **Approved additions (MDI names → `mdi:<name>`)**  
   Exact set from the operator (78 names). Deduplicate against the existing list (`water-pump` already present).

```
flower-tulip
flower-tulip-outline
flower-outline
fence-electric
fence
greenhouse
heat-wave
pine-tree-fire
pine-tree-variant
sprinkler-variant
sprinkler
water-pump
watering-can
weather-dust
battery-charging-wireless-alert
boom-gate-alert
dog-side
volume-high
volume-off
car
car-battery
key-chain-variant
oil
cellphone-basic
phone-dial
database
emoticon-angry
blender
bowl-mix
glass-mug
toaster-oven
map-marker
wrench
amplifier
bathtub
boom-gate
boom-gate-up
bulkhead-light
cctv
ceiling-light
ceiling-fan-light
ceiling-light-outline
chandelier
coach-lamp
coffee-maker
curtains
curtains-closed
desk-lamp
fire-extinguisher
floor-lamp
floor-lamp-dual
floor-lamp-torchiere
floor-lamp-torchiere-variant
floor-plan
globe-light
heating-coil
home-thermometer
lamps
led-on
led-strip
led-strip-variant
light-flood-down
light-flood-up
lightbulb-fluorescent-tube
lightbulb-group
lightbulb-spot
molecule-co2
patio-heater
pipe
post-lamp
string-lights
track-light
vacuum
vanity-light
wall-sconce
wall-sconce-flat
wall-sconce-round
water-boiler
```

2. **Keep existing icons**  
   Do not remove today’s door/window/motion/… entries.

3. **No CDN meta**  
   Bake the union into `BINARY_SENSOR_ICON_CHOICES` (or successor array). Optional client-side filter is fine if the grid feels long; not a product requirement for this change.

4. **Missing glyph**  
   If a name is absent from `@mdi/font@7.4.47`, skip or substitute only after noting it in apply notes—do not invent a different icon without asking.

## Risks / Trade-offs

- **[Risk] Name not in linked font version** → Mitigation: verify against MDI 7.4.47 during apply; ask before substituting.

## Migration Plan

- Existing bindings unchanged. New icons available in the picker after host UI update.

## Open Questions

None — list is operator-approved.
