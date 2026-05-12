#pragma once

#include <gst/gst.h>

#include <librtsp/export.hpp>
#include <string>

namespace librtsp {

/// @brief Codec used by convenience Source subclasses.
enum class Codec {
  H264,
  H265,
};

/// @brief Abstract input source feeding one RTSP endpoint.
///
/// Implementations return a gst-launch-1.0 syntax fragment whose final
/// element is an RTP payloader named "pay0". The Stream owning this
/// Source passes the fragment to a GstRTSPMediaFactory.
class LIBRTSP_API Source {
 public:
  virtual ~Source();

  /// @brief Build the GStreamer launch fragment for this source.
  virtual std::string pipeline() const = 0;

  /// @brief Internal hook called by Stream after a GstRTSPMedia has been
  /// configured for a session. Default is a no-op. Sources that need to
  /// reach into the pipeline (e.g. AppSrcSource locating its appsrc
  /// element) override this.
  /// @param media_bin Root GstBin of the freshly-built media pipeline.
  virtual void on_media_configured(GstElement* /*media_bin*/) {}

  /// @brief Internal hook called by Stream just before a media pipeline
  /// is torn down (last session ended, or factory shutting down).
  virtual void on_media_destroyed() {}
};

}  // namespace librtsp
