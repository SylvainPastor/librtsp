#include <librtsp/server/pipeline_source.hpp>
#include <utility>

namespace librtsp {

GStreamerPipelineSource::GStreamerPipelineSource(std::string launch)
    : launch_(std::move(launch)) {}

}  // namespace librtsp
