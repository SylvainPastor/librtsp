#include "timer.hpp"

#include <utility>

namespace librtsp {

namespace {

gboolean trampoline(gpointer user_data) {
  auto *fn = static_cast<Timer::Callback *>(user_data);
  return (*fn)() ? TRUE : FALSE;
}

void destroy_holder(gpointer user_data) {
  delete static_cast<Timer::Callback *>(user_data);
}

}  // namespace

Timer::Timer(GSource *src, GMainContext *ctx, Callback cb) : src_(src) {
  auto *holder = new Callback(std::move(cb));
  g_source_set_callback(src_, &trampoline, holder, &destroy_holder);
  g_source_attach(src_, ctx);
  // Keep our own ref so src_ stays valid until stop()/dtor.
  // The context holds its own ref (added by g_source_attach).
}

Timer::~Timer() { stop(); }

Timer::Timer(Timer &&other) noexcept : src_(other.src_) {
  other.src_ = nullptr;
}

Timer &Timer::operator=(Timer &&other) noexcept {
  if (this != &other) {
    stop();
    src_ = other.src_;
    other.src_ = nullptr;
  }
  return *this;
}

bool Timer::is_running() const {
  return src_ != nullptr && !g_source_is_destroyed(src_);
}

void Timer::stop() {
  if (!src_) {
    return;
  }
  if (!g_source_is_destroyed(src_)) {
    g_source_destroy(src_);
  }
  g_source_unref(src_);
  src_ = nullptr;
}

}  // namespace librtsp
