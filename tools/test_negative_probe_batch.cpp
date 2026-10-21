#include <cassert>
#include <cstdint>
#include "negative_probe_batch.h"

int main() {
  static_assert(negative_probe_batch::kSelectorMs == 25, "batch selector changed");
  static_assert(negative_probe_batch::kDurationMs == 30000, "batch duration changed");
  uint8_t counts[5] = {};
  const uint16_t expected[5] = {0, 10, 15, 20, 25};
  for (uint16_t i = 0; i < negative_probe_batch::kScheduleLength; ++i) {
    const uint16_t width = negative_probe_batch::width(i);
    bool found = false;
    for (uint8_t j = 0; j < 5; ++j) {
      if (width == expected[j]) { ++counts[j]; found = true; break; }
    }
    assert(found);
    if (i > 0) assert(width != negative_probe_batch::width(i - 1));
  }
  for (const auto count : counts) assert(count == 4);
  assert(negative_probe_batch::width(negative_probe_batch::kScheduleLength) ==
         negative_probe_batch::width(0));
}
