#pragma once

#if defined(BOARD_ROLE_HOST)

#include "GlobalSettings.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct HostSmtpJpegPart;

bool hostSmtpSendConfigured(
    const EmailSettings &email,
    const char *receivers,
    const char *subject,
    const char *body,
    const uint8_t *const *jpegData,
    const size_t *jpegSize,
    uint8_t jpegCount
);

bool hostSmtpAccountCheck(const EmailSettings &email, char *errorOut, size_t errorMax);

#endif
