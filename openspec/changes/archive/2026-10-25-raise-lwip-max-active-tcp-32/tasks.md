# Tasks

## 1. Host PlatformIO LWIP / AsyncTCP config

- [x] 1.1 On `esp32-s3-host` in `platformio.ini`, set `custom_sdkconfig` so `CONFIG_LWIP_MAX_ACTIVE_TCP`, `CONFIG_LWIP_MAX_SOCKETS`, and `CONFIG_LWIP_MAX_LISTENING_TCP` are each **48**
- [x] 1.2 Add host `build_flags` `-DCONFIG_ASYNC_TCP_QUEUE_SIZE=128` (keep other AsyncTCP defaults unless a conflict appears)
- [x] 1.3 Do **not** apply these overrides to `esp32-c6-slave` unless a build error forces a shared `[env]` change (prefer host-only)

## 2. Verify and document

- [x] 2.1 Clean/rebuild `esp32-s3-host` and confirm generated/merged sdkconfig (or `sdkconfig.h`) shows active TCP / sockets / listening TCP ≥ 32
- [x] 2.2 Confirm `MQTT_BROKER_CLIENT_MAX` (24) remains ≤ the new LWIP active TCP ceiling; change only if apply finds a hard conflict
- [x] 2.3 Smoke: host still serves HTTP console and accepts an MQTT TCP connect after flash

## 3. Firmware version

- [x] 3.1 Apply bumped `FIRMWARE_VERSION` to `3.5.60` (propose was `3.5.59`; apply edited `platformio.ini` + Defines)
