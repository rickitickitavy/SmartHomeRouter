# Tasks

## 1. Host CPU sampler and status API

- [ ] 1.1 Add host FreeRTOS idle-based sampler for CPU0/CPU1 (unavailable until first window); verify values move under load and stay in 0–100
- [ ] 1.2 Publish `hostCpu0Percent` / `hostCpu1Percent` (null when unavailable) in gateway status JSON; verify `/api/status` (or existing status GET) returns them

## 2. Slave CPU and SPI

- [ ] 2.1 Add slave FreeRTOS idle-based sampler (single percent); verify logs or unit path under load
- [ ] 2.2 Include slave CPU in status replies; host parse + cache for Status; verify host Status JSON `slaveCpuPercent` updates after a status poll and shows unavailable when unlinked
- [ ] 2.3 Confirm SPI OTA begin/chunk/end paths are untouched; verify a joined OTA smoke still negotiates status version as today

## 3. Status UI

- [ ] 3.1 Add Status **CPU** group (Host CPU0, Host CPU1, Slave CPU) after Free memory in `data/index.html` and `data/user.html`; verify both pages show matching values and em dash when null
- [ ] 3.2 Confirm Status column order includes CPU after Free memory on wide layout; verify on operator and simple-user Status

## 4. Integration

- [ ] 4.1 Flash host **4.2.59** + slave **4.2.07**; idle vs load check that host cores and slave percent track activity without debugger instrumentation beyond Status
