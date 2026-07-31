#include "utils.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void sd_log(sd_log_level_t level, const char *format, ...) {
    static const char *const level_names[] = {"debug", "info", "warning",
                                               "error"};
    char message[512];
    const char *level_name = "error";
    size_t length;
    va_list arguments;

    if (level >= SD_LOG_LEVEL_DEBUG && level <= SD_LOG_LEVEL_ERROR) {
        level_name = level_names[(int)level];
    }
    message[0] = '\0';
    if (format != NULL) {
        va_start(arguments, format);
        (void)vsnprintf(message, sizeof(message), format, arguments);
        va_end(arguments);
    }
    length = strlen(message);
    while (length > 0U &&
           (message[length - 1U] == '\n' || message[length - 1U] == '\r')) {
        message[--length] = '\0';
    }
    for (length = 0U; message[length] != '\0'; ++length) {
        if (message[length] == '\n' || message[length] == '\r') {
            message[length] = ' ';
        }
    }
    (void)printf("[Sens-Decision] %s: %s\n", level_name, message);
}

float sd_clampf(float value, float minimum, float maximum) {
    if (!isfinite(value) || !isfinite(minimum) || !isfinite(maximum) ||
        minimum > maximum) {
        return 0.0f;
    }
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

float sd_normalize_angle(float angle) {
    if (!isfinite(angle)) {
        return 0.0f;
    }
    angle = fmodf(angle + SD_PI, 2.0f * SD_PI);
    if (angle < 0.0f) {
        angle += 2.0f * SD_PI;
    }
    return angle - SD_PI;
}
