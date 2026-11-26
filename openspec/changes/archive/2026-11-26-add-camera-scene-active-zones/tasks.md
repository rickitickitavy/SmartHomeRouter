## 1. Scene model and persistence

- [x] 1.1 Add scene `kind` (`plan` | `camera`; missing ⇒ plan), Camera fields `cameraId`, `refreshInterval` (default 3, clamp 1..60), and `zones[]` to scene store/API/export/restore
- [x] 1.2 Define zone schema: polygon (normalized image-box coords), transparency, colorOn/colorOff when transparency &lt; 1, state binding, actionOn/actionOff (device|automation)
- [x] 1.3 Ensure Camera scenes do not require or surface pins; Plan scenes unchanged for bake/pins

## 2. Scene editor UI

- [x] 2.1 Add fixed kind dropdown (**Camera** | **Plan**) before **Edit scene**
- [x] 2.2 When kind is Camera, hide **Edit scene**; show fixed registered-camera dropdown after kind; hide camera dropdown for Plan
- [x] 2.3 Camera bind preview: zone polygon editor (floor-like vertex tools, **no Shift lock**); design-time fill always 70% transparent
- [x] 2.4 Zone inspector: transparency, ON/OFF colors (required when not fully transparent), state binding, ON/OFF actions, refresh interval field
- [x] 2.5 Save only after successful Save; Cancel discards unsaved new zones/scenes per no-preadd rule
- [x] 2.6 **Edit scene** invisible when kind is Camera (Plan-only geometry editor)

## 3. Live view (Place + simple-user)

- [x] 3.1 Render Camera scene background from selected camera JPEG; refresh every scene `refreshInterval` seconds (reuse console camera refresh helpers where practical)
- [x] 3.2 Draw zones with runtime transparency and ON/OFF colors from bound state; scale with image box
- [x] 3.3 Click: hit-test polygon → run action for **current** state (console-equivalent handler)
- [x] 3.4 Apply the same live-view behavior on Place (`index.html`) and simple-user (`user.html`)
- [x] 3.5 Missing/deleted camera: clear unavailable UI without crash

## 4. Export / version

- [x] 4.1 Include kind/camera/refresh/zones in scene and global settings export/restore
- [x] 4.2 Host firmware bumped to **4.6.22** for this apply
