// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#include "LapTimer.hpp"

#include <algorithm>

namespace ktsu { namespace racebox { namespace ui {

namespace {
// GPS time of week spans one week in milliseconds.
constexpr uint32_t kTowWrapMs = 7u * 24u * 60u * 60u * 1000u;
} // namespace

uint32_t LapTimer::elapsed(uint32_t fromTow, uint32_t toTow) {
  if (toTow >= fromTow) return toTow - fromTow;
  // Week rollover between the two samples.
  return (kTowWrapMs - fromTow) + toTow;
}

void LapTimer::update(const ktsu::racebox::ble::RaceboxData& data) {
  latestTowMs_ = data.iTowMs;
  hasSample_ = true;
  if (state_ == State::Running && data.speedKmh > topSpeedKmh_) {
    topSpeedKmh_ = data.speedKmh;
  }
}

void LapTimer::start() {
  if (state_ == State::Running) return;
  state_ = State::Running;
  sessionStartTowMs_ = latestTowMs_;
  lapStartTowMs_ = latestTowMs_;
  frozenSessionMs_ = 0;
  frozenLapMs_ = 0;
  topSpeedKmh_ = 0.0f;
  laps_.clear();
}

void LapTimer::stop() {
  if (state_ != State::Running) return;
  // Capture before leaving Running: the getters below read the live elapsed
  // time only in that state, and further samples must not move these.
  frozenSessionMs_ = elapsed(sessionStartTowMs_, latestTowMs_);
  frozenLapMs_ = elapsed(lapStartTowMs_, latestTowMs_);
  state_ = State::Stopped;
}

void LapTimer::lap() {
  if (state_ != State::Running) return;
  if (laps_.size() >= kMaxLaps) return; // keep memory bounded on a long session
  laps_.push_back(elapsed(lapStartTowMs_, latestTowMs_));
  lapStartTowMs_ = latestTowMs_;
}

void LapTimer::reset() {
  state_ = State::Idle;
  laps_.clear();
  topSpeedKmh_ = 0.0f;
  sessionStartTowMs_ = latestTowMs_;
  lapStartTowMs_ = latestTowMs_;
  frozenSessionMs_ = 0;
  frozenLapMs_ = 0;
}

uint32_t LapTimer::sessionMs() const {
  if (state_ == State::Stopped) return frozenSessionMs_;
  if (state_ != State::Running) return 0;
  return elapsed(sessionStartTowMs_, latestTowMs_);
}

uint32_t LapTimer::currentLapMs() const {
  if (state_ == State::Stopped) return frozenLapMs_;
  if (state_ != State::Running) return 0;
  return elapsed(lapStartTowMs_, latestTowMs_);
}

int LapTimer::bestLapIndex() const {
  if (laps_.empty()) return -1;
  const auto it = std::min_element(laps_.begin(), laps_.end());
  return static_cast<int>(std::distance(laps_.begin(), it));
}

uint32_t LapTimer::bestLapMs() const {
  const int idx = bestLapIndex();
  return idx < 0 ? 0u : laps_[static_cast<size_t>(idx)];
}

uint32_t LapTimer::lastLapMs() const { return laps_.empty() ? 0u : laps_.back(); }

} } } // namespaces
