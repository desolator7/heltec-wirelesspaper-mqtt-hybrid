#pragma once

// Copy to config.h. All deployment preferences live in that ignored file.
constexpr unsigned int SLEEP_MINUTES = 5;
constexpr unsigned long MQTT_WAIT_MS = 5000;
constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 10000;
constexpr unsigned long NETWORK_RETRY_INTERVAL_MS = 10000;
constexpr unsigned int MQTT_CONNECT_ATTEMPTS = 3;
constexpr unsigned int MQTT_SOCKET_TIMEOUT_SECONDS = 5;
constexpr unsigned int TLS_HANDSHAKE_TIMEOUT_SECONDS = 10;
constexpr unsigned long DISPLAY_DEBOUNCE_MS = 250;
constexpr unsigned long DISPLAY_MIN_REFRESH_INTERVAL_MS = 1000;
constexpr unsigned long DISPLAY_MAX_DEFER_MS = 5000;
constexpr unsigned int FULL_REFRESH_EVERY = 10;

// 0/1/2/3 = 0/90/180/270 degrees. 3 is landscape on Wireless Paper.
constexpr int DEFAULT_DISPLAY_ROTATION = 3;
constexpr int TAB_X_LANDSCAPE = 140;
constexpr int TAB_X_PORTRAIT = 74;
constexpr bool REDRAW_UPDATE_FIELD_ON_EVERY_WAKE = true;

// Certificate verification is the public template's default.
// true explicitly opts into TLS without broker authentication.
constexpr bool MQTT_TLS_INSECURE = false;
constexpr const char* NTP_SERVER = "pool.ntp.org";
constexpr unsigned long NTP_SYNC_TIMEOUT_MS = 10000;

// Check ADC wiring/polarity against your own board revision.
constexpr bool ESP_BATTERY_ENABLED = true;
constexpr int ESP_BATTERY_ADC_PIN = 20;
constexpr float ESP_BATTERY_DIVIDER = 2.0f;
constexpr int ESP_BATTERY_CTRL_PIN = 19;
constexpr int ESP_BATTERY_CTRL_ACTIVE_LEVEL = HIGH;
constexpr bool ESP_BATTERY_TRY_INVERTED_CTRL_LEVEL = true;
constexpr unsigned long ESP_BATTERY_LIVE_PUBLISH_INTERVAL_MS = 60000;
