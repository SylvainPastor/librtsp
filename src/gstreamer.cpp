#include "gstreamer.hpp"

#include <gst/gst.h>

#include <mutex>
#include <stdexcept>
#include <string>

namespace librtsp {

namespace {

std::once_flag& init_flag() {
  static std::once_flag flag;
  return flag;
}

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
  });
}

}  // namespace

void Gstreamer::init() { do_init(nullptr, nullptr); }

void Gstreamer::init(int* argc, char*** argv) { do_init(argc, argv); }

bool Gstreamer::is_initialized() { return gst_is_initialized() != FALSE; }

}  // namespace librtsp
