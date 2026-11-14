#include <Arduino.h>
#include <WiFi.h>
#include "esp32-hal-hosted.h"

// Embedded by PlatformIO board_build.embed_files
extern const uint8_t esp32c6_hosted_bin_start[] asm("_binary_data_esp32c6_hosted_bin_start");
extern const uint8_t esp32c6_hosted_bin_end[] asm("_binary_data_esp32c6_hosted_bin_end");

static bool pushHostedImage() {
  const uint8_t *data = esp32c6_hosted_bin_start;
  const size_t total = (size_t)(esp32c6_hosted_bin_end - esp32c6_hosted_bin_start);
  Serial.printf("Hosted image size: %u bytes\n", (unsigned)total);
  if (total < 1024) {
    Serial.println("ERROR: embedded image missing");
    return false;
  }

  if (!hostedIsInitialized()) {
    Serial.println("ERROR: ESP-Hosted not initialized");
    return false;
  }

  Serial.println("Begin SDIO OTA to co-processor...");
  if (!hostedBeginUpdate()) {
    Serial.println("ERROR: hostedBeginUpdate failed (C6 may be unreachable on SDIO)");
    return false;
  }

  const size_t chunk = 2048;
  size_t sent = 0;
  while (sent < total) {
    size_t n = total - sent;
    if (n > chunk) {
      n = chunk;
    }
    if (!hostedWriteUpdate((uint8_t *)(data + sent), n)) {
      Serial.printf("\nERROR: write failed at %u/%u\n", (unsigned)sent, (unsigned)total);
      return false;
    }
    sent += n;
    if ((sent & 0xFFFF) == 0 || sent == total) {
      Serial.printf("\r%d%%", (int)((sent * 100) / total));
      Serial.flush();
    }
  }
  Serial.println();

  if (!hostedEndUpdate()) {
    Serial.println("ERROR: hostedEndUpdate failed");
    return false;
  }
  Serial.println("Activate...");
  if (!hostedActivateUpdate()) {
    Serial.println("ERROR: hostedActivateUpdate failed");
    return false;
  }
  Serial.println("SUCCESS: co-processor image written and activated");
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== P4 Hosted C6 local updater (2.12.11) ===");

  // Init Hosted transport (same path as product Wi-Fi bring-up).
  WiFi.mode(WIFI_STA);
  delay(200);

  if (!hostedIsInitialized()) {
    Serial.println("ERROR: Hosted still not initialized after WiFi.mode");
    return;
  }

  Serial.printf("Hosted update available flag: %d\n", (int)hostedHasUpdate());
  const bool ok = pushHostedImage();
  Serial.println(ok ? "Done. Reflash product firmware (esp32-p4-host)." : "Failed.");
  if (ok) {
    delay(1000);
    ESP.restart();
  }
}

void loop() {
  delay(1000);
}
