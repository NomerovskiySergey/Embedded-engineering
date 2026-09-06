#include "providers/tryvoha_events_parser.h"

#include <cstddef>
#include <cstdint>

namespace alertsiren {
namespace {

constexpr std::size_t kMaximumPayloadBytes = 32768;
constexpr std::string_view kTargetOblast = "Дніпропетровська";
constexpr std::string_view kTargetCity = "Дніпро";

bool hexValue(const char value, uint32_t &digit) {
  if (value >= '0' && value <= '9') {
    digit = static_cast<uint32_t>(value - '0');
    return true;
  }
  if (value >= 'a' && value <= 'f') {
    digit = static_cast<uint32_t>(value - 'a' + 10);
    return true;
  }
  if (value >= 'A' && value <= 'F') {
    digit = static_cast<uint32_t>(value - 'A' + 10);
    return true;
  }
  return false;
}

bool readHexCodeUnit(const std::string_view encoded, std::size_t &cursor,
                     uint32_t &codeUnit) {
  if (cursor + 4U > encoded.size()) return false;
  codeUnit = 0;
  for (std::size_t count = 0; count < 4U; ++count) {
    uint32_t digit = 0;
    if (!hexValue(encoded[cursor++], digit)) return false;
    codeUnit = codeUnit * 16U + digit;
  }
  return true;
}

bool appendCodePointUtf8(const uint32_t codePoint, char (&bytes)[4],
                         std::size_t &size) {
  if (codePoint <= 0x7FU) {
    bytes[0] = static_cast<char>(codePoint);
    size = 1U;
  } else if (codePoint <= 0x7FFU) {
    bytes[0] = static_cast<char>(0xC0U | (codePoint >> 6U));
    bytes[1] = static_cast<char>(0x80U | (codePoint & 0x3FU));
    size = 2U;
  } else if (codePoint <= 0xFFFFU) {
    if (codePoint >= 0xD800U && codePoint <= 0xDFFFU) return false;
    bytes[0] = static_cast<char>(0xE0U | (codePoint >> 12U));
    bytes[1] = static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU));
    bytes[2] = static_cast<char>(0x80U | (codePoint & 0x3FU));
    size = 3U;
  } else if (codePoint <= 0x10FFFFU) {
    bytes[0] = static_cast<char>(0xF0U | (codePoint >> 18U));
    bytes[1] = static_cast<char>(0x80U | ((codePoint >> 12U) & 0x3FU));
    bytes[2] = static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU));
    bytes[3] = static_cast<char>(0x80U | (codePoint & 0x3FU));
    size = 4U;
  } else {
    return false;
  }
  return true;
}

bool jsonStringEquals(const std::string_view encoded,
                      const std::string_view expectedUtf8) {
  std::size_t source = 0;
  std::size_t expected = 0;
  while (source < encoded.size()) {
    if (encoded[source] != '\\') {
      if (expected >= expectedUtf8.size() ||
          encoded[source++] != expectedUtf8[expected++]) return false;
      continue;
    }
    ++source;
    if (source >= encoded.size()) return false;
    uint32_t codePoint = 0;
    if (encoded[source] == 'u') {
      ++source;
      if (!readHexCodeUnit(encoded, source, codePoint)) return false;
      if (codePoint >= 0xD800U && codePoint <= 0xDBFFU) {
        if (source + 2U > encoded.size() || encoded[source] != '\\' ||
            encoded[source + 1U] != 'u') return false;
        source += 2U;
        uint32_t low = 0;
        if (!readHexCodeUnit(encoded, source, low) || low < 0xDC00U ||
            low > 0xDFFFU) return false;
        codePoint = 0x10000U + ((codePoint - 0xD800U) << 10U) +
                    (low - 0xDC00U);
      }
    } else {
      const char escape = encoded[source++];
      switch (escape) {
        case '"': codePoint = '"'; break;
        case '\\': codePoint = '\\'; break;
        case '/': codePoint = '/'; break;
        case 'b': codePoint = '\b'; break;
        case 'f': codePoint = '\f'; break;
        case 'n': codePoint = '\n'; break;
        case 'r': codePoint = '\r'; break;
        case 't': codePoint = '\t'; break;
        default: return false;
      }
    }
    char bytes[4] = {};
    std::size_t size = 0;
    if (!appendCodePointUtf8(codePoint, bytes, size) ||
        expected + size > expectedUtf8.size()) return false;
    for (std::size_t index = 0; index < size; ++index) {
      if (bytes[index] != expectedUtf8[expected++]) return false;
    }
  }
  return expected == expectedUtf8.size();
}

bool isJsonArray(const std::string_view payload) {
  const auto first = payload.find_first_not_of(" \t\r\n");
  const auto last = payload.find_last_not_of(" \t\r\n");
  return first != std::string_view::npos && payload[first] == '[' &&
         payload[last] == ']';
}

bool readString(const std::string_view object, const std::string_view key,
                std::string_view &value) {
  const auto keyPosition = object.find(key);
  if (keyPosition == std::string_view::npos) return false;
  const auto colon = object.find(':', keyPosition + key.size());
  if (colon == std::string_view::npos) return false;
  const auto start = object.find('"', colon + 1U);
  if (start == std::string_view::npos) return false;
  const auto end = object.find('"', start + 1U);
  if (end == std::string_view::npos) return false;
  value = object.substr(start + 1U, end - start - 1U);
  return true;
}

bool readBoolean(const std::string_view object, const std::string_view key,
                 bool &value) {
  const auto keyPosition = object.find(key);
  if (keyPosition == std::string_view::npos) return false;
  const auto colon = object.find(':', keyPosition + key.size());
  if (colon == std::string_view::npos) return false;
  const auto start = object.find_first_not_of(" \t\r\n", colon + 1U);
  if (start == std::string_view::npos) return false;
  const auto delimited = [&object](const std::size_t end) {
    const auto next = object.find_first_not_of(" \t\r\n", end);
    return next != std::string_view::npos &&
           (object[next] == ',' || object[next] == '}');
  };
  if (object.substr(start, 4U) == "true" && delimited(start + 4U)) {
    value = true;
    return true;
  }
  if (object.substr(start, 5U) == "false" && delimited(start + 5U)) {
    value = false;
    return true;
  }
  return false;
}

bool occursAtMostOnce(const std::string_view object,
                      const std::string_view key) {
  const auto first = object.find(key);
  return first == std::string_view::npos ||
         object.find(key, first + key.size()) == std::string_view::npos;
}

bool criticalFieldsAreUnique(const std::string_view object) {
  constexpr std::string_view keys[] = {
      "\"type\"",       "\"subtype\"",   "\"oblast\"",
      "\"target_name\"", "\"is_active\"", "\"expires_at\""};
  for (const auto key : keys) {
    if (!occursAtMostOnce(object, key)) return false;
  }
  return true;
}

bool parseDigits(const std::string_view value, const std::size_t start,
                 const std::size_t count, int &result) {
  result = 0;
  if (start + count > value.size()) return false;
  for (std::size_t index = start; index < start + count; ++index) {
    if (value[index] < '0' || value[index] > '9') return false;
    result = result * 10 + (value[index] - '0');
  }
  return true;
}

int64_t daysFromCivil(int year, const unsigned month, const unsigned day) {
  year -= month <= 2U;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yearOfEra = static_cast<unsigned>(year - era * 400);
  const unsigned adjustedMonth = month > 2U ? month - 3U : month + 9U;
  const unsigned dayOfYear =
      (153U * adjustedMonth + 2U) / 5U + day - 1U;
  const unsigned dayOfEra =
      yearOfEra * 365U + yearOfEra / 4U - yearOfEra / 100U + dayOfYear;
  return static_cast<int64_t>(era) * 146097LL + dayOfEra - 719468LL;
}

bool validCalendarDate(const int year, const int month, const int day) {
  constexpr int daysByMonth[] = {31, 28, 31, 30, 31, 30,
                                 31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12 || day < 1) return false;
  int maximumDay = daysByMonth[month - 1];
  const bool leapYear =
      (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
  if (month == 2 && leapYear) maximumDay = 29;
  return day <= maximumDay;
}

bool parseUtcTimestamp(const std::string_view value, int64_t &epoch) {
  if (value.size() < 20U || value[4] != '-' || value[7] != '-' ||
      value[10] != 'T' || value[13] != ':' || value[16] != ':' ||
      value.back() != 'Z') return false;
  if (value.size() != 20U) {
    if (value[19] != '.' || value.size() == 21U) return false;
    for (std::size_t index = 20U; index + 1U < value.size(); ++index) {
      if (value[index] < '0' || value[index] > '9') return false;
    }
  }
  int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
  if (!parseDigits(value, 0, 4, year) || !parseDigits(value, 5, 2, month) ||
      !parseDigits(value, 8, 2, day) || !parseDigits(value, 11, 2, hour) ||
      !parseDigits(value, 14, 2, minute) ||
      !parseDigits(value, 17, 2, second) ||
      !validCalendarDate(year, month, day) || hour > 23 || minute > 59 ||
      second > 60) {
    return false;
  }
  epoch = daysFromCivil(year, static_cast<unsigned>(month),
                        static_cast<unsigned>(day)) * 86400LL +
          hour * 3600LL + minute * 60LL + second;
  return true;
}

bool relevant(const std::string_view object) {
  std::string_view oblast;
  if (readString(object, "\"oblast\"", oblast) &&
      jsonStringEquals(oblast, kTargetOblast)) {
    return true;
  }
  std::string_view target;
  return readString(object, "\"target_name\"", target) &&
         jsonStringEquals(target, kTargetCity);
}

bool classify(const std::string_view object, ThreatType &type) {
  std::string_view code;
  if (!readString(object, "\"type\"", code)) return false;
  if (code == "m") {
    std::string_view subtype;
    type = readString(object, "\"subtype\"", subtype) && subtype == "kab"
               ? ThreatType::GuidedBomb
               : ThreatType::Missile;
  } else if (code == "d") {
    type = ThreatType::StrikeDrone;
  } else if (code == "rd") {
    type = ThreatType::ReconDrone;
  } else if (code == "a") {
    type = ThreatType::Warning;
  } else if (code == "u") {
    type = ThreatType::Other;
  } else {
    type = ThreatType::Other;
  }
  return true;
}

}  // namespace

ThreatResult parseTryvohaEvents(const std::string_view payload,
                                const int64_t nowEpochSeconds) {
  if (payload.empty() || payload.size() > kMaximumPayloadBytes ||
      !isJsonArray(payload)) {
    return {false, ThreatType::Unknown};
  }

  ThreatType selected = ThreatType::Unknown;
  bool found = false;
  std::size_t cursor = 0;
  while ((cursor = payload.find('{', cursor)) != std::string_view::npos) {
    const auto end = payload.find('}', cursor + 1U);
    if (end == std::string_view::npos) return {false, ThreatType::Unknown};
    const auto object = payload.substr(cursor, end - cursor + 1U);
    if (!criticalFieldsAreUnique(object)) return {false, ThreatType::Unknown};
    bool active = false;
    if (!readBoolean(object, "\"is_active\"", active)) {
      return {false, ThreatType::Unknown};
    }
    if (active && relevant(object)) {
      ThreatType current = ThreatType::Unknown;
      if (!classify(object, current)) return {false, ThreatType::Unknown};
      if (nowEpochSeconds > 0) {
        std::string_view expiry;
        int64_t expiryEpoch = 0;
        if (!readString(object, "\"expires_at\"", expiry) ||
            !parseUtcTimestamp(expiry, expiryEpoch)) {
          return {false, ThreatType::Unknown};
        }
        if (expiryEpoch <= nowEpochSeconds) {
          cursor = end + 1U;
          continue;
        }
      }
      if (!found) {
        selected = current;
        found = true;
      } else if (selected != current) {
        selected = ThreatType::Multiple;
      }
    }
    cursor = end + 1U;
  }
  return {true, found ? selected : ThreatType::Unknown};
}

}  // namespace alertsiren
