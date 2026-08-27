// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "../ble/RaceboxData.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ktsu { namespace racebox { namespace ui {

// Session and lap timing driven purely by GPS time-of-week from the telemetry
// stream, so timing is independent of when notifications happen to be handled
// and of any drift in the local clock.
//
// Free of ESP-IDF dependencies -- unit tested in test/host/TestLapTimer.cpp.
class LapTimer {
 public:
  static constexpr size_t kMaxLaps = 64;

  enum class State { Idle, Running };

  // Feed every decoded telemetry sample. Ignored while idle.
  void update(const ktsu::racebox::ble::RaceboxData& data);

  // Begin a session anchored at the most recent sample. No-op if running.
  void start();
  // End the session, keeping recorded laps for review. No-op if idle.
  void stop();
  // Close the current lap and open a new one. No-op if idle.
  void lap();
  // Discard all recorded laps and return to idle.
  void reset();

  State state() const { return state_; }
  bool running() const { return state_ == State::Running; }

  // Elapsed session time in milliseconds.
  uint32_t sessionMs() const;
  // Time in the lap currently being timed.
  uint32_t currentLapMs() const;

  const std::vector<uint32_t>& laps() const { return laps_; }
  size_t lapCount() const { return laps_.size(); }
  // Index of the fastest completed lap, or -1 when none are recorded.
  int bestLapIndex() const;
  uint32_t bestLapMs() const;
  uint32_t lastLapMs() const;

  // Peak speed seen during the session.
  float topSpeedKmh() const { return topSpeedKmh_; }
  bool hasSample() const { return hasSample_; }

 private:
  // GPS time-of-week wraps once a week; treat a backwards jump as a rollover.
  static uint32_t elapsed(uint32_t fromTow, uint32_t toTow);

  State state_ = State::Idle;
  bool hasSample_ = false;
  uint32_t latestTowMs_ = 0;
  uint32_t sessionStartTowMs_ = 0;
  uint32_t lapStartTowMs_ = 0;
  float topSpeedKmh_ = 0.0f;
  std::vector<uint32_t> laps_;
};

} } } // namespaces
