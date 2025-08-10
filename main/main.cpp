// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#include "ble/RaceBoxClient.hpp"
#include "io/EncoderInput.hpp"
#include "ui/Display.hpp"

#include "common/IdfCompat.hpp"

using ktsu::racebox::ble::RaceBoxClient;
using ktsu::racebox::io::EncoderInput;
using ktsu::racebox::ui::Display;

static const char* TAG = "app";

static Display s_display;
static EncoderInput s_encoderInput(/*pinA=*/2, /*pinB=*/1, /*pinButton=*/3);
static RaceBoxClient s_raceboxClient;

static void onEncoderEvent(const ktsu::racebox::io::EncoderEvent& event) {
  switch (event.type) {
    case ktsu::racebox::io::EncoderEventType::Rotate: s_display.onRotate(event.delta); break;
    case ktsu::racebox::io::EncoderEventType::Click: s_display.onClick(); break;
    case ktsu::racebox::io::EncoderEventType::LongPress: s_display.onLongPress(); break;
  }
}

static void onRaceboxData(const ktsu::racebox::ble::RaceboxData& data) { s_display.updateTelemetry(data); }

extern "C" void app_main(void) {
  ESP_LOGI(TAG, "RaceBox (ESP-IDF) starting...");

  s_display.begin();
  s_encoderInput.begin();
  s_encoderInput.setListener(onEncoderEvent);
  s_raceboxClient.setTelemetryListener(onRaceboxData);
  s_raceboxClient.begin();

  while (true) {
    s_encoderInput.tick();
    s_raceboxClient.loop();
    s_display.loop();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}


