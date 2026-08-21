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
namespace
{
    WiFiServer server(80);
    bool serverRunning = false;
    char requestHeader[BUFFER_SIZE] = {};

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
        char buffer[BUFFER_SIZE] = {};
        snprintf(buffer, sizeof(buffer),
                 "{\"mac\":\"%s\",\"uptime\":%lu,\"rssi\":%d,\"chip_temp\":%.1f,\"free_heap\":%u,\"min_free_heap\":%u,\"wake_count\":%u,\"version\":\"%s\"}",
                 mac,
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
                InternalStorage::begin("Wifi", false);
                InternalStorage::writeString("ssid", doc["ssid"]);
                InternalStorage::writeString("password", doc["password"]);
                InternalStorage::end();
                client.print("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 3\r\n\r\n{}\n");
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
        else
        {
            client.print("HTTP/1.1 404 Not Found\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
        }
        client.stop();
        return true;
    }
}