#include <cstdint>
#include <cstring>
#include <iostream>

#include "ui/dashboard_model.h"

namespace {
int failures = 0;
#define CHECK(condition) do { if (!(condition)) { std::cerr << __func__ << ':' << __LINE__ << ": check failed: " << #condition << '\n'; ++failures; } } while (false)

void layout_fits_ultrawide_display_without_overlap() {
  using namespace codexlimits::dashboard;
  CHECK(kScreenWidth == 320 && kScreenHeight == 172);
  CHECK(kHeader.x == 0 && kHeader.y == 0 && kHeader.width == 320 && kHeader.height == 26);
  CHECK(kLeftCard.x == 8 && kLeftCard.y == 34 && kLeftCard.width == 146 && kLeftCard.height == 130);
  CHECK(kRightCard.x == 166 && kRightCard.y == 34 && kRightCard.width == 146 && kRightCard.height == 130);
  CHECK(contains(kScreen, kHeader));
  CHECK(contains(kScreen, kLeftCard));
  CHECK(contains(kScreen, kRightCard));
  CHECK(!overlaps(kLeftCard, kRightCard));
}

void card_content_is_bounded() {
  using namespace codexlimits::dashboard;
  const Rect leftContent[] = {kLeftTitle, kLeftValue, kLeftBar, kLeftFooter};
  const Rect rightContent[] = {kRightTitle, kRightValue, kRightBar, kRightFooter};
  for (const Rect &item : leftContent) CHECK(contains(kLeftCard, item));
  for (const Rect &item : rightContent) CHECK(contains(kRightCard, item));
}

void model_is_bounded_and_matches_approved_design() {
  using namespace codexlimits::dashboard;
  const DashboardData initial = initialDashboardData();
  CHECK(std::strcmp(initial.left.title, "TOTAL API TOKENS") == 0);
  CHECK(std::strcmp(initial.left.value, "84.5k / 100k") == 0);
  CHECK(std::strcmp(initial.left.footer, "Reset: in 14 hours") == 0);
  CHECK(initial.left.percent == 84U);
  CHECK(std::strcmp(initial.right.title, "CURRENT RATE LIMIT") == 0);
  CHECK(std::strcmp(initial.right.value, "142 / 150") == 0);
  CHECK(std::strcmp(initial.right.footer, "RPM: 42/60 | TPM: 12.5k") == 0);
  CHECK(initial.right.percent == 94U);
  CHECK(clampPercent(-1) == 0U);
  CHECK(clampPercent(0) == 0U);
  CHECK(clampPercent(100) == 100U);
  CHECK(clampPercent(101) == 100U);
}

void lvgl_service_schedule_handles_rollover() {
  using codexlimits::dashboard::serviceDue;
  CHECK(!serviceDue(100U, 104U, 5U));
  CHECK(serviceDue(100U, 105U, 5U));
  CHECK(!serviceDue(UINT32_MAX - 2U, 1U, 5U));
  CHECK(serviceDue(UINT32_MAX - 2U, 2U, 5U));
}

void hardware_profile_remains_verified() {
  using namespace codexlimits::dashboard;
  CHECK(kNativeWidth == 172 && kNativeHeight == 320);
  CHECK(kMosiPin == 6 && kClockPin == 7);
  CHECK(kChipSelectPin == 14 && kDataCommandPin == 15);
  CHECK(kResetPin == 21 && kBacklightPin == 22);
  CHECK(kColumnGap == 34);
  CHECK(kPixelClockHz == 12000000U);
  CHECK(kBgrOrder);
}

void landscape_transform_and_transport_are_explicit() {
  using namespace codexlimits::dashboard;
  CHECK(kSwapAxes);
  CHECK(!kMirrorX);
  CHECK(kMirrorY);
  CHECK(kPanelXGap == 0);
  CHECK(kPanelYGap == 34);
  CHECK(kNativeSwappedRgb565);
}

void flush_regions_are_validated_and_inclusive() {
  using namespace codexlimits::dashboard;
  const FlushRegion full = makeFlushRegion(0, 0, 319, 171);
  CHECK(full.valid && full.x == 0 && full.y == 0);
  CHECK(full.width == 320 && full.height == 172);
  const FlushRegion pixel = makeFlushRegion(9, 7, 9, 7);
  CHECK(pixel.valid && pixel.width == 1 && pixel.height == 1);
  CHECK(!makeFlushRegion(-1, 0, 2, 2).valid);
  CHECK(!makeFlushRegion(0, -1, 2, 2).valid);
  CHECK(!makeFlushRegion(10, 10, 9, 10).valid);
  CHECK(!makeFlushRegion(0, 0, 320, 171).valid);
  CHECK(!makeFlushRegion(0, 0, 319, 172).valid);
}
}  // namespace

int main() {
  layout_fits_ultrawide_display_without_overlap();
  card_content_is_bounded();
  model_is_bounded_and_matches_approved_design();
  lvgl_service_schedule_handles_rollover();
  hardware_profile_remains_verified();
  landscape_transform_and_transport_are_explicit();
  flush_regions_are_validated_and_inclusive();
  if (failures != 0) {
    std::cerr << failures << " dashboard checks failed\n";
    return 1;
  }
  std::cout << "All Codex dashboard host tests passed\n";
  return 0;
}
