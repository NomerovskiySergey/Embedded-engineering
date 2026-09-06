#include "providers/tryvoha_parser.h"

#include <cstddef>

namespace alertsiren {
namespace {

constexpr size_t kMaximumPayloadBytes = 4096;
constexpr std::string_view kExpectedOblast = "dnipropetrovska";
constexpr std::string_view kTargetDistrict = "dniprovskii-raion";

bool isJsonObject(const std::string_view payload) {
  const size_t first = payload.find_first_not_of(" \t\r\n");
  const size_t last = payload.find_last_not_of(" \t\r\n");
  return first != std::string_view::npos && payload[first] == '{' &&
         payload[last] == '}';
}

bool hasStringValue(const std::string_view payload, const std::string_view key,
                    const std::string_view value) {
  const std::string_view quote = "\"";
  const size_t keyPosition = payload.find(key);
  if (keyPosition == std::string_view::npos) {
    return false;
  }
  const size_t colon = payload.find(':', keyPosition + key.size());
  if (colon == std::string_view::npos) {
    return false;
  }
  const size_t valueStart = payload.find('"', colon + 1U);
  if (valueStart == std::string_view::npos) {
    return false;
  }
  return payload.substr(valueStart + quote.size(), value.size()) == value &&
         valueStart + value.size() + 1U < payload.size() &&
         payload[valueStart + value.size() + 1U] == '"';
}

bool readBoolean(const std::string_view payload, const std::string_view key,
                 bool &value) {
  const size_t keyPosition = payload.find(key);
  if (keyPosition == std::string_view::npos) {
    return false;
  }
  const size_t colon = payload.find(':', keyPosition + key.size());
  if (colon == std::string_view::npos) {
    return false;
  }
  const size_t start = payload.find_first_not_of(" \t\r\n", colon + 1U);
  if (start == std::string_view::npos) {
    return false;
  }
  const auto tokenIsDelimited = [&payload](const size_t end) {
    const size_t next = payload.find_first_not_of(" \t\r\n", end);
    return next != std::string_view::npos &&
           (payload[next] == ',' || payload[next] == '}');
  };
  if (payload.substr(start, 4U) == "true" && tokenIsDelimited(start + 4U)) {
    value = true;
    return true;
  }
  if (payload.substr(start, 5U) == "false" && tokenIsDelimited(start + 5U)) {
    value = false;
    return true;
  }
  return false;
}

bool occursExactlyOnce(const std::string_view payload,
                       const std::string_view key) {
  const size_t first = payload.find(key);
  return first != std::string_view::npos &&
         payload.find(key, first + key.size()) == std::string_view::npos;
}

bool readDistrictArray(const std::string_view payload, std::string_view &array) {
  constexpr std::string_view key = "\"districts_active\"";
  const size_t keyPosition = payload.find(key);
  if (keyPosition == std::string_view::npos) {
    return false;
  }
  const size_t start = payload.find('[', keyPosition + key.size());
  if (start == std::string_view::npos) {
    return false;
  }
  const size_t end = payload.find(']', start + 1U);
  if (end == std::string_view::npos) {
    return false;
  }
  array = payload.substr(start, end - start + 1U);
  return true;
}

}  // namespace

ProviderResult parseTryvohaResponse(const std::string_view payload) {
  if (payload.empty() || payload.size() > kMaximumPayloadBytes ||
      !isJsonObject(payload) ||
      !hasStringValue(payload, "\"slug\"", kExpectedOblast)) {
    return {false, AlertState::Unknown};
  }

  bool oblastActive = false;
  bool activeAnywhere = false;
  std::string_view districts;
  if (!occursExactlyOnce(payload, "\"active\"") ||
      !occursExactlyOnce(payload, "\"active_anywhere\"") ||
      !occursExactlyOnce(payload, "\"districts_active\"") ||
      !readBoolean(payload, "\"active\"", oblastActive) ||
      !readBoolean(payload, "\"active_anywhere\"", activeAnywhere) ||
      !readDistrictArray(payload, districts)) {
    return {false, AlertState::Unknown};
  }
  if (oblastActive && !activeAnywhere) {
    return {false, AlertState::Unknown};
  }
  if (oblastActive || hasStringValue(districts, "\"slug\"", kTargetDistrict)) {
    return {true, AlertState::Alert};
  }
  return {true, AlertState::Clear};
}

}  // namespace alertsiren
