#include <WiFi.h>
#include "SensorManager.h"
#include <ArduinoJson.h>
#include "InternalStorage.h"
#include "ProvisioningPage.h"
#include "DigestAuth.h"
#include "DigestCrypto.h"
#include "System.h"

extern const char FIRMWARE_VERSION[];
extern uint32_t wakeCount;

constexpr uint BUFFER_SIZE = 1024;
constexpr uint BODY_SIZE = 256;
// WPA2-PSK verlangt mindestens 8 Zeichen -- das Device-Passwort dient auch
// als AP-Passwort, also muss es diese Grenze einhalten.
constexpr size_t MIN_PASSWORD_LEN = 8;
// Inaktivitaets-Timeout beim Header-Lesen: bricht ab, wenn nach dem Verbinden
// keine (weiteren) Daten kommen. Schuetzt vor halb-offenen/abgerissenen
// Verbindungen, bei denen client.connected() haengen bleibt (kein RST).
constexpr unsigned long REQUEST_READ_TIMEOUT_MS = 1500UL;
// Timeout fuer den WLAN-Testconnect bei POST /provision/wifi — bewusst gleich
// dem WIFI_CONNECTION_TIMEOUT aus WiFiManager.cpp, aber lokal dupliziert statt
// geteilt: HttpServer haengt sonst von WiFiManager ab, WiFiManager schon von
// HttpServer (startet/stoppt ihn) — ein echter Include-Zirkel auf Modulebene.
constexpr unsigned long WIFI_TEST_CONNECT_TIMEOUT_MS = 5000UL;
namespace
{
    WiFiServer server(80);
    bool serverRunning = false;
    char requestHeader[BUFFER_SIZE] = {};

    // Testet STA-Credentials, ohne den aktuellen Modus dauerhaft zu verlassen.
    // Zwei Aufrufkontexte laut REST-API-Doku zu /provision/wifi:
    //  - AP-Onboarding (provisioned=false): Modus AP+STA-Concurrent, damit der
    //    bereits laufende Onboarding-AP fuer Browser/Handy aktiv bleibt.
    //  - Netzwechsel im laufenden STA-Betrieb (provisioned=true, MainUnit-Umzug
    //    mit bekanntem Zielnetz): kein AP vorhanden/konfiguriert -> reiner
    //    STA-Test, sonst wuerde WIFI_MODE_APSTA kurzzeitig einen unkonfigurierten,
    //    ungeschuetzten AP oeffnen (verboten, siehe Sicherheit in CLAUDE.md).
    // In beiden Faellen wird am Ende exakt der Ausgangsmodus wiederhergestellt.
    bool testStaConnect(const char *ssid, const char *password)
    {
        wifi_mode_t originalMode = WiFi.getMode();
        wifi_mode_t testMode = (originalMode == WIFI_MODE_AP) ? WIFI_MODE_APSTA : WIFI_MODE_STA;
        WiFi.persistent(false); // nur ein Test, kein Grund den WiFi-NVS-Blob zu beschreiben
        WiFi.mode(testMode);
        WiFi.begin(ssid, password);
        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TEST_CONNECT_TIMEOUT_MS)
        {
            delay(500);
        }
        bool connected = WiFi.status() == WL_CONNECTED;
        WiFi.disconnect(true);
        WiFi.mode(originalMode);
        return connected;
    }

    // Erste Stelle im Code, an der user-eingegebener Freitext (Geraetename) in
    // handgebautes JSON eingebettet wird — Escaping ist laut CLAUDE.md Pflicht
    // (\", \\, \n, \r, \t, \uXXXX), sonst kann ein Name das Response-JSON brechen.
    void escapeJsonString(const char *in, char *out, size_t outLen)
    {
        size_t o = 0;
        for (size_t i = 0; in[i] != '\0' && o + 1 < outLen; i++)
        {
            unsigned char c = (unsigned char)in[i];
            const char *rep = nullptr;
            switch (c)
            {
                case '"': rep = "\\\""; break;
                case '\\': rep = "\\\\"; break;
                case '\n': rep = "\\n"; break;
                case '\r': rep = "\\r"; break;
                case '\t': rep = "\\t"; break;
            }
            if (rep)
            {
                size_t rl = strlen(rep);
                if (o + rl >= outLen) break;
                memcpy(out + o, rep, rl);
                o += rl;
            }
            else if (c < 0x20)
            {
                if (o + 6 >= outLen) break;
                o += snprintf(out + o, outLen - o, "\\u%04x", c);
            }
            else
            {
                out[o++] = (char)c;
            }
        }
        out[o] = '\0';
    }

    void printSensorInfo(WiFiClient &client)
    {
        char buffer[BUFFER_SIZE] = {};
        SensorManager::getSensorDataJson(buffer, sizeof(buffer));
        client.printf("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: %d\r\n\r\n", strlen(buffer) + 1);
        client.print(buffer);
        client.print("\n");
    }
    void printStatus(WiFiClient &client)
    {
        char mac[18] = {};
        WiFi.macAddress().toCharArray(mac, sizeof(mac));
        char name[System::DEVICE_NAME_LEN] = {};
        System::getActiveDeviceName(name, sizeof(name));
        char nameEscaped[System::DEVICE_NAME_LEN * 6] = {};
        escapeJsonString(name, nameEscaped, sizeof(nameEscaped));
        char ssid[33] = {};
        WiFi.SSID().toCharArray(ssid, sizeof(ssid));
        char ssidEscaped[33 * 6] = {};
        escapeJsonString(ssid, ssidEscaped, sizeof(ssidEscaped));
        bool provisioned = false;
        InternalStorage::begin("Wifi", true);
        InternalStorage::readBool("provisioned", provisioned);
        InternalStorage::end();
        char ha1[DigestCrypto::SHA256_HEX_LEN + 1] = {};
        bool passwordSet = System::getActiveHa1(ha1, sizeof(ha1));
        char buffer[BUFFER_SIZE] = {};
        snprintf(buffer, sizeof(buffer),
                 "{\"mac\":\"%s\",\"device_name\":\"%s\",\"ssid\":\"%s\",\"provisioned\":%s,\"password_set\":%s,\"uptime\":%lu,\"rssi\":%d,\"chip_temp\":%.1f,\"free_heap\":%u,\"min_free_heap\":%u,\"wake_count\":%u,\"version\":\"%s\"}",
                 mac,
                 nameEscaped,
                 ssidEscaped,
                 provisioned ? "true" : "false",
                 passwordSet ? "true" : "false",
                 millis(),
                 WiFi.RSSI(),
                 temperatureRead(),
                 esp_get_free_heap_size(),
                 esp_get_minimum_free_heap_size(),
                 wakeCount,
                 FIRMWARE_VERSION);
        client.printf("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: %d\r\n\r\n", strlen(buffer) + 1);
        client.print(buffer);
        client.print("\n");
    }
    void printCalibrationInfo(WiFiClient &client)
    {
        char buffer[BUFFER_SIZE] = {};
        SensorManager::getCalibrationInfoJson(buffer, sizeof(buffer));
        client.printf("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: %d\r\n\r\n", strlen(buffer) + 1);
        client.print(buffer);
        client.print("\n");
    }
}
namespace HttpServer
{

    void begin()
    {
        if (serverRunning)
            return;
        server.begin();
        serverRunning = true;
    }
    void end()
    {
        if (!serverRunning)
            return;
        server.end();
        serverRunning = false;
    }
    bool handle()
    {
        WiFiClient client = server.available();
        if (!client)
            return false;
        memset(requestHeader, 0, BUFFER_SIZE);
        int idx = 0;
        unsigned long lastData = millis();
        while (client.connected() && idx < BUFFER_SIZE - 1)
        {
            if (client.available())
            {
                requestHeader[idx++] = client.read();
                lastData = millis();
                if (idx >= 4 &&
                    requestHeader[idx - 4] == '\r' && requestHeader[idx - 3] == '\n' &&
                    requestHeader[idx - 2] == '\r' && requestHeader[idx - 1] == '\n')
                {
                    break;
                }
            }
            else if (millis() - lastData > REQUEST_READ_TIMEOUT_MS)
            {
                // Keine Daten mehr -> abgerissene/stockende Verbindung aufgeben.
                client.stop();
                return false;
            }
        }
        char method[8] = {};
        char path[64] = {};
        const char *sp1 = strchr(requestHeader, ' ');
        if (sp1)
        {
            size_t mlen = (size_t)(sp1 - requestHeader);
            if (mlen >= sizeof(method))
                mlen = sizeof(method) - 1;
            memcpy(method, requestHeader, mlen);
            method[mlen] = '\0';

            const char *sp2 = strchr(sp1 + 1, ' ');
            if (sp2)
            {
                size_t plen = (size_t)(sp2 - (sp1 + 1));
                if (plen >= sizeof(path))
                    plen = sizeof(path) - 1;
                memcpy(path, sp1 + 1, plen);
                path[plen] = '\0';
            }
        }
        char deviceHa1[DigestCrypto::SHA256_HEX_LEN + 1] = {};
        bool ownPassword = System::getActiveHa1(deviceHa1, sizeof(deviceHa1));
        Serial.printf("[INFO] %s %s | ha1: %s\n", method, path, ownPassword ? "own" : "default");
        if (!DigestAuth::verify(requestHeader, method, path, deviceHa1))
        {
            Serial.println("[WARN] auth failed");
            char wwwAuth[160] = {};
            DigestAuth::buildWwwAuthenticate(wwwAuth, sizeof(wwwAuth));
            client.printf("HTTP/1.1 401 Unauthorized\r\nWWW-Authenticate: %s\r\nConnection: close\r\nContent-Length: 0\r\n\r\n", wwwAuth);
            client.stop();
            return true;
        }
        Serial.println("[INFO] auth ok");

        uint8_t sensorIdx;
        if (strstr(requestHeader, "GET / "))
        {
            // Seite liegt im Flash (const char[]) -> direkt streamen, kein Heap.
            client.printf("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\nContent-Length: %u\r\n\r\n", strlen(PROVISIONING_HTML));
            client.print(PROVISIONING_HTML);
        }
        else if (strstr(requestHeader, "GET /sensors") != 0)
        {
            printSensorInfo(client);
        }
        else if (strstr(requestHeader, "GET /status") != 0)
        {
            printStatus(client);
        }
        else if (strstr(requestHeader, "GET /calibrationinfo") != 0)
        {
            printCalibrationInfo(client);
        }
        else if (strstr(requestHeader, "POST /provision/wifi ") != 0)
        {
            StaticJsonDocument<BODY_SIZE> doc;
            DeserializationError err = deserializeJson(doc, client);
            if (err || doc["ssid"].isNull() || doc["password"].isNull())
            {
                // Parse-Fehler oder fehlende Felder: nichts schreiben, eine Response.
                client.print("HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
            }
            else
            {
                const char *ssid = doc["ssid"];
                const char *pw = doc["password"];
                bool connected = testStaConnect(ssid, pw);
                Serial.printf("[INFO] WiFi test-connect ssid=\"%s\": %s\n", ssid, connected ? "ok" : "failed");
                // Nur bei Erfolg in NVS uebernehmen — bei Fehlschlag bleiben die
                // alten (funktionierenden) Zugangsdaten stehen, damit initWifi()
                // beim naechsten Reconnect-Versuch von selbst zum alten Netz
                // zurueckfindet, statt mit kaputten neuen Daten haengen zu bleiben.
                if (connected)
                {
                    InternalStorage::begin("Wifi", false);
                    InternalStorage::writeString("ssid", ssid);
                    InternalStorage::writeString("password", pw);
                    InternalStorage::end();
                }
                char body[24] = {};
                snprintf(body, sizeof(body), "{\"connected\":%s}\n", connected ? "true" : "false");
                client.printf("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: %d\r\n\r\n", strlen(body));
                client.print(body);
            }
        }
        else if (sscanf(requestHeader, "POST /calibrate/%hhu", &sensorIdx) == 1)
        {
            StaticJsonDocument<BODY_SIZE> doc;
            DeserializationError err = deserializeJson(doc, client);
            if (err || !SensorManager::calibrateSensor(sensorIdx, doc.as<JsonObjectConst>()))
                client.print("HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
            else
                client.print("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 3\r\n\r\n{}\n");
        }
        else if (sscanf(requestHeader, "POST /reset/%hhu", &sensorIdx) == 1)
        {
            if (SensorManager::resetSensor(sensorIdx))
                client.print("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 3\r\n\r\n{}\n");
            else
                client.print("HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
        }
        else if (strstr(requestHeader, "POST /factoryreset") != 0)
        {
            InternalStorage::erase();
            client.print("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 3\r\n\r\n{}\n");
            client.stop();
            delay(100);
            Serial.println("restart now");
            ESP.restart();
        }
        else if (strstr(requestHeader, "POST /provision/finish"))
        {
            char ha1[DigestCrypto::SHA256_HEX_LEN + 1] = {};
            if (!System::getActiveHa1(ha1, sizeof(ha1)))
            {
                Serial.println("[WARN] finish blocked: no device password set");
                client.print("HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
                client.stop();
                return true;
            }
            InternalStorage::begin("Wifi", false);
            InternalStorage::writeBool("provisioned", true);
            InternalStorage::end();
            client.print("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 3\r\n\r\n{}\n");
            client.stop();
            delay(100);
            Serial.println("restart now");
            ESP.restart();
        }
        else if (strstr(requestHeader, "POST /provision/wifi/reset") != 0)
        {
            InternalStorage::begin("Wifi", false);
            InternalStorage::writeBool("provisioned", false);
            InternalStorage::end();
            client.print("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 3\r\n\r\n{}\n");
            client.stop();
            delay(100);
            Serial.println("restart now");
            ESP.restart();
        }
        else if (strstr(requestHeader, "POST /provision/password") != 0)
        {
            StaticJsonDocument<BODY_SIZE> doc;
            DeserializationError err = deserializeJson(doc, client);
            const char *pw = doc["password"];
            if (err || doc["password"].isNull() || strlen(pw) < MIN_PASSWORD_LEN)
            {
                client.print("HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
            }
            else
            {
                char ha1[DigestCrypto::SHA256_HEX_LEN + 1] = {};
                DigestCrypto::computeHa1(pw, ha1, sizeof(ha1));
                System::storeDeviceHa1(ha1);
                System::storeDevicePassword(pw);
                Serial.println("[INFO] device password updated");
                client.print("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 3\r\n\r\n{}\n");
            }
        }
        else if (strstr(requestHeader, "POST /provision/name") != 0)
        {
            StaticJsonDocument<BODY_SIZE> doc;
            DeserializationError err = deserializeJson(doc, client);
            const char *name = doc["name"];
            if (err || doc["name"].isNull() || strlen(name) == 0)
            {
                client.print("HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
            }
            else
            {
                System::storeDeviceName(name);
                Serial.printf("[INFO] device name set to \"%s\"\n", name);
                client.print("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 3\r\n\r\n{}\n");
            }
        }
        else
        {
            client.print("HTTP/1.1 404 Not Found\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
        }
        client.stop();
        return true;
    }
}