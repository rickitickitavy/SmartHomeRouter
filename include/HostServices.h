#pragma once

#if defined(BOARD_ROLE_HOST)

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "Defines.h"

// Remaining IDF host services for migrate-host-to-esp-idf (scaffold → product parity).

void hostServicesBegin(void);
void hostServicesUpdate(void);

// 3.3 PSRAM JSON helpers
void *hostPsramAlloc(size_t bytes);
void hostPsramFree(void *pointer);
bool hostPsramAllocFailedRecently(void);

// 3.4 low-memory fail-safe
void hostFailsafeSetThresholdKb(uint16_t thresholdKb);
void hostFailsafeCheck(void);

// 4.x MQTT
bool hostMqttClientEnabled(void);
bool hostMqttBrokerEnabled(void);
bool hostMqttClientConnected(void);

// 4.3 store path smoke (LittleFS round-trip; full WebConsole parity is follow-on)
bool hostStoresSmoke(void);

// 5.x SPI host
bool hostSpiIsLinkHealthy(void);
bool hostSpiTryEnqueue(uint8_t cmd, const uint8_t *payload, uint16_t length);
bool hostSpiIsOutboundIdle(void);

// 6.4 slave SPI OTA (exclusive in-flight; empty-last-frame allowed)
bool hostSpiOtaBegin(uint32_t imageSize);
bool hostSpiOtaChunk(const uint8_t *data, uint16_t length);
bool hostSpiOtaEnd(bool emptyLastFrame);
bool hostSpiOtaIsActive(void);

// 6.x SMTP / OTA
struct HostSmtpJpegPart {
    uint8_t *data; // PSRAM-owned; transfers on successful enqueue
    size_t size;
};
bool hostSmtpIsConfigured(void);
bool hostSmtpEnqueue(
    const char *subject,
    const char *body,
    const HostSmtpJpegPart *jpegParts = nullptr,
    int jpegCount = 0
);
bool hostSmtpEnqueueTo(
    const char *receivers,
    const char *subject,
    const char *body,
    const HostSmtpJpegPart *jpegParts = nullptr,
    int jpegCount = 0
);
bool hostSmtpRequestAccountCheck(void);
uint8_t hostSmtpQueueDepth(void);
bool hostOtaApplyHostImage(const char *path);
bool hostOtaHostedC6Needed(const char *packageVersion, const char *runningVersion, bool runningReadable);

// 7.x interconnect AEAD + listen/pair scaffold
bool hostInterconnectIsListening(void);
bool hostInterconnectAeadSeal(
    const uint8_t secret[INTERCONNECT_SECRET_LEN],
    const uint8_t peerMac[6],
    uint8_t type,
    const uint8_t *payload,
    uint16_t payloadLen,
    uint8_t *outFrame,
    size_t outMax,
    size_t *outLen
);
bool hostInterconnectAeadOpen(
    const uint8_t secret[INTERCONNECT_SECRET_LEN],
    const uint8_t peerMac[6],
    const uint8_t *frame,
    size_t frameLen,
    uint8_t *typeOut,
    uint8_t *payloadOut,
    uint16_t payloadMax,
    uint16_t *payloadLenOut
);
bool hostInterconnectPairingWaitActive(void);
bool hostInterconnectStartPairingWait(void);
void hostInterconnectCancelPairingWait(void);

#endif
