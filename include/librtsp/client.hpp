#pragma once

#include <cstdint>
#include <librtsp/export.hpp>
#include <memory>
#include <optional>
#include <string>

namespace librtsp {

class Sink;

/// @brief Stream-level metadata. Polled via Client::stream_info().
///
/// In this iteration only `codec` is populated (from the rtspsrc
/// pad-added signal, derived from RTP caps). `width`, `height` and
/// `framerate` are best-effort and may stay zero unless the active Sink
/// also publishes them.
struct StreamInfo {
  std::string codec;  ///< RTP encoding-name (e.g. "H264", "H265").
  int width = 0;
  int height = 0;
  double framerate = 0.0;
};

/// @brief RTSP client. Connects to one URL, decodes, and feeds a Sink.
class LIBRTSP_API Client {
 public:
  enum class Transport {
    Auto,
    Tcp,
    Udp,
  };

  struct Config {
    /// rtsp://host:port/path
    std::string url;
    /// rtpjitterbuffer latency in milliseconds.
    std::uint32_t latency_ms = 100;
    Transport transport = Transport::Auto;
  };

  Client();
  explicit Client(Config cfg);
  ~Client();

  Client(const Client&) = delete;
  Client& operator=(const Client&) = delete;

  /// @brief Plug a Sink. Must be called before start().
  void set_sink(std::shared_ptr<Sink> sink);

  /// @brief Parse the pipeline, attach to the EventLoop, transition to
  /// PLAYING, and spawn the worker thread. No-op if already started.
  /// @return false if no sink is set or pipeline construction failed.
  bool start();

  /// @brief Quit the loop, join the worker, transition the pipeline to
  /// NULL, and drop refs.
  void stop();

  /// @brief True while the worker thread is running.
  bool is_started() const;

  /// @brief Latest known stream info, or nullopt before the first SDP is
  /// received.
  std::optional<StreamInfo> stream_info() const;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace librtsp
