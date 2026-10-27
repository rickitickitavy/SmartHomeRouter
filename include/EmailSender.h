#pragma once

#include "SettingsManager.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

class EmailSender {
public:
    explicit EmailSender(SettingsManager *settingsManager);

    bool enqueue(const char *receivers, const char *theme, const char *body);
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
    };

    SettingsManager *settingsManager;
    QueueHandle_t sendQueue = nullptr;
    StaticQueue_t sendQueueControl{};
    uint8_t *sendQueueStorage = nullptr;
    TaskHandle_t workerTask = nullptr;
    volatile bool accountCheckRequested = false;
    AccountCheckState accountCheckState = AccountCheckState::NeverChecked;
    uint32_t lastCheckMs = 0;
    uint32_t emailsSentCount = 0;
    uint32_t lastIntervalCheckMs = 0;

    static void workerEntry(void *argument);
    void workerLoop();
    void sendJob(const EmailJob &job);
    bool connectAccountCheck();
    void runAccountCheckIfNeeded();
    static char *duplicatePsramString(const char *source, size_t maxLength);
    static void freeJob(EmailJob *job);
};
