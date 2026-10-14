#pragma once
#include <cerrno>
#include <cstdlib>
#include <cstdint>

namespace autonomous_brake_limit {
constexpr uint16_t kDefaultMs = 50;
inline bool valid(long value) { return value == 0 || value == 20 || value == 50; }
inline bool parse(const char* text, uint16_t& result) {
  if (!text || !*text) return false;
  char* end = nullptr; errno = 0;
  const long value = std::strtol(text, &end, 10);
  if (errno || end == text || *end || !valid(value)) return false;
  result = static_cast<uint16_t>(value); return true;
}
} // namespace autonomous_brake_limit
