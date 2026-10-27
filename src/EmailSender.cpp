#include "EmailSender.h"
#include "Logger.h"
#include "Defines.h"

#include <esp_heap_caps.h>
#include <string.h>

#if BOARD_ROLE_HOST
#include <ESP_Mail_Client.h>
#endif

namespace {
constexpr uint32_t kEmailWorkerStackWords = 8192;
// Below loopTask (1) and async_tcp (10) so TLS crypto cannot starve UI/MQTT.
constexpr UBaseType_t kEmailWorkerPriority = 0;
// CPU1: IDLE1 is not TWDT-watched. async_tcp is pinned to CPU0 via build flag
// so long TLS here cannot starve its WDT feed (that was the CPU1 failure mode).
constexpr BaseType_t kEmailWorkerCore = 1;
constexpr UBaseType_t kEmailQueueLength = EMAIL_SEND_QUEUE_MAX;

#if BOARD_ROLE_HOST
SemaphoreHandle_t gEmailSmtpMutex = nullptr;
portMUX_TYPE gEmailWorkerMux = portMUX_INITIALIZER_UNLOCKED;

void ensureEmailSmtpMutex() {
    if (gEmailSmtpMutex == nullptr) {
        gEmailSmtpMutex = xSemaphoreCreateMutex();
    }
}
#endif
}

EmailSender::EmailSender(SettingsManager *settingsManager)
    : settingsManager(settingsManager) {
#if BOARD_ROLE_HOST
    ensureEmailSmtpMutex();
    sendQueueStorage = static_cast<uint8_t *>(
        heap_caps_malloc(kEmailQueueLength * sizeof(EmailJob *), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
    );
    if (sendQueueStorage == nullptr) {
        sendQueueStorage = static_cast<uint8_t *>(ps_malloc(kEmailQueueLength * sizeof(EmailJob *)));
    }
    if (sendQueueStorage != nullptr) {
        sendQueue = xQueueCreateStatic(
            kEmailQueueLength,
            sizeof(EmailJob *),
            sendQueueStorage,
            &sendQueueControl
        );
    }
    if (sendQueue == nullptr) {
        LOGGER.warning("Email queue create failed");
    }
    // Worker task (and its ~32 KB stack) is created only when work arrives.
#endif
}

bool EmailSender::isConfigured(SettingsManager *settingsManager) {
    if (settingsManager == nullptr) {
        return false;
    }
    return SettingsManager::isEmailConfigured(settingsManager->getSettings()->email);
}

const char *EmailSender::accountStatusText() const {
    if (!isConfigured(settingsManager)) {
        return "not configured";
    }
    if (sendInFlight && accountCheckState == AccountCheckState::Ok) {
        return "configured/sending";
    }
    if (accountCheckState == AccountCheckState::Ok) {
        return "configured";
    }
    return "configured/fails";
}

bool EmailSender::hasAccountCheckResult() const {
    return lastCheckMs != 0;
}

uint32_t EmailSender::secondsSinceLastCheck() const {
    if (lastCheckMs == 0) {
        return 0;
    }
    return (millis() - lastCheckMs) / 1000UL;
}

uint32_t EmailSender::emailsSent() const {
    return emailsSentCount;
}

uint32_t EmailSender::emailsInQueue() const {
    uint32_t waiting = 0;
    if (sendQueue != nullptr) {
        waiting = (uint32_t)uxQueueMessagesWaiting(sendQueue);
    }
    if (sendInFlight) {
        waiting += 1;
    }
    return waiting;
}

bool EmailSender::hasPendingWork() const {
    if (accountCheckRequested) {
        return true;
    }
    if (sendQueue == nullptr) {
        return false;
    }
    return uxQueueMessagesWaiting(sendQueue) > 0;
}

bool EmailSender::ensureWorkerRunning() {
#if !BOARD_ROLE_HOST
    return false;
#else
    portENTER_CRITICAL(&gEmailWorkerMux);
    if (workerTask != nullptr) {
        portEXIT_CRITICAL(&gEmailWorkerMux);
        return true;
    }
    portEXIT_CRITICAL(&gEmailWorkerMux);

    TaskHandle_t createdHandle = nullptr;
    const BaseType_t created = xTaskCreatePinnedToCore(
        workerEntry,
        "emailSmtp",
        kEmailWorkerStackWords,
        this,
        kEmailWorkerPriority,
        &createdHandle,
        kEmailWorkerCore
    );
    if (created != pdPASS || createdHandle == nullptr) {
        LOGGER.warning("Email worker create failed");
        return false;
    }

    portENTER_CRITICAL(&gEmailWorkerMux);
    if (workerTask != nullptr) {
        portEXIT_CRITICAL(&gEmailWorkerMux);
        vTaskDelete(createdHandle);
        return true;
    }
    workerTask = createdHandle;
    portEXIT_CRITICAL(&gEmailWorkerMux);
    return true;
#endif
}

char *EmailSender::duplicatePsramString(const char *source, size_t maxLength) {
    const char *text = source != nullptr ? source : "";
    size_t length = strlen(text);
    if (length > maxLength) {
        length = maxLength;
    }
    char *copy = static_cast<char *>(
        heap_caps_malloc(length + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
    );
    if (copy == nullptr) {
        copy = static_cast<char *>(ps_malloc(length + 1));
    }
    if (copy == nullptr) {
        return nullptr;
    }
    memcpy(copy, text, length);
    copy[length] = '\0';
    return copy;
}

void EmailSender::freeJob(EmailJob *job) {
    if (job == nullptr) {
        return;
    }
    free(job->receivers);
    free(job->theme);
    free(job->body);
    job->receivers = nullptr;
    job->theme = nullptr;
    job->body = nullptr;
    free(job);
}

bool EmailSender::enqueue(const char *receivers, const char *theme, const char *body) {
#if !BOARD_ROLE_HOST
    (void)receivers;
    (void)theme;
    (void)body;
    LOGGER.warning("Email action ignored: host-only feature");
    return false;
#else
    if (!isConfigured(settingsManager)) {
        LOGGER.warning("Email action ignored: account not configured");
        return false;
    }
    if (sendQueue == nullptr) {
        LOGGER.warning("Email action ignored: send queue unavailable");
        return false;
    }

    EmailJob *job = static_cast<EmailJob *>(
        heap_caps_malloc(sizeof(EmailJob), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
    );
    if (job == nullptr) {
        job = static_cast<EmailJob *>(ps_malloc(sizeof(EmailJob)));
    }
    if (job == nullptr) {
        LOGGER.warning("Email action ignored: out of PSRAM for send job");
        return false;
    }
    memset(job, 0, sizeof(*job));
    job->sender = this;
    job->receivers = duplicatePsramString(receivers, AUTOMATION_EMAIL_RECEIVERS_MAX - 1);
    job->theme = duplicatePsramString(theme, AUTOMATION_EMAIL_THEME_MAX - 1);
    job->body = duplicatePsramString(
        body,
        AUTOMATION_EMAIL_MESSAGE_MAX + AUTOMATION_EMAIL_PARAMS_MAX * 96
    );
    if (job->receivers == nullptr || job->theme == nullptr || job->body == nullptr) {
        freeJob(job);
        LOGGER.warning("Email action ignored: out of PSRAM for send job");
        return false;
    }

    if (xQueueSend(sendQueue, &job, 0) != pdTRUE) {
        freeJob(job);
        LOGGER.warning("Email action ignored: send queue full");
        return false;
    }
    if (!ensureWorkerRunning()) {
        LOGGER.warning("Email action queued but worker start failed");
        return false;
    }
    return true;
#endif
}

void EmailSender::requestAccountCheck() {
#if BOARD_ROLE_HOST
    if (!isConfigured(settingsManager)) {
        accountCheckState = AccountCheckState::NeverChecked;
        lastCheckMs = 0;
        return;
    }
    accountCheckRequested = true;
    if (!ensureWorkerRunning()) {
        LOGGER.warning("Email account check requested but worker start failed");
    }
#else
    (void)0;
#endif
}

void EmailSender::tick() {
#if BOARD_ROLE_HOST
    if (!isConfigured(settingsManager)) {
        return;
    }
    const EmailSettings &emailSettings = settingsManager->getSettings()->email;
    const uint32_t intervalMs =
        (uint32_t)emailSettings.checkAccountIntervalMin * 60UL * 1000UL;
    if (intervalMs == 0) {
        return;
    }
    const uint32_t nowMs = millis();
    if (lastIntervalCheckMs == 0) {
        lastIntervalCheckMs = nowMs;
        return;
    }
    if ((nowMs - lastIntervalCheckMs) >= intervalMs) {
        lastIntervalCheckMs = nowMs;
        accountCheckRequested = true;
        if (!ensureWorkerRunning()) {
            LOGGER.warning("Email interval check requested but worker start failed");
        }
    }
#else
    (void)0;
#endif
}

void EmailSender::workerEntry(void *argument) {
    EmailSender *sender = static_cast<EmailSender *>(argument);
    if (sender != nullptr) {
        sender->workerLoop();
    }
    vTaskDelete(nullptr);
}

void EmailSender::workerLoop() {
#if BOARD_ROLE_HOST
    for (;;) {
        runAccountCheckIfNeeded();

        EmailJob *job = nullptr;
        if (sendQueue != nullptr) {
            const BaseType_t gotJob =
                xQueueReceive(sendQueue, &job, pdMS_TO_TICKS(50));
            if (gotJob == pdTRUE && job != nullptr) {
                sendInFlight = true;
                ensureEmailSmtpMutex();
                if (gEmailSmtpMutex != nullptr) {
                    xSemaphoreTake(gEmailSmtpMutex, portMAX_DELAY);
                }
                sendJob(*job);
                if (gEmailSmtpMutex != nullptr) {
                    xSemaphoreGive(gEmailSmtpMutex);
                }
                sendInFlight = false;
                freeJob(job);
                continue;
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(50));
        }

        if (hasPendingWork()) {
            continue;
        }

        portENTER_CRITICAL(&gEmailWorkerMux);
        if (hasPendingWork()) {
            portEXIT_CRITICAL(&gEmailWorkerMux);
            continue;
        }
        workerTask = nullptr;
        portEXIT_CRITICAL(&gEmailWorkerMux);

        if (hasPendingWork()) {
            if (!ensureWorkerRunning()) {
                portENTER_CRITICAL(&gEmailWorkerMux);
                workerTask = xTaskGetCurrentTaskHandle();
                portEXIT_CRITICAL(&gEmailWorkerMux);
                continue;
            }
            break;
        }
        LOGGER.info("Email SMTP worker idle, exiting");
        break;
    }
#else
    vTaskDelay(portMAX_DELAY);
#endif
}

void EmailSender::runAccountCheckIfNeeded() {
#if BOARD_ROLE_HOST
    if (!accountCheckRequested) {
        return;
    }
    accountCheckRequested = false;
    if (!isConfigured(settingsManager)) {
        accountCheckState = AccountCheckState::NeverChecked;
        lastCheckMs = 0;
        return;
    }
    ensureEmailSmtpMutex();
    if (gEmailSmtpMutex != nullptr) {
        xSemaphoreTake(gEmailSmtpMutex, portMAX_DELAY);
    }
    const bool ok = connectAccountCheck();
    if (gEmailSmtpMutex != nullptr) {
        xSemaphoreGive(gEmailSmtpMutex);
    }
    lastCheckMs = millis();
    lastIntervalCheckMs = lastCheckMs;
    accountCheckState = ok ? AccountCheckState::Ok : AccountCheckState::Failed;
    if (ok) {
        LOGGER.info("Email SMTP account check ok");
    }
#else
    (void)0;
#endif
}

bool EmailSender::connectAccountCheck() {
#if !BOARD_ROLE_HOST
    return false;
#else
    if (settingsManager == nullptr || !isConfigured(settingsManager)) {
        return false;
    }
    const EmailSettings &emailSettings = settingsManager->getSettings()->email;
    SMTPSession smtp;
    Session_Config config;

    config.server.host_name = emailSettings.host;
    config.server.port = emailSettings.port;
    if (emailSettings.authEnabled) {
        config.login.email = emailSettings.username;
        config.login.password = emailSettings.password;
    }
    if (emailSettings.ehloHostname[0] != '\0') {
        config.login.user_domain = emailSettings.ehloHostname;
    } else {
        config.login.user_domain = F("z2m-gateway");
    }
    config.time.ntp_server = F("pool.ntp.org,time.nist.gov");
    config.time.gmt_offset = 0;
    config.time.day_light_offset = 0;
    if (emailSettings.encryption == EmailEncryptionNone) {
        config.secure.mode = esp_mail_secure_mode_nonsecure;
    } else {
        config.secure.mode = esp_mail_secure_mode_ssl_tls;
    }

    uint16_t tcpTimeoutSec = (uint16_t)(emailSettings.connectTimeoutMs / 1000);
    if (tcpTimeoutSec < 15) {
        tcpTimeoutSec = 15;
    }
    smtp.setTCPTimeout(tcpTimeoutSec);
    if (!smtp.connect(&config)) {
        String detail;
        detail.reserve(280);
        detail += "Email SMTP account check failed: host=";
        detail += emailSettings.host;
        detail += " port=";
        detail += String(emailSettings.port);
        detail += " err=";
        detail += String(smtp.errorCode());
        detail += " reason=";
        detail += smtp.errorReason();
        LOGGER.warning(detail);
        smtp.closeSession();
        return false;
    }
    smtp.closeSession();
    return true;
#endif
}

void EmailSender::sendJob(const EmailJob &job) {
#if !BOARD_ROLE_HOST
    (void)job;
#else
    if (settingsManager == nullptr || !isConfigured(settingsManager)) {
        LOGGER.warning("Email send skipped: account not configured");
        return;
    }
    const EmailSettings &emailSettings = settingsManager->getSettings()->email;

    // SMTP client exists only for this send; destroyed when this scope ends.
    {
        SMTPSession smtp;
        Session_Config config;
        SMTP_Message message;

        config.server.host_name = emailSettings.host;
        config.server.port = emailSettings.port;
        if (emailSettings.authEnabled) {
            config.login.email = emailSettings.username;
            config.login.password = emailSettings.password;
        }
        if (emailSettings.ehloHostname[0] != '\0') {
            config.login.user_domain = emailSettings.ehloHostname;
        } else {
            config.login.user_domain = F("z2m-gateway");
        }
        config.time.ntp_server = F("pool.ntp.org,time.nist.gov");
        config.time.gmt_offset = 0;
        config.time.day_light_offset = 0;

        if (emailSettings.encryption == EmailEncryptionNone) {
            config.secure.mode = esp_mail_secure_mode_nonsecure;
        } else {
            config.secure.mode = esp_mail_secure_mode_ssl_tls;
        }

        message.sender.name =
            emailSettings.fromName[0] != '\0' ? emailSettings.fromName : emailSettings.fromAddress;
        message.sender.email = emailSettings.fromAddress;
        if (emailSettings.replyTo[0] != '\0') {
            message.addHeader("Reply-To: " + String(emailSettings.replyTo));
        }
        message.subject = job.theme != nullptr ? job.theme : "";
        message.text.content = job.body != nullptr ? job.body : "";

        if (job.receivers != nullptr) {
            String receiversText = job.receivers;
            int start = 0;
            while (start <= (int)receiversText.length()) {
                int separator = receiversText.indexOf(';', start);
                if (separator < 0) {
                    separator = receiversText.length();
                }
                String address = receiversText.substring(start, separator);
                address.trim();
                if (address.length() > 0) {
                    message.addRecipient(address.c_str(), address.c_str());
                }
                if (separator >= (int)receiversText.length()) {
                    break;
                }
                start = separator + 1;
            }
        }

        uint16_t tcpTimeoutSec = (uint16_t)(emailSettings.connectTimeoutMs / 1000);
        const uint16_t sendTimeoutSec = (uint16_t)(emailSettings.sendTimeoutMs / 1000);
        if (sendTimeoutSec > tcpTimeoutSec) {
            tcpTimeoutSec = sendTimeoutSec;
        }
        if (tcpTimeoutSec < 15) {
            tcpTimeoutSec = 15;
        }
        smtp.setTCPTimeout(tcpTimeoutSec);
        auto logSmtpFailure = [&](const char *phase) {
            const char *encryptionName = emailEncryptionModeName(emailSettings.encryption);
            String detail;
            detail.reserve(320);
            detail += "Email SMTP ";
            detail += phase;
            detail += " failed: host=";
            detail += emailSettings.host;
            detail += " port=";
            detail += String(emailSettings.port);
            detail += " enc=";
            detail += encryptionName != nullptr ? encryptionName : "?";
            detail += " auth=";
            detail += emailSettings.authEnabled ? "on" : "off";
            if (emailSettings.authEnabled && emailSettings.username[0] != '\0') {
                detail += " user=";
                detail += emailSettings.username;
            }
            detail += " status=";
            detail += String(smtp.statusCode());
            detail += " err=";
            detail += String(smtp.errorCode());
            detail += " reason=";
            detail += smtp.errorReason();
            const String statusText = smtp.statusMessage();
            if (statusText.length() > 0) {
                detail += " server=";
                detail += statusText;
            }
            LOGGER.warning(detail);
        };
        if (!smtp.connect(&config)) {
            logSmtpFailure("connect");
            smtp.closeSession();
            return;
        }
        if (!MailClient.sendMail(&smtp, &message)) {
            logSmtpFailure("send");
            smtp.closeSession();
            return;
        }
        LOGGER.info("Email SMTP send ok");
        emailsSentCount++;
        smtp.closeSession();
    }
#endif
}
