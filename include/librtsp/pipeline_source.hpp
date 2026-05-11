#pragma once

#include <librtsp/export.hpp>
#include <librtsp/source.hpp>
#include <string>

namespace librtsp {

/// @brief Source that forwards a user-supplied gst-launch-1.0 line as-is.
/// The fragment must produce an RTP payloader named "pay0", for example:
///   "( videotestsrc ! videoconvert ! x264enc ! rtph264pay name=pay0 pt=96 )"
class LIBRTSP_API GStreamerPipelineSource : public Source {
 public:
  explicit GStreamerPipelineSource(std::string launch);

  std::string pipeline() const override { return launch_; }

 private:
  std::string launch_;
};

}  // namespace librtsp
