#include <Arduino.h>
#include <LittleFS.h>
#include <esp_system.h>
#include <time.h>

#include "pins.h"
#include "FirmwareOta.h"
#include "Defines.h"
#include "Logger.h"
#include "SettingsManager.h"
#include "DeviceTopicMap.h"
#include "UserStore.h"
#include "JsonField.h"
#include "InterChipSlave.h"
#include "ZigbeeCoordinator.h"
#include "StatusRgb.h"
#include "DeviceStore.h"
#include "DeviceConfigCatalog.h"
#include "DeviceManufacturer.h"
#include "ZigbeeDeviceType.h"
#include "ZigbeeCluster.h"
#include "UserDataPayload.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#if !defined(ZIGBEE_MODE_ZCZR)
#error "Zigbee coordinator mode is not selected (ZIGBEE_MODE_ZCZR)"
#endif

static SettingsManager *settingsManager = nullptr;
static DeviceTopicMap *topicMap = nullptr;
static UserStore *userStore = nullptr;
static DeviceStore *deviceStore = nullptr;

#if defined(BOARD_ROLE_SLAVE)
#include "InterChipSlave.h"
#endif

#if defined(BOARD_ROLE_SLAVE)
#include "ZigbeeCoordinator.h"
#endif

#if defined(BOARD_ROLE_SLAVE) && !defined(ZIGBEE_MODE_ZCZR)
#error "Zigbee coordinator mode is not selected (ZIGBEE_MODE_ZCZR)"
#endif

#if defined(BOARD_ROLE_SLAVE)
static ZigbeeCoordinator *zigbeeCoordinator = nullptr;
#endif

#if defined(BOARD_ROLE_SLAVE)
static bool slaveZigbeeStarted = false;
static bool slaveZigbeeStarting = false;
static uint32_t slaveZigbeeStartDeadlineMs = 0;
static constexpr uint32_t kSlaveZigbeeStartTimeoutMs = 30000;
#endif

#if defined(BOARD_ROLE_SLAVE)
static void onSlaveUserSync(uint8_t flags, const UserRecord *user);
#endif

#if defined(BOARD_ROLE_SLAVE)
static void onSlaveUserSync(uint8_t flags, const UserRecord *user) {
    if ((flags & SPI_USER_SYNC_RESET) != 0) {
        LOGGER.warning("Host cannot replace the full user store");
        return;
    }
    if (userStore == nullptr) {
        return;
    }
    if ((flags & SPI_USER_SYNC_DELETE) != 0 && user != nullptr) {
        if (userStore->userCount() <= 1) {
            LOGGER.warning("Refusing to delete the last slave user");
            return;
        }
        userStore->removeByName(user->userName, true);
        return;
    }
    if ((flags & SPI_USER_SYNC_ENTRY) != 0 && user != nullptr) {
        userStore->upsertFromRecord(user, true);
    }
}
#endif

#if defined(BOARD_ROLE_SLAVE)
static void onSlaveRebindResult(const uint8_t ieee[8], uint8_t outcome);
static void onSlaveRebindDevice(const uint8_t ieee[8]);

static void startZigbeeOnOwnTask(uint8_t channel, uint8_t permitJoinSec) {
    struct StartArgs {
        uint8_t channel;
        uint8_t permitJoinSec;
    };
    StartArgs *args = new StartArgs{channel, permitJoinSec};
    const BaseType_t created = xTaskCreate(
        [](void *context) {
            StartArgs *startArgs = static_cast<StartArgs *>(context);
            INTER_CHIP_SLAVE.setPumpPaused(true);
            vTaskDelay(pdMS_TO_TICKS(40));
            bool started = false;
            if (zigbeeCoordinator != nullptr) {
                started = zigbeeCoordinator->begin(startArgs->channel, startArgs->permitJoinSec);
            }
            if (!started) {
                LOGGER.error("Zigbee start failed — check ZCZR partition / erase flash");
                INTER_CHIP_SLAVE.setZigbeeStarted(false);
                slaveZigbeeStarting = false;
            } else {
                slaveZigbeeStarted = true;
                slaveZigbeeStarting = false;
                slaveZigbeeStartDeadlineMs = 0;
                INTER_CHIP_SLAVE.setZigbeeStarted(true);
            }
            INTER_CHIP_SLAVE.resumeAfterRadioPause();
            delete startArgs;
            vTaskDelete(nullptr);
        },
        "zbStart",
        16384,
        args,
        1,
        nullptr
    );
    if (created != pdPASS) {
        delete args;
        slaveZigbeeStarting = false;
        LOGGER.error("Zigbee start task failed");
        INTER_CHIP_SLAVE.setPumpPaused(false);
    }
}

static void createSlaveCoordinator() {
    if (zigbeeCoordinator != nullptr) {
        return;
    }
    zigbeeCoordinator = new ZigbeeCoordinator();
    zigbeeCoordinator->setRegistryChangedHandler(nullptr);
    zigbeeCoordinator->setJoinClosedHandler(
        []() { INTER_CHIP_SLAVE.enqueueJoinClosed(); }
    );
    zigbeeCoordinator->setRebindResultHandler(onSlaveRebindResult);
    zigbeeCoordinator->setDeviceBoundHandler(
        [](const BoundZigbeeDevice *device) {
            if (device == nullptr) {
                return;
            }
            INTER_CHIP_SLAVE.enqueueDeviceJoin(
                device->ieee,
                device->shortAddr,
                device->endpoint,
                device->manufacturer,
                device->model,
                device->zigbeeType
            );
        }
    );
    zigbeeCoordinator->setLightStateHandler(
        [](const char *message, const uint8_t ieee[8], uint8_t endpoint, uint16_t shortAddr, int8_t rssiDbm) {
            INTER_CHIP_SLAVE.enqueueAttrReport(message, ieee, endpoint, shortAddr, rssiDbm);
        }
    );
    zigbeeCoordinator->attachLibraryCallbacks(
        [](bool on, uint8_t endpoint, esp_zb_zcl_addr_t source) {
            if (zigbeeCoordinator != nullptr) {
                zigbeeCoordinator->handleLightStateWithSource(on, endpoint, source);
            }
        }
    );
    zigbeeCoordinator->setZigbeeLostHandler([](const char *reason) {
        INTER_CHIP_SLAVE.markZigbeeLost(reason);
    });
}

static void onSlaveSettings(uint8_t channel, uint8_t permitJoinSec, uint32_t unixSec) {
    (void)unixSec;
    createSlaveCoordinator();
    if (slaveZigbeeStarted || slaveZigbeeStarting) {
        LOGGER.info("Settings again; coordinator already started");
        return;
    }
    slaveZigbeeStarting = true;
    slaveZigbeeStartDeadlineMs = millis() + kSlaveZigbeeStartTimeoutMs;
    INTER_CHIP_SLAVE.setZigbeeStarted(false);
    startZigbeeOnOwnTask(channel, permitJoinSec);
}

static bool onSlavePermitJoin(uint8_t seconds) {
    if (zigbeeCoordinator == nullptr || !zigbeeCoordinator->isStarted()) {
        return false;
    }
    if (seconds == 0) {
        return zigbeeCoordinator->closeJoin();
    }
    return zigbeeCoordinator->permitJoin(seconds);
}

static void onSlaveRebindDevice(const uint8_t ieee[8]) {
    if (zigbeeCoordinator == nullptr) {
        INTER_CHIP_SLAVE.enqueueRebindResult(ieee, SPI_REBIND_OUTCOME_FAILED);
        return;
    }
    zigbeeCoordinator->startRebind(ieee);
}

static void onSlaveRebindResult(const uint8_t ieee[8], uint8_t outcome) {
    INTER_CHIP_SLAVE.enqueueRebindResult(ieee, outcome);
}

static void onSlaveDeviceSync(uint8_t flags, const DeviceTopicEntry *entry) {
    DeviceTopicMap *storeMap = deviceStore != nullptr ? deviceStore->deviceMap() : nullptr;
    if ((flags & SPI_DEVICE_SYNC_RESET) != 0) {
        // Empty authoritative list (Repeater): clear leftovers and stay not-ready so
        // bidirectional radio works with no device list (propose: no known-list on Repeater).
        if (zigbeeCoordinator != nullptr) {
            zigbeeCoordinator->clearRegisteredDevices();
        } else if (storeMap != nullptr) {
            storeMap->clearAll();
        }
        LOGGER.info("Device registry cleared (not ready; no device list)");
        return;
    }
    if ((flags & SPI_DEVICE_SYNC_DELETE) != 0 && entry != nullptr) {
        if (zigbeeCoordinator != nullptr) {
            zigbeeCoordinator->removeRegisteredDevice(entry->ieee);
        } else if (storeMap != nullptr) {
            storeMap->removeByIeee(entry->ieee);
        }
    } else if ((flags & SPI_DEVICE_SYNC_ENTRY) != 0 && entry != nullptr) {
        if (zigbeeCoordinator != nullptr) {
            zigbeeCoordinator->upsertRegisteredDevice(entry);
        } else if (storeMap != nullptr) {
            storeMap->upsertFromEntry(entry, false);
        }
    }
    if (zigbeeCoordinator != nullptr) {
        zigbeeCoordinator->markRegistryReady();
    }
}

static void onSlaveOnOff(const uint8_t ieee[8], const char *command, uint8_t endpoint) {
    if (zigbeeCoordinator == nullptr) {
        return;
    }
    zigbeeCoordinator->controlOnOff(ieee, command, endpoint);
}

static void onSlaveWriteAttr(
    const uint8_t ieee[8],
    uint8_t endpoint,
    uint16_t clusterId,
    uint16_t attributeId,
    uint8_t dataType,
    uint32_t attributeValue
) {
    if (zigbeeCoordinator == nullptr) {
        return;
    }
    zigbeeCoordinator->writeAttribute(ieee, endpoint, clusterId, attributeId, dataType, attributeValue);
}

static void onSlaveReadAttr(
    const uint8_t ieee[8],
    uint8_t endpoint,
    uint16_t clusterId,
    uint16_t attributeId
) {
    if (zigbeeCoordinator == nullptr) {
        return;
    }
    zigbeeCoordinator->readAttribute(ieee, endpoint, clusterId, attributeId);
}

static void onSlaveZclCommand(
    const uint8_t ieee[8],
    uint8_t endpoint,
    uint16_t clusterId,
    uint8_t commandId,
    const uint8_t *payload,
    uint8_t payloadLength
) {
    if (zigbeeCoordinator == nullptr) {
        return;
    }
    zigbeeCoordinator->sendClusterCommand(ieee, endpoint, clusterId, commandId, payload, payloadLength);
}
#endif

#if defined(BOARD_ROLE_SLAVE)
static void setupSlave() {
    STATUS_RGB.setReadyGreen(true);
    STATUS_RGB.setBootHeld(true);
    LOGGER.setRoleLabel("slave");
    LOGGER.setStoreRing(false);
    INTER_CHIP_SLAVE.setSettingsHandler(onSlaveSettings);
    INTER_CHIP_SLAVE.setPermitJoinHandler(onSlavePermitJoin);
    INTER_CHIP_SLAVE.setRebindDeviceHandler(onSlaveRebindDevice);
    INTER_CHIP_SLAVE.setZigbeeHealthChecker([]() {
        return zigbeeCoordinator != nullptr && zigbeeCoordinator->checkResponsive();
    });
    INTER_CHIP_SLAVE.setOnOffHandler(onSlaveOnOff);
    INTER_CHIP_SLAVE.setWriteAttrHandler(onSlaveWriteAttr);
    INTER_CHIP_SLAVE.setReadAttrHandler(onSlaveReadAttr);
    INTER_CHIP_SLAVE.setZclCommandHandler(onSlaveZclCommand);
    INTER_CHIP_SLAVE.setDeviceSyncHandler(onSlaveDeviceSync);
    INTER_CHIP_SLAVE.setUserSyncHandler(onSlaveUserSync);
    deviceStore = new DeviceStore();
    deviceStore->begin(false);
    userStore = new UserStore();
    userStore->begin(false);
    INTER_CHIP_SLAVE.setUserMapSource(userStore);
    createSlaveCoordinator();
    if (zigbeeCoordinator != nullptr) {
        zigbeeCoordinator->setRegisteredMap(deviceStore->deviceMap());
        if (deviceStore->deviceMap()->usedCount() > 0) {
            zigbeeCoordinator->markRegistryReady();
        }
    }
    INTER_CHIP_SLAVE.setDeviceMapSource(deviceStore->deviceMap());
    INTER_CHIP_SLAVE.setDevicesFileSource(nullptr);
    INTER_CHIP_SLAVE.begin();
    LOGGER.setLineHook(interChipSlaveLogHook);
    LOGGER.info("role=slave");
    LOGGER.info("z2m-gateway slave " FIRMWARE_VERSION_SLAVE);
    LOGGER.info("Reset " + String((int)esp_reset_reason()));
    LOGGER.info("Zigbee runtime copy only; devices and users stay on the host");
}
#endif






void setup() {
    STATUS_RGB.begin();
    Serial.begin(SERIAL_CONSOLE_BAUD);
    Serial.setTxTimeoutMs(20);
    delay(400);
    setupSlave();
}


void loop() {
    FIRMWARE_OTA.pump();
    if (FIRMWARE_OTA.isApplyingImage()) {
        if (FIRMWARE_OTA.consumeFirmwareRestart()) {
            delay(300);
            ESP.restart();
        }
        delay(0);
        return;
    }

    STATUS_RGB.service();
    INTER_CHIP_SLAVE.applyDeferredSettings();
    INTER_CHIP_SLAVE.applyDeferredRadioCommands();
    if (INTER_CHIP_SLAVE.consumeZigbeeLostReboot()) {
        LOGGER.error("Zigbee lost; rebooting slave");
        delay(200);
        ESP.restart();
    }
    if (zigbeeCoordinator != nullptr && zigbeeCoordinator->isStarted()) {
        STATUS_RGB.setBootHeld(false);
        slaveZigbeeStartDeadlineMs = 0;
    } else if (slaveZigbeeStartDeadlineMs != 0
        && (int32_t)(millis() - slaveZigbeeStartDeadlineMs) >= 0) {
        LOGGER.error("Zigbee did not start within 30s; rebooting slave");
        delay(200);
        ESP.restart();
    }
    INTER_CHIP_SLAVE.pumpDeviceDump();
    INTER_CHIP_SLAVE.pumpUserDump();
    if (deviceStore != nullptr) {
        deviceStore->persistIfDue();
    }
    if (userStore != nullptr) {
        userStore->persistIfDue();
    }
    INTER_CHIP_SLAVE.pumpDevicesFileDump();
    if (FIRMWARE_OTA.consumeFirmwareRestart()) {
        delay(300);
        ESP.restart();
    }
    if (zigbeeCoordinator != nullptr) {
        zigbeeCoordinator->dispatch();
    }
    delay(5);
}

