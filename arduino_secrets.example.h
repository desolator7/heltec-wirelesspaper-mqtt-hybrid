#pragma once

// Copy to arduino_secrets.h and fill in locally. No values have fallbacks.
#define SECRET_WIFI_SSID ""
#define SECRET_WIFI_PASSWORD ""
#define SECRET_MQTT_SERVER ""
#define SECRET_MQTT_PORT 8883
#define SECRET_MQTT_USER ""
#define SECRET_MQTT_PASSWORD ""
#define SECRET_MQTT_CLIENT_ID ""

// Unique device prefix without a trailing slash or MQTT wildcards.
#define SECRET_MQTT_TOPIC_PREFIX ""

// PEM root CA for the MQTT broker. Required when MQTT_TLS_INSECURE is false.
#define SECRET_MQTT_CA_CERT ""
