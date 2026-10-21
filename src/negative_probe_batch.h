#pragma once
#include <cstdint>

namespace negative_probe_batch {
// Four balanced five-event blocks.  Each width occurs four times before the
// schedule repeats; adjacent blocks do not duplicate their boundary width.
static constexpr uint16_t kSelectorMs = 25;
static constexpr uint32_t kDurationMs = 30000;
static constexpr uint16_t kWidthsMs[20] = {
    0, 10, 15, 20, 25,
    20, 15, 10, 25, 0,
    25, 20, 0, 10, 15,
    10, 0, 25, 15, 20};
static constexpr uint16_t kScheduleLength = 20;
inline uint16_t width(uint16_t negative_crossing_index) {
  return kWidthsMs[negative_crossing_index % kScheduleLength];
}
}  // namespace negative_probe_batch
