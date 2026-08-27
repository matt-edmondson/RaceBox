// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <cstdint>
#include <functional>

#if __has_include("driver/pulse_cnt.h")
  #define RACEBOX_HAVE_PCNT 1
  #include "driver/pulse_cnt.h"
#endif

namespace ktsu { namespace racebox { namespace io {

enum class EncoderEventType { Rotate, Click, LongPress };

struct EncoderEvent {
  EncoderEventType type;
  int32_t delta = 0; // detents; positive is clockwise
};

// Rotary encoder with hardware quadrature decoding and a debounced button.
//
// Rotation is counted by the PCNT peripheral with its glitch filter enabled, so
// detents are never lost between polls and contact bounce on the A/B lines is
// rejected in hardware. The button is debounced in software: its level must be
// stable for `debounceMs` before a transition is accepted.
//
// Long press is reported once, at the moment the hold threshold is crossed,
// rather than on release -- so the user gets feedback while still holding. A
// release that already produced a LongPress does not also produce a Click.
class EncoderInput {
 public:
  using Listener = std::function<void(const EncoderEvent&)>;

  EncoderInput(int pinA, int pinB, int pinButton);
  ~EncoderInput();

  EncoderInput(const EncoderInput&) = delete;
  EncoderInput& operator=(const EncoderInput&) = delete;

  // Returns false if the hardware could not be configured; the caller should
  // log and continue rather than aborting, so a wiring fault doesn't brick boot.
  bool begin();

  // Poll for rotation and button changes, emitting events to the listener.
  void tick();

  void setListener(Listener listener) { listener_ = std::move(listener); }

  bool ready() const { return ready_; }

 private:
  uint32_t nowMs() const;
  bool readButtonRaw() const;
  void pollRotation();
  void pollButton();

  int pinA_;
  int pinB_;
  int buttonPin_;
  bool ready_ = false;
  Listener listener_;

  // Rotation accumulator: leftover quadrature counts below one full detent.
  int32_t countRemainder_ = 0;

#ifdef RACEBOX_HAVE_PCNT
  pcnt_unit_handle_t pcntUnit_ = nullptr;
  pcnt_channel_handle_t pcntChanA_ = nullptr;
  pcnt_channel_handle_t pcntChanB_ = nullptr;
#else
  // Software fallback used when building outside ESP-IDF.
  int lastEncoded_ = 0;
#endif

  // Debounced button state.
  bool rawButton_ = true;     // true == released (idle-high with pull-up)
  bool stableButton_ = true;
  uint32_t lastEdgeMs_ = 0;
  uint32_t pressStartMs_ = 0;
  bool longPressSent_ = false;
};

} } } // namespaces
