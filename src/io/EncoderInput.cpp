// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#include "EncoderInput.hpp"
#include <Arduino.h>

using namespace ktsu::racebox::io;

EncoderInput::EncoderInput(int pinA, int pinB, int pinButton)
  : encoder_(pinA, pinB, RotaryEncoder::LatchMode::FOUR3), buttonPin_(pinButton) {}

void EncoderInput::begin() {
  pinMode(buttonPin_, INPUT_PULLUP);
  lastBtn_ = digitalRead(buttonPin_);
}

void EncoderInput::tick() {
  encoder_.tick();
  int pos = encoder_.getPosition();
  int delta = pos - lastPos_;
  if (delta != 0) {
    lastPos_ = pos;
    if (listener_) {
      EncoderEvent event;
      event.type = EncoderEventType::Rotate;
      event.delta = delta;
      listener_(event);
    }
  }

  bool btn = digitalRead(buttonPin_);
  uint32_t now = millis();
  if (!btn && lastBtn_) {
    pressStartMs_ = now;
  }
  if (btn && !lastBtn_) {
    uint32_t held = now - pressStartMs_;
    if (listener_) {
      EncoderEvent event;
      event.type = (held > 600) ? EncoderEventType::LongPress : EncoderEventType::Click;
      event.delta = 0;
      listener_(event);
    }
  }
  lastBtn_ = btn;
}


