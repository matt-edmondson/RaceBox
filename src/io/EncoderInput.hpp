// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <RotaryEncoder.h>
#include <functional>

namespace ktsu { namespace racebox { namespace io {

enum class EncoderEventType { Rotate, Click, LongPress };

struct EncoderEvent {
  EncoderEventType type;
  int32_t delta = 0;
};

class EncoderInput {
 public:
  using Listener = std::function<void(const EncoderEvent&)>;
  EncoderInput(int pinA, int pinB, int pinButton);
  void begin();
  void tick();
  void setListener(Listener listener) { listener_ = listener; }

 private:
  RotaryEncoder encoder_;
  int buttonPin_;
  int lastPos_ = 0;
  bool lastBtn_ = true;
  uint32_t pressStartMs_ = 0;
  Listener listener_;
};

} } } // namespaces


