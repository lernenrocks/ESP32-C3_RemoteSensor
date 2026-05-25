#include "WiFiManager.h"
#include <WiFi.h>
#include "wifi_config.h"
#include <Arduino.h>
#include "HttpServer.h"


#define WIFI_CONNECTION_TRY_COOLDOWN 10000UL 
#define WIFI_CONNECTION_TIMEOUT 5000UL

namespace WiFiManager
{

    void initWifi()
    {
        static unsigned long lastTry = ULONG_MAX;
        if (millis() - lastTry < WIFI_CONNECTION_TRY_COOLDOWN)
        {
            return;
        }
        WiFi.disconnect(true);
        vTaskDelay(pdMS_TO_TICKS(1000));
        WiFi.mode(WIFI_STA);
        WiFi.setAutoConnect(true);
        WiFi.begin(WIFI_SSID, WIFI_PW);
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
    bool isConnected()
    {
        return WiFi.status() == WL_CONNECTED;
    }
}