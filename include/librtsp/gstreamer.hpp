#pragma once

#include <librtsp/export.hpp>

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
};

}  // namespace librtsp
