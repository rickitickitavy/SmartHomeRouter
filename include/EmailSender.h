#pragma once

#include "Defines.h"
#include "SettingsManager.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

class EmailSender {
public:
    explicit EmailSender(SettingsManager *settingsManager);

    struct JpegPart {
        uint8_t *data;  // PSRAM-owned; transfers to queue on success
        size_t size;
    };

    // jpegParts: optional JPEG blobs in PSRAM; ownership transfers to the queue on success.
    // On failure the caller still owns all parts. Never copy image bytes into internal RAM.
    bool enqueue(
        const char *receivers,
        const char *theme,
        const char *body,
        const JpegPart *jpegParts = nullptr,
        int jpegCount = 0
    );
    // Convenience: single optional JPEG (same ownership rules).
    bool enqueue(
        const char *receivers,
        const char *theme,
        const char *body,
        uint8_t *jpegPsramOwned,
        size_t jpegSize
    );
    void requestAccountCheck();
    void tick();
    static bool isConfigured(SettingsManager *settingsManager);

    const char *accountStatusText() const;
    bool hasAccountCheckResult() const;
    uint32_t secondsSinceLastCheck() const;
    uint32_t emailsSent() const;
    uint32_t emailsInQueue() const;

private:
    enum class AccountCheckState : uint8_t {
        NeverChecked = 0,
        Ok = 1,
        Failed = 2
    };

    struct EmailJob {
        EmailSender *sender;
        char *receivers;
        char *theme;
        char *body;
        uint8_t jpegCount;
        uint8_t *jpegData[AUTOMATION_EMAIL_CAMERA_ATTACHES_MAX];
        size_t jpegSize[AUTOMATION_EMAIL_CAMERA_ATTACHES_MAX];
        char jpegFilename[AUTOMATION_EMAIL_CAMERA_ATTACHES_MAX][24];
    };

    SettingsManager *settingsManager;
    QueueHandle_t sendQueue = nullptr;
    StaticQueue_t sendQueueControl{};
    uint8_t *sendQueueStorage = nullptr;
    TaskHandle_t workerTask = nullptr;
    volatile bool accountCheckRequested = false;
    volatile bool sendInFlight = false;
    AccountCheckState accountCheckState = AccountCheckState::NeverChecked;
    uint32_t lastCheckMs = 0;
    uint32_t emailsSentCount = 0;
    uint32_t lastIntervalCheckMs = 0;

    bool ensureWorkerRunning();
    bool hasPendingWork() const;
    static void workerEntry(void *argument);
    void workerLoop();
    void sendJob(const EmailJob &job);
    bool connectAccountCheck();
    void runAccountCheckIfNeeded();
    static char *duplicatePsramString(const char *source, size_t maxLength);
    static void freeJob(EmailJob *job);
};
