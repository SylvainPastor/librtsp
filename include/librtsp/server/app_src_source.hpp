#pragma once

#include <gst/gst.h>

#include <atomic>
#include <cstdint>
#include <librtsp/export.hpp>
#include <librtsp/server/source.hpp>
#include <mutex>
#include <string>

namespace librtsp {

/// @brief Source that exposes a GStreamer appsrc element so frames produced
/// outside GStreamer (ROS 2 topics, OpenCV captures, custom producers)
/// can be pushed into the RTSP stream.
///
/// The library hides all GStreamer machinery: the user constructs an
/// AppSrcSource describing the input format and codec, hands it to the
/// Server, and calls push_frame() from any thread when a new frame is
/// available. The appsrc element is created when the first RTSP client
/// connects and released when the last one disconnects; frames pushed
/// while no client is connected are dropped.
class LIBRTSP_API AppSrcSource : public Source {
 public:
  struct Config {
    /// Output codec used by the encoder/payloader pair.
    Codec codec = Codec::H264;

    /// GStreamer caps describing the format of pushed buffers.
    /// Example: "video/x-raw,format=BGR,width=640,height=480,framerate=30/1"
    std::string caps =
        "video/x-raw,format=BGR,width=640,height=480,framerate=30/1";

    /// If true, push_frame() blocks when appsrc has signalled
    /// "enough-data" instead of dropping the frame.
    bool block_on_full = false;
  };

  explicit AppSrcSource(Config cfg);
  ~AppSrcSource() override;

  AppSrcSource(const AppSrcSource&) = delete;
  AppSrcSource& operator=(const AppSrcSource&) = delete;

  std::string pipeline() const override;

  /// @brief Push one frame from CPU memory. Thread-safe.
  /// @param data    Pointer to the frame bytes; copied internally.
  /// @param size    Length of @p data in bytes.
  /// @param pts_ns  Optional presentation timestamp in nanoseconds.
  ///                Negative means "let appsrc auto-stamp".
  /// @return true if the frame was queued onto the appsrc; false if no
  ///         RTSP session is active or appsrc backpressure dropped it.
  bool push_frame(const std::uint8_t* data, std::size_t size,
                  std::int64_t pts_ns = -1);

  // Source hooks (invoked by Stream).
  void on_media_configured(GstElement* media_bin) override;
  void on_media_destroyed() override;

 private:
  static void on_need_data_cb(GstElement* src, guint length, gpointer user);
  static void on_enough_data_cb(GstElement* src, gpointer user);

  Config cfg_;
  std::string element_name_;

  std::mutex mutex_;
  GstElement* appsrc_{nullptr};  // ref held while attached
  std::atomic<bool> enough_data_{false};
};

}  // namespace librtsp
