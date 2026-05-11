#pragma once

#include <librtsp/export.hpp>
#include <memory>
#include <sstream>
#include <string_view>

namespace librtsp {

enum class LogLevel {
  Trace,
  Debug,
  Info,
  Warn,
  Error,
};

/// @brief Abstract logging sink.
///
/// The library does not perform any logging on its own. Consumers install a
/// concrete Logger via set_logger() to bridge log records to their host
/// system (ROS 2, spdlog, std::cerr, ...).
class LIBRTSP_API Logger {
 public:
  virtual ~Logger() = default;

  /// @brief Receive a log record.
  /// Implementations must be safe to call from multiple threads concurrently.
  /// @param level   Severity.
  /// @param file    __FILE__ at the call site.
  /// @param line    __LINE__ at the call site.
  /// @param message Pre-formatted message body.
  virtual void log(LogLevel level, const char* file, int line,
                   std::string_view message) = 0;
};

/// @brief Install the logger used by the library.
/// Pass nullptr to disable logging. Thread-safe.
LIBRTSP_API void set_logger(std::shared_ptr<Logger> logger);

/// @brief Get the currently installed logger.
/// Returns nullptr if none is set. Thread-safe.
LIBRTSP_API std::shared_ptr<Logger> get_logger();

}  // namespace librtsp

/// @brief Internal call-site macro. Short-circuits when no logger is set.
/// Compose messages with stream operators:
///   LIBRTSP_INFO("client " << uri << " connected");
#define LIBRTSP_LOG(level, ...)                                          \
  do {                                                                   \
    if (auto _librtsp_lg = ::librtsp::get_logger()) {                    \
      std::ostringstream _librtsp_oss;                                   \
      _librtsp_oss << __VA_ARGS__;                                       \
      _librtsp_lg->log((level), __FILE__, __LINE__, _librtsp_oss.str()); \
    }                                                                    \
  } while (0)

#define LIBRTSP_TRACE(...) LIBRTSP_LOG(::librtsp::LogLevel::Trace, __VA_ARGS__)
#define LIBRTSP_DEBUG(...) LIBRTSP_LOG(::librtsp::LogLevel::Debug, __VA_ARGS__)
#define LIBRTSP_INFO(...) LIBRTSP_LOG(::librtsp::LogLevel::Info, __VA_ARGS__)
#define LIBRTSP_WARN(...) LIBRTSP_LOG(::librtsp::LogLevel::Warn, __VA_ARGS__)
#define LIBRTSP_ERROR(...) LIBRTSP_LOG(::librtsp::LogLevel::Error, __VA_ARGS__)
