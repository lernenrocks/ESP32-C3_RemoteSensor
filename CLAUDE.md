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
- **Light Sleep** zwischen Abfragen (implementiert, Option B): manuelles `esp_light_sleep_start()` in `loop()`, gegated auf `isConnected()` — der C3 schläft nur im verbundenen STA-Betrieb, nie während Reconnect oder AP-Modus. WiFi-Assoziation bleibt aktiv, eingehende TCP-Verbindung weckt den C3.
  - **Wakeup-Quellen**: WiFi (eingehende Anfrage) + GPIO9 (Factory-Reset). Bewusst kein Timer, kein UART.
  - **Pflicht-Kopplung**: `esp_sleep_enable_wifi_wakeup()` funktioniert nur mit `esp_wifi_set_ps(WIFI_PS_MIN_MODEM)` (in WiFiManager nach Connect gesetzt). Der C3 wacht am DTIM-Beacon (~4–5×/s).
  - **Nach dem Wake**: kurzes Wach-Fenster (`HTTP_GRACE_MS`, 200ms) pumpt `HttpServer::handle()`, bis der TCP-Handshake durch ist; behandelt `ESP_ERR_SLEEP_REJECT` (259) als Normalfall (pending WiFi-RX), nicht als Fehler.
  - **USB-Konsole stirbt im Sleep**: Der USB-Serial-JTAG wird im Light Sleep abgeschaltet → Serial-Monitor flappt, Port enumeriert neu (ttyACM0↔1). Sleep daher über WiFi beobachten (`/status`, `wake_count`), nicht über die Konsole. Debug-Build ohne Sleep via `-D DISABLE_LIGHT_SLEEP` (siehe PlatformIO-Struktur).
  - **Option B statt automatic light sleep (A)**: A bräuchte `esp_pm_configure` + Tickless-Idle im sdkconfig — im Arduino-Framework nur per Build-Migration. B verifiziert: 1,5h stabil bei RSSI −54, kein Heap-Leck, Reconnect erholt sich selbst (Out-of-Range-Test).
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
- `value` ist `float` — kalibrierter Wert (kg, °C, %, etc.), abzüglich der manuellen Korrektur aus `POST /valueoffset/:idx` (siehe dort)
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

**Hinweis**: Kalibrierungswerte werden in NVS persistiert (Namespace `sensor_N`, Keys `offset`/`scale`) — sie überleben den Neustart. Siehe POST /reset/:idx und NVS-Struktur.

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
Setzt die Kalibrierung eines einzelnen Sensors zurück auf scale=1, offset=0 — sofort im RAM und in NVS. Kein Neustart.

Ermöglicht Neukalibrierung eines einzelnen Sensors ohne die anderen zu beeinflussen.

Response: `200 OK` mit `{}`

### POST /rename/:idx
Setzt den Anzeigenamen des Sensors mit Index `:idx` — rein kosmetisch, wie `POST /provision/name` auf Geräteebene, nur pro Sensor statt global. Kein Neustart, sofort aktiv.

```json
{ "name": "Waage Terrasse" }
```

Response: `200 OK` mit `{}` — `400 Bad Request` wenn `name` fehlt, leer, zu lang (`>= SensorManager::SENSOR_NAME_LEN`) oder `idx` ungültig ist. Persistiert in NVS (Namespace `SensorMeta`, Key `sName<idx>`), NVS-only — kein RAM-Cache, da selten gelesen (nur bei `GET /calibrationinfo`, kein Hot-Path).

**Routen-Namensgebung bewusst `/rename`, nicht `/name`**: `POST /provision/name` (Geräte-Ebene) existiert schon — ein zweites, gleich benanntes `/name/:idx` hätte zwei unterschiedliche Dinge unter demselben Wort geführt. `/rename/:idx` bleibt trotzdem im etablierten flachen Muster von `/calibrate/:idx`/`/reset/:idx` (Verb zuerst, Index danach), keine Verschachtelung wie `/sensors/:idx/name`.

### POST /valueoffset/:idx
Setzt eine manuelle Korrektur ("Tara") auf den Sensor mit Index `:idx` — wird vom kalibrierten Messwert abgezogen, bevor `GET /sensors` antwortet. Kein Neustart, sofort aktiv.

```json
{ "value": 2.5 }
```

Response: `200 OK` mit `{}` — `400 Bad Request` wenn `value` fehlt, keine gültige Zahl ist, oder `idx` ungültig ist.

- **Bewusst getrennt von der eigentlichen Kalibrierung** (`POST /calibrate/:idx`): Die Kalibrierung (Skalierungsfaktor, Sensor-Nullpunkt) bleibt Sensor-intern (`HX711Sensor`, NVS-Namespace `sensor_<pId>`). Die manuelle Korrektur liegt dagegen im `SensorManager` selbst (NVS-Namespace `SensorMeta`, Key `vOffset<idx>`) und gilt **generisch für jeden Sensortyp** — nicht nur für die Waage. Dadurch kein `SensorBase`-Umbau nötig (kein `supportsTara()`-Flag, keine Sensortyp-Sonderbehandlung).
- `SensorManager::calibrateSensor()`/`resetSensor()` setzen die Korrektur bei Erfolg automatisch auf `0` zurück (RAM + NVS) — eine neue Kalibrierung oder ein Reset macht eine alte manuelle Korrektur ungültig.
- Anders als der Name **RAM-gecacht** (`SensorEntry::valueOffset`), weil er bei **jedem** `GET /sensors`-Request angewendet wird — der am häufigsten abgefragte Endpoint (2–3s-Polling während Beregnung). Ein NVS-Zugriff pro Request wäre hier ein echtes Timing-Problem, siehe `HX711Sensor`, das Kalibrierwerte aus demselben Grund im Konstruktor lädt statt bei jedem Read.

### POST /factoryreset
Löscht alle NVS-Daten (Credentials + Kalibrierung aller Sensoren) und rebootet. Entspricht dem Drücken von GPIO9 beim Boot.

**Achtung**: Die Response kommt nicht mehr an — der C3 rebootet sofort. Der Client bekommt einen Connection Reset; das ist erwartetes Verhalten.

### GET /
Liefert die Provisioning-HTML-Seite. Nur sinnvoll im AP-Modus (kein WiFi konfiguriert oder `provisioned = false`). Im Normalbetrieb erreichbar aber nicht vorgesehen.

### POST /provision/wifi
Testet die übergebenen Credentials live, bevor sie übernommen werden. Kein Neustart — Kalibrierung kann danach noch durchgeführt werden.

```json
{ "ssid": "MyNetwork", "password": "secret" }
```

Response: `200 OK` mit `{"connected": true}` bzw. `{"connected": false}` — `400 Bad Request` wenn `ssid` oder `password` fehlen.

**Live-Testconnect statt Blind-Speichern**: Der C3 verbindet sich testweise mit den übergebenen Daten (Timeout ~5s, wie `WIFI_CONNECTION_TIMEOUT`) und schreibt **nur bei Erfolg** in NVS. Der Ausgangszustand wird danach in jedem Fall wiederhergestellt:

- **Im AP-Modus (Onboarding, `provisioned=false`)**: Testconnect läuft im AP+STA-Concurrent-Modus (`WIFI_MODE_APSTA`) — der eigene AP bleibt aktiv, Browser/Handy bleiben verbunden, die Response kommt zuverlässig an. Nach dem Test geht's zurück zu reinem AP, kein Dauerbetrieb im Concurrent-Modus (Kanal-Pinning würde die Verbindung des Browsers stören).
- **Im STA-Modus (laufender Betrieb, z. B. Umzug zu neuer MainUnit bei bereits bekanntem Zielnetz)**: reiner STA-Test (kein APSTA — sonst würde kurzzeitig ein unkonfigurierter, ungeschützter AP aufgehen). **Achtung**: Der anfragende Client ist in diesem Fall meist selbst nur über die *gerade getestete* Verbindung erreichbar — die Response geht dabei häufig verloren, weil die eigene Verbindung während des Tests kurz getrennt wird. Bekanntes, akzeptiertes Verhalten, kein Bug. Bei Erfolg zieht der C3 über den normalen Reconnect-Zyklus in `WiFiManager::initWifi()` von selbst zum neuen Netz um (auch ohne `POST /provision/finish`); bei Fehlschlag bleiben die alten Zugangsdaten in NVS erhalten, der C3 findet von selbst zurück zum alten Netz.

### POST /provision/finish
Setzt `provisioned = true` in NVS und rebootet. Abschluss des Onboarding-Flows nach WiFi + Kalibrierung.

**Achtung**: Response kommt nicht mehr an — C3 rebootet sofort.

### POST /provision/wifi/reset
Setzt `provisioned = false` in NVS und rebootet. C3 startet im AP-Modus für Neukonfiguration — Kalibrierungsdaten bleiben erhalten. SSID/Passwort in NVS werden dabei nicht gelöscht, nur der Provisionierungs-Status.

**Anwendungsfall**: C3 wird auf eine andere MainUnit übertragen, deren Netzwerkdaten beim Aufruf noch nicht bekannt sind (z. B. interaktive Neukonfiguration vor Ort über die GUI). Muss aufgerufen werden, **während der C3 noch im alten Netz erreichbar ist** — danach lässt er sich ohne AP-Fallback-Mechanismus (siehe Roadmap) nicht mehr per HTTP ansprechen. Ist das Ziel-Netzwerk beim Umzug schon bekannt, geht `POST /provision/wifi` + `POST /provision/finish` direkt, ganz ohne diesen Zwischenschritt — beide sind seit der Auth-Vereinheitlichung genau wie dieser Endpoint durch Digest Auth geschützt, egal ob AP oder STA.

### POST /provision/password
Setzt ein neues Gerätepasswort — Digest-Auth-Hash (`System/ha1`) **und** WPA2-Klartext (`System/password`) gleichzeitig. Kein Neustart, sofort aktiv.

```json
{ "password": "mindestens8Zeichen" }
```

Response: `200 OK` mit `{}` — `400 Bad Request` wenn `password` fehlt, kürzer als 8 oder länger als 63 Zeichen ist (WPA2-PSK-Minimum/-Maximum, weil das Gerätepasswort auch das AP-Passwort ist; serverseitig geprüft, nicht nur in der GUI). Die Obergrenze verhindert zusätzlich, dass `DigestCrypto::computeHa1()` ein sehr langes Passwort intern stillschweigend abschneidet.

**Achtung für die GUI**: Dieser Aufruf rekonfiguriert live das WPA2-Passwort des Onboarding-APs. Der Browser verliert dadurch die WLAN-Verbindung zum Gerät selbst, nicht nur die HTTP-Session — siehe "Auth im Browser" weiter unten.

### GET /calibrationinfo
Beschreibt Sensortyp, Kalibrierungsschritte und die aktuell aktiven Kalibrierwerte pro Sensor. Wird beim Onboarding abgefragt — die Companion App baut den Kalibrierungsworkflow dynamisch daraus auf. Kein hardcodiertes Sensor-Wissen in der App nötig. Wird auch von der MainUnit gebraucht (aktuelle Werte, kein eigener Weg an `scale`/`offset` sonst) und ermöglicht der Companion App, die SensorNode auch ohne MainUnit-Bindung sinnvoll anzuzeigen.

`index` = Sensor-ID (entspricht `sensor:N` in `GET /sensors`). Jeder Schritt in `steps` hat `instruction` (Anweisung an den User) und `key` (NVS-Schlüssel). Optional `ref` — wenn vorhanden, zeigt die App ein Eingabefeld für den Referenzwert. `current` liefert die **aktuell aktiven** Kalibrierwerte des Sensors als Read-back — `{}` bei Sensoren ohne Kalibrierung (z. B. DHT22).

```json
[
  {
    "index": 0,
    "name": "Waage Terrasse",
    "type": "HX711",
    "unit": "g",
    "offset": 2.5,
    "steps": [
      { "instruction": "Remove all weight, then confirm", "key": "offset" },
      { "instruction": "Add known weight", "key": "ref_weight", "ref": "ref_weight", "unit": "g" }
    ],
    "current": { "offset": 87244, "scale": 41.15 }
  },
  {
    "index": 1,
    "name": "Sensor 1",
    "type": "DHT22_TEMP",
    "unit": "°C",
    "offset": 0,
    "steps": [],
    "current": {}
  }
]
```

- `name` — `SensorManager::getSensorName()`, user-gesetzter Anzeigename, Fallback `"Sensor <index>"` falls nie gesetzt. Über `POST /rename/:idx` änderbar, siehe REST-API-Abschnitt dort.
- `offset` (top-level) — manuelle Korrektur/Tara, `SensorManager::setValueOffset()`. **Nicht zu verwechseln** mit `current.offset` (Sensor-eigener Kalibrier-Nullpunkt, z. B. bei HX711) — beide heißen zufällig gleich, meinen aber unterschiedliche Dinge auf unterschiedlichen Ebenen (Sensor-Kalibrierung vs. nachträgliche Korrektur). Über `POST /valueoffset/:idx` änderbar.
- `unit` (top-level, pro Sensor-Objekt) — `SensorBase::getUnit()`, z. B. `"g"`, `"°C"`, `"%"`. Nicht zu verwechseln mit dem gleichnamigen `unit`-Feld innerhalb eines `steps`-Eintrags (dort: Einheit des einzugebenden Referenzwerts).

`raw_at_weight` liest der C3 beim `POST /calibrate` selbst — der Client übermittelt nur `offset` und `ref_weight`. `scale` berechnet der C3 aus `(raw_at_weight - offset) / ref_weight`.

**Kein Restore-Pfad**: `current` ist rein informativ/dokumentierend. `offset` ließe sich direkt zurückschreiben, `scale` aber nicht ohne Live-Messung (`POST /calibrate` berechnet `scale` immer aus einer aktuellen Rohwert-Messung mit aufgelegtem Referenzgewicht) — ein Restore-Modus, der `scale` direkt annimmt, liefe technisch wieder auf eine neue Kalibrierung hinaus und wurde deshalb verworfen.

### GET /status
Geräteinformationen — MAC ist persistenter Identifier, zwingend für Onboarding. Enthält auch die interne Chiptemperatur des ESP32-C3.

```json
{
  "mac": "AA:BB:CC:DD:EE:FF",
  "device_name": "Waage-Terrasse",
  "ssid": "MyNetwork",
  "provisioned": true,
  "password_set": true,
  "uptime": 3600,
  "rssi": -65,
  "chip_temp": 42.5,
  "free_heap": 225804,
  "min_free_heap": 212428,
  "wake_count": 24507,
  "version": "0.1.0"
}
```

- `device_name` — `System::getActiveDeviceName()`, MAC-basierter Default falls kein eigener Name gesetzt
- `ssid` — aktuell verbundenes WLAN (`WiFi.SSID()`), leer im AP-Modus
- `provisioned` — Spiegel von NVS `Wifi/provisioned`
- `password_set` — Spiegel des Rückgabewerts von `System::getActiveHa1()` (`true` = eigenes Passwort aktiv, `false` = noch `initialPW`)
- `free_heap` / `min_free_heap` — aktueller bzw. niedrigster je erreichter freier Heap (Low-Watermark = Leak-Indikator)
- `wake_count` — abgeschlossene Light-Sleep/Wake-Zyklen; macht den Sleep über WiFi prüfbar (steigt im Betrieb, stagniert während eines Reconnects)
- `version` — `FIRMWARE_VERSION`

---

## Authentifizierung

**Digest Auth (SHA-256)** — konsistent mit Shelly-Geräten im TerraControl-System. Gilt einheitlich für AP- und STA-Modus, keine Ausnahme für den unprovisionierten Zustand.

- SHA-256 via mbedtls (auf ESP32-C3 verfügbar, Hardware-Beschleuniger)
- **MAC im `realm`-Feld**: `terracontrol-c3-AABBCCDDEEFF` — MainUnit extrahiert MAC aus der Auth-Challenge beim Onboarding (kein separater /status-Aufruf nötig) **und laufend bei jedem weiteren Request**, um zu prüfen, ob hinter einer IP noch dieselbe MAC sitzt. Deshalb bewusst nicht durch `deviceName` ersetzbar — der ist user-änderbar, die MAC nicht.
- Nonce-Generierung via `esp_random()`
- Nonce-Expiry kann vereinfacht werden — C3 hängt am isolierten ESP32-AP, Replay-Angriffe praktisch ausgeschlossen (Replay-Schutz perspektivisch gewünscht, Design offen)
- Der MainUnit-DigestAuth-Client funktioniert unverändert gegen den C3-Server

**Initial-Passwort: ein einziger, fester, öffentlich dokumentierter Wert.**
- `"calibrateMe"`, in `include/initialPW.h` (`constexpr const char initialPW[]`), bewusst im Repo eingecheckt — kein Geheimnis, keine Zufallsgenerierung pro Gerät, kein Serial-Auslesen nötig.
- **Begründung**: Digest Auth schützt nur, *wer* zugreifen darf, nicht *was* übertragen wird — die eigentliche Übertragungssicherheit liefert WPA2, und WPA2 muss dafür nicht geheim sein, nur vorhanden. Ein individuelles Passwort pro Gerät (z. B. MAC+Salt-Ableitung) wurde geprüft und verworfen: Single-Point-of-Failure beim Salt, kein echter Zusatznutzen, sobald die Pflicht-Änderung ohnehin greift.
- **Werkspasswort und WPA2-Passwort des Onboarding-APs sind derselbe Wert** — `System::getActivePassword()`/`System::getActiveHa1()` liefern beides aus derselben Quelle (eigenes Passwort falls in NVS gesetzt, sonst `initialPW`).

**Zwei Zustände, eine Existenzprüfung:**
- Kein `System/ha1` in NVS → Gerät nutzt `initialPW` für WPA2 **und** Digest Auth (live berechnet via `DigestCrypto::computeHa1`, nie gespeichert).
- `System/ha1` vorhanden → Gerät nutzt das selbst gewählte Passwort für beides.
- Kein Sondermechanismus, kein Flag, das den Zustand über einen Factory Reset hinweg festhält — reine Existenzprüfung genügt (`System::getActiveHa1()`/`getActivePassword()`, `bool`-Rückgabe zeigt, welcher Fall gerade vorliegt).
- Pflicht-Ersteinrichtung serverseitig erzwungen: `POST /provision/finish` verweigert den Abschluss, solange `System::getActiveHa1()` noch `false` liefert (noch kein eigenes Passwort) — sonst würden viele Selbstbauer das öffentliche Default nie ändern.
- Neues Passwort muss zwischen 8 und 63 Zeichen haben (WPA2-PSK-Minimum/-Maximum, weil das Device-Passwort auch das AP-Passwort ist) — serverseitig in `POST /provision/password` geprüft, nicht nur clientseitig in der GUI.

**Selbstbauer vs. Fertigprodukt-Kunden**: kein Unterschied mehr im Mechanismus, weil das Initial-Passwort ohnehin öffentlich ist. Ein Selbstbauer, der ein anderes Default-Passwort will, ändert einfach `initialPW.h` vor dem Kompilieren — kein separater Vertriebsweg oder Config-Mechanismus nötig.

### Auth im Browser (verbindlich für die Provisioning-GUI)

- **Digest Auth läuft transparent auf Browser-Ebene, nicht in JavaScript.** Bekommt der Browser eine `401`-Antwort mit `WWW-Authenticate: Digest`, zeigt er von sich aus einen nativen Login-Dialog — das betrifft `GET /` selbst genauso wie jeden `fetch()`-Aufruf aus dem geladenen JS heraus. Es gibt **keine** Möglichkeit, Digest-Credentials aus JavaScript an `fetch()` zu übergeben (kein Äquivalent zu curls `-u`) — von Browsern schlicht nicht vorgesehen. Nicht versuchen, Digest Auth in JS nachzubauen.
- Einmal im nativen Dialog eingegeben, cached der Browser die Credentials für den Rest der Session auf **derselben Origin** — alle weiteren `fetch()`-Aufrufe der Seite (gleicher Host/Port) senden sie automatisch mit, ohne dass die Seite selbst irgendwas dafür tun muss. `ProvisioningPage.h`s bestehende `fetch()`-Aufrufe (`saveWifi()`, `loadSensors()`, etc.) machen deshalb bewusst nichts Auth-Spezifisches — kein Versehen, funktioniert einfach so.
- **Fehlerantworten (400/401/404) haben einen leeren Body** (`Content-Length: 0`), nirgends im Backend eine JSON-Fehlermeldung. `response.json()` auf einer Fehlerantwort scheitert deshalb — Fehlerbehandlung in der GUI darf sich nur auf `response.ok`/`response.status` stützen, nie auf den Body.
- **Größte Falle beim Passwort-Tab, unbedingt einplanen**: `POST /provision/password` ändert live das WPA2-Passwort des APs, mit dem der Browser gerade verbunden ist. Das ist ein Betriebssystem-Ebene-Problem, kein HTTP-Problem — die WLAN-Verbindung des Geräts (Laptop/Handy) zum C3-eigenen AP bricht ab, sobald das neue Passwort greift, unabhängig vom Browser. Die GUI kann das nicht automatisch reparieren; der User muss sein Betriebssystem-WLAN-Menü öffnen, sich mit dem **neuen** Passwort neu verbinden, und danach zur Seite zurückkehren. Klar kommunizieren ("Jetzt mit dem neuen WLAN-Passwort neu verbinden, dann hier fortfahren") statt stillschweigend auf eine Antwort zu warten, die nie ankommt — das ist bewusst eine offene UX-Entscheidung für die Umsetzung, kein vorgegebenes Muster.

---

## NVS-Struktur (Preferences)

| Namespace | Key | Typ | Beschreibung |
|-----------|-----|-----|--------------|
| `Wifi` | `ssid` | string | WLAN-SSID |
| `Wifi` | `password` | string | WLAN-Passwort |
| `Wifi` | `provisioned` | bool | `true` nach erfolgreichem Onboarding |
| `System` | `ha1` | string | User-gesetzter Digest-Auth-Passwort-Hash — Existenz entscheidet, ob `initialPW` oder eigenes Passwort aktiv ist |
| `System` | `password` | string | User-gesetztes Passwort im Klartext — fürs AP-WPA2, da Digest Auth nur den Hash liefert |
| `System` | `name` | string | optionaler, rein kosmetischer Gerätename (AP-SSID/Hostname/Status-Feld) |
| `sensor_N` | `offset` | long | Tara-Offset (leere Wägeplatte) |
| `sensor_N` | `scale` | float | Skalierungsfaktor |
| `SensorMeta` | `sName<N>` | string | User-gesetzter Sensorname, Key-Suffix statt eigener Namespace (siehe unten) |
| `SensorMeta` | `vOffset<N>` | float | Manuelle Korrektur/Tara (`POST /valueoffset/:idx`), RAM-gecacht in `SensorEntry::valueOffset` |

`N` = Array-Index des Sensors (0, 1, 2, …). `erase()` löscht alle Namespaces.

**`SensorMeta` ist ein einziger, gemeinsamer Namespace** für alle Sensoren, `N` steckt hier im Key-Namen selbst (`sName0`, `vOffset1`, …) — anders als `sensor_N`, wo `N` den Namespace-Namen bildet. Bewusst so: `sensor_N` gehört `HX711Sensor` privat (eigener `_pId`-Namespace pro Instanz, siehe HX711Sensor-Abschnitt); `SensorMeta` gehört dagegen dem `SensorManager` selbst und nutzt dessen eigenen, ohnehin überall vorhandenen `idx` — hat mit dem sensorinternen `_pId`-Schema nichts zu tun, auch wenn beide Werte praktisch identisch sind.

---

## Provisioning / Factory Reset

**Boot-Logik im WiFiManager:**
- `Wifi`-Namespace nicht vorhanden oder `provisioned = false` → AP-Modus, `GET /` liefert HTML-Seite
- `provisioned = true` → STA-Modus, normaler WiFi-Connect, Webserver für REST-API
- Der WLAN-Provisionierungsstatus (AP/STA) ist unabhängig vom Passwort-Status — ein Gerät kann bereits im STA-Modus laufen und trotzdem noch das Werkspasswort (`initialHa1`) tragen, falls die Ersteinrichtung dort noch nicht abgeschlossen wurde.

**Onboarding-Flow:**
1. C3 öffnet AP (`SensorNode-AABBCCDDEEFF`, MAC-basiert) — kein WiFi konfiguriert
2. User verbindet sich mit AP (WPA2, Initial-Passwort `calibrateMe`), öffnet Browser → `GET /`
3. **Passwort-Tab (Pflicht, zuerst)**: "Lege ein Gerätepasswort fest" — setzt `System/ha1` + `System/password` in NVS, rekonfiguriert gleichzeitig das WPA2-Passwort des APs auf denselben Wert. Verbindung bricht dabei ab, User muss sich mit dem neuen Passwort neu verbinden, bevor es weitergeht.
4. WiFi-Tab: SSID + Passwort eingeben → `POST /provision/wifi`
5. Sensor-Tabs: Kalibrierungsschritte pro Sensor → `POST /calibrate/:idx`
6. Zusammenfassung → `POST /provision/finish` → C3 setzt `provisioned = true`, rebootet. Verweigert den Abschluss, solange `System::getActiveHa1()` noch `false` liefert (kein eigenes Passwort gesetzt).
7. C3 verbindet sich mit WiFi → Normalbetrieb

**GPIO9 beim Boot gedrückt halten** → Factory Reset (löscht WLAN, Kalibrierung, `System/ha1` + `System/password` — der öffentliche `initialPW`-Default wird dadurch automatisch wieder aktiv, rebootet in AP-Modus)

**`POST /provision/wifi/reset`** → nur `provisioned = false`, Kalibrierung bleibt erhalten — für einen Netzwechsel, dessen Ziel beim Aufruf noch nicht feststeht (siehe REST-API-Abschnitt für die Abgrenzung zu `/provision/wifi` + `/provision/finish`)

---

## OTA

- **Standard-Partition-Scheme** (zwei App-Slots) von Anfang an — kein physisches Reflashen nötig für zukünftige Updates
- **OTA via Companion-App**: MainUnit lädt Firmware vom NAS (via STA/Heimnetz), serviert sie lokal über den AP; C3 pullt und updated sich selbst
- Bis Companion-App steht: USB-Fallback

---

## Sicherheit

- **Verboten: unverschlüsselter/unauthentifizierter Netzwerkzugriff, in jedem Zustand.** Weder der Onboarding-AP noch die Auth dürfen jemals "offen bis zur ersten Aktion" oder "offen für ein Zeitfenster" laufen — auch nicht kurzzeitig. WPA2 und Digest Auth sind ab dem allerersten Paket aktiv, mit dem öffentlichen, festen Initial-Passwort (`calibrateMe`, siehe Authentifizierung) bis der User ein eigenes setzt. Das Initial-Passwort kann nicht "verloren gehen", weil es öffentlich dokumentiert ist — einfach nachschlagen. Wiederherstellung ist nur bei einem vergessenen **eigenen** Passwort nötig: vollständiger Chip-Erase + Reflash (lokal via PlatformIO für den Entwickler, via Web-Flash-Tool/WebSerial für einen Kunden ohne Dev-Setup — WebSerial läuft nicht in Safari, weder iOS noch Mac).
- **NVS-Verschlüsselung** (AES-256, Schlüssel in eFuse, Hardware-Beschleuniger) — verhindert Credential-Extraktion bei gestohlenen Geräten. Da Firmware öffentlich ist, schützt Flash-Verschlüsselung keine IP — NVS-Verschlüsselung reicht für Credential-Schutz.
- **JTAG deaktivieren** — ohne JTAG-Sperre können Credentials aus dem RAM eines laufenden Geräts ausgelesen werden, auch wenn NVS verschlüsselt ist. Angriff: physischer Zugang + laufendes Gerät + JTAG-Interface → RAM-Dump → Credentials im Klartext. ESP32-C3 erlaubt JTAG per eFuse dauerhaft zu deaktivieren.
- Für Marktreife: Secure Boot + vollständige Flash-Verschlüsselung
- **Secure Boot V2 für signierte OTA-Updates** (RSA-3072, öffentlicher Schlüssel in eFuse, privater Schlüssel bleibt beim Hersteller) — ermöglicht Ferngesteuertes Zurücksetzen des Admin-Passworts per signiertem OTA-Update, ohne dass eine geteilte Passwortliste auf den Geräten liegt. eFuses sind einmalig/unumkehrbar beschreibbar — sorgfältiges Schlüsselmanagement nötig, privater Schlüssel darf nie verlorengehen. Zurückgestellt, aktuell reicht lokales Neu-Kompilieren + USB-Reflash.

---

## Code-Architektur

### SensorBase (NVI-Pattern)
`include/SensorBase.h` — abstrakte Basisklasse, ausschließlich Header.

- `read(float& value)` — **public, non-virtual** — einziger Einstiegspunkt; ruft intern `isValid()` und `readRaw()` auf
- `isValid()` — **private pure virtual** — prüft ob Sensor antwortet
- `readRaw(float& buffer)` — **private pure virtual** — schreibt Rohwert in Buffer
- `getCalibrationJson(char* buffer, size_t len)` — **public pure virtual** — Kalibrierschritte als JSON-Array (`instruction`/`key`/optional `ref`+`unit`), für `GET /calibrationinfo`
- `getCalibrationValuesJson(char* buffer, size_t len)` — **public pure virtual** — aktuell aktive Kalibrierwerte als JSON-Objekt (Read-back, siehe `GET /calibrationinfo`); Sensoren ohne Kalibrierung liefern `{}`, weiterhin `true`
- `getPrecision()` — **public pure virtual** — Nachkommastellen für die Formatierung in `GET /sensors`
- `getUnit()` — **public pure virtual** — Einheit als compile-time-konstanter String (`"g"`, `"°C"`, `"%"`), nie `nullptr`
- `calibrate(const JsonObjectConst data)` — **public pure virtual** — setzt Kalibrierungsparameter, `true` auch bei Sensoren ohne Kalibrierung
- `reset()` — **public pure virtual**, `void` — setzt Kalibrierung auf Ausgangszustand zurück (z. B. `scale=1, offset=0`)
- Kein `id()` — der Array-Index im SensorManager ist die ID (`sensor:0` = Index 0). Gilt für die **öffentliche Schnittstelle**: kein Getter, kein Weg für andere Module, einen Sensor nach seiner Position zu fragen. Eine konkrete Unterklasse darf ihren Index trotzdem **privat** halten, wenn sie ihn für einen eigenen, internen Zweck braucht (Beispiel: `HX711Sensor::_pId` für den eigenen NVS-Namespace, siehe unten) — solange er nie über einen Getter nach außen dringt und nie als allgemeines Identitätsmerkmal dient. Bewusst **nicht** in `SensorBase` zentralisiert, obwohl ein künftiger zweiter kalibrierbarer Sensortyp vermutlich dasselbe braucht: verfrühte Abstraktion für einen Fall, den's noch nicht gibt. Erst hochziehen, wenn ein zweites echtes Beispiel existiert.
- Virtueller Destruktor: `virtual ~SensorBase() = default`

Konvention: private Member mit `_`-Prefix (`_scale`).

### SensorManager
`include/SensorManager.h` / `src/SensorManager.cpp` — Namespace, keine Klasse.

- `constexpr uint8_t MAX_SENSORS = 4`
- `constexpr size_t SENSOR_NAME_LEN = 32` — öffentlich, weil `HttpServer.cpp` sie zur Validierung/Puffergröße braucht (analog zu `System::DEVICE_NAME_LEN`)
- `void initSensors()` — Hardware-Init, kein Rückgabewert (Validierung erfolgt beim Lesen)
- `bool getSensorDataJson(char* buf, size_t len)` — baut JSON on-demand, gibt false bei Fehler; wendet pro Sensor `SensorEntry::valueOffset` auf den gelesenen Wert an, bevor er formatiert wird
- `bool getCalibrationInfoJson(char* buf, size_t len)` — baut das JSON-Array für `GET /calibrationinfo` aus `getCalibrationJson()`/`getCalibrationValuesJson()`/`getUnit()` aller Sensoren, plus `name` (`getSensorName()`) und `offset` (`SensorEntry::valueOffset`)
- `bool getSensorName(uint8_t idx, char* buffer, size_t len)` / `bool setSensorName(uint8_t idx, const char* name)` — NVS-only, kein RAM-Cache (siehe `POST /rename/:idx` im REST-API-Abschnitt für die Begründung). `setSensorName()` validiert `idx`-Bounds und Länge selbst (kein doppelter Check in `HttpServer.cpp`) — gleiches Prinzip wie `calibrateSensor()`/`resetSensor()`, die ihren `idx` auch selbst prüfen.
- `bool setValueOffset(uint8_t idx, float value)` — schreibt zuerst nach NVS, übernimmt den RAM-Wert (`SensorEntry::valueOffset`) nur bei erfolgreichem Schreiben — RAM und NVS können so nicht auseinanderlaufen, wenn der NVS-Schreibvorgang mal fehlschlägt.
- `bool calibrateSensor(uint8_t idx, JsonObjectConst data)` / `bool resetSensor(uint8_t idx)` — reichen an den Sensor mit Index `idx` durch, `false` bei ungültigem Index; setzen bei Erfolg zusätzlich `valueOffset` auf `0` zurück (`calibrateSensor()` nur im Erfolgsfall, `resetSensor()` unconditional) — eine neue Kalibrierung/ein Reset macht eine alte manuelle Korrektur ungültig
- Konkrete Sensoren werden in `initSensors()` unconditional instanziiert (HX711 + 2× DHT22) — keine `#ifdef`-Sensorauswahl, siehe PlatformIO-Struktur weiter unten
- Internes Array `SensorEntry sensorArray[MAX_SENSORS]` (anonymer Namespace) — `SensorEntry` bündelt `SensorBase* sensor`, Typ-String (`type`) und `float valueOffset` (RAM-Cache der manuellen Korrektur, aus NVS geladen in `addSensor()`)
- **Name und `valueOffset` liegen bewusst im `SensorManager`, nicht auf dem Sensor-Objekt selbst** — anders als Kalibrierwerte (`HX711Sensor`-intern). Grund: Beide brauchen eine Instanz-Identität (welcher Sensor?) zum Ablegen in NVS, aber `SensorBase` hat bewusst kein `id()` (siehe oben) — der Index käme also entweder von außen als Parameter (macht der Sensor selbst nie autonom) oder gar nicht. Zusätzlich bräuchte `SensorBase` dafür erstmals eine eigene `.cpp` und eine NVS-Abhängigkeit, die aktuell *jeder* Sensor mitschleppen würde, auch die komplett NVS-freien DHT22. Der Manager kennt seinen `idx` ohnehin schon (`calibrateSensor(idx, ...)` etc.) — ihn dort zu verorten kostet keine neue Abhängigkeit.

### HX711Sensor
`include/HX711Sensor.h` / `src/HX711Sensor.cpp`

- Konstruktor: `HX711Sensor(int dout, int sck, uint8_t pId)` — `pId` wird **privat** als `_pId` gehalten (kein `id()` nach außen, siehe SensorBase-Konvention oben), dient nur dem eigenen NVS-Namespace: `nvsNamespace()` baut daraus bei jedem Zugriff frisch `"sensor_<pId>"` (kein gecachter/globaler Puffer — ein früherer globaler Puffer dafür war ein Bug, der bei mehr als einem HX711 falsche Kalibrierdaten gelesen/geschrieben hätte, mittlerweile gefixt)
- `isValid()`: `return _scale.is_ready()`
- `readRaw()`: `buffer = static_cast<float>(_scale.get_units(10))` — 10 Messungen gemittelt, expliziter Cast; wendet intern Kalibrierfaktor und Tara aus NVS an
- Kalibrierungsparameter in NVS: Skalierungsfaktor + Tara, Namespace `sensor_<pId>` (Keys `offset`/`scale`)
- Anzahl Messungen anpassen wenn Werte schwanken (erhöhen) oder TCP-Timeout der MainUnit überschritten wird (reduzieren)

### DigestCrypto
`include/DigestCrypto.h` / `src/DigestCrypto.cpp` — Namespace, keine Klasse.

- Unterste Schicht der Auth-Kette — reine Krypto-/Hilfsfunktionen, kennt weder NVS noch HTTP. `DigestAuth` und `System` hängen beide nur nach hier unten ab, nie voneinander (bricht so einen sonst zwangsläufigen Zirkel: `System` braucht Hashing für den Default-Hash, `DigestAuth::verify()` bräuchte `System` fürs gespeicherte Ha1).
- `sha256Hex(const char *input, char *output, size_t inLen)` — SHA-256 via mbedtls. `inLen` ist die **Input**-Länge (für `mbedtls_sha256`), nicht die Puffergröße von `output` — die ist immer fix 65 Bytes und wird von der Funktion selbst nicht geprüft; der Aufrufer muss vorher sicherstellen, dass `output` groß genug ist.
- `buildRealm(char *out, size_t len)` — `terracontrol-c3-<MAC>`.
- `generateNonce(char *out, size_t len)` — via `esp_random()`.
- `computeHa1(const char *password, char *out, size_t outLen)` — `HA1 = SHA256("admin:<realm>:<password>")`, Username `admin` fest verdrahtet, privat im anonymen Namespace (kein Setter, kein User-änderbarer Wert).
- Öffentliche `constexpr size_t SHA256_HEX_LEN=64, NONCE_HEX_LEN=16, REALM_BUF_LEN=32` — bewusst hier zentral und öffentlich statt privat pro Modul dupliziert: andere Module brauchen sie zum Dimensionieren eigener Stack-Puffer (`char buf[SHA256_HEX_LEN+1]`), das braucht einen compile-time-Wert — ein Getter könnte das nicht liefern, Array-Größen sind keine Laufzeitwerte.

### DigestHeaderParser
`include/DigestHeaderParser.h` / `src/DigestHeaderParser.cpp` — Namespace, keine Klasse.

- `extractValue(const char *line, const char *key, char *dest, size_t destSize)` — liest ein Feld aus einer Digest-Authorization-Header-Zeile (z. B. `nonce`). Versucht zuerst `key="value"` (RFC 7616 quotet die meisten Felder), dann unquoted `key=value` (für `nc`/`algorithm`). `false` + leeres `dest`, wenn nicht gefunden.
- **Wortgrenzen-Prüfung beim Suchen** (`findFieldStart()`, privat): ein Treffer wird nur akzeptiert, wenn das Zeichen davor nicht alphanumerisch ist. Grund: `"cnonce="` enthält `"nonce="` als exakte Teilzeichenkette — ein simples `strstr` nach `nonce=` würde bei einem Header, der `cnonce` vor `nonce` listet (RFC 7616 schreibt keine Feldreihenfolge vor), den `cnonce`-Wert statt des echten `nonce`-Werts liefern. Über einen Unit-Test (`test/test_digest_header_parser/`) gefunden und gefixt.
- Eigenständiges Modul, **keine** mbedtls-/WiFi-/NVS-Abhängigkeit — bewusst aus `DigestAuth` herausgezogen, weil es der einzige Teil der Auth-Kette ist, der komplett hardwarefrei testbar ist (reine String-Logik). Siehe `## Tests` weiter unten.

### DigestAuth
`include/DigestAuth.h` / `src/DigestAuth.cpp` — Namespace, keine Klasse.

- Reine Verifikationslogik, zustandslos — kennt weder NVS noch `System`.
- `buildWwwAuthenticate(char *out, size_t len)` — baut den `WWW-Authenticate`-Header für die 401-Challenge (Realm + Nonce via `DigestCrypto`).
- `verify(const char *authHeader, const char *method, const char *path, const char *ha1)` — prüft die Digest-Response gegen das übergebene `ha1`. Kein eigener NVS-Zugriff — der Aufrufer (`HttpServer`) holt `ha1` vorher selbst über `System::getActiveHa1()` und reicht es durch. Feld-Extraktion aus dem Authorization-Header läuft über `DigestHeaderParser::extractValue()`.
- **`path` ist der tatsächliche Request-Pfad, nicht das `uri=`-Feld aus dem Authorization-Header** — matcht die Shelly-Gen2/3-Server-Konvention, gegen die der MainUnit-Client gebaut ist.

### System
`include/System.h` / `src/System.cpp` — Namespace, keine Klasse.

- Einziger Besitzer des NVS-Namespace `System` (Gerätename, Device-Passwort + -Hash).
- `constexpr size_t DEVICE_NAME_LEN = 32` — zentrale Puffergröße für den Gerätenamen, analog zu `DigestCrypto::SHA256_HEX_LEN`.
- `loadDeviceName(buf, len)` / `storeDeviceName(name)` — rein kosmetisch, nie Teil der Digest-Auth-Realm oder des Usernamens.
- `getActiveDeviceName(buf, len)` — **einziger öffentlicher Lese-Weg** mit Fallback: eigener Name falls in NVS gesetzt, sonst MAC-basierter Default. `bool`-Rückgabe wie bei `getActiveHa1()`/`getActivePassword()` (`true` = eigener Name aktiv). Aktuell einziger Aufrufer: `device_name` in `GET /status`. Die AP-SSID bleibt bewusst direkt MAC-basiert (nicht über diese Funktion), unabhängig vom user-gesetzten Namen — siehe WiFiManager-Abschnitt.
- `provideDeviceDefaultPassword(buf, len)` — Klartext-Default aus `initialPW.h`, nie in NVS gespeichert.
- `loadDevicePassword(buf, len)` / `storeDevicePassword(pw)` — eigenes Passwort im Klartext (für `WiFi.softAP()`, da Digest Auth nur den Hash liefert).
- `storeDeviceHa1(ha1)` — Hash-Setter, öffentlich.
- `getActiveHa1(buf, len)` / `getActivePassword(buf, len)` — **einziger öffentlicher Lese-Weg** an Ha1/Passwort. Intern: eigenes `load...()`, bei leerem Ergebnis Fallback auf `provideDeviceDefault...()`. `bool`-Rückgabe zeigt, welcher Fall vorlag (`true` = eigenes Passwort aktiv, `false` = Default-Fallback) — kein `try`-Präfix, weil der Puffer *immer* gültig gefüllt wird, anders als bei der klassischen `TryGetValue`-Konvention.
- `loadDeviceHa1()`/`provideDeviceDefaultHa1()` sind bewusst **privat** (anonymer Namespace) — nur `getActiveHa1()` erreicht sie, kein zweiter öffentlicher Weg an den Hash.
- Load-Funktionen sind durchgängig `void` — Existenzprüfung läuft über den (vorher genullten) Puffer, nicht über einen Rückgabewert.

### WiFiManager
`include/WiFiManager.h` / `src/WiFiManager.cpp` — Namespace, keine Klasse.

- `initWifi()` — Verbindungsaufbau/Reconnect mit Cooldown (10s, `WIFI_CONNECTION_TRY_COOLDOWN`) und Timeout (5s, `WIFI_CONNECTION_TIMEOUT`) pro Versuch. **Erster Connect läuft sofort** (kein Cooldown). Setzt `WiFi.setAutoReconnect(true)` → der WiFi-Treiber fängt transiente Drops selbst ab, `initWifi()` greift nur als Fallback. Nach erfolgreichem STA-Connect: `esp_wifi_set_ps(WIFI_PS_MIN_MODEM)` (Pflicht für den WiFi-Wakeup). Registriert einmalig einen Disconnect-Handler, der Abrisse als `[WARN] WiFi lost ... reason:N` loggt (RF-Diagnose: 200 = Beacon-Timeout, 201 = AP nicht gefunden).
- AP-Zweig holt sein WPA2-Passwort über `System::getActivePassword()` — kein offenes WLAN mehr, eigenes Passwort falls gesetzt, sonst `initialPW`. SSID ist immer MAC-basiert (`SensorNode-<MAC>`), unabhängig vom Passwort.
- `isConnected()` — wraps `WiFi.status() == WL_CONNECTED`
- `heartbeat()` — **debug-only** (`DISABLE_LIGHT_SLEEP`): druckt alle 5s den Zustand (IP/RSSI oder `DISCONNECTED`). Heap-sicher (IP aus Oktetten, kein `String`). Im Sleep-Build still.
- In `loop()` aufrufen wenn `!isConnected()` — kein Aufruf in `setup()`
- **Abhängigkeit**: `WiFiManager` startet und stoppt den `HttpServer` — `HttpServer::begin()` nie direkt aufrufen

### HttpServer
`include/HttpServer.h` / `src/HttpServer.cpp` — Namespace, keine Klasse.

- `begin()` / `end()` — werden ausschließlich vom WiFiManager aufgerufen, nie direkt. **Idempotent** (mehrfaches `begin()` ist no-op) → kein Listening-Socket-Leak, wenn der STA-Pfad bei Reconnect erneut `begin()`t.
- `handle()` — in `loop()` aufrufen; nimmt eingehende Verbindung an, liest Header bis `\r\n\r\n` (mit 1,5s-Inaktivitäts-Timeout `REQUEST_READ_TIMEOUT_MS` gegen abgerissene/halb-offene Verbindungen). **Digest-Auth-Gate greift für jede Route, ausnahmslos** — auch `GET /` und AP-Modus, kein Sonderfall mehr für den unprovisionierten Zustand. Holt sich `ha1` über `System::getActiveHa1()`, reicht es an `DigestAuth::verify()` durch; bei Erfolg Routing zum Handler, sonst 401. **Gibt `bool` zurück** (true = Client bedient) — das Sleep-Wach-Fenster in `loop()` bleibt damit wach, solange Requests kommen.
- Request-Buffer: 1024 Bytes (`BUFFER_SIZE`), Body-Puffer für JSON-Endpoints: 256 Bytes (`BODY_SIZE`)
- POST `/calibrate`, `/provision/wifi`, `/provision/password`, `/rename`, `/valueoffset`: Client wird nach Header-Lesen direkt an ArduinoJson-Parser weitergegeben — kein zweiter Buffer für den Body
- `POST /provision/password` prüft serverseitig eine Mindest-/Maximallänge von 8/63 Zeichen (`MIN_PASSWORD_LEN`/`MAX_PASSWORD_LEN`, WPA2-PSK-Minimum/-Maximum) — nicht nur clientseitig in der GUI, da der Endpoint auch direkt per REST erreichbar ist
- **`DEBUG_LOG(...)`-Makro** statt verstreuter `#ifdef DISABLE_LIGHT_SLEEP`/`Serial.printf`/`#endif`-Blöcke — expandiert zu `Serial.printf(...)` im Debug-Build, zu nichts im Release/Sleep-Build. Bewusst ein **Makro, keine Funktion**: verschwindet beim Präprozessor komplett, garantiert Null-Overhead im Sleep-Build, statt sich auf Compiler-Wegoptimierung eines No-Op-Funktionsaufrufs zu verlassen. Loggt: Methode+Pfad+Ha1-Quelle (`own`/`default`), Auth-Fehlschläge, geblockte Finish-Versuche, erfolgreiche Passwort-/Name-/Sensor-Name-/Offset-Updates.
- **Jede Route ist eine eigene, benannte Funktion** im anonymen Namespace (`handleCalibrate`, `handleProvisionWifi`, `handleSetSensorName`, …) — `handle()` selbst ist nur noch Header lesen, `method`/`path` parsen, Auth-Check, dann eine kurze Dispatch-Kette, die pro Route nur eine Zeile aufruft. Reduziert `handle()` von ursprünglich ~240 auf ~90 Zeilen.
- Routing per exaktem `strcmp(method, ...) && strcmp(path, ...)` gegen die schon geparsten `method`/`path`-Puffer (nicht `strstr` auf dem Rohheader) — ein Präfix-Match hätte z. B. `GET /statusXYZ` auf den `/status`-Handler treffen lassen. Routen mit variablem Index (`/calibrate/:idx`, `/reset/:idx`, `/rename/:idx`, `/valueoffset/:idx`) laufen stattdessen über `sscanf(requestHeader, "POST /rename/%hhu", &sensorIdx)` — exakt, weil `%hhu` nur einen tatsächlich numerischen Pfad matcht.

### JsonEscape
`include/JsonEscape.h` / `src/JsonEscape.cpp` — Namespace, keine Klasse.

- `escape(const char *in, char *out, size_t outLen)` — escaped einen String fürs Einbetten in handgebautes JSON (`\"`, `\\`, `\n`, `\r`, `\t`, sonstige Steuerzeichen als `\uXXXX`), siehe `Nicht verhandelbare Regeln → JSON` oben. Bei zu kleinem Puffer wird sauber gekürzt — nie mitten in einer Escape-Sequenz.
- Eigenständiges Modul, keine Abhängigkeit — aus `HttpServer.cpp` herausgezogen (dort ursprünglich als lokale Funktion für `device_name`/`ssid` in `GET /status`), weil reine Pufferlogik ohne Hardwarebezug komplett hardwarefrei testbar ist. Siehe `## Tests` weiter unten.
- Genutzt von `HttpServer::printStatus()` für `device_name` und `ssid` — beides potenziell user-kontrollierter Freitext, der in die Response-JSON eingebettet wird.

### PlatformIO-Struktur
```ini
[env:lolin_c3_mini]      ← einzige Build-Konfiguration, flach ohne extends
    platform, board, framework, partitions
    lib_deps: ArduinoJson, HX711, DHT sensor library
    build_flags: PIN_FACTORY_RESET, PIN_INTERNAL_LED, HX711_DOUT, HX711_SCK, DHT22_DATA
    ; -D DISABLE_LIGHT_SLEEP   ← Debug-Flag, auskommentiert = Sleep an (Release)
```

Keine Build-Flags für Sensortypen — der SensorManager kennt die Hardware direkt. Kein `#ifdef` für Sensorkonfiguration.

**`DISABLE_LIGHT_SLEEP`** (Debug): einkommentieren → kein Light Sleep, USB-Serial-JTAG bleibt stabil (Konsole/Upload durchgehend), `WiFiManager::heartbeat()` aktiv. Default (auskommentiert) = Sleep an. Achtung: Nur **ein** Prozess kann den einen USB-CDC-Port halten (Monitor *oder* esptool) — bei „port busy" beim Upload erst den Monitor schließen.

### Anwendungsfälle
- **Waage**: 1x HX711 → `sensor:0`
- **Terrariumbox**: 2–3x DHT22 (je Temp + Humidity = 2 Einträge) + Bodenfeuchte → bis zu 7 Einträge
- **Chiptemperatur**: ESP32-C3 interner Sensor → `GET /status` (`chip_temp`), nicht in `/sensors`

---

## Tests

**Zwei-Klassen-Ansatz**, entlang der tatsächlichen Testbarkeit ohne Hardware:

1. **Hostnative Unit-Tests** (`pio test -e native`) — für Module ohne Arduino-/ESP-IDF-/mbedtls-Abhängigkeit. Läuft komplett auf dem Entwicklungsrechner, kein Board, keine Flash-Zeit. Aktuell abgedeckt: `JsonEscape` (`test/test_json_escape/`), `DigestHeaderParser` (`test/test_digest_header_parser/`).
2. **Alles Hardware-Gekoppelte** (WiFi, NVS, HX711/DHT22, `DigestCrypto::sha256Hex`/`computeHa1`/`buildRealm` via mbedtls+`WiFi.macAddress()`, `DigestAuth::verify()` als Ganzes) — bewusst **nicht** automatisiert, sondern manuell/über den laufenden Betrieb geprüft. Begründung: Auth läuft bei jedem Request mit, ein produktiver Dauerbetrieb deckt `verify()`/`sha256Hex` also faktisch mit ab, ohne dass ein eigenes Test-Rig nötig wäre — bei diesem Projektumfang lohnt sich kein Hardware-in-the-Loop-Test-Setup.

`[env:native]` in `platformio.ini` filtert `src/` per `build_src_filter` auf die getesteten Module herunter (`test_build_src = yes` nötig, sonst wird `src/` beim Testbuild gar nicht erst mitkompiliert) — der Rest der Firmware (alles mit `<WiFi.h>`/`<Preferences.h>`/mbedtls) würde nativ ohnehin nicht kompilieren. Ein Modul kommt erst in den Filter, wenn es nachweislich abhängigkeitsfrei ist.

**Warum `DigestHeaderParser` ein eigenes Modul ist statt `private` in `DigestAuth`**: Es ist der einzige Teil der Auth-Kette ohne Krypto-Abhängigkeit (reines String-Parsing) — und zugleich der bug-anfälligste handgeschriebene Teil. Der Unit-Test hat das direkt bestätigt: `extractValue()` matchte `nonce=` versehentlich innerhalb von `cnonce=...` (Teilzeichenkette), bevor die Wortgrenzen-Prüfung (`findFieldStart()`) das behoben hat — ein Fund, den die reine Hardware-Verifikation vorher nicht aufgedeckt hatte, weil deren Feldreihenfolge den Bug zufällig nicht auslöste. Siehe `DigestHeaderParser`-Abschnitt oben.

---

## Roadmap

### Ziel für diese Version
Drei Punkte, bewusst abgegrenzt gegen den Rest der Roadmap (siehe "Zurückgestellt" unten):
1. **Provisioning-Page-GUI fertigstellen** — Passwort-Tab (Digest-Auth-Ersteinrichtung) **und** Anzeige der aktuellen Live-Messwerte pro Sensor (nicht nur Kalibrierschritte als Formular — die GUI soll beim Kalibrieren live zeigen, was der Sensor gerade misst).
2. **`GET /calibrationinfo` um aktuelle Kalibrierwerte erweitern** (Read-back) — wird von der **MainUnit** gebraucht (nicht nur als Backup-Sicherheitsnetz für die Companion App) ✅ *Firmware fertig, Hardwaretest noch offen*
3. **WLAN-Testconnect bei `POST /provision/wifi`** (AP+STA-Concurrent-Modus) ✅ *Firmware fertig, Hardwaretest noch offen*

1. **HX711 + HTTP-Server** — GET /sensors, GET /status, GET /calibrationinfo ✅
2. **Light Sleep** — zwischen Abfragen, WiFi-Assoziation aktiv ✅ (Option B, manueller Sleep + Modem-Sleep, verifiziert)
3. **Provisioning-AP** — Factory Reset, GPIO9-Reset, HTML-Formular (WiFi + dynamische Sensor-Kalibrierung), NVS ✅ (end-to-end auf Hardware verifiziert)
4. **Digest Auth** — SHA-256 Challenge/Response auf den HTTP-Endpoints (siehe Abschnitt Authentifizierung) ← *Backend fertig, GUI (Passwort-Tab + Live-Messwerte) + Hardware-Test noch offen — Teil dieser Version*
   - Initial-Passwort ist ein einziger, öffentlich dokumentierter Wert (`calibrateMe`, siehe Authentifizierung) — kein Unterschied mehr zwischen Selbstbauer- und Fertigprodukt-Weg, kein Serial-Auslesen nötig. Wechsel zu einem eigenen Gerätepasswort ("Lege ein Gerätepasswort fest") ist **Pflicht**, serverseitig über `POST /provision/finish` erzwungen (`System::getActiveHa1()` muss `true` liefern) — sonst würden viele das öffentliche Default nie ändern.
   - Module: `DigestCrypto` (Hashing/Realm/Nonce, zustandslos), `DigestAuth` (Verifikation, bekommt `ha1` als Parameter, kennt NVS nicht), `System` (Namespace `System`, einziger Besitzer der Credential-NVS-Zugriffe, öffentliche API nur `getActiveHa1()`/`getActivePassword()` fürs Lesen).
5. **Stromsparen durch CPU-Drosselung** — CPU-Taktreduktion zusätzlich zum Light Sleep (`setCpuFrequencyMhz` bzw. DFS) — *zurückgestellt, nicht Teil dieser Version*
6. **NVS-Verschlüsselung** — Credentials sicher ablegen — *zurückgestellt, nicht Teil dieser Version*
7. **OTA** — Standard-Partition-Scheme, Companion-App-Trigger — *zurückgestellt, nicht Teil dieser Version*
   - **Achtung bei Kombination mit Digest Auth**: Betrifft nur den Fertigprodukt-Weg (Selbstbauer kompilieren bei Bedarf ohnehin neu, kein OTA-Konflikt dort). Für Fertigprodukt-Kunden ist das Initial-Passwort individuell pro Gerät vergeben. Ein gemeinsames OTA-Image für mehrere Geräte würde dieses Schema aufbrechen (alle Geräte teilen sich dann denselben kompilierten Fallback-Wert). Lösung dafür bei Bedarf: Initial-Passwort aus MAC + einem gemeinsamen Salt ableiten (HMAC), analog zum bestehenden Realm-Muster — jedes Gerät bleibt eindeutig, kein externer Safe nötig. Nicht vor OTA umsetzen.

### Kleinere Ergänzungen (additiv, nachrüstbar)
- **Sensor-Name + manuelle Korrektur (Tara)** — ✅ *Firmware + GUI fertig, Hardwaretest noch offen*. `POST /rename/:idx` (kosmetischer Name, NVS-only) und `POST /valueoffset/:idx` (Korrekturwert, RAM+NVS, wird bei jedem `GET /sensors` abgezogen) — beide im `SensorManager`, nicht auf dem Sensor-Objekt, siehe SensorManager-Abschnitt für die Begründung. GUI: Klick-zum-Bearbeiten direkt an der Sensor-Kartenüberschrift (Name) bzw. einer eigenen Zeile (Korrektur) — kein Save-Button/keine Erfolgsmeldung beim Gerätenamen-Pendant im Header (`toggleDeviceNameEdit()`), da bewusst auf sichtbares Feedback verzichtet wurde (siehe `ProvisioningPage.h`).
- **GET /calibrationinfo um aktuelle Kalibrierwerte erweitern** — ✅ *Firmware fertig, Hardwaretest noch offen*. Neue virtuelle Methode `getCalibrationValuesJson()` auf `SensorBase` (NVI-Pattern, analog zu `getCalibrationJson()`) — `HX711Sensor` liest `_scale.get_offset()`/`_scale.get_scale()` direkt aus der Live-Instanz (nicht aus NVS, damit immer der tatsächlich aktive Wert zurückkommt), DHT22-Sensoren liefern `{}` (keine Kalibrierung). `SensorManager::getCalibrationInfoJson()` baut daraus das neue `"current"`-Feld pro Sensor. Details siehe REST-API-Abschnitt zu `GET /calibrationinfo`.
- **Stotterndes GPIO9-Reset-Blinken** — *zurückgestellt, nicht Teil dieser Version*. Ursprünglich auf den blockierenden 5-s-Connect in `initWifi()` zurückgeführt; Hardwaretest zeigt aber: mit `DISABLE_LIGHT_SLEEP` stottert es nicht mehr, Ursache vermutlich Light Sleep statt `initWifi()`. Bestätigungstest mit aktivem Light Sleep steht noch aus.
- **WLAN-Testconnect bei `POST /provision/wifi` (AP+STA-Concurrent-Modus)** — ✅ *Firmware fertig* (`HttpServer.cpp`, lokale Funktion `testStaConnect()` — bewusst **nicht** in `WiFiManager`, um keinen Include-Zirkel zu erzeugen: `WiFiManager` hängt schon von `HttpServer` ab, um ihn zu starten/stoppen). Merkt sich vor dem Test den Ausgangsmodus und stellt ihn danach in jedem Fall wieder her:
  - **AP-Modus (Onboarding)**: Concurrent-Modus (`WIFI_MODE_APSTA`) — der eigene AP bleibt aktiv, Browser/Handy bleiben verbunden, kein Dauerbetrieb im Concurrent-Modus danach (Kanal-Pinning würde die Verbindung des Browsers stören).
  - **STA-Modus (laufender Betrieb, Netzwechsel)**: reiner STA-Test, kein APSTA — sonst würde kurzzeitig ein unkonfigurierter, ungeschützter AP aufgehen. Response geht dabei oft verloren (die eigene Verbindung des anfragenden Clients ist ja die gerade getestete) — bekanntes, akzeptiertes Verhalten.
  - NVS wird **nur bei Erfolg** beschrieben, siehe REST-API-Abschnitt zu `POST /provision/wifi` für Details zum Selbstheilungsverhalten bei Fehlschlag.
  - Löst die **Lockout-Sackgasse bei falschen Credentials** an der Wurzel — Fehler wird vor dem Umschalten auf reinen STA-Modus erkannt, nicht erst danach per Fallback-Heuristik. Ersetzt damit die früher angedachte Lösung "`initWifi()` nicht-blockierend + AP-Fallback nach N Fehlversuchen".
  - **Bewusst nicht gelöst**: Credentials waren beim Onboarding korrekt, funktionieren später nicht mehr (z. B. MainUnit-Tausch mit neuem Netz) — seltener Fall, Recovery bleibt GPIO9-Factory-Reset. Geprüfte und verworfene Alternative: periodisches SSID-Broadcasting über Serial — beißt sich mit Light Sleep, weil USB-Serial-JTAG im Sleep-Build abgeschaltet ist (nur im `DISABLE_LIGHT_SLEEP`-Debug-Build nutzbar, kein Produktionsweg).

---

## Beziehung zur MainUnit

Der C3 verbindet sich mit dem **Soft-AP der MainUnit** (nicht mit dem Heimrouter).
Die MainUnit identifiziert den C3 über seine **MAC-Adresse** — IP wird dynamisch über `syncApClients()` aktuell gehalten.
TCP-Timeout auf MainUnit-Seite für C3-Requests: **~2000ms** — 500ms ist zu knapp wenn mehrere Sensoren mit Mittelung abgefragt werden (3 Sensoren × 3 Messungen × 100ms = 900ms + Overhead).
`TCP_MAX_TIME` (5000ms global) gilt für Shellys — für C3 wird ein separater Timeout-Wert eingeführt.
