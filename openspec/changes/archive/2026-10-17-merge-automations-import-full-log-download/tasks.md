# Tasks

## 1. Automations Import all merge

- [x] 1.1 Add merge/upsert import path (keep names not in file; replace matching names; add new) used by Automations Import all — verify: host has A+B, import only A′ → A updated, B remains
- [x] 1.2 Update Import all confirm copy to describe merge (not replace-all wipe) — verify: dialog text matches
- [x] 1.3 Keep settings restore automations slice as replace-all if it shares helpers — verify: settings restore with automations array still replaces that group; Import all still merges
- [x] 1.4 Bump `FIRMWARE_VERSION` build by 1 on apply (propose `3.5.14`) — verify: `include/Defines.h`

## 2. Download full log

- [x] 2.1 Ensure `GET /api/log/download` streams the full ring used length (not `kWebViewMaxBytes`); fix server/client truncation if download is stuck at ~64 KiB — verify: with ring > 64 KiB, downloaded file size ≈ `X-Log-Ring-Used` / used length
- [x] 2.2 Leave on-screen `GET /api/log` at the 64 KiB tail cap — verify: Log tab still loads quickly and does not require the full ring

## 3. Soak

- [ ] 3.1 Manual: Import all merge + Download full log after prolonged logging — verify: merge keeps extras; download larger than 64 KiB when ring is full enough
