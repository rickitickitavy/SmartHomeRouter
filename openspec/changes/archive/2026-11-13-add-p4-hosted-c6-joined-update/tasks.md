## 1. Package members

- [x] 1.1 Join: optional `--hosted-c6` + `--hosted-c6-version` → `hosted-c6.bin` + `hosted-c6.version.txt`
- [x] 1.2 Split: extract `hosted-c6.bin` when present
- [x] 1.3 One short README note: optional third member; present + needed only

## 2. P4 apply (fragile `firmware-update-process`; no SPI protocol edits)

- [x] 2.1 After ZIP stage: if no `hosted-c6.bin` → skip
- [x] 2.2 If present: require version text; read running version once; match → skip; differ → SDIO apply; unreadable → fail Hosted step
- [x] 2.3 Then existing slave SPI / host apply unchanged; S3 ignores Hosted members

## 3. Wrap-up

- [x] 3.1 Host build bump only when implementing; no Status Hosted poll; no extra features

## 4. Host LED3 web activity (S3 + P4)

- [x] 4.1 Host LED3 color blue; ON for any HTTP/WebSocket request, OFF when response finished (refcount)
- [x] 4.2 README / status-rgb-led delta for the new meaning
