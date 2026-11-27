#include "OnvifClient.h"

#include <string.h>

bool onvifResolveMedia(
    const char * /*deviceServiceUrl*/,
    const char * /*username*/,
    const char * /*password*/,
    OnvifResolvedMedia *out,
    char *errorText,
    size_t errorMax
) {
    if (out != nullptr) {
        memset(out, 0, sizeof(*out));
    }
    if (errorText != nullptr && errorMax > 0) {
        strncpy(errorText, "ONVIF resolve not available on IDF scaffold", errorMax - 1);
        errorText[errorMax - 1] = '\0';
    }
    return false;
}
