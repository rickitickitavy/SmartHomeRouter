# Design

## Context

Icon view style already stores `icon`, optional ON/OFF colors, and `iconScale` on `ConsoleIconConfig` / binding JSON. Scene pins use the same icon painting helpers in Place and `user.html` (pins fill `sizeW`/`sizeH`; scale field is N/A). See proposal.md for motivation.

## Goals / Non-Goals

**Goals:**
- Persist and apply Flip for every place icons are authored and shown in consoles.
- CSS-only mirror via `transform` (scaleX/scaleY −1), compatible with existing color/scale/align.

**Non-Goals:**
- Rotating icons by arbitrary degrees.
- Separate flip for ON vs OFF states.
- Changing which bindings may use icon view style.
- Slave / Zigbee protocol changes.

## Decisions

1. **Field name `iconFlip`** on binding/pin JSON and in `ConsoleIconConfig` (uint8 or small enum).  
   Stored ids: `normal` | `vertical` | `horizontal` | `both`.  
   UI labels: Normal | Vertical | Horizontal | Vert + horiz.  
   Missing/unknown → `normal`.

2. **CSS application** on the painted `.mdi` (or wrap) element:  
   - normal: no flip transform (or `none`)  
   - vertical: `scaleY(-1)`  
   - horizontal: `scaleX(-1)`  
   - both: `scale(-1)` / `scaleX(-1) scaleY(-1)`  
   Combine with existing scale via separate properties or multiply transforms carefully so viewport/`iconScale` still work.

3. **Editor UX**  
   Show Flip select in the same icon group as icon picker / Icon scale, visible only when view style is `icon` (same gate as iconScale). Scene pin icon authoring gets the same control when the pin uses icon style.

4. **Surfaces**  
   Apply in operator Place, simple-user page, and Consoles editor preview — same helper path as today for icon paint. Scene cells that render pin icons MUST read `iconFlip` from the pin/binding payload.

5. **Export / restore**  
   Consoles and scenes JSON already round-trip binding/pin objects; ensure parsers keep unknown-safe default and writers emit `iconFlip` when not normal (or always emit for clarity — prefer always emit for simpler diffs).

## Risks / Trade-offs

- [Optical centering after flip] → Keep align rules; flip is about glyph mirror, not layout box.
- [Transform vs parent scale] → Apply flip on the glyph node; keep `--console-icon-scale` on the wrap as today.
- [Legacy stores without field] → Default Normal; no migration rewrite required.

## Migration Plan

- Existing bindings/pins without `iconFlip` render Normal.
- No automatic data rewrite on load.

## Open Questions

- None blocking; label spelling uses **Vert + horiz** (user’s “vart + horiz”).
