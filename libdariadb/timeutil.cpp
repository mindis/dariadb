#include "timeutil.h"

namespace dariadb {
namespace timeutil {
Time from_chrono(const std::chrono::high_resolution_clock::time_point &t) {
  auto now = t.time_since_epoch();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

Time current_time() {
  auto now = std::chrono::high_resolution_clock::now();
  return dariadb::timeutil::from_chrono(now);
}
}
}
