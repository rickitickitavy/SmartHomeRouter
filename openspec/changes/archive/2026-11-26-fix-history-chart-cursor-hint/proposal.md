# Change: Fix history chart cursor hint

## Why

On the history chart (operator and simple-user), the cursor hint label is hard to read: multiline content collapses into overlapping characters. Near the right edge of the plot, the hint is clipped by the chart border even though there is room to place it on the opposite side of the cursor.

## What Changes

- Make the cursor hint fully readable by laying out time and value on **separate lines** (2–3 lines as needed) with a normal line-height (not inherited `line-height: 0` from the canvas wrap).
- Position the hint so that if it would overflow the chart viewport (especially the right edge), it flips to the **opposite side** of the cursor (and clamps within the wrap). Apply the same logic for left/top/bottom overflow where needed.
- Keep Place/operator (`index.html`) and simple-user (`user.html`) behavior synchronized; shared CSS in `all.css`.

## Capabilities

### Modified Capabilities

- `web-console`: History chart cursor tooltip readability and edge-aware placement.

## Impact

- UI only (history chart overlay). No store/API/firmware protocol changes beyond host version bump for the UI FS bundle.
- Host firmware build bumped to **4.6.38** at propose.
