#pragma once

#include <gst/gst.h>

#include <functional>
#include <librtsp/export.hpp>
#include <librtsp/frame.hpp>
#include <librtsp/sink.hpp>
#include <mutex>
#include <string>

namespace librtsp {

/// @brief Sink that delivers decoded frames to a user-supplied callback.
///
/// The library inserts a decodebin + videoconvert + capsfilter + appsink
/// chain after rtspsrc. Each new sample produced by the appsink fires the
/// user's callback with a Frame whose bytes are valid for the duration of
/// the call. To keep a frame for later processing, call Frame::retain()
/// inside the callback.
///
/// The callback runs on the Client's worker thread. The user is
/// responsible for marshalling to whatever main loop they need (Qt event
/// queue, ROS 2 callback group, etc.).
class LIBRTSP_API CallbackSink : public Sink {
 public:
  using Callback = std::function<void(const Frame&)>;

  struct Config {
    /// Requested raw-video format presented to the user (e.g. "BGR",
    /// "RGB", "I420"). The library inserts the right videoconvert/caps
    /// so the appsink receives this format.
    std::string output_format = "BGR";
    /// appsink "max-buffers" — bounds the queue between the pipeline and
    /// the user callback.
    int max_buffers = 1;
    /// Drop oldest frame instead of blocking when the queue is full.
    bool drop = true;
  };

  CallbackSink(Config cfg, Callback cb);
  ~CallbackSink() override;

  CallbackSink(const CallbackSink&) = delete;
  CallbackSink& operator=(const CallbackSink&) = delete;

  std::string pipeline() const override;
  void on_media_configured(GstElement* media_bin) override;
  void on_media_destroyed() override;

 private:
  static GstFlowReturn on_new_sample_cb(GstElement* sink, gpointer user);

  void dispatch(GstSample* sample);

  Config cfg_;
  Callback cb_;
  std::string element_name_;

  std::mutex mutex_;
  GstElement* appsink_{nullptr};  ///< Owns one ref while attached.
};

}  // namespace librtsp
