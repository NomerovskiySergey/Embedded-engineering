#include "platform/waveshare_display_profile.h"

namespace alertsiren {
namespace {

constexpr uint8_t kColorMode[] = {0x55};
constexpr uint8_t kRamControl[] = {0x00, 0xE8};
constexpr uint8_t kPorchControl[] = {0x0C, 0x0C, 0x00, 0x33, 0x33};
constexpr uint8_t kGateControl[] = {0x75};
constexpr uint8_t kVcom[] = {0x1A};
constexpr uint8_t kLcmControl[] = {0x80};
constexpr uint8_t kVdvVrhEnable[] = {0x01, 0xFF};
constexpr uint8_t kVrh[] = {0x13};
constexpr uint8_t kVdv[] = {0x20};
constexpr uint8_t kFrameRate[] = {0x0F};
constexpr uint8_t kPowerControl[] = {0xA4, 0xA1};
constexpr uint8_t kPositiveGamma[] = {
    0xD0, 0x0D, 0x14, 0x0D, 0x0D, 0x09, 0x38,
    0x44, 0x4E, 0x3A, 0x17, 0x18, 0x2F, 0x30};
constexpr uint8_t kNegativeGamma[] = {
    0xD0, 0x09, 0x0F, 0x08, 0x07, 0x14, 0x37,
    0x44, 0x4D, 0x38, 0x15, 0x16, 0x2C, 0x2E};

constexpr DisplayInitCommand kCommands[] = {
    {0x11, nullptr, 0, 100},
    {0x3A, kColorMode, sizeof(kColorMode), 0},
    {0xB0, kRamControl, sizeof(kRamControl), 0},
    {0xB2, kPorchControl, sizeof(kPorchControl), 0},
    {0xB7, kGateControl, sizeof(kGateControl), 0},
    {0xBB, kVcom, sizeof(kVcom), 0},
    {0xC0, kLcmControl, sizeof(kLcmControl), 0},
    {0xC2, kVdvVrhEnable, sizeof(kVdvVrhEnable), 0},
    {0xC3, kVrh, sizeof(kVrh), 0},
    {0xC4, kVdv, sizeof(kVdv), 0},
    {0xC6, kFrameRate, sizeof(kFrameRate), 0},
    {0xD0, kPowerControl, sizeof(kPowerControl), 0},
    {0xE0, kPositiveGamma, sizeof(kPositiveGamma), 0},
    {0xE1, kNegativeGamma, sizeof(kNegativeGamma), 0},
    {0x21, nullptr, 0, 0},
    {0x29, nullptr, 0, 20},
};

constexpr WaveshareDisplayProfile kProfile = {
    12000000U, 34, 0, true, false, true, kCommands,
    sizeof(kCommands) / sizeof(kCommands[0])};

}  // namespace

const WaveshareDisplayProfile &waveshareDisplayProfile() { return kProfile; }

}  // namespace alertsiren
