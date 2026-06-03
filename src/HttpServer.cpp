#include <WiFi.h>
#include "SensorManager.h"
#include <ArduinoJson.h>
#include "InternalStorage.h"

extern const char FIRMWARE_VERSION[];

constexpr uint BUFFER_SIZE = 512;
constexpr uint BODY_SIZE = 256;
namespace
{
    WiFiServer server(80);
    char requestHeader[BUFFER_SIZE] = {};

    const char getSensors[] = "GET /sensors";
    const char getStatus[] = "GET /status";
    const char getCalibrationInfo[] = "GET /calibrationinfo";

    void printSensorInfo(WiFiClient &client)
    {
        char buffer[BUFFER_SIZE] = {};
        SensorManager::getSensorDataJson(buffer, sizeof(buffer));
        client.printf("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: %d\r\n\r\n", strlen(buffer));
        client.print(buffer);
    }
    void printStatus(WiFiClient &client)
    {
        char mac[18] = {};
        WiFi.macAddress().toCharArray(mac, sizeof(mac));
        char buffer[BUFFER_SIZE] = {};
        snprintf(buffer, sizeof(buffer),
                 "{\"mac\":\"%s\",\"uptime\":%lu,\"rssi\":%d,\"chip_temp\":%.1f,\"free_heap\":%u,\"version\":\"%s\"}",
                 mac,
                 millis(),
                 WiFi.RSSI(),
                 temperatureRead(),
                 esp_get_free_heap_size(),
                 FIRMWARE_VERSION);
        client.printf("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: %d\r\n\r\n", strlen(buffer));
        client.print(buffer);
    }
    void printCalibrationInfo(WiFiClient &client)
    {
        char buffer[BUFFER_SIZE] = {};
        SensorManager::getCalibrationInfoJson(buffer, sizeof(buffer));
        client.printf("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: %d\r\n\r\n", strlen(buffer));
        client.print(buffer);
    }
}
namespace HttpServer
{

    void begin()
    {
        server.begin();
    }
    void end()
    {
        server.end();
    }
    void handle()
    {
        WiFiClient client = server.available();
        if (!client)
            return;
        memset(requestHeader, 0, BUFFER_SIZE);
        int idx = 0;
        while (client.connected() && idx < BUFFER_SIZE - 1)
        {
            if (client.available())
            {
                requestHeader[idx++] = client.read();
                if (idx >= 4 &&
                    requestHeader[idx - 4] == '\r' && requestHeader[idx - 3] == '\n' &&
                    requestHeader[idx - 2] == '\r' && requestHeader[idx - 1] == '\n')
                {
                    break;
                }
            }
        }
        uint8_t sensorIdx;
        if (strstr(requestHeader, "GET / "))
        {
            // build interface
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
                    return;
                }
                InternalStorage::begin("Credentials", false);
                InternalStorage::writeString("ssid", doc["ssid"]);
                InternalStorage::writeString("password", doc["password"]);
                InternalStorage::end();
                client.print("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 2\r\n\r\n{}");
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
                client.print("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 2\r\n\r\n{}");
        }
        else if (sscanf(requestHeader, "POST /reset/%hhu", &sensorIdx) == 1)
        {
            if (SensorManager::resetSensor(sensorIdx))
                client.print("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 2\r\n\r\n{}");
            else
                client.print("HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
        }
        else if (strstr(requestHeader, "POST /factoryreset") != 0)
        {
            InternalStorage::erase();
            client.print("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 2\r\n\r\n{}");
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
    }
}