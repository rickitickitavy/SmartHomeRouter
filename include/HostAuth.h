#pragma once

#if defined(BOARD_ROLE_HOST)

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Minimal bring-up auth (factory admin/admin) until UserStore is ported to IDF.
void hostAuthBegin(void);
bool hostAuthLogin(const char *userName, const char *password, bool rememberMe, char *tokenHexOut, size_t tokenHexOutSize, uint32_t *maxAgeSecOut);
void hostAuthLogout(const char *tokenHex);
bool hostAuthSessionUser(const char *cookieHeader, char *userNameOut, size_t userNameOutSize, bool *isAdminOut);

#endif
