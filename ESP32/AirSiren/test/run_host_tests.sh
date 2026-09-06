#!/bin/sh
set -eu

sdk_path="$(xcrun --show-sdk-path)"
test_binary="${TMPDIR:-/tmp}/airsiren_host_tests"

c++ \
  -isystem "$sdk_path/usr/include/c++/v1" \
  -isysroot "$sdk_path" \
  -std=c++17 -Wall -Wextra -Werror -pedantic \
  -I main \
  test/host/test_main.cpp \
  main/domain/alert_status.cpp \
  main/domain/poll_schedule.cpp \
  main/domain/threat_status.cpp \
  main/providers/tryvoha_parser.cpp \
  main/providers/tryvoha_events_parser.cpp \
  main/platform/waveshare_display_profile.cpp \
  -o "$test_binary"

"$test_binary"
