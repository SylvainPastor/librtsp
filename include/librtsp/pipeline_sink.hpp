#pragma once

#include <librtsp/export.hpp>
#include <librtsp/sink.hpp>
#include <string>

namespace librtsp {

/// @brief Sink that forwards a user-supplied gst-launch-1.0 fragment
/// as-is. The fragment is placed after `rtspsrc … !`, for example:
///   "decodebin ! videoconvert ! autovideosink sync=false"
class LIBRTSP_API GStreamerPipelineSink : public Sink {
 public:
  explicit GStreamerPipelineSink(std::string launch);

  std::string pipeline() const override { return launch_; }

 private:
  std::string launch_;
};

}  // namespace librtsp
