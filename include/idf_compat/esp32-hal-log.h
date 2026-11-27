#pragma once

#include <stdio.h>

// Keep logging macros from expanding into ESP_LOG multi-branch expressions
// that can break surrounding C++ scopes in vendored Arduino Zigbee sources.
#ifndef log_e
#define log_e(...) do { printf("E arduino\n"); } while (0)
#endif
#ifndef log_w
#define log_w(...) do { printf("W arduino\n"); } while (0)
#endif
#ifndef log_i
#define log_i(...) do { printf("I arduino\n"); } while (0)
#endif
#ifndef log_d
#define log_d(...) ((void)0)
#endif
#ifndef log_v
#define log_v(...) ((void)0)
#endif
