#pragma once

#if defined(BOARD_ROLE_HOST)

#include <stdbool.h>

bool hostHttpBegin(void);
void hostHttpEnd(void);
// Stop + start listen with bind retries (STA/Hosted rebind). Safe from host service loop.
bool hostHttpRebind(void);
bool hostHttpIsRunning(void);

#endif
