# Tasks

## 1. Host CPU sampler and status API

- [x] 1.1 Add host FreeRTOS idle-based sampler for CPU0/CPU1 (unavailable until first window); verify values move under load and stay in 0–100
- [x] 1.2 Publish `hostCpu0Percent` / `hostCpu1Percent` (null when unavailable) in gateway status JSON; verify `/api/status` (or existing status GET) returns them

## 2. Slave CPU and SPI

- [x] 2.1 Cancelled — host (master) only; no slave sampler
- [x] 2.2 Cancelled — host only; no SPI status CPU field
- [x] 2.3 Confirm SPI OTA begin/chunk/end paths are untouched (no InterChip/SpiProtocol edits in this change)

## 3. Status UI

- [x] 3.1 Add Status **CPU** group (Host CPU0, Host CPU1) after Free memory in `data/index.html` and `data/user.html`; verify both pages show matching values and em dash when null
- [x] 3.2 Confirm Status column order includes CPU after Free memory; verify on operator and simple-user Status

## 4. Integration

- [x] 4.1 Host build bump **4.2.79**; slave unchanged (**4.2.09**); flash host and check Status CPU under idle vs load

## Workflow follow-up

- Archive the change after review when implementation is complete.
- Sync delta specs into main specs if archive does not already.
