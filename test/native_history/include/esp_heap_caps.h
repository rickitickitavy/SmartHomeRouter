#pragma once

#include <stddef.h>
#include <stdint.h>

#define MALLOC_CAP_SPIRAM 0x400
#define MALLOC_CAP_8BIT 0x04

void *heap_caps_malloc(size_t size, uint32_t caps);
void *heap_caps_calloc(size_t n, size_t size, uint32_t caps);
void heap_caps_free(void *ptr);
