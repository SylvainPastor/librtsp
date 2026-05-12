#pragma once

#include <cstdint>
#include <functional>
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

  /// @brief High-level connection state, polled via state().
  enum class State {
    Idle,        ///< Before start() or after stop().
    Connecting,  ///< start() succeeded; waiting for the RTSP handshake.
    Connected,   ///< rtspsrc emitted a pad, SDP received, media flowing.
    Error,       ///< Pipeline build failed or the bus reported an error.
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

  /// @brief Current connection state. Thread-safe.
  State state() const;

  /// @brief Last error message captured from the pipeline (empty if none).
  /// Set when state() == Error.
  std::string last_error() const;

  /// @brief Callable invoked when the connection state changes.
  /// Receives `(old_state, new_state)`. Fires on the thread that triggered
  /// the transition (worker thread for pad-added / bus errors, caller's
  /// thread for start/stop). Several callbacks may run concurrently if
  /// they're triggered by transitions on different threads, so the user
  /// is responsible for any further synchronization the callback needs.
  using StateCallback = std::function<void(State old_state, State new_state)>;

  /// @brief Opaque id returned by subscribe_state(), passed to
  /// unsubscribe_state().
  using SubscriptionId = std::uint64_t;

  /// @brief Subscribe to state-change notifications. Thread-safe.
  /// @return Subscription id (non-zero) usable to remove the callback.
  SubscriptionId subscribe_state(StateCallback callback);

  /// @brief Remove a previously-registered callback. Thread-safe.
  /// After this returns, the callback will not be invoked for any
  /// *subsequent* state change. If the callback is currently running on
  /// another thread, that invocation completes normally.
  /// @return true if a matching subscription existed and was removed.
  bool unsubscribe_state(SubscriptionId id);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

/// @brief Human-readable name for a Client::State.
LIBRTSP_API const char* to_string(Client::State state);

}  // namespace librtsp
