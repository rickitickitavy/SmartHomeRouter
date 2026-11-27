#pragma once

#if defined(BOARD_ROLE_HOST)

#include <stdbool.h>

// IDF host Wi-Fi bring-up. SoftAP / STA recovery semantics match WiFiController.
// Console listen rebind is armed here; HTTP rebind itself lands in later tasks.

using HostWifiRebindFn = void (*)(void);

void hostWifiSetRebindHandler(HostWifiRebindFn handler);

#if defined(BOARD_HOST_S3)
void hostWifiBeginS3(bool forceAp, const char *staSsid, const char *staPassword, const char *hostname);
void hostWifiUpdateS3(void);
bool hostWifiIsApModeS3(void);
bool hostWifiIsStaConnectedS3(void);
bool hostWifiHasUsableInterfaceS3(void);
#endif

#if defined(BOARD_HOST_P4)
// ESP-Hosted (C6 SDIO): same SoftAP/STA recovery + Hosted transport recovery rebind arm.
void hostWifiBeginP4(bool forceAp, const char *staSsid, const char *staPassword, const char *hostname);
void hostWifiUpdateP4(void);
bool hostWifiIsApModeP4(void);
bool hostWifiIsStaConnectedP4(void);
bool hostWifiHasUsableInterfaceP4(void);
#endif

#endif
