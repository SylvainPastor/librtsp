#include <librtsp/common/event_loop.hpp>
#include <stdexcept>
#include <utility>

namespace librtsp {

EventLoop::EventLoop() {
  ctx_ = g_main_context_new();
  if (!ctx_) {
    throw std::runtime_error("Failed to create GMainContext");
  }
  loop_ = g_main_loop_new(ctx_, FALSE /*is_running*/);
  if (!loop_) {
    g_main_context_unref(ctx_);
    ctx_ = nullptr;
    throw std::runtime_error("Failed to create GMainLoop");
  }
}

EventLoop::~EventLoop() {
  if (loop_) {
    g_main_loop_unref(loop_);
    loop_ = nullptr;
  }
  if (ctx_) {
    g_main_context_unref(ctx_);
    ctx_ = nullptr;
  }
}

void EventLoop::loop() {
  g_main_context_push_thread_default(ctx_);
  g_main_loop_run(loop_);
  g_main_context_pop_thread_default(ctx_);
}

void EventLoop::quit() { g_main_loop_quit(loop_); }

Timer EventLoop::create_timeout_ms(guint interval_ms, Timer::Callback cb) {
  GSource* src = g_timeout_source_new(interval_ms);
  return Timer(src, ctx_, std::move(cb));
}

Timer EventLoop::create_timeout_s(guint interval_s, Timer::Callback cb) {
  GSource* src = g_timeout_source_new_seconds(interval_s);
  return Timer(src, ctx_, std::move(cb));
}

}  // namespace librtsp
