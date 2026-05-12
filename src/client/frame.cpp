#include <librtsp/client/frame.hpp>
#include <utility>

namespace librtsp {

FrameHandle Frame::retain() const {
  if (!sample_) {
    return FrameHandle();
  }
  gst_sample_ref(sample_);
  return FrameHandle(sample_, width, height, format, pts_ns);
}

FrameHandle::FrameHandle(GstSample* sample, int width, int height,
                         std::string format, std::int64_t pts_ns)
    : sample_(sample),
      width_(width),
      height_(height),
      format_(std::move(format)),
      pts_ns_(pts_ns) {
  GstBuffer* buf = gst_sample_get_buffer(sample_);
  if (buf && gst_buffer_map(buf, &map_, GST_MAP_READ)) {
    mapped_ = true;
  }
}

FrameHandle::~FrameHandle() { release(); }

FrameHandle::FrameHandle(FrameHandle&& other) noexcept
    : sample_(other.sample_),
      map_(other.map_),
      mapped_(other.mapped_),
      width_(other.width_),
      height_(other.height_),
      format_(std::move(other.format_)),
      pts_ns_(other.pts_ns_) {
  other.sample_ = nullptr;
  other.mapped_ = false;
}

FrameHandle& FrameHandle::operator=(FrameHandle&& other) noexcept {
  if (this != &other) {
    release();
    sample_ = other.sample_;
    map_ = other.map_;
    mapped_ = other.mapped_;
    width_ = other.width_;
    height_ = other.height_;
    format_ = std::move(other.format_);
    pts_ns_ = other.pts_ns_;
    other.sample_ = nullptr;
    other.mapped_ = false;
  }
  return *this;
}

const std::uint8_t* FrameHandle::data() const {
  return mapped_ ? map_.data : nullptr;
}

std::size_t FrameHandle::size() const { return mapped_ ? map_.size : 0; }

void FrameHandle::release() {
  if (mapped_) {
    GstBuffer* buf = gst_sample_get_buffer(sample_);
    if (buf) {
      gst_buffer_unmap(buf, &map_);
    }
    mapped_ = false;
  }
  if (sample_) {
    gst_sample_unref(sample_);
    sample_ = nullptr;
  }
}

}  // namespace librtsp
