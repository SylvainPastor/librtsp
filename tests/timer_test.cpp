#include <gtest/gtest.h>

#include <atomic>
#include <librtsp/common/event_loop.hpp>
#include <librtsp/common/timer.hpp>
#include <thread>
#include <utility>

TEST(TimerTest, DefaultConstructedIsNotRunning) {
  librtsp::Timer t;
  EXPECT_FALSE(t.is_running());
}

TEST(TimerTest, CreatedTimerIsRunning) {
  librtsp::EventLoop loop;
  auto t = loop.create_timeout_ms(1000, [] { return true; });
  EXPECT_TRUE(t.is_running());
}

TEST(TimerTest, StopMakesNotRunning) {
  librtsp::EventLoop loop;
  auto t = loop.create_timeout_ms(1000, [] { return true; });
  t.stop();
  EXPECT_FALSE(t.is_running());
}

TEST(TimerTest, StopIsIdempotent) {
  librtsp::EventLoop loop;
  auto t = loop.create_timeout_ms(1000, [] { return true; });
  t.stop();
  t.stop();  // must not crash
  EXPECT_FALSE(t.is_running());
}

TEST(TimerTest, MoveConstructTransfersOwnership) {
  librtsp::EventLoop loop;
  auto t1 = loop.create_timeout_ms(1000, [] { return true; });
  EXPECT_TRUE(t1.is_running());

  librtsp::Timer t2 = std::move(t1);
  EXPECT_FALSE(t1.is_running());
  EXPECT_TRUE(t2.is_running());
}

TEST(TimerTest, MoveAssignStopsTargetAndTakesOwnership) {
  librtsp::EventLoop loop;
  auto t1 = loop.create_timeout_ms(1000, [] { return true; });
  auto t2 = loop.create_timeout_ms(1000, [] { return true; });
  EXPECT_TRUE(t1.is_running());
  EXPECT_TRUE(t2.is_running());

  t1 = std::move(t2);
  EXPECT_TRUE(t1.is_running());
  EXPECT_FALSE(t2.is_running());
}

TEST(TimerTest, CallbackFiresMultipleTimes) {
  librtsp::EventLoop loop;
  std::atomic<int> count{0};

  auto t = loop.create_timeout_ms(10, [&count] {
    count.fetch_add(1);
    return true;
  });
  auto stopper = loop.create_timeout_ms(100, [&loop] {
    loop.quit();
    return false;
  });

  std::thread thr([&loop] { loop.loop(); });
  thr.join();

  EXPECT_GE(count.load(), 2);
}

TEST(TimerTest, CallbackReturningFalseStopsTimer) {
  librtsp::EventLoop loop;
  std::atomic<int> count{0};

  auto t = loop.create_timeout_ms(10, [&count] {
    count.fetch_add(1);
    return false;
  });
  auto stopper = loop.create_timeout_ms(80, [&loop] {
    loop.quit();
    return false;
  });

  std::thread thr([&loop] { loop.loop(); });
  thr.join();

  EXPECT_EQ(count.load(), 1);
  EXPECT_FALSE(t.is_running());
}

TEST(TimerTest, ScopeExitStopsTimerBeforeItFires) {
  librtsp::EventLoop loop;
  std::atomic<int> count{0};

  {
    auto t = loop.create_timeout_ms(10, [&count] {
      count.fetch_add(1);
      return true;
    });
    EXPECT_TRUE(t.is_running());
  }
  // Timer destroyed before the loop runs; the callback must not fire.
  auto stopper = loop.create_timeout_ms(60, [&loop] {
    loop.quit();
    return false;
  });

  std::thread thr([&loop] { loop.loop(); });
  thr.join();

  EXPECT_EQ(count.load(), 0);
}
