#pragma once

#include <cstdint>
#include <librtsp/export.hpp>
#include <librtsp/source.hpp>
#include <memory>
#include <string>

namespace librtsp {

class Stream;

/// @brief RTSP server.
///
/// Wraps a GstRTSPServer. Each Server owns a private EventLoop so multiple
/// servers can run independently in the same process. start() spawns a
/// dedicated thread that iterates the loop until stop() is called.
///
/// Stream registration (mounting media factories on endpoints) is handled
/// via add_stream / remove_stream; the returned Stream is also retained by
/// the Server.
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
  bool is_started() const;

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

  /// @brief Called by Stream::remove() to drop the matching entry from
  /// the internal streams map.
  void detach_stream(const std::string& endpoint);

  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace librtsp
