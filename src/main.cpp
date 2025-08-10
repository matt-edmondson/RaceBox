// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#include <Arduino.h>
#include "config/Pins.hpp"
#include "config/BleUuids.hpp"
#include "io/EncoderInput.hpp"
#include "ui/Display.hpp"
#include "ble/RaceBoxClient.hpp"

using ktsu::racebox::ble::RaceBoxClient;
using ktsu::racebox::io::EncoderInput;
using ktsu::racebox::ui::Display;

static Display display;
static EncoderInput encoderInput(ktsu::Pins::encoderPinA, ktsu::Pins::encoderPinB, ktsu::Pins::encoderButtonPin);
static RaceBoxClient raceboxClient;

void onEncoderEvent(const ktsu::racebox::io::EncoderEvent &event) {
  if (event.type == ktsu::racebox::io::EncoderEventType::Rotate) {
    display.onRotate(event.delta);
  } else if (event.type == ktsu::racebox::io::EncoderEventType::Click) {
    display.onClick();
  } else if (event.type == ktsu::racebox::io::EncoderEventType::LongPress) {
    display.onLongPress();
  }
}

void onRaceboxData(const ktsu::racebox::ble::RaceboxData &data) {
  display.updateTelemetry(data);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("RaceBox Mini Interface starting...");

  display.begin();
  encoderInput.begin();
  encoderInput.setListener(onEncoderEvent);

  raceboxClient.setTelemetryListener(onRaceboxData);
  raceboxClient.begin();
}

void loop() {
  encoderInput.tick();
  raceboxClient.loop();
  display.loop();
}


