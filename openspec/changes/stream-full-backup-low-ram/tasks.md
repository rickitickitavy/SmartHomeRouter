## 1. Streaming ZIP writer

- [x] 1.1 Add a store-method ZIP stream encoder (bit-3 data descriptors + deferred central directory) that emits into a caller buffer without retaining member payloads or the finished archive
- [x] 1.2 Ensure `StoreZipReader` (Full restore) accepts ZIPs produced with data descriptors / streamed local headers
- [x] 1.3 Keep or retire the old DRAM-copy `StoreZipWriter` path so Full backup no longer uses it

## 2. Full backup HTTP path

- [x] 2.1 Replace `fullBackupBuildZip` + `OwnedZipBytes` whole-buffer response with a stream state owned for the AsyncWebServer chunk callback
- [x] 2.2 Use a fixed **4–8 KiB** work/send buffer for generation and HTTP fills
- [x] 2.3 Stream `manifest.json` / `settings.json` from existing PSRAM/string sources without DRAM ZIP staging copies
- [x] 2.4 Stream scene bake JPEGs from LittleFS in buffer-sized reads (prefer no full-file DRAM copy)

## 3. History member

- [x] 3.1 When Include historical data is on, stream `history/snapshot.bin` from on-disk snapshot and/or PSRAM snapshot source without DRAM ZIP entry duplication or a finished-archive buffer
- [x] 3.2 When Include historical data is off, omit the history member as today

## 4. Verify + version

- [x] 4.1 Confirm Full backup ZIP still opens under Full restore; Free RAM no longer collapses by whole-archive size during/after download
- [x] 4.2 Confirm host firmware build is **4.6.41** (apply bump; propose was 4.6.40)
