#!/bin/sh
set -eu

sdk_path="$(xcrun --show-sdk-path)"
test_binary="${TMPDIR:-/tmp}/codex_limits_host_tests"

c++ \
  -isystem "$sdk_path/usr/include/c++/v1" \
  -isysroot "$sdk_path" \
  -std=c++17 -Wall -Wextra -Werror -pedantic \
  -I main \
  test/host/test_main.cpp \
  main/domain/limits_model.cpp \
  main/domain/poll_schedule.cpp \
  main/providers/limits_parser.cpp \
  -o "$test_binary"

"$test_binary"
