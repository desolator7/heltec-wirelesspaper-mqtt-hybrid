#pragma once

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace DisplayLogic {

constexpr int LINE_COUNT = 10;
constexpr std::size_t TEXT_CAPACITY = 100;  // Includes the terminating NUL.

inline bool payloadFits(const unsigned char* payload, std::size_t length) {
  return length < TEXT_CAPACITY &&
         (length == 0 || (payload != nullptr && std::memchr(payload, '\0', length) == nullptr));
}

inline bool parseRotationNumber(const char* text, int& rotationOut) {
  if (text == nullptr || *text == '\0') return false;
  const char* digit = (*text == '-') ? text + 1 : text;
  if (*digit == '\0') return false;
  for (const char* p = digit; *p; ++p) {
    if (*p < '0' || *p > '9') return false;
  }
  errno = 0;
  char* end = nullptr;
  const long raw = std::strtol(text, &end, 10);
  if (errno == ERANGE || *end != '\0') return false;
  if (raw >= -3 && raw <= 3) {
    rotationOut = static_cast<int>((raw % 4 + 4) % 4);
    return true;
  }
  const int degrees = static_cast<int>((raw % 360 + 360) % 360);
  if (degrees % 90 != 0) return false;
  rotationOut = degrees / 90;
  return true;
}

inline bool usesSmallFont(bool small, bool compact, bool large, bool bold) {
  return small || (compact && !large && !bold);
}

inline bool refreshDue(bool dirty, bool sleeping, uint32_t now, uint32_t lastChange,
                       uint32_t lastRefresh, uint32_t debounce, uint32_t minimumInterval,
                       uint32_t maximumDefer) {
  const uint32_t sinceRefresh = now - lastRefresh;
  return dirty && (sleeping || (sinceRefresh >= minimumInterval &&
         (now - lastChange >= debounce || sinceRefresh >= maximumDefer)));
}

inline int batteryVoltageToPercent(float voltage) {
  constexpr float volts[] = {3.30f, 3.50f, 3.60f, 3.70f, 3.75f, 3.80f,
                             3.85f, 3.92f, 4.00f, 4.10f, 4.20f};
  if (!(voltage > volts[0])) return 0;
  if (voltage >= volts[10]) return 100;
  for (int i = 1; i <= 10; ++i) {
    if (voltage <= volts[i]) {
      const float fraction = (voltage - volts[i - 1]) / (volts[i] - volts[i - 1]);
      return (i - 1) * 10 + static_cast<int>(fraction * 10.0f + 0.0001f);
    }
  }
  return 0;
}

}  // namespace DisplayLogic
