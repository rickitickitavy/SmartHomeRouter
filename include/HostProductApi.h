#pragma once

#if defined(BOARD_ROLE_HOST)

#include "GlobalSettings.h"
#include "esp_http_server.h"

#include <stdbool.h>

bool hostProductBegin(void);
void hostProductRegisterRoutes(httpd_handle_t server);
void hostProductTick(void);
const EmailSettings *hostProductEmailSettings(void);

#endif
