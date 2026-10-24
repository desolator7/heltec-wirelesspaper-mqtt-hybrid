/**
 * Heltec Wireless Paper v1.2 – MQTT Display (HYBRID ROTATION + MQTT BATTERY)
 * Akku-/Deep-Sleep-Modus vs. USB-/Live-Modus per MQTT steuerbar.
 *
 * Neu in dieser Version:
 *   - Rotation per Konstante und/oder retained MQTT-Topic <prefix>/rotation
 *   - Landscape-Layout wie bisher: 6 Felder
 *   - Portrait-/Hochkant-Layout: 9 Datenzeilen + Update-Feld
 *   - ESP32-/Board-Akkustand als MQTT-Rueckgabewert, nicht auf dem Display
 *   - verpflichtende lokale arduino_secrets.h und config.h ohne Fallbacks
 *
 * MQTT-Payload-Steuerzeichen am Zeilenanfang:
 *   **        = fett
 *   !!        = invertiert
 *   ##        = kleine Standardschrift
 *   ++        = große Schrift
 *   [ZUG]     = Zug-Icon
 *   [MUELL]   = Müll-Icon
 *   [SONNE]   = Sonnen-Icon
 *   [SOLAR]   = Solar-Icon
 *   [TEMP]    = Temperatur-Icon
 *   [BATT]    = Batterie-Icon
 *
 * Inline:
 *   [TAB]     = springt auf feste X-Position für rechte Spalte
 */

#include <heltec-eink-modules.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>

#include <ctime>
#include "display_logic.h"

#if !__has_include("arduino_secrets.h")
#error "Copy arduino_secrets.example.h to arduino_secrets.h and configure it."
#endif
#include "arduino_secrets.h"
#if !__has_include("config.h")
#error "Copy config.example.h to config.h and configure it."
#endif
#include "config.h"
#include "config_validation.h"

#define GxEPD_BLACK BLACK
#define GxEPD_WHITE WHITE

constexpr int LINE_COUNT = DisplayLogic::LINE_COUNT;
constexpr size_t MAX_LEN = DisplayLogic::TEXT_CAPACITY;

constexpr const char* WIFI_SSID = SECRET_WIFI_SSID;
constexpr const char* WIFI_PASSWORD = SECRET_WIFI_PASSWORD;
constexpr const char* MQTT_SERVER = SECRET_MQTT_SERVER;
constexpr int MQTT_PORT = SECRET_MQTT_PORT;
constexpr const char* MQTT_USER = SECRET_MQTT_USER;
constexpr const char* MQTT_PASSWORD = SECRET_MQTT_PASSWORD;
constexpr const char* MQTT_CLIENT_ID = SECRET_MQTT_CLIENT_ID;

// Only generic suffixes are part of the firmware; the prefix is local.
const char* const TOPIC_ZEILEN[LINE_COUNT] = {
  SECRET_MQTT_TOPIC_PREFIX "/zeile1", SECRET_MQTT_TOPIC_PREFIX "/zeile2",
  SECRET_MQTT_TOPIC_PREFIX "/zeile3", SECRET_MQTT_TOPIC_PREFIX "/zeile4",
  SECRET_MQTT_TOPIC_PREFIX "/zeile5", SECRET_MQTT_TOPIC_PREFIX "/zeile6",
  SECRET_MQTT_TOPIC_PREFIX "/zeile7", SECRET_MQTT_TOPIC_PREFIX "/zeile8",
  SECRET_MQTT_TOPIC_PREFIX "/zeile9", SECRET_MQTT_TOPIC_PREFIX "/zeile10"
};
constexpr const char* TOPIC_STATUS = SECRET_MQTT_TOPIC_PREFIX "/status";
constexpr const char* TOPIC_MODUS = SECRET_MQTT_TOPIC_PREFIX "/modus";
constexpr const char* TOPIC_ROTATION = SECRET_MQTT_TOPIC_PREFIX "/rotation";
constexpr const char* TOPIC_ESP_BATTERY_PERCENT = SECRET_MQTT_TOPIC_PREFIX "/esp/battery_percent";
constexpr const char* TOPIC_ESP_BATTERY_VOLTAGE = SECRET_MQTT_TOPIC_PREFIX "/esp/battery_voltage";
constexpr const char* TOPIC_ESP_BATTERY_JSON = SECRET_MQTT_TOPIC_PREFIX "/esp/battery";

// ── Icons 16x16 ──────────────────────────────────────────────────────────────

const unsigned char icon_sonne[] PROGMEM = {
  0x01, 0x80, 0x11, 0x88, 0x09, 0x90, 0x05, 0xa0, 0x33, 0xcc, 0x07, 0xe0, 0x0f, 0xf0, 0x0f, 0xf0,
  0x0f, 0xf0, 0x0f, 0xf0, 0x07, 0xe0, 0x33, 0xcc, 0x05, 0xa0, 0x09, 0x90, 0x11, 0x88, 0x01, 0x80
};

const unsigned char icon_zug[] PROGMEM = {
  0x07, 0xe0, 0x0f, 0xf0, 0x1f, 0xf8, 0x1f, 0xf8, 0x10, 0x08, 0x17, 0xe8, 0x17, 0xe8, 0x17, 0xe8,
  0x17, 0xe8, 0x10, 0x08, 0x1f, 0xf8, 0x1f, 0xf8, 0x0a, 0x50, 0x0a, 0x50, 0x11, 0x88, 0x00, 0x00
};

const unsigned char icon_muell[] PROGMEM = {
  0x00, 0x00, 0x03, 0xc0, 0x0f, 0xf0, 0x1f, 0xf8, 0x00, 0x00, 0x0f, 0xf0, 0x0e, 0x70, 0x0e, 0x70,
  0x0e, 0x70, 0x0e, 0x70, 0x0e, 0x70, 0x0e, 0x70, 0x0e, 0x70, 0x0f, 0xf0, 0x0f, 0xf0, 0x00, 0x00
};

// Sonne oben, minimales Solarzellen-Gitter unten.
const unsigned char icon_solar[] PROGMEM = {
  0x00, 0x00, 0x01, 0xc0, 0x01, 0x40, 0x01, 0xc0, 0x00, 0x00, 0x3f, 0xfc, 0x21, 0x04, 0x21, 0x04,
  0x3f, 0xfc, 0x21, 0x04, 0x21, 0x04, 0x3f, 0xfc, 0x21, 0x04, 0x21, 0x04, 0x3f, 0xfc, 0x00, 0x00
};

const unsigned char icon_temp[] PROGMEM = {
  0x03, 0x80, 0x04, 0x40, 0x04, 0x58, 0x04, 0x40, 0x05, 0x50, 0x05, 0x40, 0x05, 0x58, 0x05, 0x40,
  0x0d, 0x60, 0x1f, 0xe0, 0x1f, 0xe0, 0x1f, 0xe0, 0x0f, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Aufrechte Batterie mit Pluspol oben und Blitzsymbol.
const unsigned char icon_batt[] PROGMEM = {
  0x00, 0x00, 0x06, 0x60, 0x0f, 0xf0, 0x0f, 0xf0, 0x08, 0x10, 0x08, 0x10, 0x09, 0x30, 0x0b, 0x30,
  0x0f, 0xf0, 0x0e, 0x30, 0x0c, 0x30, 0x08, 0x10, 0x08, 0x10, 0x0f, 0xf0, 0x00, 0x00, 0x00, 0x00
};

// ── RTC Speicher: bleibt über Deep Sleep erhalten ────────────────────────────

RTC_DATA_ATTR char rtc_zeilen[LINE_COUNT][MAX_LEN] = {
  "init", "init", "init", "init", "init", "init", "", "", "", "Update"
};
RTC_DATA_ATTR bool  rtc_isLiveMode = false;
RTC_DATA_ATTR uint32_t bootCount = 0;
RTC_DATA_ATTR int   rtc_displayRotation = DEFAULT_DISPLAY_ROTATION;
RTC_DATA_ATTR float rtc_espBatteryVoltage = -1.0f;
RTC_DATA_ATTR int   rtc_espBatteryPercent = -1;

// ── Globale Variablen ────────────────────────────────────────────────────────

EInkDisplay_WirelessPaperV1_2 display;
WiFiClientSecure wifiClient;
PubSubClient mqtt(wifiClient);

String zeilen[LINE_COUNT];

bool displayDirty = false;
unsigned int updateCounter = 0;
bool sleepRequested = false;
unsigned long lastNetworkAttemptMs = 0;
unsigned long lastDisplayChangeMs = 0;
unsigned long lastDisplayRefreshMs = 0;
unsigned long lastBatteryPublishMs = 0;

// ── Vorwärtsdeklarationen ────────────────────────────────────────────────────

bool connectWiFi();
bool connectMQTT();
bool ensureTlsTime();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void drawRow(String text, int xOffset, int width, int yBaseline, int lineY, int tabX, bool compactDefault);
void drawDisplay();
void drawLandscapeLayout(int w, int h);
void drawPortraitLayout(int w, int h);
void goToSleep();
void saveToRTC();
void showBootScreen();
bool setIfChanged(String& target, const String& value);
void applyDisplayRotation();
bool isPortraitLayout();
bool parseRotationPayload(String value, int& rotationOut);
void updateEspBatteryStatus();
void publishEspBatteryStatus();
float readEspBatteryVoltage();
float readEspBatteryVoltageWithCtrlLevel(int ctrlLevel, const char* label);
bool isPlausibleBatteryVoltage(float voltage);
int batteryVoltageToPercent(float voltage);
String buildUpdateFooter(const String& baseText);

// ── Setup ────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  if (++bootCount == 0) bootCount = 1;
  Serial.printf("\n=== Wakeup #%lu: Heltec MQTT Hybrid ===\n", static_cast<unsigned long>(bootCount));

  if (rtc_displayRotation < 0 || rtc_displayRotation > 3) {
    rtc_displayRotation = DEFAULT_DISPLAY_ROTATION;
  }

  // Akku möglichst vor WLAN lesen. Manche ESP32/ESP32-S3-Boards nutzen dafür ADC2,
  // und ADC2 kann bei aktivem WLAN problematisch sein.
  updateEspBatteryStatus();

  // Sofortiges Lebenszeichen nach komplettem Neustart, hilfreich beim Debuggen.
  if (bootCount == 1) {
    showBootScreen();
  }

  for (int i = 0; i < LINE_COUNT; i++) {
    zeilen[i].reserve(MAX_LEN - 1);
    zeilen[i] = rtc_zeilen[i];
  }

  if (bootCount == 1) {
    zeilen[0] = "**Heltec Hybrid";
    zeilen[1] = "Warte auf MQTT...";
    zeilen[2] = "Nachrichten...";
    zeilen[3] = "";
    zeilen[4] = "";
    zeilen[5] = "Update";
    zeilen[6] = "";
    zeilen[7] = "";
    zeilen[8] = "";
    zeilen[9] = "Update";
    displayDirty = true;
  }

  if (MQTT_TLS_INSECURE) {
    Serial.println("TLS: Zertifikatspruefung lokal deaktiviert.");
    wifiClient.setInsecure();
  } else {
    wifiClient.setCACert(SECRET_MQTT_CA_CERT);
  }
  wifiClient.setHandshakeTimeout(TLS_HANDSHAKE_TIMEOUT_SECONDS);

  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  mqtt.setSocketTimeout(MQTT_SOCKET_TIMEOUT_SECONDS);
  if (!mqtt.setBufferSize(768)) {
    Serial.println("MQTT-Puffer konnte nicht reserviert werden.");
    goToSleep();
    return;
  }

  if (connectWiFi()) {
    for (unsigned int attempt = 0; attempt < MQTT_CONNECT_ATTEMPTS; ++attempt) {
      if (connectMQTT()) break;
      if (attempt + 1 < MQTT_CONNECT_ATTEMPTS) delay(1000);
    }
  }
  lastNetworkAttemptMs = millis();

  Serial.printf("Lausche fuer %lu Sekunden auf MQTT...\n", MQTT_WAIT_MS / 1000UL);
  unsigned long startWait = millis();
  while (mqtt.connected() && millis() - startWait < MQTT_WAIT_MS) {
    mqtt.loop();
    delay(10);
  }

  if (!rtc_isLiveMode && REDRAW_UPDATE_FIELD_ON_EVERY_WAKE) {
    displayDirty = true;
  }

  if (displayDirty) {
    Serial.println("-> Daten/Updatefeld geaendert. Aktualisiere Display.");
    drawDisplay();
    saveToRTC();
    displayDirty = false;
  } else {
    Serial.println("-> Keine Aenderungen fuer das Display.");
  }

  if (!rtc_isLiveMode) {
    goToSleep();
  } else {
    Serial.println("-> LIVE-MODUS aktiv. Bleibe wach.");
  }
}

// ── Loop: wird nur im Live-Modus dauerhaft erreicht ──────────────────────────

void loop() {
  if ((WiFi.status() != WL_CONNECTED || !mqtt.connected()) &&
      millis() - lastNetworkAttemptMs >= NETWORK_RETRY_INTERVAL_MS) {
    if (WiFi.status() == WL_CONNECTED || connectWiFi()) {
      connectMQTT();
    }
    // Start the retry interval after the potentially blocking attempt.
    lastNetworkAttemptMs = millis();
  }

  if (mqtt.connected()) mqtt.loop();

  if (mqtt.connected() && ESP_BATTERY_ENABLED && ESP_BATTERY_ADC_PIN >= 0 &&
      millis() - lastBatteryPublishMs >= ESP_BATTERY_LIVE_PUBLISH_INTERVAL_MS) {
    updateEspBatteryStatus();
    publishEspBatteryStatus();
  }

  // Collect a burst of retained messages before one expensive E-Ink refresh.
  if (DisplayLogic::refreshDue(displayDirty, sleepRequested, millis(), lastDisplayChangeMs,
      lastDisplayRefreshMs, DISPLAY_DEBOUNCE_MS, DISPLAY_MIN_REFRESH_INTERVAL_MS, DISPLAY_MAX_DEFER_MS)) {
    drawDisplay();
    saveToRTC();
    displayDirty = false;
  }

  // Never disconnect or re-enter mqtt.loop() from inside its callback.
  if (sleepRequested) goToSleep();
  delay(10);
}

// ── Hilfsfunktionen ──────────────────────────────────────────────────────────

void applyDisplayRotation() {
  display.setRotation(rtc_displayRotation);
  display.setTextWrap(false);
}

bool isPortraitLayout() {
  return display.height() > display.width();
}

void showBootScreen() {
  Serial.println("Erster Boot: Zeichne Startbildschirm...");
  applyDisplayRotation();
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  display.setFont(&FreeSans9pt7b);
  display.setTextSize(1);
  display.setCursor(10, 40);
  display.print("System startet...");
  display.setCursor(10, 70);
  display.print("Verbinde mit WLAN...");
  display.update();
  lastDisplayRefreshMs = millis();
}

bool setIfChanged(String& target, const String& value) {
  if (target == value) {
    return false;
  }
  target = value;
  lastDisplayChangeMs = millis();
  return true;
}

void saveToRTC() {
  for (int i = 0; i < LINE_COUNT; i++) {
    zeilen[i].toCharArray(rtc_zeilen[i], MAX_LEN);
  }
}

void goToSleep() {
  saveToRTC();
  Serial.println("Schalte Hardware ab...");

  if (mqtt.connected()) {
    mqtt.publish(TOPIC_STATUS, "offline", true);
    delay(100);
    mqtt.disconnect();
  }

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  // Heltec-Bibliothek: ersetzt display.sleep(), das bei dieser Displayklasse nicht existiert.
  if (ESP_BATTERY_CTRL_PIN >= 0) {
    pinMode(ESP_BATTERY_CTRL_PIN, INPUT);
  }
  Platform::prepareToSleep();

  Serial.printf("Gehe in Deep Sleep fuer %u Minuten...\n", SLEEP_MINUTES);
  esp_sleep_enable_timer_wakeup(SLEEP_MINUTES * 60ULL * 1000000ULL);
  esp_deep_sleep_start();
}

bool connectWiFi() {
  Serial.print("Verbinde mit WLAN");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    delay(250);
    Serial.print(".");
  }
  const bool connected = WiFi.status() == WL_CONNECTED;
  Serial.println(connected ? " verbunden." : " fehlgeschlagen.");
  return connected;
}

bool ensureTlsTime() {
  if (MQTT_TLS_INSECURE || time(nullptr) >= 1704067200) return true;
  configTime(0, 0, NTP_SERVER);
  const unsigned long start = millis();
  while (time(nullptr) < 1704067200 && millis() - start < NTP_SYNC_TIMEOUT_MS) {
    delay(100);
  }
  if (time(nullptr) >= 1704067200) return true;
  Serial.println("Zeitsynchronisierung fuer TLS fehlgeschlagen.");
  return false;
}

bool connectMQTT() {
  if (mqtt.connected()) return true;
  if (WiFi.status() != WL_CONNECTED || !ensureTlsTime()) return false;
  Serial.print("Verbinde mit MQTT...");

  const char* user = strlen(MQTT_USER) > 0 ? MQTT_USER : nullptr;
  const char* password = user != nullptr ? MQTT_PASSWORD : nullptr;
  if (!mqtt.connect(MQTT_CLIENT_ID, user, password, TOPIC_STATUS, 0, true, "offline")) {
    Serial.printf("fehlgeschlagen, rc=%d\n", mqtt.state());
    return false;
  }

  bool subscribed = true;
  for (int i = 0; i < LINE_COUNT; i++) {
    subscribed = mqtt.subscribe(TOPIC_ZEILEN[i]) && subscribed;
  }
  subscribed = mqtt.subscribe(TOPIC_MODUS) && subscribed;
  subscribed = mqtt.subscribe(TOPIC_ROTATION) && subscribed;
  if (!subscribed) {
    Serial.println("Abonnieren fehlgeschlagen; Verbindung wird erneut aufgebaut.");
    mqtt.disconnect();
    return false;
  }

  Serial.println("ok");
  mqtt.publish(TOPIC_STATUS, "online", true);
  publishEspBatteryStatus();
  return true;
}

bool parseRotationPayload(String value, int& rotationOut) {
  value.trim();
  value.toLowerCase();
  value.replace("deg", "");
  value.replace("grad", "");
  value.replace("°", "");
  value.trim();

  if (value == "portrait" || value == "hochkant" || value == "vertical" || value == "vertikal") {
    rotationOut = 0;
    return true;
  }
  if (value == "portrait_flip" || value == "portrait-flip" || value == "hochkant_flip" || value == "hochkant-flip") {
    rotationOut = 2;
    return true;
  }
  if (value == "landscape" || value == "quer" || value == "querformat") {
    rotationOut = 3;  // entspricht dem bisherigen display.landscape() auf Wireless Paper
    return true;
  }
  if (value == "landscape_flip" || value == "landscape-flip" || value == "quer_flip" || value == "quer-flip") {
    rotationOut = 1;
    return true;
  }

  return DisplayLogic::parseRotationNumber(value.c_str(), rotationOut);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  if (!DisplayLogic::payloadFits(payload, length)) {
    Serial.println("MQTT-Payload ignoriert: maximal 99 Bytes, keine NUL-Zeichen.");
    return;
  }
  String nachricht(reinterpret_cast<const char*>(payload), length);
  nachricht.trim();

  if (strcmp(topic, TOPIC_MODUS) == 0) {
    if (nachricht == "live") {
      rtc_isLiveMode = true;
      sleepRequested = false;
      Serial.println("Befehl empfangen: LIVE-Modus");
    } else if (nachricht == "sleep") {
      rtc_isLiveMode = false;
      sleepRequested = true;
      Serial.println("Befehl empfangen: DEEP-SLEEP-Modus");
    }
    return;
  }

  if (strcmp(topic, TOPIC_ROTATION) == 0) {
    int newRotation = rtc_displayRotation;
    if (parseRotationPayload(nachricht, newRotation)) {
      if (newRotation != rtc_displayRotation) {
        rtc_displayRotation = newRotation;
        updateCounter = 0;  // A rotated image needs a full refresh.
        displayDirty = true;
        lastDisplayChangeMs = millis();
        Serial.printf("Rotation gesetzt: %d\n", rtc_displayRotation);
      }
    } else {
      Serial.printf("Ungueltige Rotation ignoriert: %s\n", nachricht.c_str());
    }
    return;
  }

  for (int i = 0; i < LINE_COUNT; i++) {
    if (strcmp(topic, TOPIC_ZEILEN[i]) == 0) {
      displayDirty |= setIfChanged(zeilen[i], nachricht);
      return;
    }
  }
}

void updateEspBatteryStatus() {
  const float voltage = readEspBatteryVoltage();
  rtc_espBatteryVoltage = voltage > 0.0f ? voltage : -1.0f;
  rtc_espBatteryPercent = voltage > 0.0f ? batteryVoltageToPercent(voltage) : -1;
}

float readEspBatteryVoltage() {
#if defined(ARDUINO_ARCH_ESP32)
  if (!ESP_BATTERY_ENABLED || ESP_BATTERY_ADC_PIN < 0) {
    Serial.println("Akkumessung deaktiviert.");
    return -1.0f;
  }

  float voltage = readEspBatteryVoltageWithCtrlLevel(ESP_BATTERY_CTRL_ACTIVE_LEVEL, "primaer");
  if (voltage > 0.0f) {
    return voltage;
  }

  if (ESP_BATTERY_TRY_INVERTED_CTRL_LEVEL && ESP_BATTERY_CTRL_PIN >= 0) {
    int invertedLevel = (ESP_BATTERY_CTRL_ACTIVE_LEVEL == HIGH) ? LOW : HIGH;
    Serial.println("Primaere Akkumessung ungueltig. Versuche invertierten ADC_CTRL-Level...");

    voltage = readEspBatteryVoltageWithCtrlLevel(invertedLevel, "invertiert");
    if (voltage > 0.0f) {
      return voltage;
    }
  }

  Serial.println("Akkumessung fehlgeschlagen: kein plausibler Wert.");
  return -1.0f;
#else
  return -1.0f;
#endif
}

float readEspBatteryVoltageWithCtrlLevel(int ctrlLevel, const char* label) {
#if defined(ARDUINO_ARCH_ESP32)
  if (ESP_BATTERY_CTRL_PIN >= 0) {
    pinMode(ESP_BATTERY_CTRL_PIN, OUTPUT);
    digitalWrite(ESP_BATTERY_CTRL_PIN, ctrlLevel);
    delay(30);
  }

  analogReadResolution(12);
  analogSetPinAttenuation(ESP_BATTERY_ADC_PIN, ADC_11db);
  delay(10);

  const int samples = 12;
  uint32_t sumRaw = 0;
  uint32_t sumMv = 0;

  for (int i = 0; i < samples; i++) {
    int raw = analogRead(ESP_BATTERY_ADC_PIN);
    int mv = analogReadMilliVolts(ESP_BATTERY_ADC_PIN);
    sumRaw += raw;
    sumMv += mv;
    delay(4);
  }

  if (ESP_BATTERY_CTRL_PIN >= 0) {
    pinMode(ESP_BATTERY_CTRL_PIN, INPUT);
  }

  float rawAvg = sumRaw / (float)samples;
  float adcMv = sumMv / (float)samples;
  float batteryVoltage = (adcMv * ESP_BATTERY_DIVIDER) / 1000.0f;

  Serial.printf(
    "Akkumessung (%s, ADC_CTRL=%s): raw=%.0f, adc=%.1f mV, batt=%.2f V\n",
    label,
    ctrlLevel == HIGH ? "HIGH" : "LOW",
    rawAvg,
    adcMv,
    batteryVoltage
  );

  if (!isPlausibleBatteryVoltage(batteryVoltage)) {
    Serial.printf("Akkumessung unplausibel: %.2f V\n", batteryVoltage);
    return -1.0f;
  }

  Serial.printf("Akkumessung ok: %.2f V, %d %%\n", batteryVoltage, batteryVoltageToPercent(batteryVoltage));
  return batteryVoltage;
#else
  return -1.0f;
#endif
}

bool isPlausibleBatteryVoltage(float voltage) {
  // 1S-LiPo: ungefähr 3.0–4.2 V. Etwas breiter für Mess-/USB-Sonderfälle.
  return voltage >= 2.5f && voltage <= 5.5f;
}

int batteryVoltageToPercent(float voltage) {
  return DisplayLogic::batteryVoltageToPercent(voltage);
}

void publishEspBatteryStatus() {
  if (!mqtt.connected()) {
    return;
  }

  char percentPayload[16];
  char voltagePayload[16];

  if (!ESP_BATTERY_ENABLED || ESP_BATTERY_ADC_PIN < 0 || rtc_espBatteryPercent < 0) {
    snprintf(percentPayload, sizeof(percentPayload), "unknown");
  } else {
    snprintf(percentPayload, sizeof(percentPayload), "%d", rtc_espBatteryPercent);
  }

  if (!ESP_BATTERY_ENABLED || ESP_BATTERY_ADC_PIN < 0 || rtc_espBatteryVoltage <= 0.0f) {
    snprintf(voltagePayload, sizeof(voltagePayload), "unknown");
  } else {
    snprintf(voltagePayload, sizeof(voltagePayload), "%.2f", rtc_espBatteryVoltage);
  }

  char json[80];
  if (rtc_espBatteryPercent < 0 || rtc_espBatteryVoltage <= 0.0f) {
    snprintf(json, sizeof(json), "{\"percent\":null,\"voltage\":null,\"status\":\"unknown\"}");
  } else {
    snprintf(json, sizeof(json), "{\"percent\":%d,\"voltage\":%.2f,\"status\":\"ok\"}",
             rtc_espBatteryPercent, rtc_espBatteryVoltage);
  }
  const bool percentSent = mqtt.publish(TOPIC_ESP_BATTERY_PERCENT, percentPayload, true);
  const bool voltageSent = mqtt.publish(TOPIC_ESP_BATTERY_VOLTAGE, voltagePayload, true);
  const bool jsonSent = mqtt.publish(TOPIC_ESP_BATTERY_JSON, json, true);
  if (!percentSent || !voltageSent || !jsonSent) {
    Serial.println("MQTT Akku-Status konnte nicht vollstaendig gesendet werden.");
  }

  lastBatteryPublishMs = millis();
  Serial.printf("MQTT Akku-Status: percent=%s, voltage=%s V\n", percentPayload, voltagePayload);
}

String buildUpdateFooter(const String& baseText) {
  String footer = baseText;
  footer.trim();

  if (footer.length() == 0 || footer == "init") {
    footer = "Update";
  }

  // Kleine Schrift erzwingen, aber vorhandene Steuerzeichen dahinter weiter erlauben.
  return "##" + footer;
}

// ── Display-Funktionen ───────────────────────────────────────────────────────

void drawRow(String text, int xOffset, int width, int yBaseline, int lineY, int tabX, bool compactDefault) {
  bool isBold = false;
  bool isInverted = false;
  bool isSmall = false;
  bool isLarge = false;
  const unsigned char* currentIcon = nullptr;

  text.replace("→", "->");
  text.replace("←", "<-");
  text.replace("°C", " C");
  text.replace("\\n", "\n");

  while (text.startsWith("**") || text.startsWith("!!") || text.startsWith("##") || text.startsWith("++") ||
         text.startsWith("[ZUG]") || text.startsWith("[MUELL]") || text.startsWith("[SONNE]") ||
         text.startsWith("[SOLAR]") || text.startsWith("[TEMP]") || text.startsWith("[BATT]")) {

    if (text.startsWith("**")) {
      isBold = true;
      text = text.substring(2);
    }
    if (text.startsWith("!!")) {
      isInverted = true;
      text = text.substring(2);
    }
    if (text.startsWith("##")) {
      isSmall = true;
      text = text.substring(2);
    }
    if (text.startsWith("++")) {
      isLarge = true;
      text = text.substring(2);
    }

    if (text.startsWith("[ZUG]")) {
      currentIcon = icon_zug;
      text = text.substring(5);
    }
    if (text.startsWith("[MUELL]")) {
      currentIcon = icon_muell;
      text = text.substring(7);
    }
    if (text.startsWith("[SONNE]")) {
      currentIcon = icon_sonne;
      text = text.substring(7);
    }
    if (text.startsWith("[SOLAR]")) {
      currentIcon = icon_solar;
      text = text.substring(7);
    }
    if (text.startsWith("[TEMP]")) {
      currentIcon = icon_temp;
      text = text.substring(6);
    }
    if (text.startsWith("[BATT]")) {
      currentIcon = icon_batt;
      text = text.substring(6);
    }
  }

  int yStart = yBaseline - 20;
  int cursorX = xOffset + 5;
  int rowHeight = 24;

  if (isInverted) {
    display.fillRect(xOffset, yStart, width, rowHeight, GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
  } else {
    display.setTextColor(GxEPD_BLACK);
  }

  if (currentIcon != nullptr) {
    uint16_t iconColor = isInverted ? GxEPD_WHITE : GxEPD_BLACK;
    display.drawBitmap(cursorX, yStart + 4, currentIcon, 16, 16, iconColor);
    cursorX += 20;
  }

  int textY = yBaseline;

  const bool smallFont = DisplayLogic::usesSmallFont(isSmall, compactDefault, isLarge, isBold);
  if (smallFont) {
    display.setFont();
    display.setTextSize(1);
    textY = compactDefault ? yStart + 8 : yStart + 2;
  } else if (isLarge) {
    if (isBold) {
      display.setFont(&FreeSansBold12pt7b);
    } else {
      display.setFont(&FreeSans12pt7b);
    }
    display.setTextSize(1);
  } else {
    if (isBold) {
      display.setFont(&FreeSansBold9pt7b);
    } else {
      display.setFont(&FreeSans9pt7b);
    }
    display.setTextSize(1);
  }

  // Mehrzeilige Ausgabe explizit behandeln.
  // Wichtig: nicht den gesamten Text vor dem Drucken kuerzen, sonst wird bei
  // "Text\nZeit" die zweite Zeile abgeschnitten, wenn die erste Zeile lang ist.
  const int charWidth = smallFont ? 6 : (isLarge ? 11 : 8);
  const int lineHeight = smallFont ? 9 : (isLarge ? 20 : 16);
  const int maxLines = smallFont ? 2 : 1;

  int lineStart = 0;
  for (int lineNo = 0; lineNo < maxLines; lineNo++) {
    int newlinePos = text.indexOf('\n', lineStart);
    String lineText = (newlinePos == -1) ? text.substring(lineStart) : text.substring(lineStart, newlinePos);

    if (lineText.length() == 0 && newlinePos == -1 && lineNo > 0) {
      break;
    }

    int currentY = textY + (lineNo * lineHeight);
    int tabPos = lineText.indexOf("[TAB]");

    if (tabPos != -1) {
      String leftText = lineText.substring(0, tabPos);
      String rightText = lineText.substring(tabPos + 5);

      display.setCursor(cursorX, currentY);
      const int leftEnd = min(xOffset + width - 2, xOffset + tabX - 2);
      const int leftMaxChars = max(0, (leftEnd - cursorX) / charWidth);
      display.print(leftText.substring(0, leftMaxChars));

      int tabCursorX = xOffset + tabX;
      if (tabCursorX < xOffset + width - 5) {
        display.setCursor(tabCursorX, currentY);
        int rightMaxChars = max(1, (xOffset + width - tabCursorX - 2) / charWidth);
        display.print(rightText.substring(0, rightMaxChars));
      }
    } else {
      display.setCursor(cursorX, currentY);
      int lineMaxChars = max(1, (xOffset + width - cursorX - 2) / charWidth);
      display.print(lineText.substring(0, lineMaxChars));
    }

    if (newlinePos == -1) {
      break;
    }
    lineStart = newlinePos + 1;
  }

  if (lineY > 0) {
    uint16_t lineColor = isInverted ? GxEPD_WHITE : GxEPD_BLACK;
    display.drawLine(xOffset, lineY, xOffset + width - 1, lineY, lineColor);
  }

  display.setTextColor(GxEPD_BLACK);
}

void drawLandscapeLayout(int w, int h) {
  // Klassisches 250x122-Layout wie bisher: 4 volle Zeilen + 2 Felder unten.
  String footer = buildUpdateFooter(zeilen[5]);

  drawRow(zeilen[0], 0, w, 20, 24, TAB_X_LANDSCAPE, false);
  drawRow(zeilen[1], 0, w, 44, 48, TAB_X_LANDSCAPE, false);
  drawRow(zeilen[2], 0, w, 68, 72, TAB_X_LANDSCAPE, false);
  drawRow(zeilen[3], 0, w, 92, 96, TAB_X_LANDSCAPE, false);

  int leftWidth = min(155, w - 1);
  int rightX = leftWidth + 1;
  int rightWidth = max(1, w - rightX);
  int bottomBaseline = h - 4;
  int splitTop = h - 25;

  drawRow(zeilen[4], 0, leftWidth, bottomBaseline, 0, TAB_X_LANDSCAPE, false);
  drawRow(footer, rightX, rightWidth, bottomBaseline, 0, 52, true);
  display.drawLine(leftWidth, splitTop, leftWidth, h - 1, GxEPD_BLACK);
}

void drawPortraitLayout(int w, int h) {
  // Hochkant: 9 MQTT-Datenzeilen + Update-Feld unten.
  const int rows = 10;
  int rowHeight = h / rows;
  if (rowHeight < 22) {
    rowHeight = 22;
  }

  for (int i = 0; i < 9; i++) {
    int top = i * rowHeight;
    int baseline = top + 20;
    int lineY = min(h - 1, (i + 1) * rowHeight);
    drawRow(zeilen[i], 0, w, baseline, lineY, TAB_X_PORTRAIT, true);
  }

  int footerTop = 9 * rowHeight;
  if (footerTop > h - rowHeight) {
    footerTop = h - rowHeight;
  }
  String footer = buildUpdateFooter(zeilen[9]);
  drawRow(footer, 0, w, footerTop + 20, 0, TAB_X_PORTRAIT, true);
}

void drawDisplay() {
  applyDisplayRotation();

  int w = display.width();
  int h = display.height();
  bool portrait = isPortraitLayout();

  Serial.printf("Display: rotation=%d, width=%d, height=%d, layout=%s\n",
                rtc_displayRotation, w, h, portrait ? "portrait" : "landscape");

  // Anti-Ghosting:
  // Sleep: Vollrefresh. Live: Vollrefresh nach FULL_REFRESH_EVERY Updates.
  if (!rtc_isLiveMode || updateCounter == 0) {
    display.fastmodeOff();
  } else {
    display.fastmodeOn();
  }
  updateCounter = (updateCounter + 1) % FULL_REFRESH_EVERY;

  display.fillScreen(GxEPD_WHITE);

  if (portrait) {
    drawPortraitLayout(w, h);
  } else {
    drawLandscapeLayout(w, h);
  }

  display.update();
  lastDisplayRefreshMs = millis();
}
