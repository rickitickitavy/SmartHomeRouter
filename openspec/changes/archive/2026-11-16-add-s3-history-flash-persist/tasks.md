## 1. Hardware settings (S3)

- [x] 1.1 Add `historyPersistFlash` (default true) and `historyFlashBackupIntervalMin` (default 120, clamp 30–600) to hardware settings load/save/export/restore
- [x] 1.2 Enforce size gate: persist ON only if `historyMaxSizeKiB` ≤ 4096; auto-set persist false whenever size is saved/loaded > 4096
- [x] 1.3 Expose both fields on GET/POST `/api/hardware` for S3; omit S3-only flash controls from P4 Hardware API/UI as designed

## 2. HistoryStore / boot (S3)

- [x] 2.1 When persist ON: run periodic LittleFS full snapshot on the flash interval (same payload as reboot flush); skip periodic cycle when OFF
- [x] 2.2 When persist ON: after successful boot LittleFS restore, do **not** delete snapshot files; when OFF keep delete-after-restore
- [x] 2.3 Status API: expose remaining time before next flash save when persist ON; omit/null when OFF
- [x] 2.4 Status API: cache and expose real on-flash history size (KiB) from restore/save sessions when persist ON; no LittleFS open solely for Status size
- [x] 2.5 Keep reboot-path LittleFS flush behavior; P4 SD path unchanged

## 3. Web console

- [x] 3.1 Hardware (S3): checkbox **persist historical on internal flash** (square-only); enable ON only when max size ≤ 4096; reflect server auto-OFF after large size save
- [x] 3.2 Hardware (S3): show **interval save historical data** (30–600, default 120) when persist ON; hide/disable when OFF
- [x] 3.3 Status Historical storage: show time before save and **On-flash history size** when S3 flash persist is active (`index.html` and `user.html`)
- [x] 3.4 Status File system (S3 only): show **Historical data size** using the same cached size; omit on P4 and when persist OFF (`index.html` and `user.html`)
- [x] 3.5 Confirm Maintenance / global settings export-restore includes the new Hardware fields; update hint copy if needed

## 4. Verify

- [ ] 4.1 S3: persist ON, size ≤ 4096 → boot restores and files remain; interval save runs; Status countdown + On-flash history size + File system Historical data size present and match
- [ ] 4.2 S3: raise size > 4096 → persist forced OFF; delete-after-restore resumes; no periodic flash save; size rows hidden
- [ ] 4.3 P4 SD history persist / interval / on-SD size UX unchanged; no File system Historical data size row
