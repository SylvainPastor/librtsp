#include "server.hpp"

#include <stdexcept>
#include <string>
#include <utility>

#include "gstreamer.hpp"
#include "logger.hpp"
#include "stream.hpp"

namespace librtsp {

namespace {

void cb_client_closed(GstRTSPClient* client, gpointer user_data) {
  auto* self = static_cast<Server*>(user_data);
  self->on_client_disconnected(client);
}

void cb_client_connected(GstRTSPServer* /*server*/, GstRTSPClient* client,
                         gpointer user_data) {
  auto* self = static_cast<Server*>(user_data);
  self->on_new_client_connected(client);
}

GstRTSPFilterResult client_filter_remove(GstRTSPServer* /*server*/,
                                         GstRTSPClient* /*client*/,
                                         gpointer /*user_data*/) {
  return GST_RTSP_FILTER_REMOVE;
}

}  // namespace

Server::Server() {
  loop_ = std::make_unique<EventLoop>();
  create();
  g_signal_connect(server_, "client-connected", G_CALLBACK(cb_client_connected),
                   this);
  attach();
}

Server::Server(const std::string& address, uint16_t port) {
  if (port != 0) {
    port_ = port;
  }
  if (!address.empty()) {
    address_ = address;
  }
  loop_ = std::make_unique<EventLoop>();
  create();
  g_signal_connect(server_, "client-connected", G_CALLBACK(cb_client_connected),
                   this);
  attach();
}

Server::~Server() {
  stop();
  // Drain streams before tearing down the gst server. Each Stream's dtor
  // unmounts its factory; mounted_ is set so the dtor is a no-op afterwards.
  {
    std::lock_guard<std::mutex> lock(streams_mutex_);
    streams_.clear();
  }
  destroy();
}

bool Server::start() {
  if (running_.load()) {
    return true;
  }
  thread_ = std::make_unique<std::thread>(&Server::run, this);
  running_.store(true);
  return true;
}

void Server::stop() {
  if (thread_ && running_.load()) {
    loop_->quit();
    thread_->join();
    running_.store(false);
  }
  if (server_) {
    clear_clients_session();
  }
}

void Server::on_new_client_connected(GstRTSPClient* client) {
  std::lock_guard<std::mutex> lock(client_mutex_);
  ++client_count_;
  log_client(client, true);
  g_signal_connect(client, "closed", G_CALLBACK(cb_client_closed), this);
}

void Server::on_client_disconnected(GstRTSPClient* client) {
  std::lock_guard<std::mutex> lock(client_mutex_);
  if (client_count_ > 0) {
    --client_count_;
  }
  log_client(client, false);
}

void Server::log_client(GstRTSPClient* client, bool connected) {
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

void Server::create() {
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

void Server::destroy() {
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

void Server::attach() {
  if (attached_) {
    return;
  }
  source_id_ = gst_rtsp_server_attach(server_, loop_->context());
  if (source_id_ == 0) {
    throw std::runtime_error("Failed to attach RTSP server");
  }
  attached_ = true;

  // Periodically reap inactive sessions on the loop's context.
  cleanup_timer_ = loop_->create_timeout_s(2, [this] {
    GstRTSPSessionPool* pool = gst_rtsp_server_get_session_pool(server_);
    if (pool) {
      gst_rtsp_session_pool_cleanup(pool);
      g_object_unref(pool);
    }
    return true;
  });
}

void Server::clear_clients_session() {
  gst_rtsp_server_client_filter(server_, client_filter_remove, nullptr);
}

void Server::run() {
  loop_->loop();
  running_.store(false);
}

std::shared_ptr<Stream> Server::add_stream(const std::string& endpoint,
                                           std::shared_ptr<Source> source) {
  std::lock_guard<std::mutex> lock(streams_mutex_);
  if (streams_.find(endpoint) != streams_.end()) {
    throw std::runtime_error("Stream endpoint already exists: " + endpoint);
  }
  auto stream = std::shared_ptr<Stream>(
      new Stream(server_, this, endpoint, std::move(source)));
  streams_.emplace(endpoint, stream);
  return stream;
}

bool Server::remove_stream(const std::string& endpoint) {
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
  // sp->remove() unmounts. Its callback into detach_stream() is a no-op
  // because the map entry was just erased.
  sp->remove();
  return true;
}

void Server::detach_stream(const std::string& endpoint) {
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
  // sp dtor runs at scope exit. mounted_ is already false (cleared by the
  // caller, Stream::remove()).
}

}  // namespace librtsp
