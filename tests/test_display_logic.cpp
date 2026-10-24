#include "display_logic.h"

#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>

int main() {
  struct RotationCase { const char* input; int expected; };
  const RotationCase valid[] = {
    {"0", 0}, {"1", 1}, {"2", 2}, {"3", 3}, {"-1", 3}, {"-2", 2}, {"-3", 1},
    {"90", 1}, {"180", 2}, {"270", 3}, {"360", 0}, {"450", 1}, {"-90", 3}, {"-450", 3}
  };
  for (const auto& test : valid) {
    int result = -1;
    assert(DisplayLogic::parseRotationNumber(test.input, result));
    assert(result == test.expected);
  }
  const char* invalid[] = {nullptr, "", "-", "+", "4", "45", "91", "90junk", "1.0", " 90",
                          "9999999999999999999999999999999999999999999999999999999"};
  for (const char* input : invalid) {
    int result = 2;
    assert(!DisplayLogic::parseRotationNumber(input, result));
    assert(result == 2);
  }

  unsigned char payload[101];
  std::memset(payload, 'a', sizeof(payload));
  assert(DisplayLogic::payloadFits(payload, 99));
  assert(!DisplayLogic::payloadFits(payload, 100));
  assert(!DisplayLogic::payloadFits(payload, 101));
  assert(DisplayLogic::payloadFits(nullptr, 0));
  assert(!DisplayLogic::payloadFits(nullptr, 1));
  payload[4] = '\0';
  assert(!DisplayLogic::payloadFits(payload, 10));
  assert(DisplayLogic::payloadFits(payload, 4));

  // Bold/large portrait rows must use the selected font's metrics.
  assert(DisplayLogic::usesSmallFont(false, true, false, false));
  assert(!DisplayLogic::usesSmallFont(false, true, false, true));
  assert(!DisplayLogic::usesSmallFont(false, true, true, false));
  assert(DisplayLogic::usesSmallFont(true, true, true, true));
  assert(!DisplayLogic::usesSmallFont(false, false, false, false));

  assert(!DisplayLogic::refreshDue(false, false, 2000, 1000, 0, 250, 1000, 5000));
  assert(!DisplayLogic::refreshDue(true, false, 1100, 1000, 0, 250, 1000, 5000));
  assert(DisplayLogic::refreshDue(true, false, 1300, 1000, 0, 250, 1000, 5000));
  assert(!DisplayLogic::refreshDue(true, false, 1300, 1000, 900, 250, 1000, 5000));
  // Continuous traffic must not postpone the refresh indefinitely.
  assert(DisplayLogic::refreshDue(true, false, 5000, 4999, 0, 250, 1000, 5000));
  // A pending sleep bypasses debounce and the minimum refresh interval.
  assert(DisplayLogic::refreshDue(true, true, 101, 100, 100, 250, 1000, 5000));
  assert(!DisplayLogic::refreshDue(false, true, 101, 100, 100, 250, 1000, 5000));
  // ESP32 millis() rollover must preserve interval arithmetic.
  assert(DisplayLogic::refreshDue(true, false, 500, UINT32_MAX - 500, UINT32_MAX - 1500,
                                 250, 1000, 5000));

  const float levels[] = {3.30f, 3.50f, 3.60f, 3.70f, 3.75f, 3.80f,
                          3.85f, 3.92f, 4.00f, 4.10f, 4.20f};
  for (int i = 0; i <= 10; ++i) assert(DisplayLogic::batteryVoltageToPercent(levels[i]) == i * 10);
  assert(DisplayLogic::batteryVoltageToPercent(3.40f) == 5);
  assert(DisplayLogic::batteryVoltageToPercent(2.50f) == 0);
  assert(DisplayLogic::batteryVoltageToPercent(5.50f) == 100);
  assert(DisplayLogic::batteryVoltageToPercent(std::numeric_limits<float>::quiet_NaN()) == 0);
  int previous = 0;
  for (int millivolts = 2500; millivolts <= 5500; ++millivolts) {
    const int percent = DisplayLogic::batteryVoltageToPercent(millivolts / 1000.0f);
    assert(percent >= previous && percent >= 0 && percent <= 100);
    previous = percent;
  }
}
