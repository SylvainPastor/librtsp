#include <gtest/gtest.h>

#include <atomic>
#include <librtsp/event_loop.hpp>
#include <thread>

TEST(EventLoopTest, ConstructsWithPrivateContext) {
  librtsp::EventLoop loop;
  EXPECT_NE(loop.context(), nullptr);
}

TEST(EventLoopTest, DifferentInstancesHaveDifferentContexts) {
  librtsp::EventLoop a;
  librtsp::EventLoop b;
  EXPECT_NE(a.context(), b.context());
}

TEST(EventLoopTest, RunQuitOnMainThread) {
  librtsp::EventLoop loop;

  // One-shot timer that quits the loop. Source is attached now and will
  // fire as soon as the loop dispatches it.
  auto stopper = loop.create_timeout_ms(10, [&loop] {
    loop.quit();
    return false;
  });

  loop.loop();  // returns when stopper fires
  SUCCEED();
}

TEST(EventLoopTest, RunOnDedicatedThread) {
  librtsp::EventLoop loop;

  auto stopper = loop.create_timeout_ms(20, [&loop] {
    loop.quit();
    return false;
  });

  std::thread thr([&loop] { loop.loop(); });
  thr.join();
  SUCCEED();
}

TEST(EventLoopTest, MultipleLoopsRunConcurrently) {
  librtsp::EventLoop a;
  librtsp::EventLoop b;
  std::atomic<int> a_count{0};
  std::atomic<int> b_count{0};

  auto ta = a.create_timeout_ms(10, [&a_count] {
    a_count.fetch_add(1);
    return true;
  });
  auto sa = a.create_timeout_ms(80, [&a] {
    a.quit();
    return false;
  });

  auto tb = b.create_timeout_ms(10, [&b_count] {
    b_count.fetch_add(1);
    return true;
  });
  auto sb = b.create_timeout_ms(80, [&b] {
    b.quit();
    return false;
  });

  std::thread thr_a([&a] { a.loop(); });
  std::thread thr_b([&b] { b.loop(); });
  thr_a.join();
  thr_b.join();

  EXPECT_GE(a_count.load(), 1);
  EXPECT_GE(b_count.load(), 1);
}
