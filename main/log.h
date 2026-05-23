#pragma once

const char* TAG = "THENSOR";

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include <esp_log.h>

#define LOGV(fmt, ...) ESP_LOGV(TAG, fmt, ##__VA_ARGS__)
#define LOGD(fmt, ...) ESP_LOGD(TAG, fmt, ##__VA_ARGS__)
#define LOGI(fmt, ...) ESP_LOGI(TAG, fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) ESP_LOGW(TAG, fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) ESP_LOGE(TAG, fmt, ##__VA_ARGS__)

#define ABORTIF(x, fmt, ...) do { if(x) { LOGE(fmt, ##__VA_ARGS__); abort(); } } while(0)

static inline const char* log_level_name(esp_log_level_t level) {
    switch(level) {
        case ESP_LOG_NONE: return "NONE";
        case ESP_LOG_ERROR: return "ERROR";
        case ESP_LOG_WARN: return "WARN";
        case ESP_LOG_INFO: return "INFO";
        case ESP_LOG_DEBUG: return "DEBUG";
        case ESP_LOG_VERBOSE: return "VERBOSE";
        default: return "UNKNOWN";
    }
}