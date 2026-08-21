---
name: project-next-steps
description: Offene Punkte und Abschluss-Checkliste für laufende und kommende Arbeiten
metadata:
  type: project
---

## Digest Auth — aktuell in Arbeit

Schritt-für-Schritt-Umsetzung:
1. HA1-Management (`setPassword`, `loadStoredHa1`, `selfTestHa1`) ← *hier*
2. Nonce-Generierung + 401-Challenge (`WWW-Authenticate`-Header)
3. Authorization-Header parsen (analog `extractValue()` der MainUnit)
4. Verify-Logik (HA2 aus Request-Line, Response vergleichen)
5. Gate in `HttpServer.cpp` + Request-Buffer auf ~1024 Bytes
6. Test mit `curl --digest -u admin:testpw`

**Am Ende von Digest Auth — Abschluss-Checkliste:**
- [ ] Alle `char[]`-Buffer-Initialisierungen im Projekt durchgehen: sicherstellen, dass kein Buffer uninitialisiert in eine Funktion läuft die ihn unbeschrieben zurückgibt (Vorbild: `= {}` beim Aufrufer, kein Mischen mit explizitem `[0]='\0'` in Funktionen)
- [ ] `selfTestHa1()` und Testblock in `loop()` entfernen
- [ ] `selfTestSha256()` (Deklaration + Definition) entfernen
- [ ] `DISABLE_LIGHT_SLEEP` wieder auskommentieren

## Danach (Roadmap)

5. CPU-Drosselung (`setCpuFrequencyMhz`)
6. OTA

## Kleinere Ergänzungen (nachrüstbar)
- GET /calibrationinfo um aktuelle scale/offset-Werte erweitern (Read-back)
- `initWifi()` nicht-blockierend + AP-Fallback nach N Fehlversuchen
