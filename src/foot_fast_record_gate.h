#pragma once
#include <stdint.h>
// Call once per arrived frame only while the fast experiment is active.
// Restrict recorded rows, not live tracking, to 15 Hz average with no backlog.
inline bool fastRecordGate(uint64_t t_us, uint64_t& next_us) {
  constexpr uint64_t period_us = 66667;
  if (!t_us) return false;
  if (!next_us) { next_us = t_us + period_us; return true; }
  if (t_us < next_us) return false;
  next_us += period_us;
  if (next_us <= t_us) next_us = t_us + period_us;
  return true;
}
