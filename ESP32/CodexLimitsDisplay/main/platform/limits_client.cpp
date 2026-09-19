#include "platform/limits_client.h"

#include <cstdio>
#include <cstring>
#include <string_view>

#include "esp_http_client.h"
#include "freertos/task.h"
#include "providers/limits_parser.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#define CODEX_LIMITS_BRIDGE_URL "http://192.168.1.10:8765/v1/limits"
#define CODEX_LIMITS_BRIDGE_SECRET "GENERATE_A_LONG_RANDOM_SECRET"
#endif

namespace codexlimits {

esp_err_t limitsHttpEventHandler(esp_http_client_event_t *event) {
  auto *const client = static_cast<LimitsClient *>(event->user_data);
  if (event->event_id != HTTP_EVENT_ON_DATA || event->data_len <= 0) {
    return ESP_OK;
  }
  const std::size_t incoming = static_cast<std::size_t>(event->data_len);
  if (client->_responseLength + incoming >= LimitsClient::kResponseCapacity) {
    client->_overflow = true;
    return ESP_FAIL;
  }
  std::memcpy(client->_response + client->_responseLength, event->data,
              incoming);
  client->_responseLength += incoming;
  client->_response[client->_responseLength] = '\0';
  return ESP_OK;
}

bool LimitsClient::configured() const {
  return std::strcmp(CODEX_LIMITS_BRIDGE_SECRET,
                     "GENERATE_A_LONG_RANDOM_SECRET") != 0 &&
         CODEX_LIMITS_BRIDGE_SECRET[0] != '\0' &&
         std::strncmp(CODEX_LIMITS_BRIDGE_URL, "http://", 7U) == 0;
}

esp_err_t LimitsClient::begin() {
  _requestQueue = xQueueCreate(1U, sizeof(uint8_t));
  _resultQueue = xQueueCreate(1U, sizeof(FetchResult));
  if (_requestQueue == nullptr || _resultQueue == nullptr) {
    return ESP_ERR_NO_MEM;
  }
  const BaseType_t created =
      xTaskCreate(&LimitsClient::taskEntry, "limits_http", 8192U, this, 5U,
                  nullptr);
  return created == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

bool LimitsClient::requestFetch() {
  const uint8_t request = 1U;
  return _requestQueue != nullptr &&
         xQueueSend(_requestQueue, &request, 0U) == pdTRUE;
}

bool LimitsClient::takeResult(FetchResult &result) {
  return _resultQueue != nullptr &&
         xQueueReceive(_resultQueue, &result, 0U) == pdTRUE;
}

void LimitsClient::taskEntry(void *context) {
  static_cast<LimitsClient *>(context)->taskLoop();
}

void LimitsClient::taskLoop() {
  uint8_t request = 0U;
  while (true) {
    if (xQueueReceive(_requestQueue, &request, portMAX_DELAY) == pdTRUE) {
      const FetchResult result = fetch();
      xQueueOverwrite(_resultQueue, &result);
    }
  }
}

FetchResult LimitsClient::fetch() {
  FetchResult output = {};
  _responseLength = 0U;
  _overflow = false;
  _response[0] = '\0';
  char authorization[192] = {};
  const int headerLength =
      std::snprintf(authorization, sizeof(authorization), "Bearer %s",
                    CODEX_LIMITS_BRIDGE_SECRET);
  if (headerLength <= 7 ||
      static_cast<std::size_t>(headerLength) >= sizeof(authorization)) {
    return output;
  }
  esp_http_client_config_t config = {};
  config.url = CODEX_LIMITS_BRIDGE_URL;
  config.event_handler = &limitsHttpEventHandler;
  config.user_data = this;
  config.timeout_ms = 7000;
  esp_http_client_handle_t http = esp_http_client_init(&config);
  if (http == nullptr) return output;
  esp_http_client_set_header(http, "Authorization", authorization);
  esp_http_client_set_header(http, "Accept", "application/json");
  const esp_err_t requestResult = esp_http_client_perform(http);
  const int status = esp_http_client_get_status_code(http);
  esp_http_client_cleanup(http);
  if (requestResult != ESP_OK || status != 200 || _overflow ||
      _responseLength == 0U) {
    return output;
  }
  const ParseResult parsed = parseLimitsPayload(
      std::string_view(_response, static_cast<std::size_t>(_responseLength)));
  output.valid = parsed.valid;
  output.snapshot = parsed.snapshot;
  return output;
}

}  // namespace codexlimits
