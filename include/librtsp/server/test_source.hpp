#pragma once

#include <librtsp/export.hpp>
#include <librtsp/server/source.hpp>
#include <string>

namespace librtsp {

/// @brief Synthetic source built from videotestsrc, encoded with the
/// specified codec. Useful for smoke-testing the RTSP server.
class LIBRTSP_API TestSource : public Source {
 public:
  explicit TestSource(Codec codec = Codec::H264);

  std::string pipeline() const override;

 private:
  Codec codec_;
};

}  // namespace librtsp
