#include "platform/alert_client.h"

#include <cstring>
#include <string_view>

#include "esp_crt_bundle.h"
#include "esp_http_client.h"

namespace alertsiren {
namespace {
constexpr char kEndpoint[] =
    "https://tryvoha.online/api/v1/alerts/dnipropetrovska";
constexpr char kEventsEndpoint[] = "https://tryvoha.online/api/events";
}

esp_err_t httpEventHandler(esp_http_client_event_t *event) {
  auto *const client = static_cast<AlertClient *>(event->user_data);
  if (event->event_id != HTTP_EVENT_ON_DATA || event->data_len <= 0) {
    return ESP_OK;
  }
  const size_t incoming = static_cast<size_t>(event->data_len);
  if (client->_responseLength + incoming >= AlertClient::kResponseCapacity) {
    client->_overflow = true;
    return ESP_FAIL;
  }
  std::memcpy(client->_response + client->_responseLength, event->data,
              incoming);
  client->_responseLength += incoming;
  client->_response[client->_responseLength] = '\0';
  return ESP_OK;
}

bool AlertClient::fetchPayload(const char *const url) {
  _responseLength = 0;
  _overflow = false;
  _response[0] = '\0';
  esp_http_client_config_t config = {};
  config.url = url;
  config.event_handler = &httpEventHandler;
  config.user_data = this;
  config.timeout_ms = 7000;
  config.crt_bundle_attach = esp_crt_bundle_attach;
  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (client == nullptr) return false;
  const esp_err_t requestResult = esp_http_client_perform(client);
  const int status = esp_http_client_get_status_code(client);
  esp_http_client_cleanup(client);
  return requestResult == ESP_OK && status == 200 && !_overflow &&
         _responseLength > 0;
}

ProviderResult AlertClient::fetch() {
  if (!fetchPayload(kEndpoint)) return {false, AlertState::Unknown};
  return parseTryvohaResponse(
      std::string_view(_response, static_cast<size_t>(_responseLength)));
}

ThreatResult AlertClient::fetchThreats(const int64_t nowEpochSeconds) {
  if (!fetchPayload(kEventsEndpoint)) return {false, ThreatType::Unknown};
  return parseTryvohaEvents(
      std::string_view(_response, static_cast<size_t>(_responseLength)),
      nowEpochSeconds);
}

}  // namespace alertsiren
