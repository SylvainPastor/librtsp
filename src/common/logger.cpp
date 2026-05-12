#include <librtsp/common/logger.hpp>
#include <mutex>
#include <utility>

namespace librtsp {

namespace {

std::mutex& registry_mutex() {
  static std::mutex m;
  return m;
}

std::shared_ptr<Logger>& registry() {
  static std::shared_ptr<Logger> instance;
  return instance;
}

}  // namespace

void set_logger(std::shared_ptr<Logger> logger) {
  std::lock_guard<std::mutex> lock(registry_mutex());
  registry() = std::move(logger);
}

std::shared_ptr<Logger> get_logger() {
  // Copy under the lock, then release before returning. Callers may invoke
  // the returned logger's log() concurrently without further synchronization
  // on our side.
  std::lock_guard<std::mutex> lock(registry_mutex());
  return registry();
}

}  // namespace librtsp
