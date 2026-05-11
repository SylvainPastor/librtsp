#pragma once

#include <glib.h>

#include <functional>

#include <librtsp/export.hpp>

namespace librtsp {

class EventLoop;

/// @brief RAII wrapper around a GLib timeout source.
///
/// A Timer owns its underlying GSource. Destroying or moving the Timer
/// stops the underlying timer. Timers are obtained from EventLoop's
/// factory methods (create_timeout_ms / create_timeout_s).
class LIBRTSP_API Timer {
 public:
  /// @brief Callable invoked on each tick.
  /// Return true to keep the timer running, false to remove it.
  using Callback = std::function<bool()>;

  /// @brief Default constructor: empty Timer, no source attached.
  Timer() = default;

  /// @brief Stops the timer if still running.
  ~Timer();

  Timer(const Timer &) = delete;
  Timer &operator=(const Timer &) = delete;

  Timer(Timer &&other) noexcept;
  Timer &operator=(Timer &&other) noexcept;

  /// @brief True while the underlying source is attached and not destroyed.
  /// Returns false after the callback returned false, or after stop().
  bool is_running() const;

  /// @brief Stop the timer. Safe to call repeatedly or on an empty Timer.
  void stop();

 private:
  friend class EventLoop;

  /// @brief Internal constructor used by EventLoop factory methods.
  /// Takes ownership of @p src (refcount 1) and attaches it to @p ctx.
  Timer(GSource *src, GMainContext *ctx, Callback cb);

  /// @brief Owned GSource while attached. Null when empty.
  GSource *src_{nullptr};
};

}  // namespace librtsp
