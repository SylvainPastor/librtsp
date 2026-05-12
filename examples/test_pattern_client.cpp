// Example: connect to test_pattern_server, decode the stream, and report
// frame rate plus stream info on stdout.
//
// Build:   make build
// Run:     ./build/examples/test_pattern_client [rtsp://...]
//          (default URL: rtsp://127.0.0.1:8554/test)
//
// Companion of examples/test_pattern_server.cpp.

#include <librtsp/callback_sink.hpp>
#include <librtsp/client.hpp>
#include <librtsp/frame.hpp>
#include <librtsp/logger.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <iomanip>
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
  std::string url = "rtsp://127.0.0.1:8554/test";
  if (argc > 1) {
    url = argv[1];
  }

  librtsp::set_logger(std::make_shared<StderrLogger>());

  // Frame counter shared between worker thread (callback) and main thread.
  std::atomic<std::uint64_t> frame_count{0};
  std::atomic<int>           frame_width{0};
  std::atomic<int>           frame_height{0};

  librtsp::CallbackSink::Config sink_cfg;  // defaults: BGR, max_buffers=1, drop=true

  auto sink = std::make_shared<librtsp::CallbackSink>(
      sink_cfg,
      [&](const librtsp::Frame& f) {
        frame_count.fetch_add(1, std::memory_order_relaxed);
        frame_width.store(f.width, std::memory_order_relaxed);
        frame_height.store(f.height, std::memory_order_relaxed);
      });

  librtsp::Client::Config cfg;
  cfg.url        = url;
  cfg.latency_ms = 100;
  cfg.transport  = librtsp::Client::Transport::Auto;

  librtsp::Client client(cfg);
  client.set_sink(sink);

  if (!client.start()) {
    std::cerr << "Failed to start RTSP client\n";
    return 1;
  }

  std::signal(SIGINT,  &on_signal);
  std::signal(SIGTERM, &on_signal);

  std::cout << "Connected to " << url << "\nPress Ctrl+C to stop.\n";

  auto          last_time  = std::chrono::steady_clock::now();
  std::uint64_t last_count = 0;

  while (g_run.load()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));

    const auto         now   = std::chrono::steady_clock::now();
    const std::uint64_t cur  = frame_count.load(std::memory_order_relaxed);
    const double        dt   = std::chrono::duration<double>(now - last_time).count();
    const double        fps  = (dt > 0) ? (cur - last_count) / dt : 0.0;

    const auto info  = client.stream_info();
    const std::string codec = info ? info->codec : "?";
    const int w = frame_width.load(std::memory_order_relaxed);
    const int h = frame_height.load(std::memory_order_relaxed);

    std::cout << std::fixed << std::setprecision(2)
              << "[stats] codec=" << codec
              << " size=" << w << "x" << h
              << " frames=" << cur
              << " fps=" << fps << '\n';

    last_time  = now;
    last_count = cur;
  }

  std::cout << "Stopping...\n";
  client.stop();
  return 0;
}
