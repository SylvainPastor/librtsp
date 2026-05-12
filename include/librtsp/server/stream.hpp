#pragma once

#include <gst/rtsp-server/rtsp-server.h>

#include <atomic>
#include <librtsp/export.hpp>
#include <librtsp/server/source.hpp>
#include <memory>
#include <string>

namespace librtsp {

class Server;

/// @brief One RTSP endpoint mounted on a Server.
///
/// A Stream binds an endpoint (e.g. "/cam") to a Source. It owns the
/// underlying GstRTSPMediaFactory while mounted, and wires the
/// "media-configure" and "unprepared" signals so the Source's
/// on_media_configured / on_media_destroyed hooks are invoked at the
/// right time (e.g. AppSrcSource uses these to attach/detach its appsrc).
///
/// Streams are obtained from Server::add_stream(). Both the Server and
/// the caller keep a shared_ptr to the Stream; either remove() or
/// Server::remove_stream(endpoint) unmounts it.
class LIBRTSP_API Stream {
 public:
  ~Stream();

  Stream(const Stream&) = delete;
  Stream& operator=(const Stream&) = delete;

  /// @brief The mount endpoint.
  const std::string& endpoint() const { return endpoint_; }

  /// @brief Source feeding this stream.
  const std::shared_ptr<Source>& source() const { return source_; }

  /// @brief True until remove() is called.
  bool is_active() const { return mounted_.load(); }

  /// @brief Unmount this stream from its Server. Idempotent.
  ///
  /// After remove(), no new clients can connect to the endpoint. Existing
  /// client sessions continue until they disconnect normally; the Stream
  /// object must stay alive (via the Server's map or the caller's
  /// shared_ptr) until then so the "unprepared" signal handler can fire.
  void remove();

 private:
  friend class Server;

  Stream(GstRTSPServer* gst_server, Server* owner, std::string endpoint,
         std::shared_ptr<Source> source);

  void unmount_factory();

  static void on_media_configure_cb(GstRTSPMediaFactory* factory,
                                    GstRTSPMedia* media, gpointer user);
  static void on_media_unprepared_cb(GstRTSPMedia* media, gpointer user);

  GstRTSPServer* gst_server_{nullptr};  // not owned
  Server* owner_{nullptr};              // back-pointer for detach
  std::string endpoint_;
  std::shared_ptr<Source> source_;
  std::atomic<bool> mounted_{false};
};

}  // namespace librtsp
