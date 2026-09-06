#pragma once

#include <cstddef>
#include <cstdint>

namespace alertsiren {

struct DisplayInitCommand {
  uint8_t command;
  const uint8_t *parameters;
  std::size_t parameterCount;
  uint16_t delayMs;
};

struct WaveshareDisplayProfile {
  uint32_t pixelClockHz;
  int xGap;
  int yGap;
  bool mirrorX;
  bool mirrorY;
  bool bgrOrder;
  const DisplayInitCommand *commands;
  std::size_t commandCount;
};

const WaveshareDisplayProfile &waveshareDisplayProfile();

}  // namespace alertsiren
