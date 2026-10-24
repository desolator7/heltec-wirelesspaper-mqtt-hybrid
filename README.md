# Heltec Wireless Paper MQTT Hybrid

MQTT-Anzeige für **Heltec Wireless Paper V1.2** mit ESP32-S3 und E-Ink-Display. Der Betriebsmodus und die Ausrichtung lassen sich per retained MQTT-Nachricht ändern. Im Schlafmodus bleiben Texte, Modus und Rotation im RTC-Speicher erhalten; im Live-Modus werden Änderungen laufend angezeigt.

## Funktionen

- Querformat mit sechs Feldern, Hochformat mit neun Datenzeilen und einem Update-Feld.
- Formatierung, sechs Symbole, zwei Textspalten und kleine zweizeilige Texte.
- Akkuspannung und geschätzter Ladezustand als retained MQTT-Sensorwerte.
- MQTT-Last-Will und Statusmeldungen für Home Assistant.
- TLS mit konfigurierbarem Broker-Zertifikat und begrenzten Verbindungsversuchen.

Die Auswahl `live`/`sleep` erfolgt per MQTT. Eine automatische USB-Erkennung ist nicht implementiert. Das Update-Feld zeigt den vom Publisher gelieferten Text; die Firmware erzeugt keinen eigenen Zeitstempel.

## Einrichtung

Benötigt werden die Arduino IDE oder `arduino-cli` und diese getesteten Versionen:

| Komponente | Version |
| --- | --- |
| Heltec ESP32 Series Dev-boards | 3.3.8 |
| heltec-eink-modules | 4.6.0 |
| PubSubClient | 2.8 |

In den Arduino-Einstellungen diese zusätzliche Boardverwalter-URL eintragen:

```text
https://github.com/Heltec-Aaron-Lee/WiFi_Kit_series/releases/latest/download/package_heltec_esp32_index.json
```

Board **Heltec Wireless Paper** wählen. Die Displayklasse im Sketch ist `EInkDisplay_WirelessPaperV1_2`; für andere Displayrevisionen muss sie angepasst werden. Weitere Informationen bietet die [Anleitung der Displaybibliothek](https://github.com/todd-herbert/heltec-eink-modules/blob/main/docs/WirelessPaper/wireless_paper.md).

Die beiden Vorlagen lokal kopieren:

```sh
cp arduino_secrets.example.h arduino_secrets.h
cp config.example.h config.h
```

In `arduino_secrets.h` WLAN, Broker, Port, MQTT-Anmeldung, eine eindeutige Gerätekennung, den gewünschten Topic-Präfix und die Root-CA des Brokers eintragen. Der Präfix darf keine MQTT-Wildcards, Leerzeichen oder abschließenden Schrägstriche enthalten. Ein leerer Benutzername erlaubt explizit anonyme MQTT-Anmeldung; ein leeres WLAN-Passwort erlaubt ein offenes WLAN.

In `config.h` stehen alle persönlichen Betriebsparameter: Schlafintervall, Rotation, Spaltenpositionen, Verbindungsfristen, Refresh-Verhalten, NTP und Akku-Pins/Kalibrierung. Die Vorlagen enthalten allgemeine Ausgangswerte. Der Sketch verlangt beide lokalen Dateien und prüft Pflichtwerte beim Kompilieren. Es gibt keine eingebauten WLAN-, Broker-, Gerätekennungs- oder Topic-Präfix-Fallbacks.

Beide lokalen Dateien sowie Build-Ausgaben und lokale Sicherungen werden von Git ignoriert. Firmwaredateien enthalten die einkompilierten Zugangsdaten und gehören ebenfalls nicht ins Repository.

### TLS

Die öffentliche Vorlage setzt `MQTT_TLS_INSECURE = false`. Dafür müssen `SECRET_MQTT_CA_CERT` ein gültiges PEM-Root-Zertifikat und `NTP_SERVER` einen erreichbaren Zeitserver enthalten. Der konfigurierte Brokername muss zum Zertifikat passen. Ein Zertifikat kann als C++-Raw-String eingetragen werden:

```cpp
#define SECRET_MQTT_CA_CERT R"PEM(
-----BEGIN CERTIFICATE-----
... eigenes Root-CA-Zertifikat ...
-----END CERTIFICATE-----
)PEM"
```

Die Uhr wird vor dem TLS-Verbindungsaufbau synchronisiert, sofern noch keine plausible Systemzeit vorliegt. Ohne erfolgreiche Zeitsynchronisierung wird die verifizierte Verbindung nicht aufgebaut. Wer `MQTT_TLS_INSECURE = true` ausdrücklich setzt, deaktiviert die Prüfung des Broker-Zertifikats; die Firmware meldet diese Einstellung im seriellen Log. Sie aktiviert sie nicht automatisch bei Fehlern.

### Kompilieren

In der Arduino IDE den Sketch `heltec_wirelesspaper_mqtt_hybrid_final.ino` öffnen. Der Sketchordner muss denselben Namen wie die `.ino`-Datei haben. Alternativ erstellt das Buildskript automatisch einen passend benannten temporären Sketch, unabhängig vom Namen des Git-Checkouts:

```sh
python3 scripts/build.py
```

Falls die CLI nicht im Suchpfad liegt, `ARDUINO_CLI` setzen oder `--cli /pfad/zu/arduino-cli` übergeben. Der vollständige Boardname lautet `Heltec-esp32:esp32:heltec_wireless_paper`.

Optional erzeugt `python3 scripts/build.py --output-dir build/firmware` lokale Firmwaredateien. Die Build-Ausgaben liegen im ignorierten Verzeichnis `build/`. Das Skript lädt nichts auf ein Gerät hoch.

## MQTT-Schnittstelle

`<prefix>` steht für den ausschließlich lokal konfigurierten `SECRET_MQTT_TOPIC_PREFIX`.

| Topic | Richtung | Inhalt |
| --- | --- | --- |
| `<prefix>/zeile1` bis `/zeile10` | zum Gerät | Anzeigetext, sinnvollerweise retained |
| `<prefix>/modus` | zum Gerät | `live` oder `sleep`, retained |
| `<prefix>/rotation` | zum Gerät | `0`–`3`, Gradwerte in 90°-Schritten oder Layoutname, retained |
| `<prefix>/status` | vom Gerät | `online` / `offline`, retained; Last-Will `offline` |
| `<prefix>/esp/battery_percent` | vom Gerät | Prozentzahl oder `unknown`, retained |
| `<prefix>/esp/battery_voltage` | vom Gerät | Spannung in Volt oder `unknown`, retained |
| `<prefix>/esp/battery` | vom Gerät | JSON mit `percent`, `voltage`, `status`, retained |

Im Querformat sind Zeilen 1–4 volle Zeilen, Zeile 5 das linke untere Feld und Zeile 6 das Update-Feld. Im Hochformat sind Zeilen 1–9 Datenzeilen, Zeile 10 ist das Update-Feld.

Eine MQTT-Payload darf **höchstens 99 Bytes** enthalten; das gilt inklusive Formatierung. Längere Nachrichten und Nachrichten mit NUL-Bytes werden vollständig verworfen und protokolliert. Dadurch stimmen aktive Anzeige und RTC-Speicher nach dem Aufwachen überein. Eine leere Payload löscht die jeweilige Zeile. Die Grenze zählt Bytes, daher benötigen UTF-8-Zeichen teilweise mehrere Bytes. Die mitgelieferten GFX-Schriften unterstützen nicht beliebige Unicode-Zeichen.

Akzeptierte Rotationsnamen sind unter anderem `portrait`, `portrait_flip`, `landscape`, `landscape_flip`, `hochkant`, `quer` und `querformat`. `landscape` entspricht Rotation 3. Zahlen können die Endungen `deg`, `grad` oder `°` tragen. Ungültige oder übergroße Zahlen ändern die Rotation nicht.

Der Schlafmodus empfängt nach jedem Aufwachen für `MQTT_WAIT_MS` Nachrichten. Danach meldet das Gerät `offline` und schläft für `SLEEP_MINUTES`. Ein schlafendes Gerät kann erst beim nächsten Aufwachen einen neuen Modusbefehl empfangen. Ein retained `live`-Befehl hält es dann wach.

### Textformatierung

| Präfix / Zeichen | Wirkung |
| --- | --- |
| `**` | fett |
| `!!` | invertiert |
| `##` | kleine Standardschrift |
| `++` | große Schrift |
| `[ZUG]`, `[MUELL]`, `[SONNE]`, `[SOLAR]`, `[TEMP]`, `[BATT]` | 16×16-Symbol |
| `[TAB]` | rechte Spalte |
| `\n` oder echter Zeilenumbruch | zweite Textzeile bei kleiner Schrift |

Präfixe sind kombinierbar, beispielsweise `**[ZUG]Abfahrt[TAB]12:34`. Die linke Spalte wird vor der Tabulatorposition begrenzt. Die Zeichenbreiten sind Näherungen; bei proportionalen Schriften hängt der verfügbare Platz vom Text ab.

## Überarbeitungen

- Persönliche Werte und sämtliche Netzwerk-Fallbacks aus dem Sketch entfernt; fehlende Konfiguration führt zu einem verständlichen Buildfehler.
- Schlafbefehle werden außerhalb des MQTT-Callbacks ausgeführt. Vor dem Schlafen werden alle Zeilen gesichert; verschachtelte `mqtt.loop()`-Aufrufe entfallen.
- MQTT-Payloads werden begrenzt und einmalig übernommen. Zeilen reservieren ihren Speicher vorab; Akku-JSON entsteht in einem festen Puffer.
- Live-Updates werden kurz gesammelt und durch ein Mindestintervall begrenzt. Auch bei durchgehend neuen Daten erzwingt `DISPLAY_MAX_DEFER_MS` eine Aktualisierung. Ein Vollrefresh erfolgt regelmäßig und nach Rotationswechseln gegen Ghosting.
- Nach Verbindungsfehlern wartet der Live-Modus zwischen Versuchen. Ohne WLAN wird kein MQTT-Verbindungsversuch gestartet.
- Rotationswerte werden mit Überlaufprüfung gelesen. Die Schriftmetriken im Hochformat berücksichtigen fette und große Schrift; Tabulatorspalten überzeichnen einander nicht mehr durch unbeschränkten linken Text.
- Die Akkukurve wird zwischen Stützpunkten interpoliert. Der ADC-Steuerpin wird nach jeder Messung freigegeben. Ein wirkungsloser Akku-Redraw-Schwellwert und ungenutzte Parameter wurden entfernt.

## Tests

Die Tests benötigen Python 3.9+ und `g++`:

```sh
python3 -m unittest discover -s tests -v
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -I . tests/test_display_logic.cpp -o /tmp/test-display-logic
/tmp/test-display-logic
python3 scripts/check_public_files.py
```

Die Konfigurationstests prüfen gültige Konfiguration, fehlende/ungültige Pflichtwerte und die explizite TLS-Auswahl. Die C++-Tests prüfen Rotation, Zahlenüberläufe, Payload-Grenzen, NUL-Bytes, Schriftwahl, Refresh-Timing inklusive Timerüberlauf und eine monotone Akkukurve.

```sh
python3 scripts/build.py --example
python3 scripts/build.py --example --insecure
```

`--example` erzeugt ausschließlich in einem temporären Sketch nicht funktionsfähige Testzugangsdaten und einen Zertifikatsplatzhalter. Diese Option prüft die Kompilierbarkeit, ohne lokale Zugangsdaten einzulesen. Solche Testfirmware ist nicht für den Betrieb gedacht. GitHub Actions führt Datenschutzprüfung, Tests und beide Buildvarianten aus; Builddateien werden nicht veröffentlicht.

Die Datenschutzprüfung untersucht Git-Dateien auf versehentlich erfasste lokale Einstellungen, Firmware, private IPv4-Adressen, GitHub-Tokens und private Schlüssel. Sie ergänzt die Prüfung vor einer Veröffentlichung und erkennt nicht jede denkbare vertrauliche Angabe.

Die Software wurde für das angegebene Board kompiliert. Stromaufnahme, Akkumessung, TLS-Verbindung und Darstellung müssen zusätzlich am eigenen Gerät geprüft werden. Insbesondere ADC-Pin, Steuerpolarität und Spannungsteiler hängen von der Boardrevision ab. Akku-Prozentwerte sind eine Schätzung für eine einzelne LiPo-Zelle. Das RTC-Gedächtnis überlebt Deep Sleep, aber keinen vollständigen Stromverlust.
