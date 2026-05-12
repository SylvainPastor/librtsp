#include "server.hpp"

#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include "event_loop.hpp"
#include "gstreamer.hpp"
#include "logger.hpp"
#include "stream.hpp"
#include "timer.hpp"

namespace librtsp {

class Server::Impl {
 public:
  explicit Impl(Server* outer);
  Impl(Server* outer, const std::string& address, uint16_t port);
  ~Impl();

  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

  bool start();
  void stop();
  bool is_started() const { return running_.load(); }

  std::shared_ptr<Stream> add_stream(const std::string& endpoint,
                                     std::shared_ptr<Source> source);
  bool remove_stream(const std::string& endpoint);
  void detach_stream(const std::string& endpoint);

 private:
  void create();
  void destroy();
  void attach();
  void clear_clients_session();
  void run();
  void log_client(GstRTSPClient* client, bool connected);

  // Now private; only the static trampolines below call them.
  void on_new_client_connected(GstRTSPClient* client);
  void on_client_disconnected(GstRTSPClient* client);

  // GLib signal trampolines.
  static void cb_client_connected(GstRTSPServer*, GstRTSPClient*, gpointer);
  static void cb_client_closed(GstRTSPClient*, gpointer);
  static GstRTSPFilterResult client_filter_remove(GstRTSPServer*,
                                                  GstRTSPClient*, gpointer);

  // Back-reference for Stream's owner_ pointer.
  Server* outer_;

  uint16_t port_{554};
  std::string address_{"0.0.0.0"};

  GstRTSPServer* server_{nullptr};
  std::unique_ptr<EventLoop> loop_;
  std::unique_ptr<std::thread> thread_;
  std::atomic<bool> running_{false};

  bool attached_{false};
  guint source_id_{0};
  Timer cleanup_timer_;

  std::mutex client_mutex_;
  uint8_t client_count_{0};

  std::mutex streams_mutex_;
  std::map<std::string, std::shared_ptr<Stream>> streams_;
};

// ---------------- Server::Impl ----------------

Server::Impl::Impl(Server* outer) : outer_(outer) {
  loop_ = std::make_unique<EventLoop>();
  create();
  g_signal_connect(server_, "client-connected",
                   G_CALLBACK(&Impl::cb_client_connected), this);
  attach();
}

Server::Impl::Impl(Server* outer, const std::string& address, uint16_t port)
    : outer_(outer) {
  if (port != 0) {
    port_ = port;
  }
  if (!address.empty()) {
    address_ = address;
  }
  loop_ = std::make_unique<EventLoop>();
  create();
  g_signal_connect(server_, "client-connected",
                   G_CALLBACK(&Impl::cb_client_connected), this);
  attach();
}

Server::Impl::~Impl() {
  stop();
  {
    std::lock_guard<std::mutex> lock(streams_mutex_);
    streams_.clear();
  }
  destroy();
}

bool Server::Impl::start() {
  if (running_.load()) {
    return true;
  }
  thread_ = std::make_unique<std::thread>(&Impl::run, this);
  running_.store(true);
  return true;
}

void Server::Impl::stop() {
  if (thread_ && running_.load()) {
    loop_->quit();
    thread_->join();
    running_.store(false);
  }
  if (server_) {
    clear_clients_session();
  }
}

void Server::Impl::on_new_client_connected(GstRTSPClient* client) {
  std::lock_guard<std::mutex> lock(client_mutex_);
  ++client_count_;
  log_client(client, true);
  g_signal_connect(client, "closed", G_CALLBACK(&Impl::cb_client_closed), this);
}

void Server::Impl::on_client_disconnected(GstRTSPClient* client) {
  std::lock_guard<std::mutex> lock(client_mutex_);
  if (client_count_ > 0) {
    --client_count_;
  }
  log_client(client, false);
}

void Server::Impl::log_client(GstRTSPClient* client, bool connected) {
  GstRTSPConnection* connection = gst_rtsp_client_get_connection(client);
  if (!connection) {
    return;
  }
  GstRTSPUrl* url = gst_rtsp_connection_get_url(connection);
  if (!url) {
    return;
  }
  // Avoid g_str_has_suffix(NULL) assertion inside gst_rtsp_url_get_request_uri.
  if (!url->abspath) {
    url->abspath = g_strdup("");  // freed by gst_rtsp_url_free
  }
  gchar* uri = gst_rtsp_url_get_request_uri(url);
  const int count = static_cast<int>(client_count_);
  if (connected) {
    LIBRTSP_INFO("RTSP client " << uri << " connected, total=" << count);
  } else {
    LIBRTSP_INFO("RTSP client " << uri << " disconnected, total=" << count);
  }
  g_free(uri);
}

void Server::Impl::create() {
  if (server_) {
    return;
  }
  Gstreamer::init();
  server_ = gst_rtsp_server_new();
  if (!server_) {
    throw std::runtime_error("Failed to create GstRTSPServer");
  }
  g_object_set(server_, "service", std::to_string(port_).c_str(), nullptr);
  if (!address_.empty()) {
    g_object_set(server_, "address", address_.c_str(), nullptr);
  }
  gchar* addr = gst_rtsp_server_get_address(server_);
  gchar* svc = gst_rtsp_server_get_service(server_);
  LIBRTSP_INFO("RTSP server reachable at rtsp://" << addr << ":" << svc);
  g_free(addr);
  g_free(svc);
}

void Server::Impl::destroy() {
  if (!server_) {
    return;
  }
  if (attached_) {
    attached_ = false;
    GSource* src =
        g_main_context_find_source_by_id(loop_->context(), source_id_);
    if (src) {
      g_source_destroy(src);
    }
    source_id_ = 0;
    cleanup_timer_.stop();
  }
  g_object_unref(server_);
  server_ = nullptr;
}

void Server::Impl::attach() {
  if (attached_) {
    return;
  }
  source_id_ = gst_rtsp_server_attach(server_, loop_->context());
  if (source_id_ == 0) {
    throw std::runtime_error("Failed to attach RTSP server");
  }
  attached_ = true;

  cleanup_timer_ = loop_->create_timeout_s(2, [this] {
    GstRTSPSessionPool* pool = gst_rtsp_server_get_session_pool(server_);
    if (pool) {
      gst_rtsp_session_pool_cleanup(pool);
      g_object_unref(pool);
    }
    return true;
  });
}

void Server::Impl::clear_clients_session() {
  gst_rtsp_server_client_filter(server_, &Impl::client_filter_remove, nullptr);
}

void Server::Impl::run() {
  loop_->loop();
  running_.store(false);
}

std::shared_ptr<Stream> Server::Impl::add_stream(
    const std::string& endpoint, std::shared_ptr<Source> source) {
  std::lock_guard<std::mutex> lock(streams_mutex_);
  if (streams_.find(endpoint) != streams_.end()) {
    throw std::runtime_error("Stream endpoint already exists: " + endpoint);
  }
  auto stream = std::shared_ptr<Stream>(
      new Stream(server_, outer_, endpoint, std::move(source)));
  streams_.emplace(endpoint, stream);
  return stream;
}

bool Server::Impl::remove_stream(const std::string& endpoint) {
  std::shared_ptr<Stream> sp;
  {
    std::lock_guard<std::mutex> lock(streams_mutex_);
    auto it = streams_.find(endpoint);
    if (it == streams_.end()) {
      return false;
    }
    sp = std::move(it->second);
    streams_.erase(it);
  }
  sp->remove();
  return true;
}

void Server::Impl::detach_stream(const std::string& endpoint) {
  std::shared_ptr<Stream> sp;
  {
    std::lock_guard<std::mutex> lock(streams_mutex_);
    auto it = streams_.find(endpoint);
    if (it == streams_.end()) {
      return;
    }
    sp = std::move(it->second);
    streams_.erase(it);
  }
  // sp's dtor runs at scope exit. mounted_ is already false (cleared by
  // Stream::remove() before calling us).
}

void Server::Impl::cb_client_connected(GstRTSPServer* /*server*/,
                                       GstRTSPClient* client, gpointer user) {
  auto* self = static_cast<Impl*>(user);
  self->on_new_client_connected(client);
}

void Server::Impl::cb_client_closed(GstRTSPClient* client, gpointer user) {
  auto* self = static_cast<Impl*>(user);
  self->on_client_disconnected(client);
}

GstRTSPFilterResult Server::Impl::client_filter_remove(GstRTSPServer*,
                                                       GstRTSPClient*,
                                                       gpointer) {
  return GST_RTSP_FILTER_REMOVE;
}

// ---------------- Server (forwarding to Impl) ----------------

Server::Server() : impl_(std::make_unique<Impl>(this)) {}

Server::Server(const std::string& address, uint16_t port)
    : impl_(std::make_unique<Impl>(this, address, port)) {}

Server::~Server() = default;

bool Server::start() { return impl_->start(); }
void Server::stop() { impl_->stop(); }
bool Server::is_started() const { return impl_->is_started(); }

std::shared_ptr<Stream> Server::add_stream(const std::string& endpoint,
                                           std::shared_ptr<Source> source) {
  return impl_->add_stream(endpoint, std::move(source));
}

bool Server::remove_stream(const std::string& endpoint) {
  return impl_->remove_stream(endpoint);
}

void Server::detach_stream(const std::string& endpoint) {
  impl_->detach_stream(endpoint);
}

}  // namespace librtsp
