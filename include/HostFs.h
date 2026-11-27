#pragma once

#if defined(BOARD_ROLE_HOST)

#include <stdbool.h>

// Mount the data partition (label "spiffs") as LittleFS at "/".
// Returns true when mount succeeds (format-on-fail once, same as Arduino begin(true)).
bool hostFsMount(void);

// Write then read back a small probe file under the store root.
bool hostFsProbeRoundTrip(void);

#endif
