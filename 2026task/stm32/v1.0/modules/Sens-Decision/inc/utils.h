#ifndef SENS_DECISION_UTILS_H
#define SENS_DECISION_UTILS_H

#define SD_PI 3.14159265358979323846f

typedef enum {
    SD_LOG_LEVEL_DEBUG,
    SD_LOG_LEVEL_INFO,
    SD_LOG_LEVEL_WARNING,
    SD_LOG_LEVEL_ERROR
} sd_log_level_t;

void sd_log(sd_log_level_t level, const char *format, ...);

#define SD_LOG_DEBUG(...) sd_log(SD_LOG_LEVEL_DEBUG, __VA_ARGS__)
#define SD_LOG_INFO(...) sd_log(SD_LOG_LEVEL_INFO, __VA_ARGS__)
#define SD_LOG_WARNING(...) sd_log(SD_LOG_LEVEL_WARNING, __VA_ARGS__)
#define SD_LOG_ERROR(...) sd_log(SD_LOG_LEVEL_ERROR, __VA_ARGS__)

float sd_clampf(float value, float minimum, float maximum);
float sd_normalize_angle(float angle);

#endif
