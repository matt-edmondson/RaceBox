// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#include "EncoderInput.hpp"

#include "../common/IdfCompat.hpp"
#include "../config/Pins.hpp"

namespace ktsu { namespace racebox { namespace io {

namespace {
constexpr const char* TAG = "encoder";
// PCNT counts are cleared every poll, so the limits only need to cover a single
// polling interval's worth of movement -- far more than a hand can produce.
constexpr int kPcntHighLimit = 1000;
constexpr int kPcntLowLimit = -1000;
} // namespace

EncoderInput::EncoderInput(int pinA, int pinB, int pinButton)
  : pinA_(pinA), pinB_(pinB), buttonPin_(pinButton) {}

EncoderInput::~EncoderInput() {
#ifdef RACEBOX_HAVE_PCNT
  if (pcntUnit_) {
    pcnt_unit_stop(pcntUnit_);
    pcnt_unit_disable(pcntUnit_);
    if (pcntChanA_) pcnt_del_channel(pcntChanA_);
    if (pcntChanB_) pcnt_del_channel(pcntChanB_);
    pcnt_del_unit(pcntUnit_);
  }
#endif
}

uint32_t EncoderInput::nowMs() const {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

bool EncoderInput::readButtonRaw() const {
  return gpio_get_level(static_cast<gpio_num_t>(buttonPin_)) != 0;
}

bool EncoderInput::begin() {
  // Button: input with pull-up, active low.
  gpio_config_t btn{};
  btn.intr_type = GPIO_INTR_DISABLE;
  btn.mode = GPIO_MODE_INPUT;
  btn.pull_up_en = GPIO_PULLUP_ENABLE;
  btn.pull_down_en = GPIO_PULLDOWN_DISABLE;
  btn.pin_bit_mask = (1ULL << buttonPin_);
  if (gpio_config(&btn) != 0) {
    ESP_LOGE(TAG, "gpio_config failed for button pin %d", buttonPin_);
    return false;
  }

#ifdef RACEBOX_HAVE_PCNT
  // PCNT drives the A/B pins directly, but does not configure their pull-ups.
  gpio_set_pull_mode(static_cast<gpio_num_t>(pinA_), GPIO_PULLUP_ONLY);
  gpio_set_pull_mode(static_cast<gpio_num_t>(pinB_), GPIO_PULLUP_ONLY);

  pcnt_unit_config_t unitCfg{};
  unitCfg.high_limit = kPcntHighLimit;
  unitCfg.low_limit = kPcntLowLimit;
  esp_err_t err = pcnt_new_unit(&unitCfg, &pcntUnit_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "pcnt_new_unit failed: %s", esp_err_to_name(err));
    return false;
  }

  if (ktsu::Input::glitchFilterNs > 0) {
    pcnt_glitch_filter_config_t filterCfg{};
    filterCfg.max_glitch_ns = static_cast<uint32_t>(ktsu::Input::glitchFilterNs);
    err = pcnt_unit_set_glitch_filter(pcntUnit_, &filterCfg);
    if (err != ESP_OK) {
      // Not fatal: we lose hardware debouncing but still count correctly.
      ESP_LOGW(TAG, "glitch filter rejected (%s); continuing without it",
               esp_err_to_name(err));
    }
  }

  // Full quadrature: each channel counts edges on one phase, using the other
  // phase's level to decide direction.
  pcnt_chan_config_t chanACfg{};
  chanACfg.edge_gpio_num = pinA_;
  chanACfg.level_gpio_num = pinB_;
  err = pcnt_new_channel(pcntUnit_, &chanACfg, &pcntChanA_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "pcnt_new_channel(A) failed for GPIO %d/%d: %s", pinA_, pinB_,
             esp_err_to_name(err));
    return false;
  }

  pcnt_chan_config_t chanBCfg{};
  chanBCfg.edge_gpio_num = pinB_;
  chanBCfg.level_gpio_num = pinA_;
  err = pcnt_new_channel(pcntUnit_, &chanBCfg, &pcntChanB_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "pcnt_new_channel(B) failed for GPIO %d/%d: %s", pinB_, pinA_,
             esp_err_to_name(err));
    return false;
  }

  pcnt_channel_set_edge_action(pcntChanA_, PCNT_CHANNEL_EDGE_ACTION_DECREASE,
                               PCNT_CHANNEL_EDGE_ACTION_INCREASE);
  pcnt_channel_set_level_action(pcntChanA_, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
  pcnt_channel_set_edge_action(pcntChanB_, PCNT_CHANNEL_EDGE_ACTION_INCREASE,
                               PCNT_CHANNEL_EDGE_ACTION_DECREASE);
  pcnt_channel_set_level_action(pcntChanB_, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                PCNT_CHANNEL_LEVEL_ACTION_INVERSE);

  if ((err = pcnt_unit_enable(pcntUnit_)) != ESP_OK ||
      (err = pcnt_unit_clear_count(pcntUnit_)) != ESP_OK ||
      (err = pcnt_unit_start(pcntUnit_)) != ESP_OK) {
    ESP_LOGE(TAG, "pcnt start failed: %s", esp_err_to_name(err));
    return false;
  }
#else
  // Fallback: poll the phases and decode in software.
  gpio_config_t ab{};
  ab.intr_type = GPIO_INTR_DISABLE;
  ab.mode = GPIO_MODE_INPUT;
  ab.pull_up_en = GPIO_PULLUP_ENABLE;
  ab.pull_down_en = GPIO_PULLDOWN_DISABLE;
  ab.pin_bit_mask = (1ULL << pinA_) | (1ULL << pinB_);
  if (gpio_config(&ab) != 0) {
    ESP_LOGE(TAG, "gpio_config failed for pins A=%d B=%d", pinA_, pinB_);
    return false;
  }
  const int a = gpio_get_level(static_cast<gpio_num_t>(pinA_)) ? 1 : 0;
  const int b = gpio_get_level(static_cast<gpio_num_t>(pinB_)) ? 1 : 0;
  lastEncoded_ = (a << 1) | b;
#endif

  // Seed the button filter from the real level so the first tick() cannot
  // synthesise a press that never happened.
  rawButton_ = readButtonRaw();
  stableButton_ = rawButton_;
  lastEdgeMs_ = nowMs();
  ready_ = true;
  ESP_LOGI(TAG, "ready (A=%d B=%d BTN=%d)", pinA_, pinB_, buttonPin_);
  return true;
}

void EncoderInput::tick() {
  if (!ready_) return;
  pollRotation();
  pollButton();
}

void EncoderInput::pollRotation() {
  int32_t rawDelta = 0;

#ifdef RACEBOX_HAVE_PCNT
  int count = 0;
  if (pcnt_unit_get_count(pcntUnit_, &count) != ESP_OK) return;
  // Clearing each poll keeps the counter far from its limits, so it can never
  // wrap between reads.
  pcnt_unit_clear_count(pcntUnit_);
  rawDelta = count;
#else
  const int a = gpio_get_level(static_cast<gpio_num_t>(pinA_)) ? 1 : 0;
  const int b = gpio_get_level(static_cast<gpio_num_t>(pinB_)) ? 1 : 0;
  const int encoded = (a << 1) | b;
  const int sum = (lastEncoded_ << 2) | encoded;
  if (sum == 0b0001 || sum == 0b0111 || sum == 0b1110 || sum == 0b1000) rawDelta = 1;
  else if (sum == 0b0010 || sum == 0b1011 || sum == 0b1101 || sum == 0b0100) rawDelta = -1;
  lastEncoded_ = encoded;
#endif

  if (rawDelta == 0) return;

  // Convert quadrature counts into detents, carrying the remainder so partial
  // movement is never lost.
  countRemainder_ += rawDelta;
  const int32_t perDetent = ktsu::Input::countsPerDetent > 0 ? ktsu::Input::countsPerDetent : 1;
  const int32_t detents = countRemainder_ / perDetent;
  if (detents == 0) return;
  countRemainder_ -= detents * perDetent;

  if (listener_) {
    EncoderEvent e{EncoderEventType::Rotate, detents};
    listener_(e);
  }
}

void EncoderInput::pollButton() {
  const bool raw = readButtonRaw();
  const uint32_t now = nowMs();

  // Restart the settling window on every raw change.
  if (raw != rawButton_) {
    rawButton_ = raw;
    lastEdgeMs_ = now;
  }

  const bool settled = (now - lastEdgeMs_) >= static_cast<uint32_t>(ktsu::Input::debounceMs);
  if (settled && raw != stableButton_) {
    stableButton_ = raw;
    if (!stableButton_) {
      // Pressed (active low).
      pressStartMs_ = now;
      longPressSent_ = false;
    } else {
      // Released: a hold that already fired LongPress must not also Click.
      if (!longPressSent_ && listener_) {
        EncoderEvent e{EncoderEventType::Click, 0};
        listener_(e);
      }
      longPressSent_ = false;
    }
  }

  // Fire LongPress while still held, once the threshold is crossed.
  if (!stableButton_ && !longPressSent_ &&
      (now - pressStartMs_) >= static_cast<uint32_t>(ktsu::Input::longPressMs)) {
    longPressSent_ = true;
    if (listener_) {
      EncoderEvent e{EncoderEventType::LongPress, 0};
      listener_(e);
    }
  }
}

} } } // namespaces
