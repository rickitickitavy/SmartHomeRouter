#pragma once

#include "SettingsManager.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

class EmailSender {
public:
    explicit EmailSender(SettingsManager *settingsManager);

    bool enqueue(const char *receivers, const char *theme, const char *body);
    static bool isConfigured(SettingsManager *settingsManager);

private:
    struct EmailJob {
        EmailSender *sender;
        char *receivers;
        char *theme;
        char *body;
    };

    SettingsManager *settingsManager;

    static void oneShotEntry(void *argument);
    void sendJob(const EmailJob &job);
    static char *duplicatePsramString(const char *source, size_t maxLength);
    static void freeJob(EmailJob *job);
};
