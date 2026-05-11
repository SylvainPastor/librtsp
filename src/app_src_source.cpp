#include "app_src_source.hpp"

#include <gst/app/gstappsrc.h>

#include <cstring>
#include <sstream>
#include <utility>

#include "logger.hpp"

namespace librtsp {

namespace {

constexpr const char* kAppSrcElementName = "librtsp_appsrc";

}  // namespace

AppSrcSource::AppSrcSource(Config cfg)
    : cfg_(std::move(cfg)), element_name_(kAppSrcElementName) {}

AppSrcSource::~AppSrcSource() { on_media_destroyed(); }

std::string AppSrcSource::pipeline() const {
  std::ostringstream oss;
  oss << "( appsrc name=" << element_name_ << " is-live=true format=time"
      << " caps=\"" << cfg_.caps << "\""
      << " block=" << (cfg_.block_on_full ? "true" : "false")
      << " ! videoconvert";
  switch (cfg_.codec) {
    case Codec::H264:
      oss << " ! x264enc tune=zerolatency speed-preset=ultrafast"
          << " ! rtph264pay name=pay0 pt=96";
      break;
    case Codec::H265:
      oss << " ! x265enc tune=zerolatency speed-preset=ultrafast"
          << " ! rtph265pay name=pay0 pt=96";
      break;
  }
  oss << " )";
  return oss.str();
}

bool AppSrcSource::push_frame(const std::uint8_t* data, std::size_t size,
                              std::int64_t pts_ns) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!appsrc_) {
    return false;
  }
  if (!cfg_.block_on_full && enough_data_.load()) {
    return false;
  }

  GstBuffer* buf = gst_buffer_new_allocate(nullptr, size, nullptr);
  if (!buf) {
    LIBRTSP_WARN("AppSrcSource: gst_buffer_new_allocate(" << size
                                                          << ") failed");
    return false;
  }
  GstMapInfo map;
  if (!gst_buffer_map(buf, &map, GST_MAP_WRITE)) {
    gst_buffer_unref(buf);
    return false;
  }
  std::memcpy(map.data, data, size);
  gst_buffer_unmap(buf, &map);

  if (pts_ns >= 0) {
    GST_BUFFER_PTS(buf) = static_cast<GstClockTime>(pts_ns);
  }

  GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(appsrc_), buf);
  return ret == GST_FLOW_OK;
}

void AppSrcSource::on_media_configured(GstElement* media_bin) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (appsrc_) {
    // Already attached. Stream should call on_media_destroyed() first.
    return;
  }
  GstElement* element =
      gst_bin_get_by_name(GST_BIN(media_bin), element_name_.c_str());
  if (!element) {
    LIBRTSP_ERROR("AppSrcSource: appsrc element '"
                  << element_name_ << "' not found in media bin");
    return;
  }
  appsrc_ = element;  // owns the ref returned by gst_bin_get_by_name

  g_signal_connect(appsrc_, "need-data",
                   G_CALLBACK(&AppSrcSource::on_need_data_cb), this);
  g_signal_connect(appsrc_, "enough-data",
                   G_CALLBACK(&AppSrcSource::on_enough_data_cb), this);
}

void AppSrcSource::on_media_destroyed() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (appsrc_) {
    g_signal_handlers_disconnect_by_data(appsrc_, this);
    gst_object_unref(appsrc_);
    appsrc_ = nullptr;
  }
  enough_data_.store(false);
}

void AppSrcSource::on_need_data_cb(GstElement* /*src*/, guint /*length*/,
                                   gpointer user) {
  auto* self = static_cast<AppSrcSource*>(user);
  self->enough_data_.store(false);
}

void AppSrcSource::on_enough_data_cb(GstElement* /*src*/, gpointer user) {
  auto* self = static_cast<AppSrcSource*>(user);
  self->enough_data_.store(true);
}

}  // namespace librtsp
