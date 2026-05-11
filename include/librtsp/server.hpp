#pragma once

#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>

#include <atomic>
#include <cstdint>
#include <librtsp/export.hpp>
#include <librtsp/source.hpp>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "event_loop.hpp"
#include "timer.hpp"

namespace librtsp {

class Stream;

/// @brief RTSP server.
///
/// Wraps a GstRTSPServer. Each Server owns a private EventLoop so multiple
/// servers can run independently in the same process. The server is
/// attached to its loop's context at construction time; start() spawns a
/// dedicated thread that iterates the loop until stop() is called.
///
/// Stream registration (mounting media factories on endpoints) is handled
/// by the upcoming RtspStream class.
class LIBRTSP_API Server {
 public:
  /// @brief Default constructor. Listens on 0.0.0.0:554.
  Server();

  /// @brief Construct with explicit address and port.
  /// @param address IP address to bind. Empty defaults to 0.0.0.0.
  /// @param port    Port to listen on. 0 defaults to 554.
  Server(const std::string& address, uint16_t port);

  ~Server();

  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;

  /// @brief Spin up the event-loop thread. No-op if already started.
  bool start();

  /// @brief Quit the loop, join its thread, drop active client sessions.
  void stop();

  /// @brief True while the server's loop thread is running.
  bool is_started() const { return running_.load(); }

  /// @brief Internal: invoked when a new client connects.
  void on_new_client_connected(GstRTSPClient* client);

  /// @brief Internal: invoked when a client disconnects or times out.
  void on_client_disconnected(GstRTSPClient* client);

  /// @brief Mount a Source on this Server at the given endpoint.
  /// The returned Stream is also retained by the Server until the endpoint
  /// is removed (via Stream::remove() or remove_stream()).
  /// @throws std::runtime_error if the endpoint is already mounted.
  std::shared_ptr<Stream> add_stream(const std::string& endpoint,
                                     std::shared_ptr<Source> source);

  /// @brief Remove a stream by endpoint. Returns true if a stream was
  /// found and unmounted.
  bool remove_stream(const std::string& endpoint);

 private:
  friend class Stream;

  void create();
  void destroy();
  void attach();
  void clear_clients_session();
  void run();
  void log_client(GstRTSPClient* client, bool connected);

  // Called by Stream::remove() to drop the matching entry from streams_.
  void detach_stream(const std::string& endpoint);

  // Configuration.
  uint16_t port_{554};
  std::string address_{"0.0.0.0"};

  // GStreamer resources.
  GstRTSPServer* server_{nullptr};

  // Loop and worker thread.
  std::unique_ptr<EventLoop> loop_;
  std::unique_ptr<std::thread> thread_;
  std::atomic<bool> running_{false};

  // Source / timer state.
  bool attached_{false};
  guint source_id_{0};
  Timer cleanup_timer_;

  // Client tracking.
  std::mutex client_mutex_;
  uint8_t client_count_{0};

  // Mounted streams (endpoint -> stream).
  std::mutex streams_mutex_;
  std::map<std::string, std::shared_ptr<Stream>> streams_;
};

}  // namespace librtsp
