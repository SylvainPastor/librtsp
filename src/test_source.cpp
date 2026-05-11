#include "test_source.hpp"

#include <stdexcept>

namespace librtsp {

TestSource::TestSource(Codec codec) : codec_(codec) {}

std::string TestSource::pipeline() const {
  switch (codec_) {
    case Codec::H264:
      return "( videotestsrc is-live=true ! videoconvert ! "
             "x264enc tune=zerolatency speed-preset=ultrafast ! "
             "rtph264pay name=pay0 pt=96 )";
    case Codec::H265:
      return "( videotestsrc is-live=true ! videoconvert ! "
             "x265enc tune=zerolatency speed-preset=ultrafast ! "
             "rtph265pay name=pay0 pt=96 )";
  }
  throw std::logic_error("Unknown codec");
}

}  // namespace librtsp
