#pragma once

#include "esp_heap_caps.h"

#include <stddef.h>
#include <stdlib.h>

inline bool psramFound(void) {
    return heap_caps_get_total_size(MALLOC_CAP_SPIRAM) > 0;
}

inline void *ps_malloc(size_t size) {
    void *pointer = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (pointer == nullptr) {
        pointer = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    }
    return pointer;
}

inline void *ps_calloc(size_t count, size_t size) {
    void *pointer = heap_caps_calloc(count, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (pointer == nullptr) {
        pointer = heap_caps_calloc(count, size, MALLOC_CAP_SPIRAM);
    }
    return pointer;
}
