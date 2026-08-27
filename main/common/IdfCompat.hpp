// Lightweight compatibility layer so the code parses and lints outside ESP-IDF.
// When building under ESP-IDF the real headers are included; otherwise minimal
// stubs keep the translation units syntactically valid for host tooling.
//
// Note: the stubs exist for static analysis and editor tooling only. The pure
// logic that is genuinely unit tested on the host (UbxParser, LapTimer, Menu,
// Settings) deliberately avoids needing this header at all.

#pragma once

#if __has_include("freertos/FreeRTOS.h")
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
#else
  using TickType_t = unsigned int;
  #ifndef pdMS_TO_TICKS
  #define pdMS_TO_TICKS(ms) (ms)
  #endif
  inline void vTaskDelay(TickType_t) {}
#endif

#if __has_include("esp_err.h")
  #include "esp_err.h"
#else
  #include <cstdint>
  using esp_err_t = int;
  #ifndef ESP_OK
  #define ESP_OK 0
  #endif
  #ifndef ESP_FAIL
  #define ESP_FAIL -1
  #endif
  static inline const char* esp_err_to_name(esp_err_t) { return "ESP_OK"; }
  #ifndef ESP_ERROR_CHECK
  #define ESP_ERROR_CHECK(x) ((void)(x))
  #endif
#endif

#if __has_include("esp_log.h")
  #include "esp_log.h"
#else
  #define ESP_LOGE(TAG, FMT, ...) ((void)0)
  #define ESP_LOGW(TAG, FMT, ...) ((void)0)
  #define ESP_LOGI(TAG, FMT, ...) ((void)0)
  #define ESP_LOGD(TAG, FMT, ...) ((void)0)
  #define ESP_LOGV(TAG, FMT, ...) ((void)0)
#endif

#if __has_include("esp_timer.h")
  #include "esp_timer.h"
#else
  #include <cstdint>
  static inline int64_t esp_timer_get_time() { return 0; }
#endif

#if __has_include("driver/gpio.h")
  #include "driver/gpio.h"
#else
  #include <cstdint>
  typedef int gpio_num_t;
  typedef struct {
    uint64_t pin_bit_mask;
    int mode;
    int pull_up_en;
    int pull_down_en;
    int intr_type;
  } gpio_config_t;
  enum {
    GPIO_INTR_DISABLE = 0,
    GPIO_MODE_INPUT = 0,
    GPIO_MODE_OUTPUT = 1,
    GPIO_PULLUP_ENABLE = 1,
    GPIO_PULLUP_DISABLE = 0,
    GPIO_PULLDOWN_DISABLE = 0,
    GPIO_PULLUP_ONLY = 0,
    GPIO_FLOATING = 3,
  };
  typedef int gpio_pull_mode_t;
  static inline esp_err_t gpio_config(const gpio_config_t*) { return ESP_OK; }
  static inline int gpio_get_level(gpio_num_t) { return 1; }
  static inline esp_err_t gpio_set_level(gpio_num_t, int) { return ESP_OK; }
  static inline esp_err_t gpio_set_pull_mode(gpio_num_t, gpio_pull_mode_t) { return ESP_OK; }
#endif
