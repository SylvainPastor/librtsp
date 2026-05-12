#pragma once

#include <glib.h>

#include <librtsp/common/timer.hpp>
#include <librtsp/export.hpp>

namespace librtsp {

class LIBRTSP_API EventLoop {
 public:
  /// @brief Default constructor.
  /// Creates a private GMainContext so several EventLoop instances can
  /// coexist in the same process without contending for the global-default
  /// context.
  EventLoop();

  /// @brief Default destructor.
  virtual ~EventLoop();

  /// @brief Run loop.
  /// The private context is pushed as thread-default while the loop runs.
  void loop();

  /// @brief Stop loop.
  void quit();

  /// @brief The private GMainContext this loop iterates.
  /// Pass it to any GLib source or GStreamer attach call (e.g.
  /// gst_rtsp_server_attach) so the source is dispatched by this loop.
  GMainContext* context() const { return ctx_; }

  /// @brief Create and start a millisecond-resolution timer attached to
  /// this loop. Destroying the returned Timer stops the timer.
  /// @param interval_ms Timer interval in milliseconds.
  /// @param cb Callable invoked on each tick; return true to keep the
  ///           timer running, false to remove it.
  Timer create_timeout_ms(guint interval_ms, Timer::Callback cb);

  /// @brief Create and start a second-resolution timer attached to this
  /// loop. @see create_timeout_ms.
  Timer create_timeout_s(guint interval_s, Timer::Callback cb);

 private:
  /// @brief Private GMainContext owned by this loop.
  GMainContext* ctx_{nullptr};

  /// @brief Glib loop object.
  GMainLoop* loop_{nullptr};
};

}  // namespace librtsp
