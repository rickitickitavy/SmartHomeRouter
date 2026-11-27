#include "HostServices.h"

#if defined(BOARD_ROLE_HOST)

#include "Defines.h"
#include "HostHttpServer.h"
#include "HostProductApi.h"
#include "HostSmtp.h"
#include "SpiProtocol.h"
#include "pins.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_random.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "mbedtls/gcm.h"
#include "mqtt_client.h"
#include "SettingsManager.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *kLogTag = "host_svc";

static constexpr uint16_t kDefaultLowMemKb = 16;
static constexpr int kSmtpQueueMax = EMAIL_SEND_QUEUE_MAX;
static constexpr int kSpiOutQueue = 32;
static constexpr uint32_t kSpiReplyTimeoutMs = 3000;
static constexpr int kInterconnectPort = INTERCONNECT_PORT_DEFAULT;
static constexpr size_t kIcNonceLen = 12;
static constexpr size_t kIcTagLen = 16;
static constexpr size_t kIcHeaderLen = 1 + 1 + 2 + kIcNonceLen;
static constexpr uint32_t kPairWaitMs = INTERCONNECT_PAIR_WAIT_SEC * 1000UL;

static uint16_t s_lowMemThresholdKb = kDefaultLowMemKb;
static bool s_failsafeLatched = false;
static bool s_psramAllocFailed = false;

static esp_mqtt_client_handle_t s_mqttClient = nullptr;
static bool s_mqttClientEnabled = false;
static bool s_mqttBrokerEnabled = false;
static bool s_mqttClientConnected = false;
static int s_mqttBrokerListenFd = -1;

struct SmtpJob {
    char receivers[192];
    char subject[96];
    char body[256];
    uint8_t jpegCount;
    uint8_t *jpegData[AUTOMATION_EMAIL_CAMERA_ATTACHES_MAX];
    size_t jpegSize[AUTOMATION_EMAIL_CAMERA_ATTACHES_MAX];
};
static QueueHandle_t s_smtpQueue = nullptr;
static TaskHandle_t s_smtpTask = nullptr;
static SemaphoreHandle_t s_smtpMutex = nullptr;
static portMUX_TYPE s_smtpWorkerMux = portMUX_INITIALIZER_UNLOCKED;
static uint8_t s_smtpDepth = 0;
static bool s_smtpConfigured = false;

static spi_device_handle_t s_spiDevice = nullptr;
static TaskHandle_t s_spiTask = nullptr;
static bool s_spiLinkHealthy = false;
static bool s_spiOutboundIdle = true;
static bool s_spiOtaActive = false;
static uint8_t s_spiOtaSeq = 1;

struct SpiOutJob {
    bool used;
    SpiFrame frame;
};
static SpiOutJob s_spiOut[kSpiOutQueue];
static portMUX_TYPE s_spiMux = portMUX_INITIALIZER_UNLOCKED;

static TaskHandle_t s_interconnectTask = nullptr;
static bool s_interconnectListening = false;
static bool s_pairingWaitActive = false;
static uint32_t s_pairingWaitDeadlineMs = 0;
static uint64_t s_icNonceCounter = 1;

static uint32_t nowMs(void) {
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

void *hostPsramAlloc(size_t bytes) {
    s_psramAllocFailed = false;
    void *pointer = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (pointer == nullptr) {
        s_psramAllocFailed = true;
    }
    return pointer;
}

void hostPsramFree(void *pointer) {
    free(pointer);
}

bool hostPsramAllocFailedRecently(void) {
    return s_psramAllocFailed;
}

void hostFailsafeSetThresholdKb(uint16_t thresholdKb) {
    if (thresholdKb < 10) {
        thresholdKb = 10;
    }
    if (thresholdKb > 32) {
        thresholdKb = 32;
    }
    s_lowMemThresholdKb = thresholdKb;
}

void hostFailsafeCheck(void) {
    if (s_failsafeLatched) {
        return;
    }
    const size_t freeInternal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const size_t thresholdBytes = (size_t)s_lowMemThresholdKb * 1024u;
    if (freeInternal > thresholdBytes) {
        return;
    }
    s_failsafeLatched = true;
    ESP_LOGE(
        kLogTag,
        "Low-memory fail-safe: freeInternal=%u thresholdKb=%u",
        (unsigned)freeInternal,
        (unsigned)s_lowMemThresholdKb
    );
    hostHttpEnd();
    FILE *marker = fopen("/history_failsafe_marker", "w");
    if (marker != nullptr) {
        fputs("snapshot-attempted\n", marker);
        fclose(marker);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();
}

static void mqttEventHandler(void * /*handlerArgs*/, esp_event_base_t /*base*/, int32_t eventId, void *eventData) {
    esp_mqtt_event_handle_t event = static_cast<esp_mqtt_event_handle_t>(eventData);
    switch (eventId) {
    case MQTT_EVENT_CONNECTED:
        s_mqttClientConnected = true;
        ESP_LOGI(kLogTag, "MQTT client connected");
        break;
    case MQTT_EVENT_DISCONNECTED:
        s_mqttClientConnected = false;
        ESP_LOGW(kLogTag, "MQTT client disconnected");
        break;
    default:
        break;
    }
    (void)event;
}

static void startMqttFromFile(void) {
    FILE *file = fopen("/mqtt.txt", "r");
    char uri[128] = "mqtt://127.0.0.1:1883";
    int enableClient = 0;
    int enableBroker = 0;
    if (file != nullptr) {
        char line[128];
        if (fgets(line, sizeof(line), file) != nullptr) {
            line[strcspn(line, "\r\n")] = '\0';
            if (line[0] != '\0') {
                strncpy(uri, line, sizeof(uri) - 1);
            }
        }
        if (fgets(line, sizeof(line), file) != nullptr) {
            enableClient = atoi(line);
        }
        if (fgets(line, sizeof(line), file) != nullptr) {
            enableBroker = atoi(line);
        }
        fclose(file);
    }
    s_mqttClientEnabled = enableClient != 0;
    s_mqttBrokerEnabled = enableBroker != 0;

    if (s_mqttBrokerEnabled) {
        s_mqttBrokerListenFd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (s_mqttBrokerListenFd >= 0) {
            int yes = 1;
            setsockopt(s_mqttBrokerListenFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
            sockaddr_in addr = {};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(1883);
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
            if (bind(s_mqttBrokerListenFd, (sockaddr *)&addr, sizeof(addr)) == 0
                && listen(s_mqttBrokerListenFd, 4) == 0) {
                fcntl(s_mqttBrokerListenFd, F_SETFL, O_NONBLOCK);
                ESP_LOGI(kLogTag, "Local MQTT broker listen socket on :1883 (accept-only scaffold)");
            } else {
                close(s_mqttBrokerListenFd);
                s_mqttBrokerListenFd = -1;
                s_mqttBrokerEnabled = false;
            }
        }
    }

    if (!s_mqttClientEnabled) {
        return;
    }
    esp_mqtt_client_config_t config = {};
    config.broker.address.uri = uri;
    config.network.disable_auto_reconnect = false;
    config.session.keepalive = 30;
    s_mqttClient = esp_mqtt_client_init(&config);
    if (s_mqttClient == nullptr) {
        ESP_LOGE(kLogTag, "MQTT client init failed");
        s_mqttClientEnabled = false;
        return;
    }
    esp_mqtt_client_register_event(s_mqttClient, MQTT_EVENT_ANY, mqttEventHandler, nullptr);
    esp_mqtt_client_start(s_mqttClient);
    ESP_LOGI(kLogTag, "MQTT client started uri=%s", uri);
}

bool hostMqttClientEnabled(void) {
    return s_mqttClientEnabled;
}

bool hostMqttBrokerEnabled(void) {
    return s_mqttBrokerEnabled;
}

bool hostMqttClientConnected(void) {
    return s_mqttClientConnected;
}

static bool smokeOneStore(const char *path, const char *payload) {
    FILE *out = fopen(path, "w");
    if (out == nullptr) {
        ESP_LOGW(kLogTag, "store smoke write failed: %s", path);
        return false;
    }
    fputs(payload, out);
    fclose(out);
    FILE *in = fopen(path, "r");
    if (in == nullptr) {
        return false;
    }
    char buffer[128] = {};
    if (fgets(buffer, sizeof(buffer), in) == nullptr) {
        fclose(in);
        return false;
    }
    fclose(in);
    return strncmp(buffer, payload, strlen(payload)) == 0;
}

bool hostStoresSmoke(void) {
    // Round-trip store paths used by product export/restore (Wi-Fi excluded).
    const bool ok =
        smokeOneStore("/consoles.json", "{\"consoles\":[]}")
        && smokeOneStore("/automations.json", "{\"automations\":[]}")
        && smokeOneStore("/scenes.json", "{\"scenes\":[]}")
        && smokeOneStore("/cameras.json", "{\"cameras\":[]}")
        && smokeOneStore("/devices.json", "{\"devices\":[]}")
        && smokeOneStore("/interconnect.json", "{\"role\":\"main\",\"peers\":[]}");
    ESP_LOGI(kLogTag, "Store path smoke %s", ok ? "ok" : "FAILED");
    return ok;
}

static void freeSmtpJobJpegs(SmtpJob *job) {
    if (job == nullptr) {
        return;
    }
    for (int i = 0; i < job->jpegCount; ++i) {
        if (job->jpegData[i] != nullptr) {
            hostPsramFree(job->jpegData[i]);
            job->jpegData[i] = nullptr;
        }
    }
    job->jpegCount = 0;
}

static void smtpWorkerTask(void * /*arg*/) {
    while (true) {
        SmtpJob job = {};
        if (xQueueReceive(s_smtpQueue, &job, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (xSemaphoreTake(s_smtpMutex, portMAX_DELAY) == pdTRUE) {
            const EmailSettings *email = hostProductEmailSettings();
            if (email != nullptr) {
                hostSmtpSendConfigured(
                    *email,
                    job.receivers,
                    job.subject,
                    job.body,
                    job.jpegData,
                    job.jpegSize,
                    job.jpegCount
                );
            } else {
                ESP_LOGW(kLogTag, "SMTP worker: email settings unavailable");
            }
            freeSmtpJobJpegs(&job);
            if (s_smtpDepth > 0) {
                s_smtpDepth--;
            }
            xSemaphoreGive(s_smtpMutex);
        } else {
            freeSmtpJobJpegs(&job);
        }
    }
}

static bool ensureSmtpWorker(void) {
    if (s_smtpMutex == nullptr) {
        s_smtpMutex = xSemaphoreCreateMutex();
    }
    if (s_smtpQueue == nullptr) {
        s_smtpQueue = xQueueCreate(kSmtpQueueMax, sizeof(SmtpJob));
    }
    portENTER_CRITICAL(&s_smtpWorkerMux);
    if (s_smtpTask != nullptr) {
        portEXIT_CRITICAL(&s_smtpWorkerMux);
        return true;
    }
    portEXIT_CRITICAL(&s_smtpWorkerMux);
    if (s_smtpQueue == nullptr || s_smtpMutex == nullptr) {
        return false;
    }
    // Lazy CPU1 worker, priority below hostSvc(5); matches former emailSmtp@CPU1.
    TaskHandle_t created = nullptr;
    const BaseType_t ok = xTaskCreatePinnedToCore(
        smtpWorkerTask,
        "smtpWrk",
        8192,
        nullptr,
        0,
        &created,
        1
    );
    if (ok != pdPASS || created == nullptr) {
        return false;
    }
    portENTER_CRITICAL(&s_smtpWorkerMux);
    if (s_smtpTask != nullptr) {
        portEXIT_CRITICAL(&s_smtpWorkerMux);
        vTaskDelete(created);
        return true;
    }
    s_smtpTask = created;
    portEXIT_CRITICAL(&s_smtpWorkerMux);
    return true;
}

static void loadSmtpConfig(void) {
    const EmailSettings *email = hostProductEmailSettings();
    if (email != nullptr) {
        s_smtpConfigured = SettingsManager::isEmailConfigured(*email);
        if (s_smtpConfigured) {
            return;
        }
    }
    // Optional bring-up override: /email.txt host line.
    FILE *file = fopen("/email.txt", "r");
    if (file == nullptr) {
        s_smtpConfigured = false;
        return;
    }
    char host[96] = {};
    if (fgets(host, sizeof(host), file) != nullptr) {
        host[strcspn(host, "\r\n")] = '\0';
        s_smtpConfigured = host[0] != '\0';
    }
    fclose(file);
}

bool hostSmtpIsConfigured(void) {
    loadSmtpConfig();
    return s_smtpConfigured;
}

bool hostSmtpEnqueueTo(
    const char *receivers,
    const char *subject,
    const char *body,
    const HostSmtpJpegPart *jpegParts,
    int jpegCount
) {
    if (!hostSmtpIsConfigured()) {
        ESP_LOGW(kLogTag, "SMTP not configured; reject enqueue");
        return false;
    }
    if (jpegCount < 0 || jpegCount > AUTOMATION_EMAIL_CAMERA_ATTACHES_MAX) {
        return false;
    }
    if (!ensureSmtpWorker() || s_smtpQueue == nullptr) {
        return false;
    }
    if (s_smtpDepth >= kSmtpQueueMax) {
        ESP_LOGW(kLogTag, "SMTP queue full; reject");
        return false;
    }
    SmtpJob job = {};
    strncpy(job.receivers, receivers != nullptr ? receivers : "", sizeof(job.receivers) - 1);
    strncpy(job.subject, subject != nullptr ? subject : "", sizeof(job.subject) - 1);
    strncpy(job.body, body != nullptr ? body : "", sizeof(job.body) - 1);
    job.jpegCount = (uint8_t)jpegCount;
    for (int i = 0; i < jpegCount; ++i) {
        job.jpegData[i] = jpegParts[i].data;
        job.jpegSize[i] = jpegParts[i].size;
    }
    if (xQueueSend(s_smtpQueue, &job, 0) != pdTRUE) {
        return false;
    }
    s_smtpDepth++;
    return true;
}

bool hostSmtpEnqueue(
    const char *subject,
    const char *body,
    const HostSmtpJpegPart *jpegParts,
    int jpegCount
) {
    return hostSmtpEnqueueTo(nullptr, subject, body, jpegParts, jpegCount);
}

bool hostSmtpRequestAccountCheck(void) {
    const EmailSettings *email = hostProductEmailSettings();
    if (email == nullptr || !SettingsManager::isEmailConfigured(*email)) {
        return false;
    }
    if (!ensureSmtpWorker()) {
        return false;
    }
    char error[128] = {};
    // Run check under SMTP mutex from caller task would block HTTP; enqueue empty job? Use direct
    // mutex take with short timeout from worker-like path: check synchronously only when idle.
    if (s_smtpMutex == nullptr) {
        return false;
    }
    if (xSemaphoreTake(s_smtpMutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return false;
    }
    const bool ok = hostSmtpAccountCheck(*email, error, sizeof(error));
    xSemaphoreGive(s_smtpMutex);
    return ok;
}

uint8_t hostSmtpQueueDepth(void) {
    return s_smtpDepth;
}

bool hostOtaApplyHostImage(const char *path) {
    if (path == nullptr || path[0] == '\0') {
        return false;
    }
    FILE *file = fopen(path, "rb");
    if (file == nullptr) {
        ESP_LOGE(kLogTag, "Host OTA open failed: %s", path);
        return false;
    }
    const esp_partition_t *updatePartition = esp_ota_get_next_update_partition(nullptr);
    if (updatePartition == nullptr) {
        fclose(file);
        ESP_LOGE(kLogTag, "No OTA update partition");
        return false;
    }
    esp_ota_handle_t handle = 0;
    esp_err_t err = esp_ota_begin(updatePartition, OTA_WITH_SEQUENTIAL_WRITES, &handle);
    if (err != ESP_OK) {
        fclose(file);
        ESP_LOGE(kLogTag, "esp_ota_begin: %s", esp_err_to_name(err));
        return false;
    }
    char *chunk = static_cast<char *>(hostPsramAlloc(4096));
    if (chunk == nullptr) {
        chunk = static_cast<char *>(malloc(4096));
    }
    if (chunk == nullptr) {
        esp_ota_abort(handle);
        fclose(file);
        return false;
    }
    size_t readBytes = 0;
    while ((readBytes = fread(chunk, 1, 4096, file)) > 0) {
        err = esp_ota_write(handle, chunk, readBytes);
        if (err != ESP_OK) {
            ESP_LOGE(kLogTag, "esp_ota_write: %s", esp_err_to_name(err));
            hostPsramFree(chunk);
            esp_ota_abort(handle);
            fclose(file);
            return false;
        }
    }
    hostPsramFree(chunk);
    fclose(file);
    err = esp_ota_end(handle);
    if (err != ESP_OK) {
        ESP_LOGE(kLogTag, "esp_ota_end: %s", esp_err_to_name(err));
        return false;
    }
    err = esp_ota_set_boot_partition(updatePartition);
    if (err != ESP_OK) {
        ESP_LOGE(kLogTag, "esp_ota_set_boot_partition: %s", esp_err_to_name(err));
        return false;
    }
    ESP_LOGI(kLogTag, "Host OTA image staged from %s; reboot to apply", path);
    return true;
}

bool hostOtaHostedC6Needed(const char *packageVersion, const char *runningVersion, bool runningReadable) {
    if (packageVersion == nullptr || packageVersion[0] == '\0') {
        return false;
    }
    if (!runningReadable) {
        return true;
    }
    if (runningVersion == nullptr) {
        return true;
    }
    return strcmp(packageVersion, runningVersion) != 0;
}

static bool spiTransferFrame(const uint8_t *tx, size_t length, uint8_t *rx) {
    if (s_spiDevice == nullptr || tx == nullptr || length == 0) {
        return false;
    }
    spi_transaction_t transaction = {};
    transaction.length = length * 8;
    transaction.tx_buffer = tx;
    transaction.rx_buffer = rx;
    gpio_set_level(static_cast<gpio_num_t>(PIN_SPI_CS), 0);
    const esp_err_t err = spi_device_transmit(s_spiDevice, &transaction);
    gpio_set_level(static_cast<gpio_num_t>(PIN_SPI_CS), 1);
    return err == ESP_OK;
}

static bool spiEnqueueLocked(uint8_t cmd, const uint8_t *payload, uint16_t length) {
    for (int i = 0; i < kSpiOutQueue; ++i) {
        if (!s_spiOut[i].used) {
            s_spiOut[i].used = true;
            s_spiOut[i].frame.cmd = cmd;
            s_spiOut[i].frame.seq = s_spiOtaSeq++;
            if (s_spiOtaSeq == 0) {
                s_spiOtaSeq = 1;
            }
            s_spiOut[i].frame.length = length;
            if (payload != nullptr && length > 0) {
                memcpy(s_spiOut[i].frame.payload, payload, length);
            }
            s_spiOutboundIdle = false;
            return true;
        }
    }
    return false;
}

static bool spiHasPendingOtaLocked(void) {
    for (int i = 0; i < kSpiOutQueue; ++i) {
        if (s_spiOut[i].used && s_spiOut[i].frame.cmd == SpiCmdFirmwareOta) {
            return true;
        }
    }
    return false;
}

static void spiTask(void * /*arg*/) {
    uint8_t txBuf[SPI_MAX_FRAME];
    uint8_t rxBuf[SPI_MAX_FRAME];
    while (true) {
        SpiFrame outbound = {};
        bool haveOutbound = false;
        portENTER_CRITICAL(&s_spiMux);
        for (int i = 0; i < kSpiOutQueue; ++i) {
            if (s_spiOut[i].used) {
                outbound = s_spiOut[i].frame;
                s_spiOut[i].used = false;
                haveOutbound = true;
                break;
            }
        }
        s_spiOutboundIdle = true;
        for (int i = 0; i < kSpiOutQueue; ++i) {
            if (s_spiOut[i].used) {
                s_spiOutboundIdle = false;
                break;
            }
        }
        portEXIT_CRITICAL(&s_spiMux);

        if (haveOutbound) {
            const size_t encoded = spiEncodeFrame(outbound, txBuf, sizeof(txBuf));
            if (encoded > 0) {
                memset(rxBuf, 0, sizeof(rxBuf));
                if (spiTransferFrame(txBuf, encoded, rxBuf)) {
                    SpiFrame reply = {};
                    if (spiDecodeFrame(rxBuf, encoded, reply)) {
                        s_spiLinkHealthy = true;
                        if (reply.cmd == SpiCmdFirmwareOta) {
                            ESP_LOGI(kLogTag, "SPI OTA reply seq=%u len=%u", reply.seq, reply.length);
                        }
                    }
                } else if (outbound.cmd == SpiCmdFirmwareOta) {
                    ESP_LOGW(kLogTag, "SPI OTA transfer failed; will retry via queue");
                }
            }
        } else if (gpio_get_level(static_cast<gpio_num_t>(PIN_SPI_IRQ)) != 0) {
            memset(txBuf, 0, sizeof(txBuf));
            memset(rxBuf, 0, sizeof(rxBuf));
            SpiFrame readEvent = {};
            readEvent.cmd = SpiCmdReadEvent;
            readEvent.seq = 1;
            readEvent.length = 0;
            const size_t encoded = spiEncodeFrame(readEvent, txBuf, sizeof(txBuf));
            if (encoded > 0 && spiTransferFrame(txBuf, encoded, rxBuf)) {
                SpiFrame eventFrame = {};
                if (spiDecodeFrame(rxBuf, encoded, eventFrame)) {
                    s_spiLinkHealthy = true;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
        (void)kSpiReplyTimeoutMs;
    }
}

bool hostSpiTryEnqueue(uint8_t cmd, const uint8_t *payload, uint16_t length) {
    if (length > SPI_MAX_PAYLOAD) {
        return false;
    }
    portENTER_CRITICAL(&s_spiMux);
    if (s_spiOtaActive && cmd != SpiCmdFirmwareOta) {
        portEXIT_CRITICAL(&s_spiMux);
        return false;
    }
    if (cmd == SpiCmdFirmwareOta && spiHasPendingOtaLocked()) {
        // Exclusive in-flight: one FirmwareOta frame at a time.
        portEXIT_CRITICAL(&s_spiMux);
        return false;
    }
    const bool ok = spiEnqueueLocked(cmd, payload, length);
    portEXIT_CRITICAL(&s_spiMux);
    return ok;
}

bool hostSpiIsLinkHealthy(void) {
    return s_spiLinkHealthy;
}

bool hostSpiIsOutboundIdle(void) {
    return s_spiOutboundIdle;
}

bool hostSpiOtaIsActive(void) {
    return s_spiOtaActive;
}

bool hostSpiOtaBegin(uint32_t imageSize) {
    uint8_t sizeBytes[4] = {
        (uint8_t)(imageSize & 0xFF),
        (uint8_t)((imageSize >> 8) & 0xFF),
        (uint8_t)((imageSize >> 16) & 0xFF),
        (uint8_t)((imageSize >> 24) & 0xFF)
    };
    uint8_t payload[SPI_OTA_BEGIN_LEN];
    const uint16_t packed = spiPackFirmwareOta(payload, sizeof(payload), SPI_OTA_BEGIN, sizeBytes, 4);
    if (packed == 0) {
        return false;
    }
    portENTER_CRITICAL(&s_spiMux);
    if (s_spiOtaActive || spiHasPendingOtaLocked()) {
        portEXIT_CRITICAL(&s_spiMux);
        return false;
    }
    s_spiOtaActive = true;
    const bool ok = spiEnqueueLocked(SpiCmdFirmwareOta, payload, packed);
    if (!ok) {
        s_spiOtaActive = false;
    }
    portEXIT_CRITICAL(&s_spiMux);
    return ok;
}

bool hostSpiOtaChunk(const uint8_t *data, uint16_t length) {
    if (!s_spiOtaActive || data == nullptr || length == 0) {
        return false;
    }
    uint8_t payload[SPI_MAX_PAYLOAD];
    const uint16_t packed = spiPackFirmwareOta(payload, sizeof(payload), 0, data, length);
    if (packed == 0) {
        return false;
    }
    return hostSpiTryEnqueue(SpiCmdFirmwareOta, payload, packed);
}

bool hostSpiOtaEnd(bool emptyLastFrame) {
    if (!s_spiOtaActive) {
        return false;
    }
    uint8_t payload[SPI_MAX_PAYLOAD];
    uint16_t packed = 0;
    if (emptyLastFrame) {
        // Empty-last-frame retry rule: END flag with zero data bytes.
        packed = spiPackFirmwareOta(payload, sizeof(payload), SPI_OTA_END, nullptr, 0);
    } else {
        packed = spiPackFirmwareOta(payload, sizeof(payload), SPI_OTA_END, nullptr, 0);
    }
    if (packed == 0) {
        return false;
    }
    const bool ok = hostSpiTryEnqueue(SpiCmdFirmwareOta, payload, packed);
    if (ok) {
        portENTER_CRITICAL(&s_spiMux);
        s_spiOtaActive = false;
        portEXIT_CRITICAL(&s_spiMux);
    }
    return ok;
}

static void startSpiMaster(void) {
    gpio_config_t csConfig = {};
    csConfig.pin_bit_mask = (1ULL << PIN_SPI_CS);
    csConfig.mode = GPIO_MODE_OUTPUT;
    csConfig.pull_up_en = GPIO_PULLUP_DISABLE;
    csConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
    csConfig.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&csConfig);
    gpio_set_level(static_cast<gpio_num_t>(PIN_SPI_CS), 1);

    gpio_config_t irqConfig = {};
    irqConfig.pin_bit_mask = (1ULL << PIN_SPI_IRQ);
    irqConfig.mode = GPIO_MODE_INPUT;
    irqConfig.pull_up_en = GPIO_PULLUP_DISABLE;
    irqConfig.pull_down_en = GPIO_PULLDOWN_ENABLE;
    irqConfig.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&irqConfig);

    spi_bus_config_t busConfig = {};
    busConfig.mosi_io_num = PIN_SPI_MOSI;
    busConfig.miso_io_num = PIN_SPI_MISO;
    busConfig.sclk_io_num = PIN_SPI_SCK;
    busConfig.quadwp_io_num = -1;
    busConfig.quadhd_io_num = -1;
    busConfig.max_transfer_sz = SPI_MAX_FRAME;

    esp_err_t err = spi_bus_initialize(SPI2_HOST, &busConfig, SPI_DMA_CH_AUTO);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(kLogTag, "SPI bus init failed: %s", esp_err_to_name(err));
        return;
    }

    spi_device_interface_config_t deviceConfig = {};
    deviceConfig.clock_speed_hz = DEFAULT_SPI_SPEED_HZ;
    deviceConfig.mode = 0;
    deviceConfig.spics_io_num = -1;
    deviceConfig.queue_size = 4;
    err = spi_bus_add_device(SPI2_HOST, &deviceConfig, &s_spiDevice);
    if (err != ESP_OK) {
        ESP_LOGE(kLogTag, "SPI add device failed: %s", esp_err_to_name(err));
        return;
    }

    memset(s_spiOut, 0, sizeof(s_spiOut));
    xTaskCreate(spiTask, "hostSpi", 6144, nullptr, 6, &s_spiTask);

    uint8_t settingsPayload[2] = {15, 0};
    hostSpiTryEnqueue(SpiCmdSetSettings, settingsPayload, sizeof(settingsPayload));
    hostSpiTryEnqueue(SpiCmdPing, nullptr, 0);
    ESP_LOGI(kLogTag, "SPI master task started");
}

static void u16Be(uint8_t *out, uint16_t value) {
    out[0] = (uint8_t)((value >> 8) & 0xFF);
    out[1] = (uint8_t)(value & 0xFF);
}

static uint16_t u16FromBe(const uint8_t *in) {
    return (uint16_t)(((uint16_t)in[0] << 8) | in[1]);
}

bool hostInterconnectAeadSeal(
    const uint8_t secret[INTERCONNECT_SECRET_LEN],
    const uint8_t peerMac[6],
    uint8_t type,
    const uint8_t *payload,
    uint16_t payloadLen,
    uint8_t *outFrame,
    size_t outMax,
    size_t *outLen
) {
    if (secret == nullptr || peerMac == nullptr || outFrame == nullptr || outLen == nullptr) {
        return false;
    }
    if (payloadLen > INTERCONNECT_AEAD_MAX_PAYLOAD) {
        return false;
    }
    if (payloadLen > 0 && payload == nullptr) {
        return false;
    }
    const size_t need = kIcHeaderLen + payloadLen + kIcTagLen;
    if (outMax < need) {
        return false;
    }
    uint8_t nonce[kIcNonceLen];
    uint64_t counter = s_icNonceCounter++;
    memset(nonce, 0, sizeof(nonce));
    memcpy(nonce, &counter, sizeof(counter));
    esp_fill_random(nonce + 8, 4);

    outFrame[0] = INTERCONNECT_PROTOCOL_VERSION;
    outFrame[1] = type;
    u16Be(outFrame + 2, payloadLen);
    memcpy(outFrame + 4, nonce, kIcNonceLen);

    uint8_t aad[6 + 4];
    memcpy(aad, peerMac, 6);
    memcpy(aad + 6, outFrame, 4);

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    int rc = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, secret, INTERCONNECT_SECRET_LEN * 8);
    if (rc != 0) {
        mbedtls_gcm_free(&gcm);
        return false;
    }
    uint8_t *cipher = outFrame + kIcHeaderLen;
    uint8_t *tag = cipher + payloadLen;
    rc = mbedtls_gcm_crypt_and_tag(
        &gcm,
        MBEDTLS_GCM_ENCRYPT,
        payloadLen,
        nonce,
        kIcNonceLen,
        aad,
        sizeof(aad),
        payload == nullptr ? (const uint8_t *)"" : payload,
        cipher,
        kIcTagLen,
        tag
    );
    mbedtls_gcm_free(&gcm);
    if (rc != 0) {
        return false;
    }
    *outLen = need;
    return true;
}

bool hostInterconnectAeadOpen(
    const uint8_t secret[INTERCONNECT_SECRET_LEN],
    const uint8_t peerMac[6],
    const uint8_t *frame,
    size_t frameLen,
    uint8_t *typeOut,
    uint8_t *payloadOut,
    uint16_t payloadMax,
    uint16_t *payloadLenOut
) {
    if (secret == nullptr || peerMac == nullptr || frame == nullptr || typeOut == nullptr
        || payloadLenOut == nullptr) {
        return false;
    }
    if (frameLen < kIcHeaderLen + kIcTagLen) {
        return false;
    }
    if (frame[0] != INTERCONNECT_PROTOCOL_VERSION) {
        return false;
    }
    const uint16_t payloadLen = u16FromBe(frame + 2);
    if (payloadLen > INTERCONNECT_AEAD_MAX_PAYLOAD || payloadLen > payloadMax) {
        return false;
    }
    if (frameLen < kIcHeaderLen + payloadLen + kIcTagLen) {
        return false;
    }
    const uint8_t *nonce = frame + 4;
    const uint8_t *cipher = frame + kIcHeaderLen;
    const uint8_t *tag = cipher + payloadLen;
    uint8_t aad[6 + 4];
    memcpy(aad, peerMac, 6);
    memcpy(aad + 6, frame, 4);

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    int rc = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, secret, INTERCONNECT_SECRET_LEN * 8);
    if (rc != 0) {
        mbedtls_gcm_free(&gcm);
        return false;
    }
    uint8_t plain[INTERCONNECT_AEAD_MAX_PAYLOAD];
    rc = mbedtls_gcm_auth_decrypt(
        &gcm,
        payloadLen,
        nonce,
        kIcNonceLen,
        aad,
        sizeof(aad),
        tag,
        kIcTagLen,
        cipher,
        plain
    );
    mbedtls_gcm_free(&gcm);
    if (rc != 0) {
        return false;
    }
    *typeOut = frame[1];
    *payloadLenOut = payloadLen;
    if (payloadLen > 0 && payloadOut != nullptr) {
        memcpy(payloadOut, plain, payloadLen);
    }
    return true;
}

bool hostInterconnectPairingWaitActive(void) {
    return s_pairingWaitActive;
}

bool hostInterconnectStartPairingWait(void) {
    s_pairingWaitActive = true;
    s_pairingWaitDeadlineMs = nowMs() + kPairWaitMs;
    ESP_LOGI(kLogTag, "Interconnect pairing wait started (%u s)", (unsigned)INTERCONNECT_PAIR_WAIT_SEC);
    return true;
}

void hostInterconnectCancelPairingWait(void) {
    s_pairingWaitActive = false;
    s_pairingWaitDeadlineMs = 0;
}

static bool writeAll(int fd, const uint8_t *data, size_t length) {
    size_t sent = 0;
    while (sent < length) {
        const int n = send(fd, data + sent, length - sent, 0);
        if (n <= 0) {
            return false;
        }
        sent += (size_t)n;
    }
    return true;
}

static bool readExact(int fd, uint8_t *data, size_t length, uint32_t timeoutMs) {
    size_t got = 0;
    const uint32_t start = nowMs();
    while (got < length) {
        const int n = recv(fd, data + got, length - got, 0);
        if (n > 0) {
            got += (size_t)n;
            continue;
        }
        if ((uint32_t)(nowMs() - start) >= timeoutMs) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return true;
}

static void interconnectTask(void * /*arg*/) {
    const int listenFd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listenFd < 0) {
        vTaskDelete(nullptr);
        return;
    }
    int yes = 1;
    setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kInterconnectPort);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(listenFd, (sockaddr *)&addr, sizeof(addr)) != 0 || listen(listenFd, 4) != 0) {
        close(listenFd);
        vTaskDelete(nullptr);
        return;
    }
    s_interconnectListening = true;
    ESP_LOGI(kLogTag, "Interconnect listening on %d", kInterconnectPort);

    // Self-test AEAD seal/open round-trip once at boot.
    uint8_t secret[INTERCONNECT_SECRET_LEN];
    memset(secret, 0x5A, sizeof(secret));
    uint8_t mac[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
    const char *probe = "ping";
    uint8_t sealed[kIcHeaderLen + INTERCONNECT_AEAD_MAX_PAYLOAD + kIcTagLen];
    size_t sealedLen = 0;
    if (hostInterconnectAeadSeal(
            secret, mac, 3, (const uint8_t *)probe, (uint16_t)strlen(probe), sealed, sizeof(sealed), &sealedLen
        )) {
        uint8_t type = 0;
        uint8_t plain[32];
        uint16_t plainLen = 0;
        if (hostInterconnectAeadOpen(secret, mac, sealed, sealedLen, &type, plain, sizeof(plain), &plainLen)
            && type == 3 && plainLen == strlen(probe)
            && memcmp(plain, probe, plainLen) == 0) {
            ESP_LOGI(kLogTag, "Interconnect AEAD self-test ok");
        } else {
            ESP_LOGE(kLogTag, "Interconnect AEAD self-test open failed");
        }
    } else {
        ESP_LOGE(kLogTag, "Interconnect AEAD self-test seal failed");
    }

    while (true) {
        if (s_pairingWaitActive && (int32_t)(nowMs() - s_pairingWaitDeadlineMs) >= 0) {
            s_pairingWaitActive = false;
            ESP_LOGI(kLogTag, "Interconnect pairing wait timed out");
        }
        sockaddr_in client = {};
        socklen_t clientLen = sizeof(client);
        const int clientFd = accept(listenFd, (sockaddr *)&client, &clientLen);
        if (clientFd < 0) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        // Pairing handshake: ImHere -> copy (same as product RouterInterconnect).
        uint8_t hello[6];
        if (readExact(clientFd, hello, 6, 3000) && memcmp(hello, "ImHere", 6) == 0) {
            writeAll(clientFd, (const uint8_t *)"copy", 4);
            if (s_pairingWaitActive) {
                ESP_LOGI(kLogTag, "Interconnect pairing handshake accepted");
                s_pairingWaitActive = false;
            }
            uint8_t ok[2] = {'O', 'K'};
            writeAll(clientFd, ok, 2);
        }
        close(clientFd);
    }
}

bool hostInterconnectIsListening(void) {
    return s_interconnectListening;
}

void hostServicesBegin(void) {
    loadSmtpConfig();
    // SMTP worker is lazy — created on first successful enqueue when configured.
    if (s_smtpMutex == nullptr) {
        s_smtpMutex = xSemaphoreCreateMutex();
    }
    if (s_smtpQueue == nullptr) {
        s_smtpQueue = xQueueCreate(kSmtpQueueMax, sizeof(SmtpJob));
    }
    startMqttFromFile();
    startSpiMaster();
    hostStoresSmoke();
    xTaskCreate(interconnectTask, "icListen", 6144, nullptr, 3, &s_interconnectTask);
    ESP_LOGI(kLogTag, "Host services started");
}

void hostServicesUpdate(void) {
    hostFailsafeCheck();
    if (s_mqttBrokerListenFd >= 0) {
        sockaddr_in client = {};
        socklen_t len = sizeof(client);
        const int clientFd = accept(s_mqttBrokerListenFd, (sockaddr *)&client, &len);
        if (clientFd >= 0) {
            close(clientFd);
        }
    }
}

#endif
