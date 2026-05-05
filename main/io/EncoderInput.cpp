// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#include "EncoderInput.hpp"

#include "../common/IdfCompat.hpp"

using ktsu::racebox::io::EncoderEvent;
using ktsu::racebox::io::EncoderInput;

static inline bool read_gpio(gpio_num_t pin) { return gpio_get_level(pin) != 0; }

EncoderInput::EncoderInput(int pinA, int pinB, int pinButton)
  : pinA_(pinA), pinB_(pinB), buttonPin_(pinButton) {}

void EncoderInput::begin() {
  gpio_config_t io{};
  io.intr_type = GPIO_INTR_DISABLE;
  io.mode = GPIO_MODE_INPUT;
  io.pull_up_en = GPIO_PULLUP_ENABLE;
  io.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io.pin_bit_mask = (1ULL << pinA_) | (1ULL << pinB_) | (1ULL << buttonPin_);
  if (gpio_config(&io) != 0) {
    ESP_LOGE("encoder", "gpio_config failed for pins A=%d B=%d BTN=%d", pinA_, pinB_, buttonPin_);
    return;
  }
  // Seed state from current pin levels so the first tick() doesn't decode a phantom step.
  int a = read_gpio(static_cast<gpio_num_t>(pinA_)) ? 1 : 0;
  int b = read_gpio(static_cast<gpio_num_t>(pinB_)) ? 1 : 0;
  lastEncoded_ = (a << 1) | b;
  lastBtn_ = read_gpio(static_cast<gpio_num_t>(buttonPin_));
}

void EncoderInput::tick() {
  // Simple polling 2-bit Gray decoding
  int a = read_gpio(static_cast<gpio_num_t>(pinA_)) ? 1 : 0;
  int b = read_gpio(static_cast<gpio_num_t>(pinB_)) ? 1 : 0;
  int encoded = (a << 1) | b;
  int sum = (lastEncoded_ << 2) | encoded;
  if (sum == 0b0001 || sum == 0b0111 || sum == 0b1110 || sum == 0b1000) {
    position_++;
    if (listener_) { EncoderEvent e{EncoderEventType::Rotate, +1}; listener_(e); }
  } else if (sum == 0b0010 || sum == 0b1011 || sum == 0b1101 || sum == 0b0100) {
    position_--;
    if (listener_) { EncoderEvent e{EncoderEventType::Rotate, -1}; listener_(e); }
  }
  lastEncoded_ = encoded;

  bool btn = read_gpio(static_cast<gpio_num_t>(buttonPin_));
  uint32_t now_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000);
  if (!btn && lastBtn_) { pressStartMs_ = now_ms; }
  if (btn && !lastBtn_) {
    uint32_t held = now_ms - pressStartMs_;
    if (listener_) { EncoderEvent e{held > 600 ? EncoderEventType::LongPress : EncoderEventType::Click, 0}; listener_(e); }
  }
  lastBtn_ = btn;
}


