# TerraControl Remote Sensor — Projektkontext für Claude

## Was ist das hier?

ESP32-C3 Super Mini Firmware für Remote-Sensorknoten im TerraControl-System.
Der Knoten kalibriert Sensorwerte selbst und liefert fertige Werte auf Anfrage an die MainUnit.
Schaltentscheidungen und Schwellwerte liegen im MainUnit — Kalibrierung und Tara auf dem C3.

**Zugehöriges Hauptprojekt**: [TerraControl](https://github.com/lernenrocks/TerraControl) (ESP32 MainUnit)

---

## Zusammenarbeit

- Moderne C++-Ansätze bevorzugen und erklären
- Der User lernt aktiv — neue Konzepte kurz erklären wenn sie eingesetzt werden
- **Kein mehrfacher Umbau** — saubere Lösung sofort, auch wenn der Sprint dadurch größer wird
- **Überblick hat Priorität** — lieber früh warnen wenn ein Schritt einen späteren Umbau erzwingt
- **Code nur auf explizite Aufforderung** — erklären, diskutieren, Optionen aufzeigen; kein Code ohne explizite Anfrage

---

## Nicht verhandelbare Regeln

### Heap-Nutzung
- **Verboten**: `String`, `malloc`, `DynamicJsonDocument`, `.c_str()` auf temporärem `String`
- **Erlaubt**: `char[]` auf Stack, `StaticJsonDocument`, `snprintf`, `strlcpy`, `memcpy`
- **`new` nur für Objekte die die gesamte Laufzeit leben** — kein `delete`, keine zyklische Allokation. Begründung: einmalige Allokation ohne Freigabe fragmentiert den Heap nicht. Typischer Anwendungsfall: Sensor-Objekte im SensorManager.

### Kein HTTPClient
- Alle TCP-Kommunikation über `WiFiClient` direkt

### JSON
- `StaticJsonDocument` (ArduinoJson) — `DynamicJsonDocument` verboten
- Escape-Handling zwingend: `\"`, `\\`, `\n`, `\r`, `\t`, `\uXXXX`

### Serial-Präfixe
- `[WARN]` — unerwarteter Zustand, Betrieb weiter möglich
- `[ERROR]` — Fehler, einzelne Funktion nicht verfügbar

---

## Hardware

### ESP32-C3 Super Mini — Pinbelegung

**Linke Seite**

| Pin | Funktion | Status |
|-----|----------|--------|
| GPIO0 | — | ⛔ Strapping-Pin (Boot) |
| GPIO1 | frei | ✅ |
| GPIO2 | — | ⛔ Strapping-Pin |
| GPIO3 | HX711 DOUT | ✅ |
| GPIO4 | HX711 SCK | ✅ |
| 3.3V | HX711 VCC | ✅ |
| GND | HX711 GND | ✅ |

**Rechte Seite**

| Pin | Funktion | Status |
|-----|----------|--------|
| GPIO21 | **NIEMALS BELEGEN** | ⛔ Antennenproblem → WiFi fällt aus |
| GPIO9 | Factory Reset (BOOT-Taste) | ⛔ Strapping-Pin — nur als Input lesen |
| GPIO8 | — | ⛔ Strapping-Pin |
| GPIO20, 10, 7, 6, 5 | frei für spätere Sensoren | ✅ |

### HX711 + Wägezelle

```
HX711 DOUT  →  GPIO3
HX711 SCK   →  GPIO4
HX711 VCC   →  3.3V
HX711 GND   →  GND
```

**Wägezelle Farbbelegung (Standard):**
```
Rot    → E+   Schwarz → E-   Weiß → A-   Grün → A+
```

---

## Architektur

### Prinzip: C3 ist smarter Sensorknoten

```
C3 kalibriert → liefert fertigen Wert auf Anfrage
MainUnit kennt: Schwellwert, Hysterese, Tara-Offset
```

- Keine Schaltlogik auf dem C3
- **Kalibrierung auf dem C3** — sensor-spezifische Parameter in NVS, sensor-spezifisches Webinterface im Provisioning
- Pull/On-Demand: MainUnit fragt ab, C3 antwortet
- C3 kann auch direkt von der Companion App angesprochen werden — unabhängig von der MainUnit
- **Begründung**: Bei 50+ Sensortypen wäre die Kalibrierlogik in der MainUnit unwartbar; jede Sensorklasse kapselt ihre eigene Kalibrierung

### Trennungsprinzip: Sensor vs. Umgebung
- **Sensorspezifisch → C3**: alles was eine Eigenschaft des Sensors selbst ist (Skalierungsfaktor, Sensor-Nullpunkt, Dry/Wet-Werte)
- **Umgebungsspezifisch → MainUnit**: alles was eine Eigenschaft der Anwendung ist (Tankgewicht, Schwellwerte, Hysterese)
- Beispiel HX711: Sensor-Offset (leere Wägeplatte) → C3 / Tankgewicht (leerer Tank) → MainUnit

### Betrieb
- **Light Sleep** zwischen Abfragen: WiFi-Assoziation bleibt aktiv, eingehende TCP-Verbindung weckt den C3 sofort; spart Strom auch bei kabelgebundener Versorgung
- Kabelgebundene Stromversorgung
- Pollrate wird vom MainUnit bestimmt (adaptiv: 2–3s während Beregnung, 30s im Normalbetrieb)

---

## REST API

### GET /sensors
Kalibrierte Werte aller angeschlossenen Sensoren. Jeder Wert trägt seinen eigenen `valid`-Flag — die MainUnit muss keine eigene Plausibilitätsprüfung machen.

```json
{
  "sensor:0": { "value": 4.2, "valid": true },
  "sensor:1": { "value": null, "valid": false }
}
```

- Schlüssel immer `sensor:N` (fortlaufend, fix zur Compile-Zeit)
- `value` ist `float` — kalibrierter Wert (kg, °C, %, etc.)
- Lesen erfolgt ausschließlich on-demand beim Request, kein Background-Sampling

### POST /calibrate/:idx
Setzt Kalibrierungsparameter am Sensor mit Index `:idx`. Kein Neustart — Werte sind sofort aktiv.

**HX711-Kalibrierungsablauf (zwei Schritte, ein POST):**

1. C3 booten (Initialzustand: scale=1, offset=0)
2. Waage leer lassen → `GET /sensors` — der angezeigte Wert ist der Raw-ADC-Rohwert (weil scale=1, offset=0, also `get_units = raw / 1 = raw`)
3. Referenzgewicht auflegen
4. `POST /calibrate/0` mit `{"offset": <wert_aus_schritt2>, "ref_weight": <gramm>}`

Der C3 führt dann aus:
- `set_offset(<offset>)`
- liest aktuellen Raw-ADC-Wert mit Gewicht (`get_value`)
- berechnet `scale = (raw_mit_gewicht - offset) / ref_weight`
- ab sofort gibt `GET /sensors` kalibrierte Gramm zurück

**Achtung**: Kalibrierungswerte leben aktuell nur im RAM — nach Neustart ist neu zu kalibrieren (NVS folgt später).

**HX711-Kalibrierungsablauf (drei Schritte):**
```
POST /reset/0         → Sensor auf scale=1, offset=0 zurücksetzen
GET /sensors          → {"sensor:0": {"value": 87244, ...}}   ← Rohwert leer
# Gewicht drauf
POST /calibrate/0     → {"offset": 87244, "ref_weight": 2120}
GET /sensors          → {"sensor:0": {"value": 2120, ...}}    ← kalibriert ✓
```

Der Reset-Schritt stellt sicher, dass `GET /sensors` den Raw-ADC-Rohwert liefert — unabhängig davon ob der Sensor vorher schon kalibriert war. Nach Booten aus dem Initialzustand (scale=1, offset=0) kann der Reset-Schritt entfallen, aber der Client muss das nicht wissen.

### POST /reset/:idx
Setzt die Kalibrierung eines einzelnen Sensors zurück auf scale=1, offset=0. Kein Neustart — wirkt sofort im RAM (später auch NVS).

Ermöglicht Neukalibrierung eines einzelnen Sensors ohne die anderen zu beeinflussen.

Response: `200 OK` mit `{}`

### POST /factoryreset
Löscht alle NVS-Daten (Credentials + Kalibrierung aller Sensoren) und rebootet den C3 in den Provisioning-AP-Modus. Entspricht dem langen Drücken von GPIO9, ist aber auch erreichbar wenn GPIO9 physisch verbaut ist.

**Achtung**: Die Response kommt nicht mehr an — der C3 rebootet sofort. Der Client bekommt einen Connection Reset; das ist erwartetes Verhalten.

### GET /calibrationinfo
Beschreibt Sensortyp und Kalibrierungsschritte pro Sensor. Wird einmalig beim Onboarding abgefragt — die Companion App baut den Kalibrierungsworkflow dynamisch daraus auf. Kein hardcodiertes Sensor-Wissen in der App nötig.

Array-Index = Sensor-ID. Jeder Schritt hat `instruction` (Anweisung an den User) und `key` (NVS-Schlüssel). Optional `ref` — wenn vorhanden, zeigt die App ein Eingabefeld für den Referenzwert.

```json
[
  {
    "type": "HX711",
    "steps": [
      { "instruction": "Place empty scale", "key": "offset" },
      { "instruction": "Add known weight",  "key": "ref_weight", "ref": "ref_weight" }
    ]
  },
  {
    "type": "DHT22_TEMP",
    "steps": []
  }
]
```

`raw_at_weight` liest der C3 beim `POST /calibrate` selbst — der Client übermittelt nur `offset` und `ref_weight`. `scale` berechnet der C3 aus `(raw_at_weight - offset) / ref_weight`.

### GET /status
Geräteinformationen — MAC ist persistenter Identifier, zwingend für Onboarding. Enthält auch die interne Chiptemperatur des ESP32-C3.

```json
{
  "mac": "AA:BB:CC:DD:EE:FF",
  "uptime": 3600,
  "rssi": -65,
  "chip_temp": 42.5
}
```

---

## Authentifizierung

**Digest Auth (SHA-256)** — konsistent mit Shelly-Geräten im TerraControl-System.

- SHA-256 via mbedtls (auf ESP32-C3 verfügbar, Hardware-Beschleuniger)
- **MAC im `realm`-Feld**: `terracontrol-c3-AABBCCDDEEFF` — MainUnit extrahiert MAC aus der Auth-Challenge beim Onboarding; kein separater /status-Aufruf nötig
- Nonce-Generierung via `esp_random()`
- Nonce-Expiry kann vereinfacht werden — C3 hängt am isolierten ESP32-AP, Replay-Angriffe praktisch ausgeschlossen
- Der MainUnit-DigestAuth-Client funktioniert unverändert gegen den C3-Server

---

## Provisioning / Factory Reset

GPIO9 (BOOT-Taste) lang drücken → Factory Reset (löscht Credentials UND Kalibrierung):

1. C3 öffnet eigenen AP (`TerraControl-C3-AABBCC`)
2. Webserver startet (nur in diesem Modus — kein permanenter Heap)
3. User verbindet sich mit C3-AP, öffnet Browser
4. HTML-Formular: MainUnit-AP-Credentials eingeben
5. Sensor-spezifische Kalibrierungsschritte (je nach Sensortyp unterschiedlich)
6. C3 speichert Credentials + Kalibrierung in NVS, bootet neu
7. C3 verbindet sich mit MainUnit-AP → Normalbetrieb

Webserver und AP laufen **ausschließlich** im Provisioning-Modus.

---

## OTA

- **Standard-Partition-Scheme** (zwei App-Slots) von Anfang an — kein physisches Reflashen nötig für zukünftige Updates
- **OTA via Companion-App**: MainUnit lädt Firmware vom NAS (via STA/Heimnetz), serviert sie lokal über den AP; C3 pullt und updated sich selbst
- Bis Companion-App steht: USB-Fallback

---

## Sicherheit

- **NVS-Verschlüsselung** (AES-256, Schlüssel in eFuse, Hardware-Beschleuniger) — verhindert Credential-Extraktion bei gestohlenen Geräten. Da Firmware öffentlich ist, schützt Flash-Verschlüsselung keine IP — NVS-Verschlüsselung reicht für Credential-Schutz.
- **JTAG deaktivieren** — ohne JTAG-Sperre können Credentials aus dem RAM eines laufenden Geräts ausgelesen werden, auch wenn NVS verschlüsselt ist. Angriff: physischer Zugang + laufendes Gerät + JTAG-Interface → RAM-Dump → Credentials im Klartext. ESP32-C3 erlaubt JTAG per eFuse dauerhaft zu deaktivieren.
- Für Marktreife: Secure Boot + vollständige Flash-Verschlüsselung

---

## Code-Architektur

### SensorBase (NVI-Pattern)
`include/SensorBase.h` — abstrakte Basisklasse, ausschließlich Header.

- `read(float& value)` — **public, non-virtual** — einziger Einstiegspunkt; ruft intern `isValid()` und `readRaw()` auf
- `isValid()` — **private pure virtual** — prüft ob Sensor antwortet
- `readRaw(float& buffer)` — **private pure virtual** — schreibt Rohwert in Buffer
- Kein `id()` — der Array-Index im SensorManager ist die ID (`sensor:0` = Index 0)
- Virtueller Destruktor: `virtual ~SensorBase() = default`

Konvention: private Member mit `_`-Prefix (`_scale`).

### SensorManager
`include/SensorManager.h` / `src/SensorManager.cpp` — Namespace, keine Klasse.

- `constexpr uint8_t MAX_SENSORS = 8`
- `void initSensors()` — Hardware-Init, kein Rückgabewert (Validierung erfolgt beim Lesen)
- `bool getSensorDataJson(char* buf, size_t len)` — baut JSON on-demand, gibt false bei Fehler
- Konkrete Sensoren werden per `#ifdef SENSOR_HX711` etc. aktiviert
- Internes Array `SensorBase* _sensors[MAX_SENSORS]`

### HX711Sensor
`include/HX711Sensor.h` / `src/HX711Sensor.cpp`

- Konstruktor: `HX711Sensor(int dout, int sck)`
- `isValid()`: `return _scale.is_ready()`
- `readRaw()`: `buffer = static_cast<float>(_scale.get_value(3))` — 3 Messungen gemittelt (~300ms Blockzeit), expliziter Cast; wendet intern Kalibrierfaktor und Tara aus NVS an
- Kalibrierungsparameter in NVS: Skalierungsfaktor + Tara
- Anzahl Messungen anpassen wenn Werte schwanken (erhöhen) oder TCP-Timeout der MainUnit überschritten wird (reduzieren)

### WiFiManager
`include/WiFiManager.h` / `src/WiFiManager.cpp` — Namespace, keine Klasse.

- `initWifi()` — Verbindungsaufbau mit Cooldown (30s) und Timeout (10s) pro Versuch; ruft intern `HttpServer::begin()` bzw. `HttpServer::end()` auf
- `isConnected()` — wraps `WiFi.status() == WL_CONNECTED`
- In `loop()` aufrufen wenn `!isConnected()` — kein Aufruf in `setup()`
- **Abhängigkeit**: `WiFiManager` startet und stoppt den `HttpServer` — `HttpServer::begin()` nie direkt aufrufen

### HttpServer
`include/HttpServer.h` / `src/HttpServer.cpp` — Namespace, keine Klasse.

- `begin()` / `end()` — werden ausschließlich vom WiFiManager aufgerufen, nie direkt
- `handle()` — in `loop()` aufrufen; nimmt eingehende Verbindung an, liest Header bis `\r\n\r\n`, routet zu Handler, sendet Response, schließt Verbindung
- Request-Buffer: 512 Bytes (reicht für Digest Auth Header)
- POST `/calibrate`: Client wird nach Header-Lesen direkt an ArduinoJson-Parser weitergegeben — kein zweiter Buffer für den Body

### PlatformIO-Struktur
```ini
[env:lolin_c3_mini]      ← einzige Build-Konfiguration, flach ohne extends
    platform, board, framework, partitions
    lib_deps: ArduinoJson, HX711, DHT sensor library
    build_flags: PIN_FACTORY_RESET, HX711_DOUT, HX711_SCK, DHT22_DATA
```

Keine Build-Flags für Sensortypen — der SensorManager kennt die Hardware direkt. Kein `#ifdef` für Sensorkonfiguration.

### Anwendungsfälle
- **Waage**: 1x HX711 → `sensor:0`
- **Terrariumbox**: 2–3x DHT22 (je Temp + Humidity = 2 Einträge) + Bodenfeuchte → bis zu 7 Einträge
- **Chiptemperatur**: ESP32-C3 interner Sensor → `GET /status` (`chip_temp`), nicht in `/sensors`

---

## Roadmap

1. **HX711 + HTTP-Server** — GET /sensors, GET /status, GET /calibrationinfo, Digest Auth ← *aktuell*
2. **Light Sleep** — zwischen Abfragen, WiFi-Assoziation aktiv
3. **Provisioning-AP** — Factory Reset, HTML-Formular, NVS
4. **NVS-Verschlüsselung** — Credentials sicher ablegen
5. **OTA** — Standard-Partition-Scheme, Companion-App-Trigger

---

## Beziehung zur MainUnit

Der C3 verbindet sich mit dem **Soft-AP der MainUnit** (nicht mit dem Heimrouter).
Die MainUnit identifiziert den C3 über seine **MAC-Adresse** — IP wird dynamisch über `syncApClients()` aktuell gehalten.
TCP-Timeout auf MainUnit-Seite für C3-Requests: **~2000ms** — 500ms ist zu knapp wenn mehrere Sensoren mit Mittelung abgefragt werden (3 Sensoren × 3 Messungen × 100ms = 900ms + Overhead).
`TCP_MAX_TIME` (5000ms global) gilt für Shellys — für C3 wird ein separater Timeout-Wert eingeführt.
