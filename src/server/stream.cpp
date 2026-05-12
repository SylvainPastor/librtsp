#include <librtsp/common/logger.hpp>
#include <librtsp/server/server.hpp>
#include <librtsp/server/stream.hpp>
#include <stdexcept>
#include <utility>

namespace librtsp {

Stream::Stream(GstRTSPServer* gst_server, Server* owner, std::string endpoint,
               std::shared_ptr<Source> source)
    : gst_server_(gst_server),
      owner_(owner),
      endpoint_(std::move(endpoint)),
      source_(std::move(source)) {
  if (!source_) {
    throw std::invalid_argument("Stream: source is null");
  }

  GstRTSPMediaFactory* factory = gst_rtsp_media_factory_new();
  if (!factory) {
    throw std::runtime_error("Failed to create GstRTSPMediaFactory");
  }
  gst_rtsp_media_factory_set_shared(factory, TRUE);
  gst_rtsp_media_factory_set_launch(factory, source_->pipeline().c_str());
  g_signal_connect(factory, "media-configure",
                   G_CALLBACK(&Stream::on_media_configure_cb), this);

  GstRTSPMountPoints* mounts = gst_rtsp_server_get_mount_points(gst_server_);
  if (!mounts) {
    g_object_unref(factory);
    throw std::runtime_error("Failed to get GstRTSPMountPoints");
  }
  gst_rtsp_mount_points_add_factory(mounts, endpoint_.c_str(), factory);
  g_object_unref(mounts);
  mounted_.store(true);
  LIBRTSP_INFO("Stream mounted at " << endpoint_);
}

Stream::~Stream() {
  if (mounted_.exchange(false)) {
    unmount_factory();
  }
}

void Stream::remove() {
  if (!mounted_.exchange(false)) {
    return;
  }
  unmount_factory();
  if (owner_) {
    owner_->detach_stream(endpoint_);
    owner_ = nullptr;
  }
}

void Stream::unmount_factory() {
  GstRTSPMountPoints* mounts = gst_rtsp_server_get_mount_points(gst_server_);
  if (mounts) {
    gst_rtsp_mount_points_remove_factory(mounts, endpoint_.c_str());
    g_object_unref(mounts);
  }
  LIBRTSP_INFO("Stream unmounted from " << endpoint_);
}

void Stream::on_media_configure_cb(GstRTSPMediaFactory* /*factory*/,
                                   GstRTSPMedia* media, gpointer user) {
  auto* self = static_cast<Stream*>(user);
  GstElement* bin = gst_rtsp_media_get_element(media);
  if (bin && self->source_) {
    self->source_->on_media_configured(bin);
  }
  // Connect "unprepared" on this media instance so on_media_destroyed fires
  // when the session ends.
  g_signal_connect(media, "unprepared",
                   G_CALLBACK(&Stream::on_media_unprepared_cb), self);
  if (bin) {
    gst_object_unref(bin);
  }
}

void Stream::on_media_unprepared_cb(GstRTSPMedia* /*media*/, gpointer user) {
  auto* self = static_cast<Stream*>(user);
  if (self->source_) {
    self->source_->on_media_destroyed();
  }
}

}  // namespace librtsp
