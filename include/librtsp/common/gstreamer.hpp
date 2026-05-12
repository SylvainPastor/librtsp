#pragma once

#include <librtsp/export.hpp>
#include <string>
#include <vector>

namespace librtsp {

/// @brief Static-only initializer for the GStreamer runtime.
///
/// Wraps gst_init / gst_init_check so the library has a single, explicit
/// initialization point. Safe to call multiple times — only the first call
/// performs initialization; later calls (with or without arguments) are
/// no-ops. Thread-safe.
///
/// Users that want GStreamer to consume their command-line arguments
/// (e.g. `--gst-debug-level=4`) should call init(&argc, &argv) early in
/// main(); otherwise the library calls init() lazily when needed.
class LIBRTSP_API Gstreamer {
 public:
  Gstreamer() = delete;
  Gstreamer(const Gstreamer&) = delete;
  Gstreamer& operator=(const Gstreamer&) = delete;

  /// Initialize GStreamer without forwarding command-line arguments.
  /// @throws std::runtime_error if gst_init_check fails.
  static void init();

  /// Initialize GStreamer and let it parse its own arguments out of argv.
  /// @throws std::runtime_error if gst_init_check fails.
  static void init(int* argc, char*** argv);

  /// True once GStreamer has been initialized in this process.
  static bool is_initialized();

  /// @brief Drain GStreamer ERROR-level debug messages captured since the
  /// last call, oldest first. Useful when the bus error message is too
  /// generic. The low-level cause (e.g. "Connection refused") is logged
  /// via the GStreamer debug system and surfaced here.
  ///
  /// Capture is gated by the CMake option `LIBRTSP_CAPTURE_GST_ERRORS`
  /// (default ON). When OFF, this method always returns an empty vector
  /// and no GStreamer debug log function is installed.
  ///
  /// Note: capture only happens when the GStreamer debug threshold for
  /// the relevant category is ERROR or finer. Set `GST_DEBUG=2` (or
  /// higher) in the environment to guarantee delivery. Thread-safe.
  static std::vector<std::string> drain_recent_errors();

  /// @brief True if the library was built with LIBRTSP_CAPTURE_GST_ERRORS
  /// enabled (the default), i.e. drain_recent_errors() may return entries.
  /// False if the option was disabled at configure time, in which case
  /// drain_recent_errors() is always empty.
  static bool capture_errors_enabled();
};

}  // namespace librtsp
