# Tasks

## 1. Camera store and APIs

- [x] 1.1 Add host camera registry types (id, name, active, type ONVIF, display URL, users[], two channels with snapshot+live HTTP URLs, credentials/endpoint fields) allocated in PSRAM only — verify: load with PSRAM present keeps records off internal heap; no-PSRAM fails closed
- [x] 1.2 Persist registry to LittleFS JSON and reload after reboot — verify: save two cameras, reboot, GET list matches
- [x] 1.3 Implement camera HTTP APIs (list/get/create-on-save/update/delete/export/import replace-all with confirm) with no pre-add — verify: Cancel add leaves no row; Save creates; Import replace matches export
- [x] 1.4 Implement Active/Offline/Online/short-error status via lightweight probe (no internal-RAM image buffer) — verify: inactive→Offline; active unreachable→short error; active reachable→Online

## 2. Cameras operator UI

- [x] 2.1 Add sidebar **Cameras** section (`isAdmin` only) with table columns Active, Name, Type, URL, Users, Status — verify: non-admin sidebar hides Cameras; admin table populates from list API
- [x] 2.2 Card footer Add / Export / Import wired like Consoles/Automations patterns — verify: Export downloads JSON; Import confirm then replace-all
- [x] 2.3 Camera dialog: Name, Active, Type dropdown (ONVIF only), ONVIF settings + two channel URL pairs; Save first persist; delete confirm — verify: Type not free-text; Cancel discards draft; Save round-trips all fields including users multi-select
- [x] 2.4 Cameras footer Discover dialog: type dropdown (ONVIF), optional credentials, table of unknown discovered cams, Discover/Stop toggle (WS-Discovery then ONVIF media URI fill), Add discovered (opens Add draft with filled channel URLs), Manual (base Add), Close; camera dialog Test button re-resolves ONVIF URLs — verify: registered hosts omitted; Add discovered does not persist until Save; channel snapshot/live filled when camera answers ONVIF

## 3. Console camera cells

- [x] 3.1 Extend console persistence for composite content `camera` (camera id, mode `jpegRefresh`|`live`, channel 1|2) with defaults — verify: save/reload/reboot round-trip; older consoles without camera still load
- [x] 3.2 Consoles editor: Content option Camera with camera/mode/channel controls (sync with existing empty/text/device) — verify: preview shows camera cell; Save persists; recycle-bin rules unchanged for row deletes
- [x] 3.3 Place (`index.html`) and simple-user (`user.html`) render JPEG refresh vs live from browser-direct URLs; honor users ACL with non-revealing placeholder — verify: both pages match behavior; denied user gets no URLs; allowed user sees image/live

## 4. Email snapshot attach (fragile `smtp-send`)

- [x] 4.1 Obtain explicit approval for fragile zone `smtp-send` before editing `EmailSender` — verify: `.cursor/fragile-zones.allow` (or human-stated zone approval) names `smtp-send`
- [x] 4.2 Extend email action JSON/UI with optional attach (camera + channel); PSRAM fields round-trip — verify: automation save/list/import/export keep attach fields; editor shows controls only for e-mail target
- [x] 4.3 Fetch snapshot HTTP URL into PSRAM-only buffer (cap size); never internal RAM; on failure warn and continue text send — verify: successful attach uses PSRAM; OOM/fetch fail logs warning and text mail still queues
- [x] 4.4 Extend `EmailSender` async path for one JPEG attachment from PSRAM and free after send — verify: SMTP message includes attachment when buffer present; text-only path unchanged

## 5. Integration

- [x] 5.1 Bump `FIRMWARE_VERSION` build on apply (after propose `3.9.31`) when source lands — verify: `include/Defines.h` shows the apply build
- [ ] 5.2 Smoke: register ONVIF camera, Place + user console camera cell (JPEG + live), email action with channel attach — verify: browser loads cam URLs directly; email arrives with JPEG when cam reachable; Status Memory still healthy (no internal-heap image growth)

## Workflow follow-up

- Archive the change after review and apply are complete.
- Sync delta specs to main specs on archive / `/opsx-sync` as usual.
