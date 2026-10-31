#pragma once

#include "Defines.h"
#include <stddef.h>
#include <stdint.h>

// Host-only ONVIF media helpers. SOAP response buffers use PSRAM.
// Never holds camera image bytes — only URL/name strings into caller-owned output.

struct OnvifResolvedMedia {
    char name[CAMERA_NAME_MAX];
    char url[CAMERA_URL_MAX];
    char host[CAMERA_HOST_MAX];
    uint16_t port;
    char snapshotUrl[CAMERA_CHANNELS][CAMERA_URL_MAX];
    char liveUrl[CAMERA_CHANNELS][CAMERA_URL_MAX];
};

// deviceServiceUrl: ONVIF device XAddr (from WS-Discovery).
// username/password may be empty. Fills up to CAMERA_CHANNELS media profiles.
bool onvifResolveMedia(
    const char *deviceServiceUrl,
    const char *username,
    const char *password,
    OnvifResolvedMedia *out,
    char *errorText,
    size_t errorMax
);
