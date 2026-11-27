#pragma once

#if defined(BOARD_ROLE_HOST)

// First-step host bring-up: active-low pulse on PIN_SLAVE_RST before Wi-Fi / web /
// MQTT / SPI task start (same timing as InterChipHost::resetSlaveSynchronous).
void hostResetSlaveSynchronous(void);

#endif
