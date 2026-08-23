# Stille Bugs

Acht wiederkehrende Fehlermuster, gefunden im Firmware-Review des ESP32-C3 RemoteSensor — zum Wiedererkennen in künftigen Projekten.

Jeder Punkt hier hat im Review real etwas kaputt gemacht oder wäre fast unbemerkt geblieben — keiner davon hat beim Kompilieren angeschlagen. Das ist auch der rote Faden: **alle acht Muster versagen still**, nicht laut. Sie fallen nicht durch eine Fehlermeldung auf, sondern durch ein falsches Ergebnis, eine veraltete Doku oder einen Bug, der sich erst mit der zweiten Instanz zeigt.

*Projekt: RemoteSensor / TerraControl · Quelle: 6 Review-Themen (Security, Korrektheit, Puffergrößen, Doku-Abgleich, Fehlerbehandlung, Tests) · Stand: 2026-08-23*

---

## 01 · Puffer — Kürzen statt ablehnen

**Regel:** Wenn eine Länge eine harte Grenze hat: explizit prüfen und mit Fehler ablehnen — nicht stillschweigend kürzen und weitermachen.

**Warum:** Eine zu lange Eingabe wird beim nächsten Lesen unbemerkt gekürzt. Der User merkt nichts — das System verhält sich dauerhaft anders als das, was er eingegeben hat.

**Beispiel aus dem Projekt:** `DigestCrypto::computeHa1()`s internes `input[128]` kürzte lange Passwörter (>~92 Zeichen) still — der Hash wurde aus einem kürzeren Passwort berechnet, ohne Fehler. Fix: `MAX_PASSWORD_LEN=63` serverseitig geprüft, dieselbe Lücke danach auch bei `POST /provision/name` gefunden und geschlossen.

---

## 02 · Parsing — Teilstring-Match ohne Wortgrenze

**Regel:** Bei Suche nach einem Feldnamen oder Pfad nie nur eine reine Teilstring-Suche nutzen, wenn ein Name Präfix oder Suffix eines anderen sein kann.

**Warum:** `strstr` findet den ersten Treffer — unabhängig davon, ob er zufällig innerhalb eines längeren Wortes liegt. Ohne Grenzprüfung ist das Ergebnis vom Zufall der Zeichenkette abhängig.

**Beispiel aus dem Projekt:** Die Suche nach `"nonce="` fand denselben Text versehentlich innerhalb von `"cnonce=..."` — falsches Feld extrahiert, sobald `cnonce` vor `nonce` im Header steht (laut RFC 7616 erlaubt). Ein Unit-Test deckte es auf; Fix per Wortgrenzen-Check.

```cpp
// vorher: erster Treffer gewinnt, egal wo er sitzt
strstr(line, "nonce=\"")        // trifft auch in "cnonce=..."

// nachher: nur ein Treffer an einer echten Feldgrenze zählt
findFieldStart(line, "nonce=\""); // prüft: Zeichen davor alphanumerisch? -> weitersuchen
```

---

## 03 · Routing — Präfix- statt Exact-Match

**Regel:** Routen/Kommandos exakt vergleichen, nicht per Teilstring-Suche auf dem Rohtext — sonst matcht ein längerer, unbeabsichtigter Pfad mit.

**Warum:** Ein Tippfehler oder eine falsche Erweiterung im Client wird nicht als Fehler sichtbar, sondern führt zufällig zu einer echten Antwort — die Route "funktioniert" für Anfragen, die sie eigentlich nie hätte akzeptieren dürfen.

**Beispiel aus dem Projekt:** `strstr(header, "GET /status")` matchte auch `GET /statusXYZ` mit `200 OK` statt `404`. Fix: `strcmp` auf dem bereits exakt geparsten Pfad statt Teilstring-Suche im rohen Request-Header.

---

## 04 · Puffer — Zwei Zahlen, die eigentlich eine sein sollten

**Regel:** Wenn zwei Puffer aus denselben Daten befüllt werden, ihre Größe aus *einer* gemeinsamen Konstante ableiten — nicht zwei unabhängige Zahlen parallel pflegen.

**Warum:** Ändert sich eine Grenze, aber die zweite, eigentlich abhängige Stelle bleibt unverändert, entsteht eine stille Kürzung an einer ganz anderen Stelle im Code — ohne offensichtlichen Zusammenhang zur eigentlichen Änderung.

**Beispiel aus dem Projekt:** `HA2_INPUT_BUF_LEN` wurde ursprünglich aus einer eigenen, unabhängigen `URI_BUF_LEN` berechnet — statt aus `DigestAuth::MAX_URI_LEN`, dem Wert, der tatsächlich für den `path`-Puffer in `HttpServer.cpp` galt.

---

## 05 · RAII — Manuelles Pairing statt automatischer Freigabe

**Regel:** Wenn zwei Aufrufe immer zusammen auftreten müssen (`begin()`/`end()`, Lock/Unlock …), in einen RAII-Wrapper packen — nicht auf Disziplin an jeder einzelnen Call-Site verlassen.

**Warum:** Ein vergessenes `end()`, etwa bei einem frühen `return` in einem Fehlerpfad, hält eine Ressource offen. Das passiert nur in Randfällen und ist im Alltagstest kaum zu sehen.

**Beispiel aus dem Projekt:** `InternalStorage::begin()/end()` wurden an rund einem Dutzend Stellen manuell gepaart. Fix: `InternalStorage::Session` — der Destruktor ruft `end()` garantiert auf, auch bei vorzeitigem Funktionsende.

---

## 06 · State — Geteilter Puffer für Instanz-eigene Daten

**Regel:** Ein Wert, der sich pro Objekt-Instanz unterscheiden muss, gehört als Member in die Instanz — nicht in einen globalen oder statisch geteilten Puffer.

**Warum:** Mit nur einer Instanz fällt der Bug nie auf. Er entsteht erst, sobald eine zweite Instanz denselben geteilten Puffer überschreibt — typischerweise genau dann, wenn das Projekt wächst.

**Beispiel aus dem Projekt:** `HX711Sensor` cachte den NVS-Namespace früher in einem globalen `char[12]`. Mit zwei HX711-Sensoren hätte der zweite die Kalibrierdaten des ersten gelesen und überschrieben. Fix: `_pId` als privates Member, Namespace wird bei jedem Zugriff frisch aus der Instanz gebaut.

---

## 07 · Doku — Doku driftet lautlos vom Code weg

**Regel:** Nach jeder Erweiterung einer Response, eines Interfaces oder eines öffentlichen Members die Doku im selben Schritt mitziehen — nicht als separaten, später nachgeholten Durchgang.

**Warum:** Doku, die man beim Programmieren nicht mehr anschaut, verrottet unbemerkt. Monate später vertraut man ihr trotzdem wieder blind — sie sieht ja vollständig aus.

**Beispiel aus dem Projekt:** `GET /status` bekam im Lauf der Feature-Arbeit vier neue Felder (`device_name`, `ssid`, `provisioned`, `password_set`) — das dokumentierte JSON-Beispiel blieb auf dem alten Stand, bis der explizite Doku-Abgleich es aufdeckte.

---

## 08 · Testbarkeit — Reine Logik hinter Hardware-Code versteckt

**Regel:** Logik ohne Hardware-/Netzwerkzugriff früh von hardwaregekoppeltem Code trennen — auch wenn es anfangs bequemer ist, sie einfach in derselben Datei mitlaufen zu lassen.

**Warum:** Ohne diese Trennung lässt sich die Logik nie isoliert testen. Und genau dort verstecken sich oft die unauffälligsten Bugs — siehe Punkt 02, der ohne diese Trennung nie einen Test bekommen hätte.

**Beispiel aus dem Projekt:** `escapeJsonString`/`extractValue` mussten aus `HttpServer.cpp`/`DigestAuth.cpp` in eigene, abhängigkeitsfreie Module (`JsonEscape`, `DigestHeaderParser`) verschoben werden, um sie nativ — ohne Board — zu testen.

---

*Zusammengestellt aus dem Code-Review des ESP32-C3-RemoteSensor-Projekts.*
