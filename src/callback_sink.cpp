#include "callback_sink.hpp"

#include <gst/app/gstappsink.h>

#include <sstream>
#include <utility>

#include "logger.hpp"

namespace librtsp {

namespace {

constexpr const char* kAppSinkElementName = "librtsp_appsink";

}  // namespace

CallbackSink::CallbackSink(Config cfg, Callback cb)
    : cfg_(std::move(cfg)),
      cb_(std::move(cb)),
      element_name_(kAppSinkElementName) {}

CallbackSink::~CallbackSink() { on_media_destroyed(); }

std::string CallbackSink::pipeline() const {
  std::ostringstream oss;
  oss << "decodebin ! videoconvert"
      << " ! video/x-raw,format=" << cfg_.output_format
      << " ! appsink name=" << element_name_ << " emit-signals=true sync=false"
      << " max-buffers=" << cfg_.max_buffers
      << " drop=" << (cfg_.drop ? "true" : "false");
  return oss.str();
}

void CallbackSink::on_media_configured(GstElement* media_bin) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (appsink_) {
    return;
  }
  GstElement* element =
      gst_bin_get_by_name(GST_BIN(media_bin), element_name_.c_str());
  if (!element) {
    LIBRTSP_ERROR("CallbackSink: appsink element '"
                  << element_name_ << "' not found in media bin");
    return;
  }
  appsink_ = element;  // owns ref returned by gst_bin_get_by_name

  g_signal_connect(appsink_, "new-sample",
                   G_CALLBACK(&CallbackSink::on_new_sample_cb), this);
}

void CallbackSink::on_media_destroyed() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (appsink_) {
    g_signal_handlers_disconnect_by_data(appsink_, this);
    gst_object_unref(appsink_);
    appsink_ = nullptr;
  }
}

GstFlowReturn CallbackSink::on_new_sample_cb(GstElement* sink, gpointer user) {
  GstSample* sample = nullptr;
  g_signal_emit_by_name(sink, "pull-sample", &sample);
  if (!sample) {
    return GST_FLOW_ERROR;
  }
  static_cast<CallbackSink*>(user)->dispatch(sample);
  gst_sample_unref(sample);
  return GST_FLOW_OK;
}

void CallbackSink::dispatch(GstSample* sample) {
  if (!cb_) {
    return;
  }
  GstBuffer* buf = gst_sample_get_buffer(sample);
  if (!buf) {
    return;
  }

  int width = 0;
  int height = 0;
  const gchar* format = nullptr;
  GstCaps* caps = gst_sample_get_caps(sample);
  if (caps) {
    GstStructure* s = gst_caps_get_structure(caps, 0);
    if (s) {
      gst_structure_get_int(s, "width", &width);
      gst_structure_get_int(s, "height", &height);
      format = gst_structure_get_string(s, "format");
    }
  }

  GstMapInfo map;
  if (!gst_buffer_map(buf, &map, GST_MAP_READ)) {
    return;
  }

  Frame frame;
  frame.data = map.data;
  frame.size = map.size;
  frame.width = width;
  frame.height = height;
  frame.format = format ? format : std::string{};
  frame.pts_ns = GST_BUFFER_PTS_IS_VALID(buf)
                     ? static_cast<std::int64_t>(GST_BUFFER_PTS(buf))
                     : -1;
  frame.sample_ = sample;

  cb_(frame);

  gst_buffer_unmap(buf, &map);
}

}  // namespace librtsp
