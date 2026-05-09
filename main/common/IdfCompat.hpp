// Lightweight compatibility layer so the code linting works outside ESP-IDF.
// When building under ESP-IDF, real headers are included. When not available,
// we provide minimal stubs so the code remains syntactically valid.

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

#if __has_include("esp_log.h")
  #include "esp_log.h"
#else
  #define ESP_LOGI(TAG, FMT, ...) ((void)0)
  #define ESP_LOGW(TAG, FMT, ...) ((void)0)
  #define ESP_LOGE(TAG, FMT, ...) ((void)0)
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
  };
  static inline int gpio_config(const gpio_config_t*) { return 0; }
  static inline int gpio_get_level(gpio_num_t) { return 1; }
  static inline int gpio_set_level(gpio_num_t, int) { return 0; }
#endif


