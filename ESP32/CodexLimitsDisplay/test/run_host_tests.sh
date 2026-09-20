#!/bin/sh
set -eu

sdk_path="$(xcrun --show-sdk-path)"
base_flags="-isystem $sdk_path/usr/include/c++/v1 -isysroot $sdk_path -std=c++17 -Wall -Wextra -Werror -pedantic -I main"
limits_binary="/tmp/codex_limits_host_tests"
dashboard_binary="/tmp/codex_dashboard_host_tests"

c++ $base_flags \
  test/host/test_main.cpp \
  main/domain/limits_model.cpp \
  main/domain/poll_schedule.cpp \
  main/providers/limits_parser.cpp \
  -o "$limits_binary"

c++ $base_flags \
  test/host/test_dashboard.cpp \
  -o "$dashboard_binary"

"$limits_binary"
"$dashboard_binary"

if test -e main/platform/display.cpp || test -e main/platform/display.h; then
  echo "old display module must be removed" >&2
  exit 1
fi

grep -q "create_codex_limits_ui" main/ui/dashboard.cpp
grep -q "esp_lcd_panel_draw_bitmap" main/ui/dashboard.cpp
grep -q "lv_display_flush_ready" main/ui/dashboard.cpp
grep -q "lv_timer_handler" main/ui/dashboard.cpp
