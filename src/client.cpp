#include "client.hpp"

#include <gst/gst.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include "event_loop.hpp"
#include "gstreamer.hpp"
#include "logger.hpp"
#include "sink.hpp"

namespace librtsp {

namespace {

const char* transport_to_protocols(Client::Transport t) {
  switch (t) {
    case Client::Transport::Tcp:
      return "tcp";
    case Client::Transport::Udp:
      return "udp+udp-mcast";
    case Client::Transport::Auto:
      return nullptr;  // omit; rtspsrc default
  }
  return nullptr;
}

}  // namespace

class Client::Impl {
 public:
  explicit Impl(Config cfg);
  ~Impl();

  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

  void set_sink(std::shared_ptr<Sink> sink);
  bool start();
  void stop();
  bool is_started() const { return running_.load(); }
  std::optional<StreamInfo> stream_info() const;

 private:
  void run();
  bool build_pipeline();
  void teardown_pipeline();

  static void on_rtspsrc_pad_added(GstElement* src, GstPad* pad, gpointer user);
  static gboolean on_bus_message(GstBus* bus, GstMessage* msg, gpointer user);

  Config cfg_;

  std::shared_ptr<Sink> sink_;
  std::unique_ptr<EventLoop> loop_;
  std::unique_ptr<std::thread> thread_;
  std::atomic<bool> running_{false};

  GstElement* pipeline_{nullptr};
  guint bus_source_id_{0};

  mutable std::mutex info_mutex_;
  StreamInfo info_;
};

// ---------------- Client::Impl ----------------

Client::Impl::Impl(Config cfg) : cfg_(std::move(cfg)) {
  loop_ = std::make_unique<EventLoop>();
}

Client::Impl::~Impl() { stop(); }

void Client::Impl::set_sink(std::shared_ptr<Sink> sink) {
  if (running_.load()) {
    LIBRTSP_WARN("Client::set_sink called while running; ignored");
    return;
  }
  sink_ = std::move(sink);
}

bool Client::Impl::start() {
  if (running_.load()) {
    return true;
  }
  if (!sink_) {
    LIBRTSP_ERROR("Client::start called without a sink");
    return false;
  }
  if (cfg_.url.empty()) {
    LIBRTSP_ERROR("Client::start called with empty URL");
    return false;
  }

  Gstreamer::init();

  if (!build_pipeline()) {
    return false;
  }

  // Sink hook before transitioning to PLAYING so it can register callbacks
  // on its elements (e.g. CallbackSink connecting "new-sample").
  sink_->on_media_configured(pipeline_);

  GstStateChangeReturn ret =
      gst_element_set_state(pipeline_, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    LIBRTSP_ERROR("Failed to set pipeline to PLAYING");
    sink_->on_media_destroyed();
    teardown_pipeline();
    return false;
  }

  thread_ = std::make_unique<std::thread>(&Impl::run, this);
  running_.store(true);
  LIBRTSP_INFO("RTSP client started for " << cfg_.url);
  return true;
}

void Client::Impl::stop() {
  if (!running_.load() && !pipeline_) {
    return;
  }

  if (pipeline_) {
    gst_element_set_state(pipeline_, GST_STATE_NULL);
  }

  if (thread_ && running_.load()) {
    loop_->quit();
    thread_->join();
  }
  thread_.reset();
  running_.store(false);

  if (sink_) {
    sink_->on_media_destroyed();
  }

  teardown_pipeline();
  LIBRTSP_INFO("RTSP client stopped");
}

std::optional<StreamInfo> Client::Impl::stream_info() const {
  std::lock_guard<std::mutex> lock(info_mutex_);
  if (info_.codec.empty()) {
    return std::nullopt;
  }
  return info_;
}

void Client::Impl::run() { loop_->loop(); }

bool Client::Impl::build_pipeline() {
  std::ostringstream oss;
  oss << "rtspsrc name=librtsp_rtspsrc" << " location=" << cfg_.url
      << " latency=" << cfg_.latency_ms;
  const char* protos = transport_to_protocols(cfg_.transport);
  if (protos) {
    oss << " protocols=" << protos;
  }
  oss << " ! " << sink_->pipeline();

  GError* error = nullptr;
  GstElement* parsed = gst_parse_launch(oss.str().c_str(), &error);
  if (!parsed) {
    LIBRTSP_ERROR(
        "gst_parse_launch failed: " << (error ? error->message : "unknown"));
    if (error) {
      g_error_free(error);
    }
    return false;
  }
  pipeline_ = parsed;

  // Hook rtspsrc for codec capture.
  GstElement* rtspsrc =
      gst_bin_get_by_name(GST_BIN(pipeline_), "librtsp_rtspsrc");
  if (rtspsrc) {
    g_signal_connect(rtspsrc, "pad-added",
                     G_CALLBACK(&Impl::on_rtspsrc_pad_added), this);
    gst_object_unref(rtspsrc);
  }

  // Bus watch on this loop's context (errors / EOS).
  GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
  if (bus) {
    GSource* watch = gst_bus_create_watch(bus);
    g_source_set_callback(watch, G_SOURCE_FUNC(&Impl::on_bus_message), this,
                          nullptr);
    bus_source_id_ = g_source_attach(watch, loop_->context());
    g_source_unref(watch);
    gst_object_unref(bus);
  }
  return true;
}

void Client::Impl::teardown_pipeline() {
  if (bus_source_id_ != 0) {
    GSource* src =
        g_main_context_find_source_by_id(loop_->context(), bus_source_id_);
    if (src) {
      g_source_destroy(src);
    }
    bus_source_id_ = 0;
  }
  if (pipeline_) {
    gst_object_unref(pipeline_);
    pipeline_ = nullptr;
  }
  {
    std::lock_guard<std::mutex> lock(info_mutex_);
    info_ = StreamInfo{};
  }
}

void Client::Impl::on_rtspsrc_pad_added(GstElement* /*src*/, GstPad* pad,
                                        gpointer user) {
  auto* self = static_cast<Impl*>(user);
  GstCaps* caps = gst_pad_get_current_caps(pad);
  if (!caps) {
    caps = gst_pad_query_caps(pad, nullptr);
  }
  if (!caps) {
    return;
  }
  GstStructure* s = gst_caps_get_structure(caps, 0);
  if (s) {
    const gchar* encoding = gst_structure_get_string(s, "encoding-name");
    if (encoding) {
      std::lock_guard<std::mutex> lock(self->info_mutex_);
      self->info_.codec = encoding;
    }
  }
  gst_caps_unref(caps);
}

gboolean Client::Impl::on_bus_message(GstBus* /*bus*/, GstMessage* msg,
                                      gpointer user) {
  auto* self = static_cast<Impl*>(user);
  (void)self;
  switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR: {
      GError* err = nullptr;
      gchar* dbg = nullptr;
      gst_message_parse_error(msg, &err, &dbg);
      LIBRTSP_ERROR("GStreamer error: " << (err ? err->message : "?"));
      g_clear_error(&err);
      g_free(dbg);
      break;
    }
    case GST_MESSAGE_EOS:
      LIBRTSP_INFO("End of stream");
      break;
    default:
      break;
  }
  return TRUE;
}

// ---------------- Client (forwarding) ----------------

Client::Client() : impl_(std::make_unique<Impl>(Config{})) {}

Client::Client(Config cfg) : impl_(std::make_unique<Impl>(std::move(cfg))) {}

Client::~Client() = default;

void Client::set_sink(std::shared_ptr<Sink> sink) {
  impl_->set_sink(std::move(sink));
}

bool Client::start() { return impl_->start(); }
void Client::stop() { impl_->stop(); }
bool Client::is_started() const { return impl_->is_started(); }

std::optional<StreamInfo> Client::stream_info() const {
  return impl_->stream_info();
}

}  // namespace librtsp
