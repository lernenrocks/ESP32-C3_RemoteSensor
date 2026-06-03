#include "WiFiManager.h"
#include <WiFi.h>
#include "wifi_config.h"
#include <Arduino.h>
#include "HttpServer.h"
#include "InternalStorage.h"

#define WIFI_CONNECTION_TRY_COOLDOWN 10000UL
#define WIFI_CONNECTION_TIMEOUT 5000UL
constexpr uint SSID_LEN = 33;
constexpr uint PW_LEN = 64;

namespace WiFiManager
{

    void initWifi()
    {
        static unsigned long lastTry = ULONG_MAX;
        if (millis() - lastTry < WIFI_CONNECTION_TRY_COOLDOWN || WiFi.getMode()==WIFI_MODE_AP)
        {
            return;
        }
        char ssid[SSID_LEN] = {};
        char pw[PW_LEN] = {};
        bool provisioned = false;
        InternalStorage::begin("Wifi", true);
        InternalStorage::readString("ssid", ssid, sizeof(ssid));
        InternalStorage::readString("password", pw, sizeof(pw));
        InternalStorage::readBool("provisioned", provisioned);
        InternalStorage::end();
        WiFi.disconnect(true);
        vTaskDelay(pdMS_TO_TICKS(1000));
        WiFi.setAutoConnect(true);
        if (ssid[0] == '\0' || !provisioned)
        {
            char apSsid[32] = {};
            uint8_t mac[6];
            WiFi.macAddress(mac);
            snprintf(apSsid, sizeof(apSsid), "SensorNode-%02X%02X%02X%02X%02X%02X",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            WiFi.mode(WIFI_AP);
            WiFi.softAP(apSsid);
            HttpServer::begin();
            Serial.println(WiFi.softAPIP());
        }
        else
        {
            WiFi.mode(WIFI_STA);
            WiFi.begin(ssid, pw);
            unsigned long loopStart = millis();
            while (!isConnected() && millis() - loopStart < WIFI_CONNECTION_TIMEOUT)
            {
                vTaskDelay(pdMS_TO_TICKS(500));
            }
            lastTry = millis();
            if (isConnected())
            {
                HttpServer::begin();
                Serial.println(WiFi.localIP());
            }
            else
            {
                HttpServer::end();
            }
        }
    }
    bool isConnected()
    {
        return WiFi.status() == WL_CONNECTED;
    }
}