#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include "esp32-hal-hosted.h"
#include <Network.h>

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println();
  Serial.println("=== P4 Hosted bring-up smoke ===");
  Serial.flush();

  esp_log_level_set("rpc_core", ESP_LOG_INFO);
  esp_log_level_set("rpc_rsp", ESP_LOG_INFO);
  esp_log_level_set("rpc_wrap", ESP_LOG_INFO);
  esp_log_level_set("H_API", ESP_LOG_INFO);
  esp_log_level_set("transport", ESP_LOG_INFO);
  esp_log_level_set("H_SDIO_DRV", ESP_LOG_INFO);

  esp_err_t nvsErr = nvs_flash_init();
  if (nvsErr == ESP_ERR_NVS_NO_FREE_PAGES || nvsErr == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    nvsErr = nvs_flash_init();
  }
  Serial.printf("nvs_flash_init -> 0x%x\n", (unsigned)nvsErr);
  Serial.flush();

  WiFi.persistent(false);
  WiFi.useStaticBuffers(true);

  Serial.println("1) hostedInitWiFi");
  Serial.flush();
  bool hi = hostedInitWiFi();
  Serial.printf("hostedInitWiFi=%d\n", (int)hi);
  Serial.flush();

  Serial.println("2) Network.begin");
  Serial.flush();
  bool nb = Network.begin();
  Serial.printf("Network.begin=%d\n", (int)nb);
  Serial.flush();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  cfg.nvs_enable = false;
  cfg.static_tx_buf_num = 0;
  cfg.dynamic_tx_buf_num = 32;
  cfg.tx_buf_type = 1;
  cfg.cache_tx_buf_num = 0;
  cfg.static_rx_buf_num = 10;
  cfg.dynamic_rx_buf_num = 32;
  Serial.printf(
    "3) esp_wifi_init sizeof=%u magic=0x%x tx_type=%d cache=%d stx=%d dtx=%d srx=%d drx=%d\n",
    (unsigned)sizeof(cfg), (unsigned)cfg.magic, cfg.tx_buf_type, cfg.cache_tx_buf_num,
    cfg.static_tx_buf_num, cfg.dynamic_tx_buf_num, cfg.static_rx_buf_num, cfg.dynamic_rx_buf_num
  );
  Serial.flush();
  esp_err_t err = esp_wifi_init(&cfg);
  Serial.printf("esp_wifi_init -> 0x%x %s\n", (unsigned)err, esp_err_to_name(err));
  Serial.flush();
  if (err != ESP_OK) {
    return;
  }

  err = esp_wifi_set_mode(WIFI_MODE_AP);
  Serial.printf("set_mode(AP) -> 0x%x\n", (unsigned)err);
  Serial.flush();
  bool ok = WiFi.softAP("p4-hosted-smoke", "smoke-test-1234", 6, 0, 4);
  Serial.printf("softAP=%d ip=%s\n", (int)ok, WiFi.softAPIP().toString().c_str());
  Serial.flush();
}

void loop() {
  delay(2000);
}
