## 1. Hardware JSON apply / restore

- [x] 1.1 In `applyHardwareJson` (settings restore) and Hardware POST, replace reject-on-range for history days, sweep, max size KiB, SD/flash backup intervals with clamp to firmware min/max
- [x] 1.2 Map invalid `historyEntrySize` to nearest allowed choice (128/256/384/512/768/1024/1536/2048; tie → default 1024)
- [x] 1.3 Settings restore does not return 400 for out-of-range hardware numbers (SPI, RF SPI, brightness, low-memory threshold are clamped). Hardware POST still returns 400 for brightness 0/101
- [x] 1.4 Log when a field is clamped (name + before/after)
- [x] 1.5 Confirm settings restore no longer returns 400 for out-of-range hardware numerics

## 2. Version

- [x] 2.1 Bump `FIRMWARE_VERSION_HOST` build on apply (now **4.3.34**); leave slave unchanged

## 3. Verify

- [x] 3.1 Restore file with `historyMaxSizeKiB` above firmware max → restore OK, stored value = max
- [x] 3.2 Restore file with history days below min → stored value = min
- [x] 3.3 Restore file with invalid entry size → nearest allowed
- [x] 3.4 In-range values unchanged; brightness still 400 on 0/101 via Hardware POST; restore clamps brightness instead of 400
