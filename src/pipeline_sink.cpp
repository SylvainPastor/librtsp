#include "pipeline_sink.hpp"

#include <utility>

namespace librtsp {

GStreamerPipelineSink::GStreamerPipelineSink(std::string launch)
    : launch_(std::move(launch)) {}

}  // namespace librtsp
