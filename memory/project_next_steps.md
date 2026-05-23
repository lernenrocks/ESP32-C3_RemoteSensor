---
name: project-next-steps
description: Offene Punkte für die nächste Session
metadata:
  type: project
---

Sensorschicht ist abgeschlossen. Nächster Block: HTTP-Server.

1. **HTTP-Server** — GET /sensors, GET /status, GET /calibrationinfo, Digest Auth (SHA-256 via mbedtls)
2. **Light Sleep** — zwischen Abfragen, WiFi-Assoziation aktiv
3. **Provisioning-AP** — Factory Reset, HTML-Formular, NVS
4. **NVS-Verschlüsselung** — Credentials sicher ablegen
5. **OTA** — Standard-Partition-Scheme, Companion-App-Trigger
