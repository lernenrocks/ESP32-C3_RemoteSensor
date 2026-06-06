#include <WiFi.h>
#include "SensorManager.h"
#include <ArduinoJson.h>
#include "InternalStorage.h"

extern const char FIRMWARE_VERSION[];
extern uint32_t wakeCount;

constexpr uint BUFFER_SIZE = 512;
constexpr uint BODY_SIZE = 256;
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
        uint8_t sensorIdx;
        if (strstr(requestHeader, "GET / "))
        {
            const char body[] = "<html><body><h1>SensorNode Provisioning</h1><p>HTML interface coming soon.</p></body></html>";
            client.printf("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\nContent-Length: %d\r\n\r\n", strlen(body));
            client.print(body);
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
        else if (strstr(requestHeader, "POST /provision/wifi") != 0)
        {
            StaticJsonDocument<BODY_SIZE> doc;
            DeserializationError err = deserializeJson(doc, client);
            if (!err)
            {
                if (doc["ssid"].isNull() || doc["password"].isNull())
                {
                    client.print("HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
                }
                InternalStorage::begin("Wifi", false);
                InternalStorage::writeString("ssid", doc["ssid"]);
                InternalStorage::writeString("password", doc["password"]);
                InternalStorage::end();
                client.print("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 3\r\n\r\n{}\n");
            }
            else
            {
                client.print("HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
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
            InternalStorage::begin("Wifi", false);
            InternalStorage::writeBool("provisioned", true);
            InternalStorage::end();
            client.print("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 3\r\n\r\n{}\n");
            client.stop();
            delay(100);
            Serial.println("restart now");
            ESP.restart();
        }
        else if (strstr(requestHeader, "POST /reset"))
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
        else
        {
            client.print("HTTP/1.1 404 Not Found\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
        }
        client.stop();
        return true;
    }
}