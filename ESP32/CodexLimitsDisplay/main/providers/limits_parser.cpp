#include "providers/limits_parser.h"

#include <cctype>
#include <cstdint>
#include <cstring>
#include <limits>

namespace codexlimits {
namespace {

constexpr std::size_t kMaximumPayloadBytes = 4096U;

class Cursor {
 public:
  explicit Cursor(const std::string_view input) : _input(input) {}

  bool finished() {
    whitespace();
    return _position == _input.size();
  }

  bool consume(const char expected) {
    whitespace();
    if (_position >= _input.size() || _input[_position] != expected) {
      return false;
    }
    ++_position;
    return true;
  }

  bool string(char *const output, const std::size_t capacity) {
    whitespace();
    if (capacity == 0U || _position >= _input.size() ||
        _input[_position++] != '"') {
      return false;
    }
    std::size_t length = 0;
    while (_position < _input.size() && _input[_position] != '"') {
      const unsigned char value =
          static_cast<unsigned char>(_input[_position++]);
      if (value < 0x20U || value > 0x7EU || value == '\\' ||
          length + 1U >= capacity) {
        return false;
      }
      output[length++] = static_cast<char>(value);
    }
    if (_position >= _input.size() || _input[_position++] != '"') return false;
    output[length] = '\0';
    return length > 0U;
  }

  bool integer(int64_t &output) {
    whitespace();
    if (_position >= _input.size()) return false;
    bool negative = false;
    if (_input[_position] == '-') {
      negative = true;
      ++_position;
    }
    if (_position >= _input.size() ||
        !std::isdigit(static_cast<unsigned char>(_input[_position]))) {
      return false;
    }
    uint64_t value = 0;
    do {
      const uint8_t digit = static_cast<uint8_t>(_input[_position++] - '0');
      if (value > (std::numeric_limits<uint64_t>::max() - digit) / 10U) {
        return false;
      }
      value = value * 10U + digit;
    } while (_position < _input.size() &&
             std::isdigit(static_cast<unsigned char>(_input[_position])));
    constexpr uint64_t kInt64Maximum =
        static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
    if ((!negative && value > kInt64Maximum) ||
        (negative && value > kInt64Maximum + 1U)) {
      return false;
    }
    if (negative && value == kInt64Maximum + 1U) {
      output = std::numeric_limits<int64_t>::min();
    } else {
      output = negative ? -static_cast<int64_t>(value)
                        : static_cast<int64_t>(value);
    }
    return true;
  }

  bool boolean(bool &output) {
    whitespace();
    if (_input.substr(_position, 4U) == "true") {
      _position += 4U;
      output = true;
      return true;
    }
    if (_input.substr(_position, 5U) == "false") {
      _position += 5U;
      output = false;
      return true;
    }
    return false;
  }

  bool skipValue(const uint8_t depth = 0U) {
    if (depth > 8U) return false;
    whitespace();
    if (_position >= _input.size()) return false;
    if (_input[_position] == '"') {
      char ignored[64];
      return string(ignored, sizeof(ignored));
    }
    if (_input[_position] == '{') {
      ++_position;
      whitespace();
      if (consume('}')) return true;
      do {
        char key[64];
        if (!string(key, sizeof(key)) || !consume(':') ||
            !skipValue(static_cast<uint8_t>(depth + 1U))) {
          return false;
        }
      } while (consume(','));
      return consume('}');
    }
    if (_input[_position] == '[') {
      ++_position;
      whitespace();
      if (consume(']')) return true;
      do {
        if (!skipValue(static_cast<uint8_t>(depth + 1U))) return false;
      } while (consume(','));
      return consume(']');
    }
    int64_t ignoredInteger = 0;
    if (_input[_position] == '-' ||
        std::isdigit(static_cast<unsigned char>(_input[_position]))) {
      return integer(ignoredInteger);
    }
    bool ignoredBoolean = false;
    if (boolean(ignoredBoolean)) return true;
    if (_input.substr(_position, 4U) == "null") {
      _position += 4U;
      return true;
    }
    return false;
  }

 private:
  void whitespace() {
    while (_position < _input.size() &&
           std::isspace(static_cast<unsigned char>(_input[_position]))) {
      ++_position;
    }
  }

  std::string_view _input;
  std::size_t _position = 0;
};

bool parseWindow(Cursor &cursor, LimitWindow &window) {
  if (!cursor.consume('{')) return false;
  uint8_t seen = 0U;
  do {
    char key[24];
    if (!cursor.string(key, sizeof(key)) || !cursor.consume(':')) return false;
    uint8_t bit = 0U;
    if (std::strcmp(key, "label") == 0) {
      bit = 1U;
      if (!cursor.string(window.label, sizeof(window.label))) return false;
    } else if (std::strcmp(key, "used_percent") == 0) {
      bit = 2U;
      int64_t value = 0;
      if (!cursor.integer(value) || value < 0 || value > 100) return false;
      window.usedPercent = static_cast<uint8_t>(value);
    } else if (std::strcmp(key, "window_minutes") == 0) {
      bit = 4U;
      int64_t value = 0;
      if (!cursor.integer(value) || value <= 0 ||
          value > std::numeric_limits<uint32_t>::max()) {
        return false;
      }
      window.windowMinutes = static_cast<uint32_t>(value);
    } else if (std::strcmp(key, "resets_at") == 0) {
      bit = 8U;
      if (!cursor.integer(window.resetsAt) || window.resetsAt <= 0) return false;
    } else if (!cursor.skipValue()) {
      return false;
    }
    if (bit != 0U) {
      if ((seen & bit) != 0U) return false;
      seen = static_cast<uint8_t>(seen | bit);
    }
  } while (cursor.consume(','));
  return seen == 0x0FU && cursor.consume('}');
}

bool parseWindows(Cursor &cursor, LimitsSnapshot &snapshot) {
  if (!cursor.consume('[')) return false;
  std::size_t total = 0U;
  do {
    LimitWindow parsed = {};
    if (!parseWindow(cursor, parsed)) return false;
    if (total < snapshot.windows.size()) {
      snapshot.windows[total] = parsed;
      snapshot.count = static_cast<uint8_t>(total + 1U);
    } else {
      snapshot.truncated = true;
    }
    ++total;
  } while (cursor.consume(','));
  return total > 0U && cursor.consume(']');
}

}  // namespace

ParseResult parseLimitsPayload(const std::string_view payload) {
  ParseResult result = {false, {}};
  if (payload.empty() || payload.size() > kMaximumPayloadBytes) return result;
  Cursor cursor(payload);
  if (!cursor.consume('{')) return result;
  uint8_t seen = 0U;
  do {
    char key[24];
    if (!cursor.string(key, sizeof(key)) || !cursor.consume(':')) return result;
    uint8_t bit = 0U;
    if (std::strcmp(key, "version") == 0) {
      bit = 1U;
      int64_t version = 0;
      if (!cursor.integer(version) || version != 1) return result;
    } else if (std::strcmp(key, "generated_at") == 0) {
      bit = 2U;
      if (!cursor.integer(result.snapshot.generatedAt) ||
          result.snapshot.generatedAt <= 0) {
        return result;
      }
    } else if (std::strcmp(key, "windows") == 0) {
      bit = 4U;
      if (!parseWindows(cursor, result.snapshot)) return result;
    } else if (std::strcmp(key, "truncated") == 0) {
      bit = 8U;
      bool bridgeTruncated = false;
      if (!cursor.boolean(bridgeTruncated)) return result;
      result.snapshot.truncated = result.snapshot.truncated || bridgeTruncated;
    } else if (!cursor.skipValue()) {
      return result;
    }
    if (bit != 0U) {
      if ((seen & bit) != 0U) return result;
      seen = static_cast<uint8_t>(seen | bit);
    }
  } while (cursor.consume(','));
  result.valid = seen == 0x0FU && cursor.consume('}') && cursor.finished();
  return result;
}

}  // namespace codexlimits
