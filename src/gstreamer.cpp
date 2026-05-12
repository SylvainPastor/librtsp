#include "gstreamer.hpp"

#include <gst/gst.h>

#include <cstddef>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace librtsp {

namespace {

std::once_flag& init_flag() {
  static std::once_flag flag;
  return flag;
}

#ifdef LIBRTSP_CAPTURE_GST_ERRORS

constexpr std::size_t kMaxRecentErrors = 32;

std::mutex& err_mutex() {
  static std::mutex m;
  return m;
}

std::vector<std::string>& recent_errors() {
  static std::vector<std::string> v;
  return v;
}

void capture_gst_log(GstDebugCategory* /*cat*/, GstDebugLevel level,
                     const gchar* /*file*/, const gchar* /*function*/,
                     gint /*line*/, GObject* /*object*/,
                     GstDebugMessage* message, gpointer /*user_data*/) {
  if (level > GST_LEVEL_ERROR) {
    return;  // ERROR is the lowest numeric, FIXME/INFO/... are above.
  }
  const char* msg = gst_debug_message_get(message);
  if (!msg || !*msg) {
    return;
  }
  std::lock_guard<std::mutex> lock(err_mutex());
  auto& vec = recent_errors();
  vec.emplace_back(msg);
  if (vec.size() > kMaxRecentErrors) {
    vec.erase(vec.begin(), vec.begin() + (vec.size() - kMaxRecentErrors));
  }
}

#endif  // LIBRTSP_CAPTURE_GST_ERRORS

void do_init(int* argc, char*** argv) {
  std::call_once(init_flag(), [argc, argv] {
    if (gst_is_initialized()) {
      return;
    }
    GError* error = nullptr;
    if (!gst_init_check(argc, argv, &error)) {
      std::string msg = error ? error->message : "gst_init_check failed";
      if (error) {
        g_error_free(error);
      }
      throw std::runtime_error(msg);
    }
#ifdef LIBRTSP_CAPTURE_GST_ERRORS
    // Capture ERROR-level GStreamer debug messages so the deep cause of a
    // failure (e.g. "Connection refused") can be retrieved by callers,
    // not just the generic bus error message. Disabled when the CMake
    // option LIBRTSP_CAPTURE_GST_ERRORS is OFF.
    gst_debug_add_log_function(&capture_gst_log, nullptr, nullptr);
#endif
  });
}

}  // namespace

void Gstreamer::init() { do_init(nullptr, nullptr); }

void Gstreamer::init(int* argc, char*** argv) { do_init(argc, argv); }

bool Gstreamer::is_initialized() { return gst_is_initialized() != FALSE; }

std::vector<std::string> Gstreamer::drain_recent_errors() {
#ifdef LIBRTSP_CAPTURE_GST_ERRORS
  std::lock_guard<std::mutex> lock(err_mutex());
  std::vector<std::string> out;
  out.swap(recent_errors());
  return out;
#else
  return {};
#endif
}

bool Gstreamer::capture_errors_enabled() {
#ifdef LIBRTSP_CAPTURE_GST_ERRORS
  return true;
#else
  return false;
#endif
}

}  // namespace librtsp
