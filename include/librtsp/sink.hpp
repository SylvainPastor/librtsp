#pragma once

#include <gst/gst.h>

#include <librtsp/export.hpp>
#include <string>

namespace librtsp {

/// @brief Abstract sink: consumes the rtspsrc output and delivers it
/// somewhere (a display, a user callback, a custom pipeline, etc.).
class LIBRTSP_API Sink {
 public:
  virtual ~Sink();

  /// @brief GStreamer launch fragment placed after `rtspsrc … !`.
  /// Typically starts with decodebin or a specific depayloader.
  virtual std::string pipeline() const = 0;

  /// @brief Internal hook called by Client once the pipeline bin is built,
  /// before transitioning to PLAYING. Sinks override this to look up
  /// elements by name (e.g. CallbackSink locating its appsink).
  virtual void on_media_configured(GstElement* /*media_bin*/) {}

  /// @brief Internal hook called by Client just before the pipeline is
  /// torn down.
  virtual void on_media_destroyed() {}
};

}  // namespace librtsp
