// Unit tests for session / lap timing.
#include "TestMain.hpp"

#include "../../main/ui/LapTimer.hpp"

using ktsu::racebox::ble::RaceboxData;
using ktsu::racebox::ui::LapTimer;

namespace {
RaceboxData sampleAt(uint32_t towMs, float speedKmh = 0.0f) {
  RaceboxData d{};
  d.iTowMs = towMs;
  d.speedKmh = speedKmh;
  return d;
}
} // namespace

TEST(StartsIdleAndIgnoresSamplesUntilStarted) {
  LapTimer t;
  CHECK(t.state() == LapTimer::State::Idle);
  t.update(sampleAt(1000, 50.0f));
  CHECK_EQ(t.sessionMs(), uint32_t{0});
  CHECK_EQ(t.currentLapMs(), uint32_t{0});
  CHECK_NEAR(t.topSpeedKmh(), 0.0, 1e-6);
}

TEST(SessionTimeTracksGpsTimeOfWeek) {
  LapTimer t;
  t.update(sampleAt(10'000));
  t.start();
  t.update(sampleAt(12'500));
  CHECK_EQ(t.sessionMs(), uint32_t{2500});
  t.update(sampleAt(15'000));
  CHECK_EQ(t.sessionMs(), uint32_t{5000});
}

TEST(LapSplitsPartitionTheSession) {
  LapTimer t;
  t.update(sampleAt(0));
  t.start();

  t.update(sampleAt(30'000));
  t.lap();                       // lap 1 = 30s
  t.update(sampleAt(75'000));
  t.lap();                       // lap 2 = 45s
  t.update(sampleAt(100'000));   // 25s into lap 3

  CHECK_EQ(t.lapCount(), size_t{2});
  CHECK_EQ(t.laps()[0], uint32_t{30'000});
  CHECK_EQ(t.laps()[1], uint32_t{45'000});
  CHECK_EQ(t.currentLapMs(), uint32_t{25'000});
  CHECK_EQ(t.sessionMs(), uint32_t{100'000});
  CHECK_EQ(t.lastLapMs(), uint32_t{45'000});
}

TEST(TracksBestLap) {
  LapTimer t;
  t.update(sampleAt(0));
  t.start();
  t.update(sampleAt(50'000)); t.lap();   // 50s
  t.update(sampleAt(88'000)); t.lap();   // 38s  <- best
  t.update(sampleAt(130'000)); t.lap();  // 42s

  CHECK_EQ(t.lapCount(), size_t{3});
  CHECK_EQ(t.bestLapIndex(), 1);
  CHECK_EQ(t.bestLapMs(), uint32_t{38'000});
}

TEST(TracksTopSpeedOnlyWhileRunning) {
  LapTimer t;
  t.update(sampleAt(0, 200.0f));   // before start: must not count
  t.start();
  t.update(sampleAt(1000, 80.0f));
  t.update(sampleAt(2000, 145.5f));
  t.update(sampleAt(3000, 120.0f));
  CHECK_NEAR(t.topSpeedKmh(), 145.5, 1e-3);

  t.stop();
  t.update(sampleAt(4000, 300.0f)); // after stop: must not count
  CHECK_NEAR(t.topSpeedKmh(), 145.5, 1e-3);
}

TEST(StopFreezesTimingButKeepsLaps) {
  LapTimer t;
  t.update(sampleAt(0));
  t.start();
  t.update(sampleAt(20'000));
  t.lap();
  t.stop();

  CHECK(!t.running());
  CHECK_EQ(t.lapCount(), size_t{1});
  CHECK_EQ(t.laps()[0], uint32_t{20'000});
  CHECK_EQ(t.sessionMs(), uint32_t{0});  // not accumulating any more

  t.update(sampleAt(60'000));
  CHECK_EQ(t.sessionMs(), uint32_t{0});
  CHECK_EQ(t.lapCount(), size_t{1});     // laps survive for review
}

TEST(RestartClearsPreviousLaps) {
  LapTimer t;
  t.update(sampleAt(0));
  t.start();
  t.update(sampleAt(10'000)); t.lap();
  t.stop();
  CHECK_EQ(t.lapCount(), size_t{1});

  t.update(sampleAt(50'000));
  t.start();
  CHECK_EQ(t.lapCount(), size_t{0});
  CHECK_EQ(t.sessionMs(), uint32_t{0});
  t.update(sampleAt(53'000));
  CHECK_EQ(t.sessionMs(), uint32_t{3000});
}

TEST(HandlesGpsWeekRollover) {
  // Time of week wraps to 0 at the week boundary; a session spanning it must
  // not report a nonsense ~7-day elapsed time.
  const uint32_t weekMs = 7u * 24u * 60u * 60u * 1000u;
  LapTimer t;
  t.update(sampleAt(weekMs - 5000));
  t.start();
  t.update(sampleAt(2000)); // 7s later, past the rollover
  CHECK_EQ(t.sessionMs(), uint32_t{7000});
  CHECK_EQ(t.currentLapMs(), uint32_t{7000});
}

TEST(LapAndStartAreNoOpsInTheWrongState) {
  LapTimer t;
  t.update(sampleAt(1000));
  t.lap();                       // idle: ignored
  CHECK_EQ(t.lapCount(), size_t{0});

  t.start();
  t.update(sampleAt(5000));
  t.start();                     // already running: must not re-anchor
  CHECK_EQ(t.sessionMs(), uint32_t{4000});

  t.stop();
  t.stop();                      // idempotent
  CHECK(!t.running());
}

TEST(LapCountIsBounded) {
  LapTimer t;
  t.update(sampleAt(0));
  t.start();
  for (uint32_t i = 1; i <= LapTimer::kMaxLaps + 20; ++i) {
    t.update(sampleAt(i * 1000));
    t.lap();
  }
  CHECK_EQ(t.lapCount(), LapTimer::kMaxLaps);
}

TEST(ResetReturnsToIdleAndClears) {
  LapTimer t;
  t.update(sampleAt(0));
  t.start();
  t.update(sampleAt(9000, 99.0f));
  t.lap();
  t.reset();

  CHECK(!t.running());
  CHECK_EQ(t.lapCount(), size_t{0});
  CHECK_NEAR(t.topSpeedKmh(), 0.0, 1e-6);
}
