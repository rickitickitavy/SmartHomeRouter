#if defined(BOARD_ROLE_HOST)

#include "RouterInterconnect.h"
#include "Logger.h"
#include "StatusRgb.h"
#include "ZigbeeSpiProxy.h"

#include <esp_heap_caps.h>
#include <esp_random.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <mbedtls/gcm.h>
#include <string.h>

namespace {

constexpr size_t kNonceLen = 12;
constexpr size_t kTagLen = 16;
constexpr size_t kHeaderLen = 1 + 1 + 2 + kNonceLen;  // ver type len nonce
constexpr uint32_t kIoTimeoutMs = 3000;

void u16Be(uint8_t out[2], uint16_t value) {
    out[0] = (uint8_t)((value >> 8) & 0xFF);
    out[1] = (uint8_t)(value & 0xFF);
}

uint16_t u16FromBe(const uint8_t in[2]) {
    return (uint16_t)(((uint16_t)in[0] << 8) | in[1]);
}

}  // namespace

RouterInterconnect::RouterInterconnect(InterconnectStore *store) : interconnectStore(store) {}

void RouterInterconnect::noteLiveUiChanged() {
    if (liveChangedHandler != nullptr) {
        liveChangedHandler();
    }
}

bool RouterInterconnect::localWifiMac(uint8_t macOut[6]) {
    if (macOut == nullptr) {
        return false;
    }
    WiFi.macAddress(macOut);
    for (int i = 0; i < 6; i++) {
        if (macOut[i] != 0) {
            return true;
        }
    }
    return esp_wifi_get_mac(WIFI_IF_STA, macOut) == ESP_OK;
}

void RouterInterconnect::ensureWorkerStarted() {
    if (workerTask != nullptr) {
        return;
    }
    if (queueMutex == nullptr) {
        queueMutex = xSemaphoreCreateMutex();
        if (queueMutex == nullptr) {
            LOGGER.error("Interconnect queue mutex alloc failed");
            return;
        }
    }
    const BaseType_t created = xTaskCreate(
        workerEntry,
        "icQueue",
        8192,
        this,
        1,
        &workerTask
    );
    if (created != pdPASS) {
        workerTask = nullptr;
        LOGGER.error("Interconnect queue worker create failed");
        return;
    }
    LOGGER.info("Interconnect outbound worker started (async checkAlive/exchanges)");
}

void RouterInterconnect::workerEntry(void *argument) {
    RouterInterconnect *self = static_cast<RouterInterconnect *>(argument);
    if (self != nullptr) {
        self->workerLoop();
    }
    vTaskDelete(nullptr);
}

void RouterInterconnect::workerLoop() {
    for (;;) {
        pumpQueue();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void RouterInterconnect::begin() {
    if (interconnectStore == nullptr) {
        return;
    }
    if (jobQueue == nullptr) {
        jobQueue = (InterconnectJob *)heap_caps_calloc(
            INTERCONNECT_QUEUE_DEPTH,
            sizeof(InterconnectJob),
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
        );
        if (jobQueue == nullptr) {
            LOGGER.error("Interconnect queue alloc failed");
            return;
        }
    }
    ensureWorkerStarted();
    const uint16_t port = interconnectStore->settings().port;
    if (server != nullptr) {
        server->stop();
        delete server;
        server = nullptr;
    }
    server = new WiFiServer(port);
    server->begin();
    server->setNoDelay(true);
    LOGGER.info("Interconnect listening on " + String(port));
    justBootedSent = false;
    mainBootPushDone = false;
    if (interconnectStore->settings().role == InterconnectRoleRepeater
        && interconnectStore->settings().hasMain
        && interconnectStore->settings().mainLastAliveMs == 0) {
        // Grace window after boot until Main's first checkAlive arrives.
        interconnectStore->settingsMutable().mainLastAliveMs = millis();
        interconnectStore->settingsMutable().mainOnline = true;
        noteLiveUiChanged();
    }
}

void RouterInterconnect::loop() {
    if (interconnectStore == nullptr) {
        return;
    }
    interconnectStore->persistIfDue();
    if (pairingWaitActiveFlag && (int32_t)(millis() - pairingWaitDeadlineMs) >= 0) {
        cancelPairingWait();
        LOGGER.info("Interconnect pairing wait timed out");
    }
    acceptClients();
    maybeRefreshMainLiveness();
    // Outbound exchanges (including checkAlive) run on icQueue worker — not here.
    pumpSubnetScan();
    maybeSendJustBooted();
    maybePushSharedOnMainBoot();
    maybeSendCheckAlive();
}

PsramJsonBuffer RouterInterconnect::statusJson() const {
    PsramJsonBuffer json = interconnectStore != nullptr
        ? interconnectStore->statusJson(pairingWaitRemainingMs())
        : PsramJsonBuffer();
    if (!json.ok() || json.length() < 2) {
        return json;
    }
    // Replace trailing '}' with scan fields.
    char *bytes = json.detach(nullptr);
    if (bytes == nullptr) {
        return json;
    }
    size_t len = strlen(bytes);
    if (len > 0 && bytes[len - 1] == '}') {
        bytes[len - 1] = '\0';
    }
    PsramJsonBuffer out;
    out += bytes;
    free(bytes);
    out += ",\"scanActive\":";
    out += scanActiveFlag ? "true" : "false";
    out += ",\"scanProgressPercent\":";
    out += String(scanProgressPercent());
    out += ",\"scanHasCandidate\":";
    out += scanCandidatePending ? "true" : "false";
    if (scanCandidatePending) {
        char macText[18];
        snprintf(
            macText,
            sizeof(macText),
            "%02x:%02x:%02x:%02x:%02x:%02x",
            scanCandidate.mac[0],
            scanCandidate.mac[1],
            scanCandidate.mac[2],
            scanCandidate.mac[3],
            scanCandidate.mac[4],
            scanCandidate.mac[5]
        );
        out += ",\"scanCandidate\":{\"ip\":\"";
        out += scanCandidate.ip;
        out += "\",\"name\":\"";
        out += scanCandidate.name;
        out += "\",\"mac\":\"";
        out += macText;
        out += "\",\"hostVersion\":\"";
        out += scanCandidate.hostVersion;
        out += "\",\"port\":";
        out += String(scanPortValue);
        out += "}";
    } else {
        out += ",\"scanCandidate\":null";
    }
    out += ",\"queueUsed\":";
    out += String(jobCount);
    out += ",\"queueDepth\":";
    out += String(INTERCONNECT_QUEUE_DEPTH);
    out += ",\"rePairStatus\":\"";
    out += rePairStatusText[0] != '\0' ? rePairStatusText : "idle";
    out += "\"}";
    return out;
}

void RouterInterconnect::setRePairStatus(const char *status) {
    if (status == nullptr) {
        rePairStatusText[0] = '\0';
        return;
    }
    strncpy(rePairStatusText, status, sizeof(rePairStatusText) - 1);
    rePairStatusText[sizeof(rePairStatusText) - 1] = '\0';
    noteLiveUiChanged();
}

void RouterInterconnect::clearRePairState() {
    rePairPhase = RePairPhaseIdle;
    memset(rePairExpectedMac, 0, sizeof(rePairExpectedMac));
    rePairKeepRouterId = 0;
    rePairKeepChannel = 0;
    scanMatchMacOnly = false;
    scanAutoPairOnMatch = false;
    memset(scanExpectedMac, 0, sizeof(scanExpectedMac));
    scanReplaceRouterId = 0;
}

bool RouterInterconnect::enqueueJob(const InterconnectJob &job) {
    if (jobQueue == nullptr) {
        return false;
    }
    if (queueMutex != nullptr
        && xSemaphoreTake(queueMutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        LOGGER.warning("Interconnect enqueue mutex timeout");
        return false;
    }
    bool queued = false;
    if (jobCount >= INTERCONNECT_QUEUE_DEPTH) {
        LOGGER.warning("Interconnect queue full; rejecting new job");
    } else {
        for (int i = 0; i < INTERCONNECT_QUEUE_DEPTH; i++) {
            if (!jobQueue[i].used) {
                jobQueue[i] = job;
                jobQueue[i].used = true;
                jobCount++;
                queued = true;
                break;
            }
        }
    }
    if (queueMutex != nullptr) {
        xSemaphoreGive(queueMutex);
    }
    return queued;
}

bool RouterInterconnect::mainAllowsOutboundType(InterconnectMsgType type) const {
    return type == InterconnectMsgCheckAlive || type == InterconnectMsgVariable;
}

bool RouterInterconnect::peerAllowsOutboundType(
    const InterconnectPeer *peer,
    InterconnectMsgType type
) const {
    if (peer == nullptr) {
        return false;
    }
    if (peer->online) {
        return true;
    }
    // Offline Repeater: keep checkAlive/variable so recovery and settings can resume.
    return mainAllowsOutboundType(type);
}

bool RouterInterconnect::enqueueExchange(
    InterconnectMsgType type,
    const uint8_t peerMac[6],
    const char *peerIp,
    uint16_t peerPort,
    const uint8_t *payload,
    uint16_t payloadLen
) {
    if (peerMac == nullptr || peerIp == nullptr || peerIp[0] == '\0' || peerPort == 0) {
        return false;
    }
    if (payloadLen > INTERCONNECT_AEAD_MAX_PAYLOAD) {
        return false;
    }
    if (payloadLen > 0 && payload == nullptr) {
        return false;
    }
    if (interconnectStore != nullptr
        && interconnectStore->settings().role == InterconnectRoleMain) {
        const InterconnectPeer *peer = interconnectStore->findByMac(peerMac);
        if (peer != nullptr && !peerAllowsOutboundType(peer, type)) {
            LOGGER.warning("Interconnect ignore outbound to offline Repeater");
            return false;
        }
    }
    InterconnectJob job{};
    job.kind = InterconnectJobExchange;
    job.type = type;
    memcpy(job.peerMac, peerMac, 6);
    strncpy(job.peerIp, peerIp, sizeof(job.peerIp) - 1);
    job.peerPort = peerPort;
    job.payloadLen = payloadLen;
    if (payloadLen > 0) {
        memcpy(job.payload, payload, payloadLen);
    }
    return enqueueJob(job);
}

bool RouterInterconnect::startPairingWait() {
    if (interconnectStore == nullptr) {
        return false;
    }
    pairingWaitActiveFlag = true;
    pairingWaitDeadlineMs = millis() + (INTERCONNECT_PAIR_WAIT_SEC * 1000UL);
    STATUS_RGB.setInterconnectPairingBlink(true);
    return true;
}

void RouterInterconnect::cancelPairingWait() {
    pairingWaitActiveFlag = false;
    pairingWaitDeadlineMs = 0;
    STATUS_RGB.setInterconnectPairingBlink(false);
}

uint32_t RouterInterconnect::pairingWaitRemainingMs() const {
    if (!pairingWaitActiveFlag) {
        return 0;
    }
    const int32_t left = (int32_t)(pairingWaitDeadlineMs - millis());
    return left > 0 ? (uint32_t)left : 0;
}

bool RouterInterconnect::startPairTo(const char *ip, uint16_t port) {
    if (ip == nullptr || ip[0] == '\0' || port == 0 || interconnectStore == nullptr) {
        return false;
    }
    InterconnectJob job{};
    job.kind = InterconnectJobPairConnect;
    strncpy(job.peerIp, ip, sizeof(job.peerIp) - 1);
    job.peerPort = port;
    if (scanReplaceRouterId != 0) {
        // Encode replace target in job.peerMac as all-zero; replace id kept in scanReplaceRouterId.
        memset(job.peerMac, 0, sizeof(job.peerMac));
    }
    return enqueueJob(job);
}

bool RouterInterconnect::startRePair(const uint8_t expectedMac[6]) {
    if (expectedMac == nullptr || interconnectStore == nullptr) {
        return false;
    }
    if (interconnectStore->settings().role != InterconnectRoleMain) {
        return false;
    }
    InterconnectPeer *peer = interconnectStore->findByMac(expectedMac);
    if (peer == nullptr || !peer->used) {
        return false;
    }
    if (scanActiveFlag || scanCandidatePending || rePairPhase != RePairPhaseIdle) {
        return false;
    }
    clearRePairState();
    memcpy(rePairExpectedMac, expectedMac, 6);
    rePairKeepRouterId = peer->routerId;
    rePairKeepChannel = peer->zigbeeChannel;
    rePairPhase = RePairPhaseTryIp;
    setRePairStatus("trying");
    if (peer->ip[0] == '\0' || peer->port == 0) {
        rePairPhase = RePairPhaseScanMac;
        setRePairStatus("scanning");
        return startSubnetScanForMac(expectedMac, interconnectStore->settings().port);
    }
    InterconnectJob job{};
    job.kind = InterconnectJobPairConnect;
    strncpy(job.peerIp, peer->ip, sizeof(job.peerIp) - 1);
    job.peerPort = peer->port;
    memcpy(job.peerMac, expectedMac, 6);
    if (!enqueueJob(job)) {
        clearRePairState();
        setRePairStatus("failed");
        return false;
    }
    LOGGER.info("Interconnect re-pair trying stored IP " + String(peer->ip));
    return true;
}

bool RouterInterconnect::startPairReplace(const char *ip, uint16_t port, uint32_t keepRouterId) {
    if (interconnectStore == nullptr || keepRouterId == 0 || port == 0) {
        return false;
    }
    if (interconnectStore->settings().role != InterconnectRoleMain) {
        return false;
    }
    InterconnectPeer *peer = interconnectStore->findByRouterId(keepRouterId);
    if (peer == nullptr || !peer->used) {
        return false;
    }
    if (scanActiveFlag || scanCandidatePending || rePairPhase != RePairPhaseIdle) {
        return false;
    }
    clearRePairState();
    scanReplaceRouterId = keepRouterId;
    rePairKeepRouterId = keepRouterId;
    rePairKeepChannel = peer->zigbeeChannel;
    setRePairStatus("pairing");
    if (ip == nullptr || ip[0] == '\0') {
        return startSubnetScan(port);
    }
    return startPairTo(ip, port);
}

bool RouterInterconnect::startSubnetScan(uint16_t port) {
    if (interconnectStore == nullptr || port == 0) {
        return false;
    }
    if (interconnectStore->settings().role != InterconnectRoleMain) {
        return false;
    }
    if (scanActiveFlag || scanCandidatePending) {
        return false;
    }
    IPAddress local = WiFi.localIP();
    if (local == IPAddress((uint32_t)0)) {
        return false;
    }
    scanBase[0] = local[0];
    scanBase[1] = local[1];
    scanBase[2] = local[2];
    scanSelfHost = local[3];
    scanHost = 1;
    scanTried = 0;
    scanPortValue = port;
    scanCandidatePending = false;
    scanProbeInFlight = false;
    scanMatchMacOnly = false;
    scanAutoPairOnMatch = false;
    memset(scanExpectedMac, 0, sizeof(scanExpectedMac));
    memset(&scanCandidate, 0, sizeof(scanCandidate));
    scanActiveFlag = true;
    LOGGER.info("Interconnect subnet scan started on port " + String(port));
    return true;
}

bool RouterInterconnect::startSubnetScanForMac(const uint8_t expectedMac[6], uint16_t port) {
    if (expectedMac == nullptr || port == 0) {
        return false;
    }
    if (!startSubnetScan(port)) {
        return false;
    }
    memcpy(scanExpectedMac, expectedMac, 6);
    scanMatchMacOnly = true;
    scanAutoPairOnMatch = true;
    rePairPhase = RePairPhaseScanMac;
    setRePairStatus("scanning");
    LOGGER.info("Interconnect MAC-targeted re-pair scan started");
    return true;
}

void RouterInterconnect::cancelSubnetScan() {
    scanActiveFlag = false;
    scanCandidatePending = false;
    scanProbeInFlight = false;
    memset(&scanCandidate, 0, sizeof(scanCandidate));
    scanTried = 0;
    scanMatchMacOnly = false;
    scanAutoPairOnMatch = false;
    memset(scanExpectedMac, 0, sizeof(scanExpectedMac));
    if (rePairPhase != RePairPhaseIdle && strcmp(rePairStatusText, "ok") != 0) {
        setRePairStatus("failed");
        clearRePairState();
    }
    scanReplaceRouterId = 0;
}

uint8_t RouterInterconnect::scanProgressPercent() const {
    if (!scanActiveFlag && !scanCandidatePending) {
        return scanTried >= 254 ? 100 : 0;
    }
    const uint8_t tried = scanTried > 254 ? 254 : scanTried;
    return (uint8_t)((tried * 100u) / 254u);
}

bool RouterInterconnect::acceptScanCandidate() {
    if (!scanCandidatePending || scanCandidate.ip[0] == '\0') {
        return false;
    }
    const bool queued = startPairTo(scanCandidate.ip, scanPortValue);
    scanCandidatePending = false;
    memset(&scanCandidate, 0, sizeof(scanCandidate));
    if (queued) {
        // Continue scanning remaining hosts after this pair queues (normal Pair only).
        if (scanReplaceRouterId == 0 && rePairPhase == RePairPhaseIdle) {
            scanActiveFlag = true;
        }
    }
    return queued;
}

void RouterInterconnect::rejectScanCandidate() {
    scanCandidatePending = false;
    memset(&scanCandidate, 0, sizeof(scanCandidate));
    scanActiveFlag = true;
}

bool RouterInterconnect::writeLocalIdentityReply(WiFiClient &client) {
    if (interconnectStore == nullptr) {
        return false;
    }
    uint8_t localMac[6];
    if (!localWifiMac(localMac)) {
        return false;
    }
    const char *localName = interconnectStore->settings().routerName;
    const uint8_t nameLen = (uint8_t)strnlen(localName, INTERCONNECT_NAME_MAX);
    const char *version = FIRMWARE_VERSION_HOST;
    const uint8_t versionLen = (uint8_t)strnlen(version, INTERCONNECT_FW_VERSION_MAX);
    return writeAll(client, &nameLen, 1)
        && writeAll(client, (const uint8_t *)localName, nameLen)
        && writeAll(client, localMac, 6)
        && writeAll(client, &versionLen, 1)
        && writeAll(client, (const uint8_t *)version, versionLen);
}

bool RouterInterconnect::readPeerIdentityReply(
    WiFiClient &client,
    char *nameOut,
    size_t nameMax,
    uint8_t macOut[6],
    char *versionOut,
    size_t versionMax
) {
    if (nameOut == nullptr || macOut == nullptr || nameMax == 0) {
        return false;
    }
    uint8_t peerNameLen = 0;
    if (!readExact(client, &peerNameLen, 1, kIoTimeoutMs) || peerNameLen == 0
        || peerNameLen > INTERCONNECT_NAME_MAX || peerNameLen >= nameMax) {
        return false;
    }
    memset(nameOut, 0, nameMax);
    if (!readExact(client, (uint8_t *)nameOut, peerNameLen, kIoTimeoutMs)) {
        return false;
    }
    if (!readExact(client, macOut, 6, kIoTimeoutMs)) {
        return false;
    }
    uint8_t versionLen = 0;
    if (!readExact(client, &versionLen, 1, kIoTimeoutMs)
        || versionLen > INTERCONNECT_FW_VERSION_MAX) {
        return false;
    }
    if (versionOut != nullptr && versionMax > 0) {
        memset(versionOut, 0, versionMax);
        if (versionLen >= versionMax) {
            return false;
        }
        if (versionLen > 0 && !readExact(client, (uint8_t *)versionOut, versionLen, kIoTimeoutMs)) {
            return false;
        }
    } else if (versionLen > 0) {
        uint8_t skip[INTERCONNECT_FW_VERSION_MAX];
        if (!readExact(client, skip, versionLen, kIoTimeoutMs)) {
            return false;
        }
    }
    return true;
}

bool RouterInterconnect::discoverIdentityAsInitiator(
    const char *ip,
    uint16_t port,
    char *nameOut,
    size_t nameMax,
    uint8_t macOut[6],
    char *versionOut,
    size_t versionMax
) {
    if (ip == nullptr || nameOut == nullptr || macOut == nullptr || nameMax == 0 || port == 0) {
        return false;
    }
    WiFiClient client;
    if (!client.connect(ip, port, 250)) {
        return false;
    }
    client.setTimeout(1);
    if (!handshakeAsInitiator(client)) {
        client.stop();
        return false;
    }
    if (!writeAll(client, (const uint8_t *)"ask", 3)) {
        client.stop();
        return false;
    }
    const bool ok = readPeerIdentityReply(
        client,
        nameOut,
        nameMax,
        macOut,
        versionOut,
        versionMax
    );
    client.stop();
    return ok;
}

bool RouterInterconnect::enqueueScanProbe(const char *ip, uint16_t port) {
    if (ip == nullptr || ip[0] == '\0' || port == 0) {
        return false;
    }
    InterconnectJob job{};
    job.kind = InterconnectJobScanProbe;
    strncpy(job.peerIp, ip, sizeof(job.peerIp) - 1);
    job.peerPort = port;
    return enqueueJob(job);
}

void RouterInterconnect::runScanProbeJob(const InterconnectJob &job) {
    scanProbeInFlight = false;
    if (!scanActiveFlag || scanCandidatePending || interconnectStore == nullptr) {
        return;
    }
    char name[INTERCONNECT_NAME_MAX + 1];
    char version[INTERCONNECT_FW_VERSION_MAX + 1];
    uint8_t mac[6];
    if (!discoverIdentityAsInitiator(
            job.peerIp,
            job.peerPort,
            name,
            sizeof(name),
            mac,
            version,
            sizeof(version)
        )) {
        return;
    }
    if (!scanActiveFlag || scanCandidatePending) {
        return;
    }
    if (scanMatchMacOnly) {
        if (memcmp(mac, scanExpectedMac, 6) != 0) {
            return;
        }
        LOGGER.info(String("Interconnect re-pair scan matched MAC @") + job.peerIp);
        scanActiveFlag = false;
        InterconnectJob pairJob{};
        pairJob.kind = InterconnectJobPairConnect;
        strncpy(pairJob.peerIp, job.peerIp, sizeof(pairJob.peerIp) - 1);
        pairJob.peerPort = job.peerPort;
        memcpy(pairJob.peerMac, scanExpectedMac, 6);
        if (!enqueueJob(pairJob)) {
            setRePairStatus("failed");
            clearRePairState();
            LOGGER.warning("Interconnect re-pair queue full after MAC match");
        } else {
            setRePairStatus("pairing");
        }
        return;
    }
    strncpy(scanCandidate.ip, job.peerIp, sizeof(scanCandidate.ip) - 1);
    strncpy(scanCandidate.name, name, INTERCONNECT_NAME_MAX);
    strncpy(scanCandidate.hostVersion, version, INTERCONNECT_FW_VERSION_MAX);
    scanCandidate.hostVersion[INTERCONNECT_FW_VERSION_MAX] = '\0';
    memcpy(scanCandidate.mac, mac, 6);
    scanCandidatePending = true;
    scanActiveFlag = false;
    LOGGER.info(String("Interconnect scan found ") + name + " @" + job.peerIp);
}

void RouterInterconnect::pumpSubnetScan() {
    if (!scanActiveFlag || scanCandidatePending || scanProbeInFlight || interconnectStore == nullptr) {
        return;
    }
    while (scanHost <= 254) {
        const uint8_t host = scanHost++;
        scanTried++;
        if (host == 0 || host == scanSelfHost) {
            continue;
        }
        char ipText[16];
        snprintf(
            ipText,
            sizeof(ipText),
            "%u.%u.%u.%u",
            (unsigned)scanBase[0],
            (unsigned)scanBase[1],
            (unsigned)scanBase[2],
            (unsigned)host
        );
        // MAC-targeted re-pair must probe stored peer IP too (device may have moved).
        if (!scanMatchMacOnly && interconnectStore->findByIp(ipText) != nullptr) {
            continue;
        }
        if (!enqueueScanProbe(ipText, scanPortValue)) {
            // Queue full: retry same host next tick.
            scanHost--;
            if (scanTried > 0) {
                scanTried--;
            }
            return;
        }
        scanProbeInFlight = true;
        return;
    }
    scanActiveFlag = false;
    LOGGER.info("Interconnect subnet scan finished");
    if (scanMatchMacOnly || rePairPhase == RePairPhaseScanMac) {
        setRePairStatus("failed");
        clearRePairState();
        LOGGER.warning("Interconnect re-pair failed: MAC not found on subnet");
    }
}

bool RouterInterconnect::aeadSeal(
    const uint8_t secret[INTERCONNECT_SECRET_LEN],
    const uint8_t peerMac[6],
    uint8_t type,
    const uint8_t *payload,
    uint16_t payloadLen,
    uint8_t *outFrame,
    size_t outMax,
    size_t *outLen
) const {
    if (secret == nullptr || peerMac == nullptr || outFrame == nullptr || outLen == nullptr) {
        return false;
    }
    if (payloadLen > INTERCONNECT_AEAD_MAX_PAYLOAD) {
        return false;
    }
    if (payloadLen > 0 && payload == nullptr) {
        return false;
    }
    const size_t need = kHeaderLen + payloadLen + kTagLen;
    if (outMax < need) {
        return false;
    }
    uint8_t nonce[kNonceLen];
    uint64_t counter = nonceCounter++;
    memset(nonce, 0, sizeof(nonce));
    memcpy(nonce, &counter, sizeof(counter));
    esp_fill_random(nonce + 8, 4);

    outFrame[0] = INTERCONNECT_PROTOCOL_VERSION;
    outFrame[1] = type;
    u16Be(outFrame + 2, payloadLen);
    memcpy(outFrame + 4, nonce, kNonceLen);

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
    uint8_t *cipher = outFrame + kHeaderLen;
    uint8_t *tag = cipher + payloadLen;
    rc = mbedtls_gcm_crypt_and_tag(
        &gcm,
        MBEDTLS_GCM_ENCRYPT,
        payloadLen,
        nonce,
        kNonceLen,
        aad,
        sizeof(aad),
        payload == nullptr ? (const uint8_t *)"" : payload,
        cipher,
        kTagLen,
        tag
    );
    mbedtls_gcm_free(&gcm);
    if (rc != 0) {
        return false;
    }
    *outLen = need;
    return true;
}

bool RouterInterconnect::aeadOpen(
    const uint8_t secret[INTERCONNECT_SECRET_LEN],
    const uint8_t peerMac[6],
    const uint8_t *frame,
    size_t frameLen,
    uint8_t *typeOut,
    uint8_t *payloadOut,
    uint16_t payloadMax,
    uint16_t *payloadLenOut
) const {
    if (secret == nullptr || peerMac == nullptr || frame == nullptr || typeOut == nullptr
        || payloadLenOut == nullptr) {
        return false;
    }
    if (frameLen < kHeaderLen + kTagLen) {
        return false;
    }
    if (frame[0] != INTERCONNECT_PROTOCOL_VERSION) {
        return false;
    }
    const uint16_t payloadLen = u16FromBe(frame + 2);
    if (payloadLen > INTERCONNECT_AEAD_MAX_PAYLOAD || payloadLen > payloadMax) {
        return false;
    }
    if (frameLen < kHeaderLen + payloadLen + kTagLen) {
        return false;
    }
    const uint8_t *nonce = frame + 4;
    const uint8_t *cipher = frame + kHeaderLen;
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
        kNonceLen,
        aad,
        sizeof(aad),
        tag,
        kTagLen,
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

bool RouterInterconnect::readExact(WiFiClient &client, uint8_t *buf, size_t len, uint32_t timeoutMs) {
    size_t got = 0;
    const uint32_t startMs = millis();
    while (got < len) {
        if (!client.connected()) {
            return false;
        }
        const int available = client.available();
        if (available > 0) {
            const int n = client.read(buf + got, len - got);
            if (n <= 0) {
                return false;
            }
            got += (size_t)n;
            continue;
        }
        if ((uint32_t)(millis() - startMs) > timeoutMs) {
            return false;
        }
        delay(1);
    }
    return true;
}

bool RouterInterconnect::writeAll(WiFiClient &client, const uint8_t *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        const size_t n = client.write(buf + sent, len - sent);
        if (n == 0) {
            return false;
        }
        sent += n;
    }
    return true;
}

bool RouterInterconnect::handshakeAsInitiator(WiFiClient &client) {
    const char *hello = "ImHere";
    const char *expect = "copy";
    if (!writeAll(client, (const uint8_t *)hello, 6)) {
        return false;
    }
    uint8_t reply[4];
    if (!readExact(client, reply, 4, kIoTimeoutMs)) {
        return false;
    }
    return memcmp(reply, expect, 4) == 0;
}

bool RouterInterconnect::handshakeAsListener(WiFiClient &client) {
    uint8_t hello[6];
    if (!readExact(client, hello, 6, kIoTimeoutMs)) {
        return false;
    }
    if (memcmp(hello, "ImHere", 6) != 0) {
        return false;
    }
    return writeAll(client, (const uint8_t *)"copy", 4);
}

bool RouterInterconnect::runUniversalExchangeAsClient(
    const char *ip,
    uint16_t port,
    const uint8_t secret[INTERCONNECT_SECRET_LEN],
    const uint8_t peerMac[6],
    InterconnectMsgType type,
    const uint8_t *payload,
    uint16_t payloadLen
) {
    WiFiClient client;
    if (!client.connect(ip, port, kIoTimeoutMs)) {
        return false;
    }
    client.setTimeout(kIoTimeoutMs / 1000);
    if (!handshakeAsInitiator(client)) {
        client.stop();
        return false;
    }
    uint8_t frame[kHeaderLen + INTERCONNECT_AEAD_MAX_PAYLOAD + kTagLen];
    size_t frameLen = 0;
    if (!aeadSeal(secret, peerMac, (uint8_t)type, payload, payloadLen, frame, sizeof(frame), &frameLen)) {
        client.stop();
        return false;
    }
    if (!writeAll(client, frame, frameLen)) {
        client.stop();
        return false;
    }
    uint8_t ok[2];
    const bool gotOk = readExact(client, ok, 2, kIoTimeoutMs) && ok[0] == 'O' && ok[1] == 'K';
    client.stop();
    return gotOk;
}

void RouterInterconnect::pushVariablesToPeer(
    const uint8_t peerMac[6],
    const char *peerIp,
    uint16_t peerPort
) {
    if (peerMac == nullptr || peerIp == nullptr || peerPort == 0 || interconnectStore == nullptr) {
        return;
    }
    InterconnectPeer *peer = interconnectStore->findByMac(peerMac);
    const uint8_t channel =
        peer != nullptr && peer->zigbeeChannel >= 11 && peer->zigbeeChannel <= 26
            ? peer->zigbeeChannel
            : (localZigbeeChannelProvider != nullptr ? localZigbeeChannelProvider() : (uint8_t)11);
    char channelText[48];
    snprintf(channelText, sizeof(channelText), "zigbeeChannel=%u", (unsigned)channel);
    enqueueExchange(
        InterconnectMsgVariable,
        peerMac,
        peerIp,
        peerPort,
        (const uint8_t *)channelText,
        (uint16_t)strlen(channelText)
    );
    char aliveText[48];
    snprintf(
        aliveText,
        sizeof(aliveText),
        "checkAliveSec=%u",
        (unsigned)interconnectStore->settings().checkAliveSec
    );
    enqueueExchange(
        InterconnectMsgVariable,
        peerMac,
        peerIp,
        peerPort,
        (const uint8_t *)aliveText,
        (uint16_t)strlen(aliveText)
    );
    const uint8_t permitJoinSec = localPermitJoinSecProvider != nullptr
        ? localPermitJoinSecProvider()
        : (uint8_t)DEFAULT_PERMIT_JOIN_SEC;
    char permitText[48];
    snprintf(
        permitText,
        sizeof(permitText),
        "permitJoinOnBootSec=%u",
        (unsigned)permitJoinSec
    );
    enqueueExchange(
        InterconnectMsgVariable,
        peerMac,
        peerIp,
        peerPort,
        (const uint8_t *)permitText,
        (uint16_t)strlen(permitText)
    );
}

void RouterInterconnect::pushSharedVariablesToAllPeers() {
    if (interconnectStore == nullptr
        || interconnectStore->settings().role != InterconnectRoleMain) {
        return;
    }
    for (int peerIndex = 0; peerIndex < INTERCONNECT_MAX_REPEATERS; peerIndex++) {
        InterconnectPeer *peer = interconnectStore->peerAt(peerIndex);
        if (peer == nullptr || !peer->used || !peer->hasSecret) {
            continue;
        }
        if (peer->ip[0] == '\0' || peer->port == 0) {
            continue;
        }
        pushVariablesToPeer(peer->mac, peer->ip, peer->port);
    }
}

void RouterInterconnect::onJustBootedFromPeer(
    const uint8_t peerMac[6],
    const uint8_t *payload,
    uint16_t len
) {
    if (peerMac == nullptr || interconnectStore == nullptr) {
        return;
    }
    if (interconnectStore->settings().role != InterconnectRoleMain) {
        return;
    }
    InterconnectPeer *peer = interconnectStore->findByMac(peerMac);
    if (peer == nullptr) {
        return;
    }
    peer->online = true;
    peer->lastAliveMs = millis();
    if (payload != nullptr && len > 0) {
        const size_t copyLen =
            len > INTERCONNECT_FW_VERSION_MAX ? INTERCONNECT_FW_VERSION_MAX : (size_t)len;
        memcpy(peer->hostVersion, payload, copyLen);
        peer->hostVersion[copyLen] = '\0';
        interconnectStore->requestPersist();
    }
    noteLiveUiChanged();
    pushVariablesToPeer(peer->mac, peer->ip, peer->port);
    // Request VERSION via ask after Repeater reboot (table column).
    if (peer->ip[0] != '\0' && peer->port != 0) {
        enqueueVersionProbe(peer->ip, peer->port, peer->mac);
    }
}

bool RouterInterconnect::enqueueVersionProbe(
    const char *ip,
    uint16_t port,
    const uint8_t expectedMac[6]
) {
    if (ip == nullptr || ip[0] == '\0' || port == 0) {
        return false;
    }
    InterconnectJob job{};
    job.kind = InterconnectJobVersionProbe;
    strncpy(job.peerIp, ip, sizeof(job.peerIp) - 1);
    job.peerPort = port;
    if (expectedMac != nullptr) {
        memcpy(job.peerMac, expectedMac, 6);
    }
    return enqueueJob(job);
}

void RouterInterconnect::requestVersionsFromAllPeers() {
    if (interconnectStore == nullptr
        || interconnectStore->settings().role != InterconnectRoleMain) {
        return;
    }
    int queued = 0;
    for (int peerIndex = 0; peerIndex < INTERCONNECT_MAX_REPEATERS; peerIndex++) {
        InterconnectPeer *peer = interconnectStore->peerAt(peerIndex);
        if (peer == nullptr || !peer->used || peer->ip[0] == '\0' || peer->port == 0) {
            continue;
        }
        if (enqueueVersionProbe(peer->ip, peer->port, peer->mac)) {
            queued++;
        }
    }
    if (queued > 0) {
        LOGGER.info(
            "Interconnect Main: requesting VERSION from " + String(queued) + " Repeater(s)"
        );
    }
}

void RouterInterconnect::runVersionProbeJob(const InterconnectJob &job) {
    if (interconnectStore == nullptr
        || interconnectStore->settings().role != InterconnectRoleMain) {
        return;
    }
    char name[INTERCONNECT_NAME_MAX + 1];
    char version[INTERCONNECT_FW_VERSION_MAX + 1];
    uint8_t mac[6];
    if (!discoverIdentityAsInitiator(
            job.peerIp,
            job.peerPort,
            name,
            sizeof(name),
            mac,
            version,
            sizeof(version)
        )) {
        LOGGER.warning(
            String("Interconnect VERSION probe failed @") + job.peerIp + ":" + String(job.peerPort)
        );
        return;
    }
    InterconnectPeer *peer = interconnectStore->findByMac(mac);
    if (peer == nullptr) {
        // Prefer MAC from ask reply; fall back to job's expected MAC.
        bool macZero = true;
        for (int i = 0; i < 6; i++) {
            if (job.peerMac[i] != 0) {
                macZero = false;
                break;
            }
        }
        if (!macZero) {
            peer = interconnectStore->findByMac(job.peerMac);
        }
    }
    if (peer == nullptr) {
        LOGGER.warning(
            String("Interconnect VERSION probe: no paired peer for ") + name + " @" + job.peerIp
        );
        return;
    }
    strncpy(peer->hostVersion, version, INTERCONNECT_FW_VERSION_MAX);
    peer->hostVersion[INTERCONNECT_FW_VERSION_MAX] = '\0';
    peer->online = true;
    peer->lastAliveMs = millis();
    interconnectStore->requestPersist();
    noteLiveUiChanged();
    LOGGER.info(
        String("Interconnect VERSION ") + peer->name + "=" + peer->hostVersion
    );
}

void RouterInterconnect::maybePushSharedOnMainBoot() {
    if (mainBootPushDone || interconnectStore == nullptr) {
        return;
    }
    if (interconnectStore->settings().role != InterconnectRoleMain) {
        mainBootPushDone = true;
        return;
    }
    if (WiFi.localIP() == IPAddress((uint32_t)0)) {
        return;
    }
    if (interconnectStore->peerCount() > 0) {
        pushSharedVariablesToAllPeers();
        requestVersionsFromAllPeers();
        LOGGER.info("Interconnect Main boot: pushed shared settings to paired Repeaters");
    }
    mainBootPushDone = true;
}

void RouterInterconnect::handleExchangePayload(
    uint8_t type,
    const uint8_t *payload,
    uint16_t len,
    const uint8_t peerMac[6]
) {
    if (type == InterconnectMsgCheckAlive) {
        return;
    }
    if (type == InterconnectMsgJustBooted) {
        onJustBootedFromPeer(peerMac, payload, len);
        return;
    }
    if (type == InterconnectMsgVariable && variableApplyHandler != nullptr && payload != nullptr && len > 0) {
        char text[INTERCONNECT_AEAD_MAX_PAYLOAD + 1];
        memcpy(text, payload, len);
        text[len] = '\0';
        char *eq = strchr(text, '=');
        if (eq != nullptr) {
            *eq = '\0';
            variableApplyHandler(text, eq + 1);
        }
        return;
    }
    if (type == InterconnectMsgZigbeePacket && zigbeeInjectHandler != nullptr && payload != nullptr
        && len >= 1) {
        SpiFrame frame{};
        frame.cmd = payload[0];
        frame.length = (uint16_t)(len - 1);
        if (frame.length > SPI_MAX_PAYLOAD) {
            return;
        }
        if (frame.length > 0) {
            memcpy(frame.payload, payload + 1, frame.length);
        }
        uint32_t routerId = 0;
        InterconnectPeer *peer = interconnectStore->findByMac(peerMac);
        if (peer != nullptr) {
            routerId = peer->routerId;
            peer->packetsIn++;
            peer->lastAliveMs = millis();
            peer->online = true;
            noteLiveUiChanged();
        }
        zigbeeInjectHandler(frame, routerId);
        return;
    }
    if (type == InterconnectMsgSearchStart) {
        uint8_t seconds = DEFAULT_PERMIT_JOIN_SEC;
        if (payload != nullptr && len >= 1 && payload[0] > 0) {
            seconds = payload[0];
        }
        ZIGBEE_SPI_PROXY.permitJoin(seconds);
        return;
    }
    if (type == InterconnectMsgSearchStop) {
        ZIGBEE_SPI_PROXY.closeJoin();
        return;
    }
    if (type == InterconnectMsgRebind) {
        handleRebindRequest(payload, len);
        return;
    }
    if (type == InterconnectMsgRebindResult) {
        handleRebindResult(payload, len);
        return;
    }
}

void RouterInterconnect::handleRebindRequest(const uint8_t *payload, uint16_t len) {
    if (payload == nullptr || len < 8 || interconnectStore == nullptr) {
        return;
    }
    if (interconnectStore->settings().role != InterconnectRoleRepeater
        || !interconnectStore->settings().hasMain) {
        return;
    }
    uint8_t outcome = SPI_REBIND_OUTCOME_FAILED;
    ZIGBEE_SPI_PROXY.rebindDevice(payload, &outcome, INTERCONNECT_REBIND_WAIT_MS);
    uint8_t result[9];
    memcpy(result, payload, 8);
    result[8] = outcome;
    enqueueExchange(
        InterconnectMsgRebindResult,
        interconnectStore->settings().mainMac,
        interconnectStore->settings().mainIp,
        interconnectStore->settings().mainPort,
        result,
        9
    );
}

void RouterInterconnect::handleRebindResult(const uint8_t *payload, uint16_t len) {
    if (payload == nullptr || len < 9 || !rebindWaitActive) {
        return;
    }
    if (memcmp(payload, rebindWaitIeee, 8) != 0) {
        return;
    }
    rebindWaitOutcome = payload[8];
    rebindWaitDone = true;
}

bool RouterInterconnect::rebindRemote(
    uint32_t routerId,
    const uint8_t ieee[8],
    uint8_t *outcomeOut,
    uint32_t timeoutMs
) {
    if (outcomeOut == nullptr || ieee == nullptr || interconnectStore == nullptr) {
        return false;
    }
    InterconnectPeer *peer = interconnectStore->findByRouterId(routerId);
    if (peer == nullptr || !peer->used) {
        *outcomeOut = SPI_REBIND_OUTCOME_FAILED;
        return false;
    }
    if (!peer->online) {
        LOGGER.warning("Remote rebind blocked: Repeater offline");
        *outcomeOut = SPI_REBIND_OUTCOME_FAILED;
        return false;
    }
    rebindWaitActive = true;
    rebindWaitDone = false;
    memcpy(rebindWaitIeee, ieee, 8);
    rebindWaitOutcome = SPI_REBIND_OUTCOME_FAILED;
    rebindWaitRouterId = routerId;
    if (!enqueueExchange(
            InterconnectMsgRebind,
            peer->mac,
            peer->ip,
            peer->port,
            ieee,
            8
        )) {
        rebindWaitActive = false;
        *outcomeOut = SPI_REBIND_OUTCOME_FAILED;
        return false;
    }
    // Rebind command is queued for the async outbound worker; wait only for result.
    const uint32_t startedMs = millis();
    while (!rebindWaitDone && (uint32_t)(millis() - startedMs) < timeoutMs) {
        acceptClients();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    rebindWaitActive = false;
    *outcomeOut = rebindWaitOutcome;
    return rebindWaitDone;
}

void RouterInterconnect::handleAcceptedClient(WiFiClient &client) {
    if (!handshakeAsListener(client)) {
        client.stop();
        return;
    }
    // Peek: pairing cleartext vs AEAD version byte.
    uint32_t waitStart = millis();
    while (client.available() < 1 && (uint32_t)(millis() - waitStart) < kIoTimeoutMs) {
        delay(1);
    }
    if (client.available() < 1) {
        client.stop();
        return;
    }
    const int first = client.peek();
    if (first == INTERCONNECT_PROTOCOL_VERSION) {
        uint8_t header[kHeaderLen];
        if (!readExact(client, header, kHeaderLen, kIoTimeoutMs)) {
            client.stop();
            return;
        }
        const uint16_t payloadLen = u16FromBe(header + 2);
        if (payloadLen > INTERCONNECT_AEAD_MAX_PAYLOAD) {
            client.stop();
            return;
        }
        uint8_t body[INTERCONNECT_AEAD_MAX_PAYLOAD + kTagLen];
        if (!readExact(client, body, payloadLen + kTagLen, kIoTimeoutMs)) {
            client.stop();
            return;
        }
        uint8_t frame[kHeaderLen + INTERCONNECT_AEAD_MAX_PAYLOAD + kTagLen];
        memcpy(frame, header, kHeaderLen);
        memcpy(frame + kHeaderLen, body, payloadLen + kTagLen);
        const size_t frameLen = kHeaderLen + payloadLen + kTagLen;

        uint8_t localMac[6];
        if (!localWifiMac(localMac)) {
            client.stop();
            return;
        }
        // Try Main secret (we are Repeater) or each peer secret (we are Main).
        uint8_t type = 0;
        uint8_t plain[INTERCONNECT_AEAD_MAX_PAYLOAD];
        uint16_t plainLen = 0;
        uint8_t matchedMac[6] = {0};
        bool opened = false;
        if (interconnectStore->settings().role == InterconnectRoleRepeater
            && interconnectStore->settings().hasMain) {
            if (aeadOpen(
                    interconnectStore->settings().mainSecret,
                    localMac,
                    frame,
                    frameLen,
                    &type,
                    plain,
                    sizeof(plain),
                    &plainLen
                )) {
                opened = true;
                memcpy(matchedMac, interconnectStore->settings().mainMac, 6);
                interconnectStore->settingsMutable().mainOnline = true;
                interconnectStore->settingsMutable().mainLastAliveMs = millis();
                noteLiveUiChanged();
            }
        } else {
            for (int peerIndex = 0; peerIndex < INTERCONNECT_MAX_REPEATERS && !opened; peerIndex++) {
                InterconnectPeer *peer = interconnectStore->peerAt(peerIndex);
                if (peer == nullptr || !peer->hasSecret) {
                    continue;
                }
                if (aeadOpen(
                        peer->secret,
                        localMac,
                        frame,
                        frameLen,
                        &type,
                        plain,
                        sizeof(plain),
                        &plainLen
                    )) {
                    opened = true;
                    memcpy(matchedMac, peer->mac, 6);
                    peer->online = true;
                    peer->lastAliveMs = millis();
                    peer->packetsIn++;
                    noteLiveUiChanged();
                }
            }
        }
        if (!opened) {
            client.stop();
            return;
        }
        writeAll(client, (const uint8_t *)"OK", 2);
        client.stop();
        handleExchangePayload(type, plain, plainLen, matchedMac);
        return;
    }

    // Cleartext "ask": identity + FW version.
    if (first == 'a') {
        if (pairingWaitActiveFlag) {
            handlePairingAsListener(client);
            client.stop();
            return;
        }
        // Not pairing: answer ask so Main can refresh Repeater VERSION anytime.
        uint8_t askBuf[3];
        if (readExact(client, askBuf, 3, kIoTimeoutMs) && memcmp(askBuf, "ask", 3) == 0) {
            writeLocalIdentityReply(client);
        }
        client.stop();
        return;
    }

    client.stop();
}

bool RouterInterconnect::handlePairingAsListener(WiFiClient &client) {
    uint8_t askBuf[3];
    if (!readExact(client, askBuf, 3, kIoTimeoutMs) || memcmp(askBuf, "ask", 3) != 0) {
        return false;
    }
    if (!writeLocalIdentityReply(client)) {
        return false;
    }

    uint8_t peerNameLen = 0;
    if (!readExact(client, &peerNameLen, 1, kIoTimeoutMs) || peerNameLen == 0
        || peerNameLen > INTERCONNECT_NAME_MAX) {
        return false;
    }
    char peerName[INTERCONNECT_NAME_MAX + 1];
    memset(peerName, 0, sizeof(peerName));
    if (!readExact(client, (uint8_t *)peerName, peerNameLen, kIoTimeoutMs)) {
        return false;
    }
    uint8_t peerMac[6];
    uint8_t portBe[2];
    uint8_t secret[INTERCONNECT_SECRET_LEN];
    uint8_t routerIdBe[4];
    uint8_t channel = 0;
    if (!readExact(client, peerMac, 6, kIoTimeoutMs) || !readExact(client, portBe, 2, kIoTimeoutMs)
        || !readExact(client, secret, INTERCONNECT_SECRET_LEN, kIoTimeoutMs)
        || !readExact(client, routerIdBe, 4, kIoTimeoutMs) || !readExact(client, &channel, 1, kIoTimeoutMs)) {
        return false;
    }
    (void)routerIdBe;
    const uint16_t peerPort = u16FromBe(portBe);
    char peerIp[16];
    peerIp[0] = '\0';
    IPAddress remote = client.remoteIP();
    if (remote != IPAddress((uint32_t)0)) {
        snprintf(
            peerIp,
            sizeof(peerIp),
            "%u.%u.%u.%u",
            remote[0],
            remote[1],
            remote[2],
            remote[3]
        );
    }
    if (!writeAll(client, (const uint8_t *)"OK", 2)) {
        return false;
    }
    uint8_t finalOk[2];
    if (!readExact(client, finalOk, 2, kIoTimeoutMs) || finalOk[0] != 'O' || finalOk[1] != 'K') {
        return false;
    }
    if (!interconnectStore->setMainPeer(peerName, peerMac, peerIp, peerPort, secret)) {
        return false;
    }
    if (channel >= 11 && channel <= 26 && applyZigbeeChannelHandler != nullptr) {
        applyZigbeeChannelHandler(channel);
    }
    cancelPairingWait();
    LOGGER.info(String("Interconnect paired to Main ") + peerName + " @" + peerIp);
    justBootedSent = false;
    return true;
}

bool RouterInterconnect::runPairingAsInitiator(
    const char *ip,
    uint16_t port,
    const uint8_t *expectedMacOrNull,
    uint32_t replaceRouterIdOrZero
) {
    if (ip == nullptr || port == 0 || interconnectStore == nullptr) {
        return false;
    }
    if (interconnectStore->settings().role != InterconnectRoleMain) {
        return false;
    }

    WiFiClient client;
    if (!client.connect(ip, port, kIoTimeoutMs)) {
        LOGGER.warning(String("Interconnect pair connect failed to ") + ip);
        return false;
    }
    client.setTimeout(kIoTimeoutMs / 1000);
    if (!handshakeAsInitiator(client)) {
        client.stop();
        return false;
    }
    if (!writeAll(client, (const uint8_t *)"ask", 3)) {
        client.stop();
        return false;
    }

    char peerName[INTERCONNECT_NAME_MAX + 1];
    char peerVersion[INTERCONNECT_FW_VERSION_MAX + 1];
    uint8_t peerMac[6];
    if (!readPeerIdentityReply(
            client,
            peerName,
            sizeof(peerName),
            peerMac,
            peerVersion,
            sizeof(peerVersion)
        )) {
        client.stop();
        return false;
    }

    if (expectedMacOrNull != nullptr && memcmp(peerMac, expectedMacOrNull, 6) != 0) {
        LOGGER.warning("Interconnect pair MAC mismatch at " + String(ip));
        client.stop();
        return false;
    }

    const uint32_t replaceRouterId =
        replaceRouterIdOrZero != 0 ? replaceRouterIdOrZero : scanReplaceRouterId;
    InterconnectPeer *existing = interconnectStore->findByMac(peerMac);
    InterconnectPeer *replacePeer =
        replaceRouterId != 0 ? interconnectStore->findByRouterId(replaceRouterId) : nullptr;
    if (replaceRouterId != 0 && replacePeer == nullptr) {
        LOGGER.warning("Interconnect replace target routerId missing");
        client.stop();
        return false;
    }
    if (existing == nullptr && replacePeer == nullptr
        && interconnectStore->peerCount() >= INTERCONNECT_MAX_REPEATERS) {
        LOGGER.warning("Interconnect peer table full");
        client.stop();
        return false;
    }

    uint8_t localMac[6];
    if (!localWifiMac(localMac)) {
        client.stop();
        return false;
    }
    const uint8_t mainChannel =
        localZigbeeChannelProvider != nullptr ? localZigbeeChannelProvider() : (uint8_t)11;
    // Same-MAC re-pair and replace-with-new keep routerId + channel so device binds stay.
    uint32_t routerId = 0;
    uint8_t zigbeeChannel = 0;
    if (existing != nullptr) {
        routerId = existing->routerId;
        zigbeeChannel = existing->zigbeeChannel;
    } else if (replacePeer != nullptr) {
        routerId = replacePeer->routerId;
        zigbeeChannel = replacePeer->zigbeeChannel;
    } else {
        routerId = interconnectStore->allocateRouterId();
        zigbeeChannel = interconnectStore->allocateZigbeeChannel(mainChannel);
    }
    if (routerId == 0 || zigbeeChannel == 0) {
        LOGGER.warning("Interconnect cannot allocate routerId/channel");
        client.stop();
        return false;
    }
    uint8_t secret[INTERCONNECT_SECRET_LEN];
    esp_fill_random(secret, sizeof(secret));

    const char *localName = interconnectStore->settings().routerName;
    const uint8_t nameLen = (uint8_t)strnlen(localName, INTERCONNECT_NAME_MAX);
    const uint16_t localPort = interconnectStore->settings().port;
    uint8_t portBe[2];
    u16Be(portBe, localPort);
    uint8_t routerIdBe[4];
    routerIdBe[0] = (uint8_t)((routerId >> 24) & 0xFF);
    routerIdBe[1] = (uint8_t)((routerId >> 16) & 0xFF);
    routerIdBe[2] = (uint8_t)((routerId >> 8) & 0xFF);
    routerIdBe[3] = (uint8_t)(routerId & 0xFF);

    if (!writeAll(client, &nameLen, 1) || !writeAll(client, (const uint8_t *)localName, nameLen)
        || !writeAll(client, localMac, 6) || !writeAll(client, portBe, 2)
        || !writeAll(client, secret, INTERCONNECT_SECRET_LEN) || !writeAll(client, routerIdBe, 4)
        || !writeAll(client, &zigbeeChannel, 1)) {
        client.stop();
        return false;
    }

    uint8_t ok[2];
    if (!readExact(client, ok, 2, kIoTimeoutMs) || ok[0] != 'O' || ok[1] != 'K') {
        client.stop();
        return false;
    }
    if (!writeAll(client, (const uint8_t *)"OK", 2)) {
        client.stop();
        return false;
    }
    client.stop();

    InterconnectPeer *peer = existing;
    if (peer == nullptr && replacePeer != nullptr) {
        if (!interconnectStore->replacePeerEndpoint(
                routerId,
                peerName,
                peerMac,
                ip,
                port,
                secret
            )) {
            LOGGER.warning("Interconnect replacePeerEndpoint failed after pairing OK");
            return false;
        }
        peer = interconnectStore->findByRouterId(routerId);
    } else if (peer == nullptr) {
        peer = interconnectStore->addPeer(
            peerName,
            peerMac,
            ip,
            port,
            routerId,
            zigbeeChannel,
            secret
        );
        if (peer == nullptr) {
            LOGGER.warning("Interconnect addPeer failed after pairing OK");
            return false;
        }
    } else {
        strncpy(peer->name, peerName, INTERCONNECT_NAME_MAX);
        peer->name[INTERCONNECT_NAME_MAX] = '\0';
        strncpy(peer->ip, ip, sizeof(peer->ip) - 1);
        peer->ip[sizeof(peer->ip) - 1] = '\0';
        peer->port = port;
        memcpy(peer->secret, secret, INTERCONNECT_SECRET_LEN);
        peer->hasSecret = true;
    }
    if (peer == nullptr) {
        return false;
    }
    strncpy(peer->hostVersion, peerVersion, INTERCONNECT_FW_VERSION_MAX);
    peer->hostVersion[INTERCONNECT_FW_VERSION_MAX] = '\0';
    peer->online = true;
    peer->lastAliveMs = millis();
    interconnectStore->requestPersist();
    noteLiveUiChanged();
    const char *label = existing != nullptr
        ? "Interconnect re-paired Repeater "
        : (replacePeer != nullptr ? "Interconnect replaced Repeater " : "Interconnect paired Repeater ");
    LOGGER.info(
        String(label)
        + peerName + " id=" + String((unsigned long)routerId)
        + " ch=" + String(zigbeeChannel)
        + " ver=" + peerVersion
    );
    if (rePairPhase != RePairPhaseIdle || scanReplaceRouterId != 0) {
        setRePairStatus("ok");
        clearRePairState();
        scanReplaceRouterId = 0;
    }
    return true;
}

void RouterInterconnect::acceptClients() {
    if (server == nullptr) {
        return;
    }
    WiFiClient client = server->available();
    if (!client) {
        return;
    }
    client.setTimeout(kIoTimeoutMs / 1000);
    handleAcceptedClient(client);
}

void RouterInterconnect::maybeRefreshMainLiveness() {
    if (interconnectStore == nullptr) {
        return;
    }
    if (interconnectStore->settings().role != InterconnectRoleRepeater
        || !interconnectStore->settings().hasMain) {
        return;
    }
    const uint32_t lastAliveMs = interconnectStore->settings().mainLastAliveMs;
    if (lastAliveMs == 0) {
        return;
    }
    const uint32_t timeoutMs =
        ((uint32_t)interconnectStore->settings().checkAliveSec + INTERCONNECT_MAIN_DEAD_GRACE_SEC)
        * 1000UL;
    if ((uint32_t)(millis() - lastAliveMs) > timeoutMs) {
        if (interconnectStore->settings().mainOnline) {
            interconnectStore->settingsMutable().mainOnline = false;
            noteLiveUiChanged();
            LOGGER.warning("Interconnect Main marked dead; holding outbound queue");
        }
    }
}

void RouterInterconnect::pumpQueue() {
    if (jobQueue == nullptr || interconnectStore == nullptr) {
        return;
    }
    if (queueMutex != nullptr
        && xSemaphoreTake(queueMutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        return;
    }
    const bool repeaterHoldForDeadMain =
        interconnectStore->settings().role == InterconnectRoleRepeater
        && interconnectStore->settings().hasMain
        && !interconnectStore->settings().mainOnline;
    int selected = -1;
    InterconnectJob job{};
    for (int i = 0; i < INTERCONNECT_QUEUE_DEPTH; i++) {
        if (!jobQueue[i].used) {
            continue;
        }
        if (repeaterHoldForDeadMain && jobQueue[i].kind == InterconnectJobExchange) {
            // Keep collecting/holding outbound traffic until Main checkAlive resumes.
            continue;
        }
        job = jobQueue[i];
        if (job.kind == InterconnectJobExchange
            && interconnectStore->settings().role == InterconnectRoleMain) {
            InterconnectPeer *peer = interconnectStore->findByMac(job.peerMac);
            if (peer != nullptr && !peerAllowsOutboundType(peer, job.type)) {
                jobQueue[i].used = false;
                jobCount--;
                if (queueMutex != nullptr) {
                    xSemaphoreGive(queueMutex);
                }
                return;
            }
        }
        jobQueue[i].used = false;
        jobCount--;
        selected = i;
        break;
    }
    if (queueMutex != nullptr) {
        xSemaphoreGive(queueMutex);
    }
    if (selected < 0) {
        return;
    }

    if (job.kind == InterconnectJobExchange) {
        uint8_t secretCopy[INTERCONNECT_SECRET_LEN];
        const uint8_t *secret = nullptr;
        if (interconnectStore->settings().role == InterconnectRoleRepeater) {
            if (!interconnectStore->settings().hasMain) {
                return;
            }
            memcpy(secretCopy, interconnectStore->settings().mainSecret, INTERCONNECT_SECRET_LEN);
            secret = secretCopy;
        } else {
            InterconnectPeer *peer = interconnectStore->findByMac(job.peerMac);
            if (peer == nullptr || !peer->hasSecret) {
                return;
            }
            memcpy(secretCopy, peer->secret, INTERCONNECT_SECRET_LEN);
            secret = secretCopy;
        }
        uint8_t localMac[6];
        if (!localWifiMac(localMac)) {
            return;
        }
        // Blocking TCP/AEAD runs on the icQueue worker, not the Arduino loop.
        const bool ok = runUniversalExchangeAsClient(
            job.peerIp,
            job.peerPort,
            secret,
            job.peerMac,
            job.type,
            job.payload,
            job.payloadLen
        );
        if (ok) {
            InterconnectPeer *peer = interconnectStore->findByMac(job.peerMac);
            if (peer != nullptr) {
                peer->packetsOut++;
                peer->lastAliveMs = millis();
                peer->online = true;
                noteLiveUiChanged();
            } else if (interconnectStore->settings().role == InterconnectRoleRepeater) {
                interconnectStore->settingsMutable().mainOnline = true;
                interconnectStore->settingsMutable().mainLastAliveMs = millis();
                noteLiveUiChanged();
            }
        } else if (interconnectStore->settings().role == InterconnectRoleRepeater
                   && interconnectStore->settings().hasMain) {
            // Failed delivery while Main was thought online: keep the job for later.
            if (queueMutex == nullptr
                || xSemaphoreTake(queueMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
                if (jobCount < INTERCONNECT_QUEUE_DEPTH) {
                    for (int i = 0; i < INTERCONNECT_QUEUE_DEPTH; i++) {
                        if (!jobQueue[i].used) {
                            jobQueue[i] = job;
                            jobQueue[i].used = true;
                            jobCount++;
                            break;
                        }
                    }
                }
                interconnectStore->settingsMutable().mainOnline = false;
                noteLiveUiChanged();
                if (queueMutex != nullptr) {
                    xSemaphoreGive(queueMutex);
                }
            }
            LOGGER.warning("Interconnect send to Main failed; holding queue");
        }
    } else if (job.kind == InterconnectJobPairConnect) {
        bool macExpected = false;
        for (int i = 0; i < 6; i++) {
            if (job.peerMac[i] != 0) {
                macExpected = true;
                break;
            }
        }
        const uint32_t replaceId = scanReplaceRouterId;
        const bool ok = runPairingAsInitiator(
            job.peerIp,
            job.peerPort,
            macExpected ? job.peerMac : nullptr,
            replaceId
        );
        if (!ok) {
            LOGGER.warning(
                String("Interconnect pair failed to ") + job.peerIp + ":" + String(job.peerPort)
            );
            if (rePairPhase == RePairPhaseTryIp && macExpected) {
                LOGGER.info("Interconnect re-pair falling back to MAC scan");
                if (!startSubnetScanForMac(job.peerMac, interconnectStore->settings().port)) {
                    setRePairStatus("failed");
                    clearRePairState();
                }
            } else if (rePairPhase != RePairPhaseIdle || replaceId != 0) {
                setRePairStatus("failed");
                clearRePairState();
                scanReplaceRouterId = 0;
            }
        }
    } else if (job.kind == InterconnectJobScanProbe) {
        runScanProbeJob(job);
    } else if (job.kind == InterconnectJobVersionProbe) {
        runVersionProbeJob(job);
    }
}

void RouterInterconnect::maybeSendCheckAlive() {
    if (interconnectStore == nullptr) {
        return;
    }
    if (interconnectStore->settings().role != InterconnectRoleMain) {
        return;
    }
    const uint32_t intervalMs = (uint32_t)interconnectStore->settings().checkAliveSec * 1000UL;
    if ((uint32_t)(millis() - lastCheckAliveMs) < intervalMs) {
        return;
    }
    lastCheckAliveMs = millis();
    for (uint32_t routerId = 1; routerId <= INTERCONNECT_MAX_REPEATERS + 8; routerId++) {
        InterconnectPeer *peer = interconnectStore->findByRouterId(routerId);
        if (peer == nullptr || !peer->used) {
            continue;
        }
        enqueueExchange(
            InterconnectMsgCheckAlive,
            peer->mac,
            peer->ip,
            peer->port,
            nullptr,
            0
        );
        if (peer->lastAliveMs != 0
            && (uint32_t)(millis() - peer->lastAliveMs) > intervalMs * 2) {
            if (peer->online) {
                peer->online = false;
                noteLiveUiChanged();
            }
        }
    }
}

void RouterInterconnect::maybeSendJustBooted() {
    if (justBootedSent || interconnectStore == nullptr) {
        return;
    }
    if (interconnectStore->settings().role != InterconnectRoleRepeater
        || !interconnectStore->settings().hasMain) {
        return;
    }
    if (WiFi.localIP() == IPAddress((uint32_t)0)) {
        return;
    }
    const char *version = FIRMWARE_VERSION_HOST;
    enqueueExchange(
        InterconnectMsgJustBooted,
        interconnectStore->settings().mainMac,
        interconnectStore->settings().mainIp,
        interconnectStore->settings().mainPort,
        (const uint8_t *)version,
        (uint16_t)strlen(version)
    );
    justBootedSent = true;
}

bool RouterInterconnect::maybeForwardLocalSpiEvent(const SpiFrame &frame) {
    if (interconnectStore == nullptr) {
        return false;
    }
    if (interconnectStore->settings().role != InterconnectRoleRepeater
        || !interconnectStore->settings().hasMain) {
        return false;
    }
    switch (frame.cmd) {
        case SpiEvtDeviceJoin:
        case SpiEvtDeviceLeave:
        case SpiEvtAttrReport:
        case SpiEvtCmdResult:
        case SpiEvtJoinClosed:
        case SpiEvtRebindResult:
        case SpiEvtConfigRequest:
            break;
        default:
            return false;
    }
    // Retranslate every packet; do not filter duplicates.
    if (1u + frame.length > INTERCONNECT_AEAD_MAX_PAYLOAD) {
        return false;
    }
    uint8_t payload[INTERCONNECT_AEAD_MAX_PAYLOAD];
    payload[0] = frame.cmd;
    if (frame.length > 0) {
        memcpy(payload + 1, frame.payload, frame.length);
    }
    return enqueueExchange(
        InterconnectMsgZigbeePacket,
        interconnectStore->settings().mainMac,
        interconnectStore->settings().mainIp,
        interconnectStore->settings().mainPort,
        payload,
        (uint16_t)(1u + frame.length)
    );
}

#endif
