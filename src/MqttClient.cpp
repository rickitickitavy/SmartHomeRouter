#include "MqttClient.h"
#include "MqttBroker.h"
#include "Logger.h"
#include "StatusRgb.h"
#include "Defines.h"
#include "ZigbeeDeviceType.h"

#include <esp_mac.h>
#include <stdio.h>
#include <string.h>

MqttClient::MqttClient(SettingsManager *settingsManager, DeviceTopicMap *topicMap)
    : settingsManager(settingsManager), topicMap(topicMap), client(nullptr) {
    client = new PubSubClient(wifiClient);
    bootMs = millis();
}

MqttClient::~MqttClient() {
    if (connectTaskHandle != nullptr) {
        // Best-effort: do not join; task exits after connect and clears the handle.
        connectInProgress = false;
    }
    clearTopicSubscriptions();
    delete client;
}

void MqttClient::setMessageHandler(MessageFn handler) {
    messageHandler = handler;
}

void MqttClient::setLocalBroker(MqttBroker *broker) {
    localBroker = broker;
}

bool MqttClient::usesLocalBroker() const {
    GlobalSettings *settings = settingsManager->getSettings();
    return settings->mqtt.serverType == MqttServerTypeLocal;
}

void MqttClient::rebuildTopics() {
    GlobalSettings *settings = settingsManager->getSettings();
    const String base = String(settings->mqtt.baseTopic);
    topicStatus = base + "/bridge/status";
    topicDevices = base + "/bridge/devices";
    topicPermitJoin = base + "/bridge/permit_join";
    topicConfigDevice = base + "/bridge/config/device";
    topicServerBorn = String(settings->mqtt.serverBornTopic);
}

void MqttClient::applyRemoteBrokerTarget() {
    GlobalSettings *settings = settingsManager->getSettings();
    client->setServer(settings->mqtt.server, settings->mqtt.port);
}

void MqttClient::begin(void (*rawCallback)(char *topic, byte *payload, unsigned int length)) {
    GlobalSettings *settings = settingsManager->getSettings();
    rebuildTopics();
    applyRemoteBrokerTarget();
    client->setCallback(rawCallback);
    client->setBufferSize(1024);
    client->setSocketTimeout(settings->mqtt.clientTimeoutMs / 1000 > 0 ? settings->mqtt.clientTimeoutMs / 1000 : 1);
    bootMs = millis();
    lastBornAnnounceMs = 0;
    bornBootAnnounceDone = false;
}

void MqttClient::reloadFromSettings() {
    GlobalSettings *settings = settingsManager->getSettings();
    rebuildTopics();
    applyRemoteBrokerTarget();
    localAttached = false;
    remoteSessionAttachPending = false;
    reconnectBackoffMs = 0;
    lastReconnectMs = 0;
    bornBootAnnounceDone = false;
    lastBornAnnounceMs = 0;
    if (client != nullptr) {
        client->setSocketTimeout(
            settings->mqtt.clientTimeoutMs / 1000 > 0 ? settings->mqtt.clientTimeoutMs / 1000 : 1
        );
        if (client->connected()) {
            client->disconnect();
        }
    }
    STATUS_RGB.setMqttConnected(false);
    LOGGER.info("MQTT client reloaded from settings");
}

void MqttClient::onMessage(char *topic, byte *payload, unsigned int length) {
    // PubSubClient can underflow the payload length on a truncated PUBLISH after a
    // TCP reset; walking that length into ROM (e.g. 0x40000000) panics the host.
    static constexpr unsigned int kMaxPayloadBytes = 1024;
    if (topic == nullptr || topic[0] == '\0') {
        return;
    }
    if (length > kMaxPayloadBytes) {
        LOGGER.warning(
            "MQTT ignore corrupt/oversized message topic=" + String(topic)
            + " len=" + String((unsigned long)length)
        );
        return;
    }
    if (length > 0 && payload == nullptr) {
        LOGGER.warning("MQTT ignore message with null payload topic=" + String(topic));
        return;
    }

    String body;
    if (length > 0) {
        body.concat(reinterpret_cast<const char *>(payload), length);
    }
    LOGGER.debug("MQTT " + String(topic) + " = " + body);
    STATUS_RGB.pulseMqttCommandReceived();
    if (messageHandler != nullptr) {
        messageHandler(topic, body.c_str());
    }
}

bool MqttClient::isConnected() const {
    if (usesLocalBroker()) {
        return localAttached && localBroker != nullptr && localBroker->isListening();
    }
    return client != nullptr && client->connected();
}

void MqttClient::notePacketReceived() {
    packetsRx++;
}

void MqttClient::notePacketSent() {
    packetsTx++;
}

void MqttClient::clearTopicSubscriptions() {
    while (subscriptionHead != nullptr) {
        TopicSubscription *next = subscriptionHead->next;
        free(subscriptionHead->topic);
        free(subscriptionHead);
        subscriptionHead = next;
    }
}

MqttClient::TopicSubscription *MqttClient::findTopicSubscription(const char *topic) const {
    if (topic == nullptr || topic[0] == '\0') {
        return nullptr;
    }
    for (TopicSubscription *node = subscriptionHead; node != nullptr; node = node->next) {
        if (node->topic != nullptr && strcmp(node->topic, topic) == 0) {
            return node;
        }
    }
    return nullptr;
}

void MqttClient::keepOrSubscribe(const char *topic, bool quiet) {
    if (topic == nullptr || topic[0] == '\0') {
        return;
    }
    TopicSubscription *existing = findTopicSubscription(topic);
    if (existing != nullptr) {
        existing->keepMarked = true;
        return;
    }
    if (!client->subscribe(topic)) {
        LOGGER.warning(String("MQTT subscribe failed topic=") + topic);
        return;
    }
    TopicSubscription *node = (TopicSubscription *)calloc(1, sizeof(TopicSubscription));
    if (node == nullptr) {
        client->unsubscribe(topic);
        LOGGER.warning(String("MQTT subscribe OOM tracking topic=") + topic);
        return;
    }
    const size_t topicLen = strlen(topic);
    node->topic = (char *)malloc(topicLen + 1);
    if (node->topic == nullptr) {
        free(node);
        client->unsubscribe(topic);
        LOGGER.warning(String("MQTT subscribe OOM topic string topic=") + topic);
        return;
    }
    memcpy(node->topic, topic, topicLen + 1);
    node->keepMarked = true;
    node->next = subscriptionHead;
    subscriptionHead = node;
    if (quiet) {
        if (reconcileSubscribeCount < 0xFFFF) {
            reconcileSubscribeCount++;
        }
        LOGGER.debug(String("MQTT subscribe topic=") + topic);
    } else {
        LOGGER.info(String("MQTT subscribe topic=") + topic);
    }
}

void MqttClient::subscribeBridgeTopics() {
    if (topicPermitJoin.length() > 0) {
        client->subscribe(topicPermitJoin.c_str());
    }
    if (topicConfigDevice.length() > 0) {
        client->subscribe(topicConfigDevice.c_str());
    }
}

void MqttClient::beginSubscriptionReconcile() {
    rebuildTopics();
    if (usesLocalBroker()) {
        return;
    }
    if (!isConnected() || topicMap == nullptr) {
        return;
    }
    subscriptionReconcileActive = true;
    reconcilePhase = SubscriptionReconcilePhase::ClearMarks;
    reconcileDeviceCursor = nullptr;
    reconcileSubscribeCount = 0;
    reconcileUnsubscribeCount = 0;
}

void MqttClient::subscribeDeviceCommands() {
    beginSubscriptionReconcile();
}

uint8_t MqttClient::enqueueWantedTopicsForEntry(const DeviceTopicEntry *entry, uint8_t opBudget) {
    if (entry == nullptr || !entry->used || opBudget == 0) {
        return 0;
    }
    uint8_t usedOps = 0;
    char suffixTopic[192];

    auto takeTopic = [&](const char *topic) {
        if (usedOps >= opBudget || topic == nullptr || topic[0] == '\0') {
            return;
        }
        keepOrSubscribe(topic, true);
        usedOps++;
    };

    if (entry->transport == DeviceTransportMqtt) {
        takeTopic(deviceTopicCStr(entry->stateTopic));
        takeTopic(deviceTopicCStr(entry->availabilityTopic));
        return usedOps;
    }

    if (!devicePublishesToMqtt(entry)) {
        return 0;
    }

    if (!deviceTopicEmpty(entry->commandTopic)) {
        takeTopic(entry->commandTopic);
        if (usedOps < opBudget && DeviceTopicMap::usesTopicSuffix(entry->channelCount)) {
            snprintf(suffixTopic, sizeof(suffixTopic), "%s/+", entry->commandTopic);
            takeTopic(suffixTopic);
        }
    }

    if (!deviceTopicEmpty(entry->stateTopic) && zigbeeDeviceTypeIsMeasurement(entry->zigbeeType)) {
        takeTopic(entry->stateTopic);
        if (usedOps < opBudget && DeviceTopicMap::usesTopicSuffix(entry->channelCount)) {
            snprintf(suffixTopic, sizeof(suffixTopic), "%s/+", entry->stateTopic);
            takeTopic(suffixTopic);
        }
    }
    return usedOps;
}

void MqttClient::pumpSubscriptionReconcile(uint8_t opBudget) {
    if (!subscriptionReconcileActive || opBudget == 0) {
        return;
    }
    if (!isConnected() || usesLocalBroker() || topicMap == nullptr) {
        subscriptionReconcileActive = false;
        reconcilePhase = SubscriptionReconcilePhase::Idle;
        return;
    }

    uint8_t remaining = opBudget;
    while (remaining > 0 && subscriptionReconcileActive) {
        if (reconcilePhase == SubscriptionReconcilePhase::ClearMarks) {
            for (TopicSubscription *node = subscriptionHead; node != nullptr; node = node->next) {
                node->keepMarked = false;
            }
            reconcilePhase = SubscriptionReconcilePhase::AddBorn;
            continue;
        }

        if (reconcilePhase == SubscriptionReconcilePhase::AddBorn) {
            rebuildTopics();
            if (topicServerBorn.length() > 0) {
                keepOrSubscribe(topicServerBorn.c_str(), true);
                remaining--;
            }
            reconcilePhase = SubscriptionReconcilePhase::AddDevices;
            reconcileDeviceCursor = topicMap->first();
            continue;
        }

        if (reconcilePhase == SubscriptionReconcilePhase::AddDevices) {
            while (reconcileDeviceCursor != nullptr && !reconcileDeviceCursor->used) {
                reconcileDeviceCursor = DeviceTopicMap::nextEntry(reconcileDeviceCursor);
            }
            if (reconcileDeviceCursor == nullptr) {
                reconcilePhase = SubscriptionReconcilePhase::RemoveStale;
                continue;
            }
            // One entry can need up to four topics; finish the entry in one slice.
            if (remaining < 4) {
                break;
            }
            const uint8_t usedOps = enqueueWantedTopicsForEntry(reconcileDeviceCursor, remaining);
            reconcileDeviceCursor = DeviceTopicMap::nextEntry(reconcileDeviceCursor);
            remaining = (uint8_t)(remaining - usedOps);
            continue;
        }

        if (reconcilePhase == SubscriptionReconcilePhase::RemoveStale) {
            TopicSubscription **link = &subscriptionHead;
            bool removed = false;
            while (*link != nullptr) {
                TopicSubscription *node = *link;
                if (node->keepMarked) {
                    link = &node->next;
                    continue;
                }
                if (node->topic != nullptr) {
                    client->unsubscribe(node->topic);
                    if (reconcileUnsubscribeCount < 0xFFFF) {
                        reconcileUnsubscribeCount++;
                    }
                    LOGGER.debug(String("MQTT unsubscribe topic=") + node->topic);
                }
                *link = node->next;
                free(node->topic);
                free(node);
                remaining--;
                removed = true;
                break;
            }
            if (!removed) {
                subscriptionReconcileActive = false;
                reconcilePhase = SubscriptionReconcilePhase::Idle;
                LOGGER.info(
                    String("MQTT subscriptions ready subscribed=")
                    + String(reconcileSubscribeCount)
                    + " unsubscribed="
                    + String(reconcileUnsubscribeCount)
                );
                break;
            }
            continue;
        }

        break;
    }
}

void MqttClient::attachLocalBroker() {
    rebuildTopics();
    lastDevicesJson = "";
    localAttached = true;
    LOGGER.info("MQTT connected to local broker");
    publishStatus("online");
}

void MqttClient::connectTaskEntry(void *arg) {
    MqttClient *mqttClient = static_cast<MqttClient *>(arg);
    if (mqttClient != nullptr) {
        mqttClient->runRemoteConnect();
        mqttClient->connectTaskHandle = nullptr;
        mqttClient->connectInProgress = false;
    }
    vTaskDelete(nullptr);
}

void MqttClient::finishRemoteSessionAttach() {
    if (!client->connected()) {
        remoteSessionAttachPending = false;
        return;
    }
    remoteSessionAttachPending = false;
    reconnectBackoffMs = 0;
    lastDevicesJson = "";
    clearTopicSubscriptions();
    LOGGER.info("MQTT connected");
    rebuildTopics();
    subscribeBridgeTopics();
    publishStatus("online");
    beginSubscriptionReconcile();
}

void MqttClient::fillUniqueClientId(char *outClientId, size_t outLen) const {
    if (outClientId == nullptr || outLen == 0) {
        return;
    }
    outClientId[0] = '\0';
    String macSuffix = WiFi.macAddress();
    macSuffix.replace(':', '-');
    snprintf(outClientId, outLen, "Z2M-router-%s", macSuffix.c_str());
}

void MqttClient::runRemoteConnect() {
    GlobalSettings *settings = settingsManager->getSettings();
    applyRemoteBrokerTarget();
    char uniqueClientId[48];
    fillUniqueClientId(uniqueClientId, sizeof(uniqueClientId));
    LOGGER.info(
        "MQTT connecting to " + String(settings->mqtt.server) + ":" + String(settings->mqtt.port)
        + " as " + String(uniqueClientId)
    );

    const char *user = settings->mqtt.username[0] != '\0' ? settings->mqtt.username : nullptr;
    const char *password = user != nullptr ? settings->mqtt.password : nullptr;
    const bool ok = client->connect(uniqueClientId, user, password);
    if (!ok) {
        LOGGER.warning("MQTT connect failed, state " + String(client->state()));
        reconnectBackoffMs = reconnectBackoffMs == 0 ? 2000 : min(reconnectBackoffMs * 2, (unsigned long)30000);
        return;
    }

    remoteSessionAttachPending = true;
}

void MqttClient::reconnect() {
    if (connectInProgress || connectTaskHandle != nullptr) {
        return;
    }
    connectInProgress = true;
    const BaseType_t created = xTaskCreate(
        connectTaskEntry,
        "mqttConnect",
        4096,
        this,
        1,
        &connectTaskHandle
    );
    if (created != pdPASS) {
        connectTaskHandle = nullptr;
        connectInProgress = false;
        LOGGER.warning("MQTT connect task create failed; connecting on loop");
        runRemoteConnect();
    }
}

void MqttClient::dispatch(bool staConnected) {
    GlobalSettings *settings = settingsManager->getSettings();
    if (settings->mqtt.serverType == MqttServerTypeLocal) {
        const bool brokerUp = localBroker != nullptr && localBroker->isListening();
        if (!brokerUp) {
            localAttached = false;
            STATUS_RGB.setMqttConnected(false);
            return;
        }
        if (!localAttached) {
            attachLocalBroker();
        }
        STATUS_RGB.setMqttConnected(true);
        serviceBornAnnounce();
        return;
    }

    localAttached = false;
    if (settings->mqtt.serverType != MqttServerTypeRemote || !staConnected) {
        STATUS_RGB.setMqttConnected(false);
        return;
    }

    if (connectInProgress) {
        STATUS_RGB.setMqttConnected(false);
        return;
    }

    if (client->connected()) {
        if (remoteSessionAttachPending) {
            finishRemoteSessionAttach();
        }
        // PubSubClient subscribe/unsubscribe stays on the loop task with client->loop().
        pumpSubscriptionReconcile(4);
        STATUS_RGB.setMqttConnected(true);
        client->loop();
        serviceBornAnnounce();
        return;
    }

    remoteSessionAttachPending = false;
    subscriptionReconcileActive = false;
    reconcilePhase = SubscriptionReconcilePhase::Idle;

    STATUS_RGB.setMqttConnected(false);

    const unsigned long now = millis();
    const unsigned long waitMs = reconnectBackoffMs > 0 ? reconnectBackoffMs : (unsigned long)settings->mqtt.reconnectIntervalMs;
    if ((now - lastReconnectMs) < waitMs) {
        return;
    }
    lastReconnectMs = now;
    reconnect();
}

void MqttClient::pumpNetwork() {
    if (publishPumpInProgress) {
        return;
    }
    publishPumpInProgress = true;
    if (usesLocalBroker()) {
        if (localBroker != nullptr) {
            localBroker->pump();
        }
    } else {
        // Do not call PubSubClient::loop() between burst publishes: its blocking
        // readByte/yield path can hit a torn-down NetworkClient RX buffer and
        // LoadProhibited. Yield so lwIP can drain the TCP send queue instead.
        yield();
        delay(1);
    }
    publishPumpInProgress = false;
}

bool MqttClient::publishMessage(const char *topic, const char *payload, bool retained, bool logInfo) {
    if (!isConnected() || topic == nullptr || topic[0] == '\0') {
        return false;
    }
    const char *data = payload != nullptr ? payload : "";
    const bool sent = usesLocalBroker()
        ? localBroker->publishFromHost(topic, data, retained)
        : client->publish(topic, data, retained);
    if (sent) {
        notePacketSent();
        if (logInfo) {
            LOGGER.info(String("MQTT send topic=") + topic + " data=" + data);
        }
    } else {
        LOGGER.warning(String("MQTT send failed topic=") + topic + " data=" + data);
        // Drop a half-closed remote socket so the next loop() does not parse junk.
        if (!usesLocalBroker() && client != nullptr) {
            client->disconnect();
        }
    }
    return sent;
}

void MqttClient::publishStatus(const char *payload) {
    publishMessage(topicStatus.c_str(), payload, true);
}

void MqttClient::publishDevices(const String &json) {
    if (json == lastDevicesJson) {
        return;
    }
    if (publishMessage(topicDevices.c_str(), json.c_str(), false)) {
        lastDevicesJson = json;
    }
}

void MqttClient::publishDeviceState(const DeviceTopicEntry *entry, const char *message, uint8_t endpoint) {
    if (entry == nullptr || deviceTopicEmpty(entry->stateTopic)) {
        return;
    }
    if (entry->transport == DeviceTransportMqtt) {
        return;
    }
    if (!devicePublishesToMqtt(entry)) {
        return;
    }
    if (message == nullptr) {
        return;
    }
    if (DeviceTopicMap::usesPayloadParse(entry->channelCount) && !DeviceTopicMap::isUsableEndpoint(endpoint)) {
        LOGGER.warning("Skip state publish; parse mode needs a real endpoint");
        return;
    }
    const String topic = DeviceTopicMap::statePublishTopic(entry, endpoint);
    const String payload = DeviceTopicMap::statePublishPayload(entry, endpoint, message);
    if (publishMessage(topic.c_str(), payload.c_str(), true, entry->debug != 0)) {
        STATUS_RGB.pulseMqttPublished();
    }
}

bool MqttClient::publishDeviceCommand(const DeviceTopicEntry *entry, const char *message, uint8_t endpoint) {
    if (entry == nullptr || deviceTopicEmpty(entry->commandTopic) || message == nullptr) {
        return false;
    }
    if (entry->transport == DeviceTransportMqtt) {
        const bool sent = publishMessage(entry->commandTopic, message, false, entry->debug != 0);
        if (sent) {
            STATUS_RGB.pulseMqttPublished();
        }
        return sent;
    }
    uint8_t publishEndpoint = endpoint;
    String publishBody = String(message);
    if (DeviceTopicMap::usesPayloadParse(entry->channelCount)) {
        if (!DeviceTopicMap::isUsableEndpoint(endpoint)) {
            publishEndpoint = 1;
        }
        publishBody = DeviceTopicMap::statePublishPayload(entry, publishEndpoint, message);
        publishEndpoint = 0;
    } else if (DeviceTopicMap::usesTopicSuffix(entry->channelCount)
        && !DeviceTopicMap::isUsableEndpoint(publishEndpoint)) {
        publishEndpoint = 1;
    }
    const String topic = DeviceTopicMap::commandPublishTopic(
        entry,
        DeviceTopicMap::usesTopicSuffix(entry->channelCount) ? publishEndpoint : 1
    );
    const bool sent = publishMessage(topic.c_str(), publishBody.c_str(), false, entry->debug != 0);
    if (sent) {
        STATUS_RGB.pulseMqttPublished();
    }
    return sent;
}

bool MqttClient::publishServerBornAnnounce() {
    rebuildTopics();
    if (topicServerBorn.length() == 0) {
        return false;
    }
    return publishMessage(topicServerBorn.c_str(), "online", false);
}

void MqttClient::serviceBornAnnounce() {
    rebuildTopics();
    if (topicServerBorn.length() == 0 || !isConnected()) {
        return;
    }
    GlobalSettings *settings = settingsManager->getSettings();
    const unsigned long now = millis();
    const unsigned long intervalMs =
        (unsigned long)clampBornIntervalMin(settings->mqtt.bornIntervalMin) * 60UL * 1000UL;
    if (!bornBootAnnounceDone) {
        const unsigned long bootDelayMs = usesLocalBroker()
            ? MQTT_BORN_BOOT_DELAY_MS
            : MQTT_BORN_BOOT_DELAY_REMOTE_MS;
        if ((now - bootMs) < bootDelayMs) {
            return;
        }
        if (publishServerBornAnnounce()) {
            bornBootAnnounceDone = true;
            lastBornAnnounceMs = now;
        }
        return;
    }
    if ((now - lastBornAnnounceMs) < intervalMs) {
        return;
    }
    if (publishServerBornAnnounce()) {
        lastBornAnnounceMs = now;
    }
}
