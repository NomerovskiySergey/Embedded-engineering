#pragma once

#include <cstdint>

namespace codexlimits::dashboard {

constexpr int16_t kScreenWidth = 320;
constexpr int16_t kScreenHeight = 172;
constexpr int16_t kNativeWidth = 172;
constexpr int16_t kNativeHeight = 320;
constexpr int kMosiPin = 6;
constexpr int kClockPin = 7;
constexpr int kChipSelectPin = 14;
constexpr int kDataCommandPin = 15;
constexpr int kResetPin = 21;
constexpr int kBacklightPin = 22;
constexpr int16_t kColumnGap = 34;
constexpr uint32_t kPixelClockHz = 12000000U;
constexpr bool kBgrOrder = true;
constexpr bool kSwapAxes = true;
constexpr bool kMirrorX = false;
constexpr bool kMirrorY = true;
constexpr int16_t kPanelXGap = 0;
constexpr int16_t kPanelYGap = kColumnGap;
constexpr bool kNativeSwappedRgb565 = true;
constexpr uint32_t kLvglServiceIntervalMs = 5U;

struct Rect {
  int16_t x;
  int16_t y;
  int16_t width;
  int16_t height;
};

constexpr Rect kScreen{0, 0, kScreenWidth, kScreenHeight};
constexpr Rect kHeader{0, 0, kScreenWidth, 26};
constexpr Rect kLeftCard{8, 34, 146, 130};
constexpr Rect kRightCard{166, 34, 146, 130};
constexpr Rect kLeftTitle{18, 46, 126, 12};
constexpr Rect kLeftValue{18, 68, 126, 20};
constexpr Rect kLeftBar{18, 101, 126, 8};
constexpr Rect kLeftFooter{18, 138, 126, 12};
constexpr Rect kRightTitle{176, 46, 126, 12};
constexpr Rect kRightValue{176, 68, 126, 20};
constexpr Rect kRightBar{176, 101, 126, 8};
constexpr Rect kRightFooter{174, 138, 130, 12};

constexpr bool contains(const Rect outer, const Rect inner) {
  return outer.width >= 0 && outer.height >= 0 && inner.width >= 0 &&
         inner.height >= 0 && inner.x >= outer.x && inner.y >= outer.y &&
         inner.x + inner.width <= outer.x + outer.width &&
         inner.y + inner.height <= outer.y + outer.height;
}

constexpr bool overlaps(const Rect first, const Rect second) {
  return first.x < second.x + second.width &&
         first.x + first.width > second.x &&
         first.y < second.y + second.height &&
         first.y + first.height > second.y;
}

struct CardData {
  const char *title;
  const char *value;
  const char *footer;
  uint8_t percent;
};

struct DashboardData {
  CardData left;
  CardData right;
  bool systemActive;
};

constexpr uint8_t clampPercent(const int32_t percent) {
  return percent < 0 ? 0U : (percent > 100 ? 100U
                                           : static_cast<uint8_t>(percent));
}

constexpr DashboardData initialDashboardData() {
  return {{"TOTAL API TOKENS", "84.5k / 100k", "Reset: in 14 hours", 84U},
          {"CURRENT RATE LIMIT", "142 / 150",
           "RPM: 42/60 | TPM: 12.5k", 94U},
          true};
}

constexpr bool serviceDue(const uint32_t lastServiceMs,
                          const uint32_t nowMs,
                          const uint32_t intervalMs) {
  return static_cast<uint32_t>(nowMs - lastServiceMs) >= intervalMs;
}

struct FlushRegion {
  int16_t x;
  int16_t y;
  int16_t width;
  int16_t height;
  bool valid;
};

constexpr FlushRegion makeFlushRegion(const int32_t x1, const int32_t y1,
                                      const int32_t x2, const int32_t y2) {
  const bool valid = x1 >= 0 && y1 >= 0 && x2 >= x1 && y2 >= y1 &&
                     x2 < kScreenWidth && y2 < kScreenHeight;
  return {static_cast<int16_t>(x1), static_cast<int16_t>(y1),
          static_cast<int16_t>(valid ? x2 - x1 + 1 : 0),
          static_cast<int16_t>(valid ? y2 - y1 + 1 : 0), valid};
}

}  // namespace codexlimits::dashboard
