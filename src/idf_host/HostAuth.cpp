#include "HostAuth.h"

#if defined(BOARD_ROLE_HOST)

#include "Defines.h"

#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

struct HostAuthSession {
    bool used;
    char tokenHex[AUTH_SESSION_COOKIE_MAX];
    char userName[USER_NAME_MAX];
    bool isAdmin;
    uint32_t expiresAtMs;
};

static HostAuthSession s_sessions[AUTH_SESSION_MAX];

static uint32_t nowMs(void) {
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

void hostAuthBegin(void) {
    memset(s_sessions, 0, sizeof(s_sessions));
}

static void tokenToHex(const uint8_t *token, size_t tokenLen, char *hexOut, size_t hexOutSize) {
    static const char *kHex = "0123456789abcdef";
    size_t outIndex = 0;
    for (size_t i = 0; i < tokenLen && outIndex + 2 < hexOutSize; ++i) {
        hexOut[outIndex++] = kHex[(token[i] >> 4) & 0x0f];
        hexOut[outIndex++] = kHex[token[i] & 0x0f];
    }
    hexOut[outIndex] = '\0';
}

bool hostAuthLogin(
    const char *userName,
    const char *password,
    bool rememberMe,
    char *tokenHexOut,
    size_t tokenHexOutSize,
    uint32_t *maxAgeSecOut
) {
    if (userName == nullptr || password == nullptr || tokenHexOut == nullptr || tokenHexOutSize < 8) {
        return false;
    }
    if (strcmp(userName, "admin") != 0 || strcmp(password, "admin") != 0) {
        return false;
    }

    int freeIndex = -1;
    for (int i = 0; i < AUTH_SESSION_MAX; ++i) {
        if (!s_sessions[i].used) {
            freeIndex = i;
            break;
        }
    }
    if (freeIndex < 0) {
        freeIndex = 0;
    }

    uint8_t token[AUTH_SESSION_TOKEN_LEN];
    esp_fill_random(token, sizeof(token));
    tokenToHex(token, sizeof(token), s_sessions[freeIndex].tokenHex, sizeof(s_sessions[freeIndex].tokenHex));

    s_sessions[freeIndex].used = true;
    strncpy(s_sessions[freeIndex].userName, "admin", sizeof(s_sessions[freeIndex].userName) - 1);
    s_sessions[freeIndex].isAdmin = true;
    const uint32_t lifetimeMs = rememberMe ? AUTH_REMEMBER_MS : AUTH_SESSION_MS;
    s_sessions[freeIndex].expiresAtMs = nowMs() + lifetimeMs;

    strncpy(tokenHexOut, s_sessions[freeIndex].tokenHex, tokenHexOutSize - 1);
    tokenHexOut[tokenHexOutSize - 1] = '\0';
    if (maxAgeSecOut != nullptr) {
        *maxAgeSecOut = lifetimeMs / 1000u;
    }
    return true;
}

void hostAuthLogout(const char *tokenHex) {
    if (tokenHex == nullptr || tokenHex[0] == '\0') {
        return;
    }
    for (int i = 0; i < AUTH_SESSION_MAX; ++i) {
        if (s_sessions[i].used && strcmp(s_sessions[i].tokenHex, tokenHex) == 0) {
            s_sessions[i].used = false;
            s_sessions[i].tokenHex[0] = '\0';
            return;
        }
    }
}

static bool extractCookieToken(const char *cookieHeader, char *tokenOut, size_t tokenOutSize) {
    if (cookieHeader == nullptr || tokenOut == nullptr || tokenOutSize == 0) {
        return false;
    }
    const char *found = strstr(cookieHeader, AUTH_COOKIE_NAME "=");
    if (found == nullptr) {
        return false;
    }
    found += strlen(AUTH_COOKIE_NAME "=");
    size_t index = 0;
    while (index + 1 < tokenOutSize && found[index] != '\0' && found[index] != ';') {
        tokenOut[index] = found[index];
        index++;
    }
    tokenOut[index] = '\0';
    return tokenOut[0] != '\0';
}

bool hostAuthSessionUser(
    const char *cookieHeader,
    char *userNameOut,
    size_t userNameOutSize,
    bool *isAdminOut
) {
    char tokenHex[AUTH_SESSION_COOKIE_MAX];
    if (!extractCookieToken(cookieHeader, tokenHex, sizeof(tokenHex))) {
        return false;
    }
    const uint32_t now = nowMs();
    for (int i = 0; i < AUTH_SESSION_MAX; ++i) {
        if (!s_sessions[i].used) {
            continue;
        }
        if (strcmp(s_sessions[i].tokenHex, tokenHex) != 0) {
            continue;
        }
        if ((int32_t)(now - s_sessions[i].expiresAtMs) >= 0) {
            s_sessions[i].used = false;
            return false;
        }
        if (userNameOut != nullptr && userNameOutSize > 0) {
            strncpy(userNameOut, s_sessions[i].userName, userNameOutSize - 1);
            userNameOut[userNameOutSize - 1] = '\0';
        }
        if (isAdminOut != nullptr) {
            *isAdminOut = s_sessions[i].isAdmin;
        }
        return true;
    }
    return false;
}

#endif
