#pragma once

#include <gst/gst.h>

#include <cstddef>
#include <cstdint>
#include <librtsp/export.hpp>
#include <string>

namespace librtsp {

class FrameHandle;

/// @brief Short-lived view of a decoded video frame.
///
/// Passed by const-reference to CallbackSink's callback. The byte pointer
/// is valid ONLY during the callback invocation; the underlying GstSample
/// is unmapped right after the user's function returns. To keep the frame
/// for later use, call retain() to obtain a FrameHandle that owns its own
/// reference and mapping.
struct LIBRTSP_API Frame {
  const std::uint8_t* data;  ///< Raw bytes (valid during callback).
  std::size_t size;          ///< Byte length of @ref data.
  int width;
  int height;
  std::string format;   ///< GStreamer format name ("BGR", "I420"...).
  std::int64_t pts_ns;  ///< Presentation timestamp, -1 if unknown.

  /// Bump the underlying GstSample's refcount and return a FrameHandle
  /// whose lifetime is independent of the callback.
  FrameHandle retain() const;

 private:
  friend class CallbackSink;
  GstSample* sample_{nullptr};
};

/// @brief Owning handle to a decoded frame.
///
/// Holds a reference on the underlying GstSample and its own GstMapInfo,
/// so data() stays valid until this handle is destroyed. Movable,
/// non-copyable. Obtain one via Frame::retain().
class LIBRTSP_API FrameHandle {
 public:
  FrameHandle() = default;
  ~FrameHandle();

  FrameHandle(const FrameHandle&) = delete;
  FrameHandle& operator=(const FrameHandle&) = delete;

  FrameHandle(FrameHandle&&) noexcept;
  FrameHandle& operator=(FrameHandle&&) noexcept;

  bool valid() const { return sample_ != nullptr; }

  const std::uint8_t* data() const;
  std::size_t size() const;
  int width() const { return width_; }
  int height() const { return height_; }
  const std::string& format() const { return format_; }
  std::int64_t pts_ns() const { return pts_ns_; }

 private:
  friend struct Frame;

  FrameHandle(GstSample* sample, int width, int height, std::string format,
              std::int64_t pts_ns);

  void release();

  GstSample* sample_{nullptr};  ///< Owns one ref while non-null.
  GstMapInfo map_{};
  bool mapped_{false};

  int width_{0};
  int height_{0};
  std::string format_;
  std::int64_t pts_ns_{0};
};

}  // namespace librtsp
