// Example: serve a synthetic test pattern over RTSP.
//
// Build:   make build
// Run:     ./build/examples/test_pattern_server [port]
// Watch:   ffplay -rtsp_transport tcp rtsp://127.0.0.1:8554/test
//          gst-play-1.0 rtsp://127.0.0.1:8554/test
//          vlc rtsp://127.0.0.1:8554/test

#include <librtsp/logger.hpp>
#include <librtsp/server.hpp>
#include <librtsp/stream.hpp>
#include <librtsp/test_source.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

namespace {

std::atomic<bool> g_run{true};

void on_signal(int /*signum*/) { g_run.store(false); }

class StderrLogger : public librtsp::Logger {
 public:
  void log(librtsp::LogLevel level, const char* /*file*/, int /*line*/,
           std::string_view message) override {
    const char* tag = "INFO ";
    switch (level) {
      case librtsp::LogLevel::Trace: tag = "TRACE"; break;
      case librtsp::LogLevel::Debug: tag = "DEBUG"; break;
      case librtsp::LogLevel::Info:  tag = "INFO "; break;
      case librtsp::LogLevel::Warn:  tag = "WARN "; break;
      case librtsp::LogLevel::Error: tag = "ERROR"; break;
    }
    std::cerr << "[" << tag << "] " << message << '\n';
  }
};

}  // namespace

int main(int argc, char** argv) {
  std::uint16_t port = 8554;
  if (argc > 1) {
    port = static_cast<std::uint16_t>(std::stoi(argv[1]));
  }

  librtsp::set_logger(std::make_shared<StderrLogger>());

  librtsp::Server server("0.0.0.0", port);
  auto stream = server.add_stream(
      "/test", std::make_shared<librtsp::TestSource>(librtsp::Codec::H264));

  if (!server.start()) {
    std::cerr << "Failed to start RTSP server\n";
    return 1;
  }

  std::signal(SIGINT,  &on_signal);
  std::signal(SIGTERM, &on_signal);

  std::cout << "Streaming test pattern at rtsp://0.0.0.0:" << port << "/test\n"
            << "Press Ctrl+C to stop.\n";

  while (g_run.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  std::cout << "Stopping...\n";
  return 0;
}
