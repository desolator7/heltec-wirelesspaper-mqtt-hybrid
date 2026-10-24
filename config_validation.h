#pragma once

constexpr bool validTopicPrefix(const char* prefix) {
  if (*prefix == '\0') return false;
  const char* p = prefix;
  for (; *p; ++p) {
    if (*p == '+' || *p == '#' || *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') return false;
  }
  return p[-1] != '/';
}

static_assert(sizeof(SECRET_WIFI_SSID) > 1, "Configure SECRET_WIFI_SSID in arduino_secrets.h.");
static_assert(sizeof(SECRET_WIFI_PASSWORD) >= 1, "Define SECRET_WIFI_PASSWORD; an empty string allows open WiFi.");
static_assert(sizeof(SECRET_MQTT_SERVER) > 1, "Configure SECRET_MQTT_SERVER in arduino_secrets.h.");
static_assert(SECRET_MQTT_PORT > 0 && SECRET_MQTT_PORT <= 65535, "Invalid MQTT port.");
static_assert(sizeof(SECRET_MQTT_USER) >= 1 && sizeof(SECRET_MQTT_PASSWORD) >= 1,
              "Define MQTT credentials; empty strings allow anonymous MQTT.");
static_assert(sizeof(SECRET_MQTT_CLIENT_ID) > 1, "Configure a unique SECRET_MQTT_CLIENT_ID.");
static_assert(validTopicPrefix(SECRET_MQTT_TOPIC_PREFIX), "Configure a prefix without wildcards, whitespace or trailing slash.");
static_assert(sizeof(SECRET_MQTT_TOPIC_PREFIX) <= 129, "MQTT topic prefix must be at most 128 bytes.");
static_assert(MQTT_TLS_INSECURE || sizeof(SECRET_MQTT_CA_CERT) > 1, "Configure the broker CA or explicitly enable MQTT_TLS_INSECURE.");
static_assert(MQTT_TLS_INSECURE || NTP_SERVER[0] != '\0', "Configure NTP_SERVER for TLS certificate verification.");
static_assert(DEFAULT_DISPLAY_ROTATION >= 0 && DEFAULT_DISPLAY_ROTATION <= 3, "Rotation must be 0, 1, 2 or 3.");
static_assert(SLEEP_MINUTES > 0, "SLEEP_MINUTES must be positive.");
static_assert(MQTT_CONNECT_ATTEMPTS > 0 && FULL_REFRESH_EVERY > 0, "Attempt/refresh counts must be positive.");
static_assert(MQTT_SOCKET_TIMEOUT_SECONDS > 0 && MQTT_SOCKET_TIMEOUT_SECONDS <= 65535,
              "MQTT socket timeout must fit a positive uint16_t.");
static_assert(TLS_HANDSHAKE_TIMEOUT_SECONDS > 0, "TLS handshake timeout must be positive.");
static_assert(DISPLAY_MAX_DEFER_MS >= DISPLAY_MIN_REFRESH_INTERVAL_MS && DISPLAY_MAX_DEFER_MS > 0,
              "Maximum refresh deferral must be positive and at least the minimum refresh interval.");
static_assert(WIFI_CONNECT_TIMEOUT_MS > 0 && NETWORK_RETRY_INTERVAL_MS > 0 &&
              NTP_SYNC_TIMEOUT_MS > 0 && MQTT_WAIT_MS > 0 && ESP_BATTERY_LIVE_PUBLISH_INTERVAL_MS > 0,
              "Network, receive and battery intervals must be positive.");
static_assert(!ESP_BATTERY_ENABLED || ESP_BATTERY_ADC_PIN < 0 || ESP_BATTERY_DIVIDER > 0.0f,
              "Battery divider must be positive.");
static_assert(ESP_BATTERY_CTRL_ACTIVE_LEVEL == HIGH || ESP_BATTERY_CTRL_ACTIVE_LEVEL == LOW,
              "Battery control level must be HIGH or LOW.");
