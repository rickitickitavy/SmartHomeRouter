#!/usr/bin/env python3
"""Align Arduino Hosted Wi-Fi bring-up with hybrid-built esp_hosted / C6 2.12.x.

1) Inject wifi_task_stack_size into the Arduino-injected wifi_init_config_t so
   RPC magic matches the hybrid-built esp_hosted / C6 network_adapter layout.
2) Force Hosted TX buffer config in WiFiGeneric (dynamic TX, cache=0) so
   useStaticBuffers / PSRAM defaults cannot send STATIC or cache_tx_buf_num=4.
3) Expand sram_high in prebuilt esp32p4_es memory.ld when L2 cache is 128 KiB
   (stock ld hardcodes 256 KiB carve: len = 0x80000 - 0x40000).

Idempotent: safe to run on every esp32-p4-host build.
"""

from __future__ import annotations

import pathlib
import sys

Import("env")  # type: ignore  # noqa: F821 — PlatformIO SCons

MARKER = "wifi_task_stack_size;   /**< WIFI task stack size */"
HOSTED_BUF_MARKER = "Keep dynamic TX / no cache TX — matches C6 network_adapter defaults."


def _libs_pkg() -> pathlib.Path:
    return pathlib.Path(env.PioPlatform().get_package_dir("framework-arduinoespressif32-libs") or "")  # type: ignore


def _framework_pkg() -> pathlib.Path:
    return pathlib.Path(env.PioPlatform().get_package_dir("framework-arduinoespressif32") or "")  # type: ignore


def _candidate_headers() -> list[pathlib.Path]:
    pkg = _libs_pkg()
    out: list[pathlib.Path] = []
    if pkg.is_dir():
        out.append(
            pkg
            / "esp32p4_es"
            / "include"
            / "espressif__esp_wifi_remote"
            / "idf_v5.5"
            / "include"
            / "injected"
            / "esp_wifi.h"
        )
    return out


def _patch_header(path: pathlib.Path) -> bool:
    if not path.is_file():
        return False
    text = path.read_text(encoding="utf-8")
    if MARKER in text or "wifi_task_stack_size = WIFI_TASK_STACK_SIZE" in text:
        print(f"patch_p4_hosted_wifi_init_header: already patched {path}")
        return False

    old_struct = (
        "    int                    tx_hetb_queue_num;      /**< WiFi TX HE TB QUEUE number for STA HE TB PPDU transmission */\n"
        "    bool                   dump_hesigb_enable;     /**< enable dump sigb field */\n"
        "    int                    magic;                  /**< WiFi init magic number, it should be the last field */\n"
        "} wifi_init_config_t;"
    )
    new_struct = (
        "    int                    tx_hetb_queue_num;      /**< WiFi TX HE TB QUEUE number for STA HE TB PPDU transmission */\n"
        "    bool                   dump_hesigb_enable;     /**< enable dump sigb field */\n"
        "    int                    wifi_task_stack_size;   /**< WIFI task stack size */\n"
        "    int                    magic;                  /**< WiFi init magic number, it should be the last field */\n"
        "} wifi_init_config_t;"
    )
    if old_struct not in text:
        print(f"patch_p4_hosted_wifi_init_header: struct pattern missing in {path}", file=sys.stderr)
        return False
    text = text.replace(old_struct, new_struct, 1)

    if "WIFI_TASK_STACK_SIZE" not in text:
        insert_after = (
            "#if CONFIG_WIFI_RMT_PASSIVE_HIDDEN_AP_SUPPORT\n"
            "#define WIFI_ENABLE_PASSIVE_HIDDEN_AP (1<<9)\n"
            "#else\n"
            "#define WIFI_ENABLE_PASSIVE_HIDDEN_AP 0\n"
            "#endif\n"
        )
        stack_macros = (
            insert_after
            + "\n"
            + "#if CONFIG_WIFI_RMT_ENABLE_WPA3_OWE_STA\n"
            + "#define WIFI_ENABLE_WPA3_OWE_STA (1<<10)\n"
            + "#else\n"
            + "#define WIFI_ENABLE_WPA3_OWE_STA 0\n"
            + "#endif\n"
            + "\n"
            + "#if CONFIG_WIFI_RMT_ENABLE_WPA3_OWE_STA || CONFIG_WIFI_RMT_ENABLE_WPA3_SAE\n"
            + "#define WIFI_TASK_STACK_SIZE_BASE   6144\n"
            + "#else\n"
            + "#define WIFI_TASK_STACK_SIZE_BASE   3072\n"
            + "#endif\n"
            + "\n"
            + "#if !WIFI_NANO_FORMAT_ENABLED\n"
            + "#define WIFI_TASK_STACK_SIZE (WIFI_TASK_STACK_SIZE_BASE + 512)\n"
            + "#else\n"
            + "#define WIFI_TASK_STACK_SIZE WIFI_TASK_STACK_SIZE_BASE\n"
            + "#endif\n"
        )
        if insert_after not in text:
            print(f"patch_p4_hosted_wifi_init_header: macro anchor missing in {path}", file=sys.stderr)
            return False
        text = text.replace(insert_after, stack_macros, 1)

    old_default_tail = (
        "    .tx_hetb_queue_num = WIFI_TX_HETB_QUEUE_NUM, \\\n"
        "    .dump_hesigb_enable = WIFI_DUMP_HESIGB_ENABLED, \\\n"
        "    .magic = WIFI_INIT_CONFIG_MAGIC\\\n"
        "}"
    )
    new_default_tail = (
        "    .tx_hetb_queue_num = WIFI_TX_HETB_QUEUE_NUM, \\\n"
        "    .dump_hesigb_enable = WIFI_DUMP_HESIGB_ENABLED, \\\n"
        "    .wifi_task_stack_size = WIFI_TASK_STACK_SIZE, \\\n"
        "    .magic = WIFI_INIT_CONFIG_MAGIC\\\n"
        "}"
    )
    if old_default_tail not in text:
        print(f"patch_p4_hosted_wifi_init_header: DEFAULT macro tail missing in {path}", file=sys.stderr)
        return False
    text = text.replace(old_default_tail, new_default_tail, 1)

    text = text.replace(
        "                           WIFI_ENABLE_PASSIVE_HIDDEN_AP)\n",
        "                           WIFI_ENABLE_PASSIVE_HIDDEN_AP | \\\n"
        "                           WIFI_ENABLE_WPA3_OWE_STA)\n",
        1,
    )

    path.write_text(text, encoding="utf-8")
    print(f"patch_p4_hosted_wifi_init_header: patched {path}")
    return True


def _patch_wifi_generic(path: pathlib.Path) -> bool:
    if not path.is_file():
        print(f"patch_p4_hosted_wifi_init_header: WiFiGeneric missing {path}", file=sys.stderr)
        return False
    text = path.read_text(encoding="utf-8")
    if HOSTED_BUF_MARKER in text:
        print(f"patch_p4_hosted_wifi_init_header: WiFiGeneric already patched {path}")
        return False

    old = """bool wifiLowLevelInit(bool persistent) {
  if (!lowLevelInitDone) {
    lowLevelInitDone = true;
#if CONFIG_ESP_HOSTED_ENABLED
    if (!hostedInitWiFi()) {
      lowLevelInitDone = false;
      return lowLevelInitDone;
    }
#endif
    if (!Network.begin()) {
      lowLevelInitDone = false;
      return lowLevelInitDone;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

#if CONFIG_ESP_HOSTED_ENABLED
    // required for proper work when esp-hosted is used.
    cfg.nvs_enable = false;
    persistent = false;
#endif

    if (!WiFiGenericClass::useStaticBuffers()) {
      cfg.static_tx_buf_num = 0;
      cfg.dynamic_tx_buf_num = 32;
      cfg.tx_buf_type = 1;
      cfg.cache_tx_buf_num = 4;  // can't be zero!
      cfg.static_rx_buf_num = 4;
      cfg.dynamic_rx_buf_num = 32;
    }

    esp_err_t err = esp_wifi_init(&cfg);
    if (err) {
      log_e("esp_wifi_init 0x%x: %s", err, esp_err_to_name(err));
      lowLevelInitDone = false;
      return lowLevelInitDone;
    }
"""

    new = """bool wifiLowLevelInit(bool persistent) {
  if (!lowLevelInitDone) {
    lowLevelInitDone = true;
#if CONFIG_ESP_HOSTED_ENABLED
    if (!hostedInitWiFi()) {
      lowLevelInitDone = false;
      return lowLevelInitDone;
    }
#endif
    if (!Network.begin()) {
      lowLevelInitDone = false;
      return lowLevelInitDone;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

#if CONFIG_ESP_HOSTED_ENABLED
    // required for proper work when esp-hosted is used.
    cfg.nvs_enable = false;
    persistent = false;
    // Keep dynamic TX / no cache TX — matches C6 network_adapter defaults.
    cfg.static_tx_buf_num = 0;
    cfg.dynamic_tx_buf_num = 32;
    cfg.tx_buf_type = 1;
    cfg.cache_tx_buf_num = 0;
    cfg.static_rx_buf_num = 10;
    cfg.dynamic_rx_buf_num = 32;
#else
    if (!WiFiGenericClass::useStaticBuffers()) {
      cfg.static_tx_buf_num = 0;
      cfg.dynamic_tx_buf_num = 32;
      cfg.tx_buf_type = 1;
      cfg.cache_tx_buf_num = 4;  // can't be zero!
      cfg.static_rx_buf_num = 4;
      cfg.dynamic_rx_buf_num = 32;
    }
#endif

#if CONFIG_ESP_HOSTED_ENABLED
    log_i(
      "wifi_init magic=0x%x sizeof=%u stack=%d tx_type=%d cache=%d",
      (unsigned)cfg.magic, (unsigned)sizeof(cfg), cfg.wifi_task_stack_size, cfg.tx_buf_type,
      cfg.cache_tx_buf_num
    );
#endif
    esp_err_t err = esp_wifi_init(&cfg);
    if (err) {
      log_e("esp_wifi_init 0x%x: %s", err, esp_err_to_name(err));
      lowLevelInitDone = false;
      return lowLevelInitDone;
    }
#if CONFIG_ESP_HOSTED_ENABLED
    log_i("esp_wifi_init ok");
#endif
"""

    if old not in text:
        print(f"patch_p4_hosted_wifi_init_header: WiFiGeneric pattern missing in {path}", file=sys.stderr)
        return False
    path.write_text(text.replace(old, new, 1), encoding="utf-8")
    print(f"patch_p4_hosted_wifi_init_header: patched WiFiGeneric {path}")
    return True


def _patch_memory_ld_l2_128(path: pathlib.Path) -> bool:
    """Match linker sram_high to CONFIG_CACHE_L2_CACHE_SIZE=0x20000 (128 KiB)."""
    if not path.is_file():
        return False
    text = path.read_text(encoding="utf-8")
    old = "sram_high (RW) : org = 0x4FF40000, len = 0x80000 - 0x40000"
    new = "sram_high (RW) : org = 0x4FF40000, len = 0x80000 - 0x20000"
    if new in text:
        print(f"patch_p4_hosted_wifi_init_header: memory.ld already L2=128 {path}")
        return False
    if old not in text:
        print(f"patch_p4_hosted_wifi_init_header: memory.ld pattern missing in {path}", file=sys.stderr)
        return False
    path.write_text(text.replace(old, new, 1), encoding="utf-8")
    print(f"patch_p4_hosted_wifi_init_header: memory.ld L2 256->128 {path}")
    return True


def _patch_package_sdkconfig_l2_128(path: pathlib.Path) -> bool:
    """Force prebuilt package sdkconfig.h to 128 KiB L2 (belt-and-suspenders)."""
    if not path.is_file():
        return False
    text = path.read_text(encoding="utf-8")
    if "CONFIG_CACHE_L2_CACHE_SIZE 0x20000" in text and "CONFIG_CACHE_L2_CACHE_128KB 1" in text:
        print(f"patch_p4_hosted_wifi_init_header: package sdkconfig already L2=128 {path}")
        return False
    original = text
    text = text.replace("#define CONFIG_CACHE_L2_CACHE_256KB 1", "/* CONFIG_CACHE_L2_CACHE_256KB was 1 */")
    text = text.replace("#define CONFIG_CACHE_L2_CACHE_SIZE 0x40000", "#define CONFIG_CACHE_L2_CACHE_SIZE 0x20000")
    if "#define CONFIG_CACHE_L2_CACHE_128KB 1" not in text:
        text = text.replace(
            "#define CONFIG_CACHE_L2_CACHE_SIZE 0x20000",
            "#define CONFIG_CACHE_L2_CACHE_128KB 1\n#define CONFIG_CACHE_L2_CACHE_SIZE 0x20000",
            1,
        )
    if text == original:
        print(f"patch_p4_hosted_wifi_init_header: package sdkconfig L2 pattern missing in {path}", file=sys.stderr)
        return False
    path.write_text(text, encoding="utf-8")
    print(f"patch_p4_hosted_wifi_init_header: package sdkconfig L2 256->128 {path}")
    return True


patched_any = False
for header in _candidate_headers():
    patched_any = _patch_header(header) or patched_any

wifi_generic = _framework_pkg() / "libraries" / "WiFi" / "src" / "WiFiGeneric.cpp"
patched_any = _patch_wifi_generic(wifi_generic) or patched_any

libs_pkg = _libs_pkg()
memory_ld = libs_pkg / "esp32p4_es" / "ld" / "memory.ld"
patched_any = _patch_memory_ld_l2_128(memory_ld) or patched_any
for flash_mode in ("dio_qspi", "qio_qspi"):
    sdkconfig_h = libs_pkg / "esp32p4_es" / flash_mode / "include" / "sdkconfig.h"
    patched_any = _patch_package_sdkconfig_l2_128(sdkconfig_h) or patched_any
if not patched_any:
    pass
