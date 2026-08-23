#include "WiFiManager.h"
#include <WiFi.h>
#include <Arduino.h>
#include "HttpServer.h"
#include "InternalStorage.h"
#include "esp_wifi.h"
#include "System.h"

#define WIFI_CONNECTION_TRY_COOLDOWN 10000UL
#define WIFI_CONNECTION_TIMEOUT 5000UL
constexpr uint SSID_LEN = 33;
constexpr uint PW_LEN = 64;

namespace WiFiManager
{

    void initWifi()
    {
#ifdef DISABLE_LIGHT_SLEEP
        // Diagnostics: log the disconnect reason once. Shows in plain text
        // whether drops come from the link (200 = BEACON_TIMEOUT) or from
        // auth/router. Only registered in the debug build — in the sleep
        // build the console flaps on every wake/sleep cycle anyway, runtime
        // logs aren't reliably visible there (see CLAUDE.md, Light Sleep section).
        static bool eventsRegistered = false;
        if (!eventsRegistered)
        {
            WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t info) {
                Serial.printf("[WARN] WiFi lost @%lu ms, reason: %u\n",
                              millis(), info.wifi_sta_disconnected.reason);
            }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
            eventsRegistered = true;
        }
#endif

        // AP/provisioning mode runs independently — nothing to do here.
        if (WiFi.getMode() == WIFI_MODE_AP)
            return;

        // Cooldown only applies to subsequent attempts; the first connect
        // runs immediately (previously the ULONG_MAX starting value delayed
        // it by ~10s).
        static unsigned long lastTry = 0;
        static bool everTried = false;
        if (everTried && millis() - lastTry < WIFI_CONNECTION_TRY_COOLDOWN)
            return;

        char ssid[SSID_LEN] = {};
        char pw[PW_LEN] = {};
        bool provisioned = false;
        {
            InternalStorage::Session session("Wifi", true);
            InternalStorage::readString("ssid", ssid, sizeof(ssid));
            InternalStorage::readString("password", pw, sizeof(pw));
            InternalStorage::readBool("provisioned", provisioned);
        }

        if (ssid[0] == '\0' || !provisioned)
        {
            char apSsid[32] = {};
            uint8_t mac[6];
            WiFi.macAddress(mac);
            snprintf(apSsid, sizeof(apSsid), "SensorNode-%02X%02X%02X%02X%02X%02X",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            char apPw[PW_LEN] = {};
            System::getActivePassword(apPw, sizeof(apPw));
            WiFi.mode(WIFI_AP);
            WiFi.softAP(apSsid, apPw);
            HttpServer::begin();
#ifdef DISABLE_LIGHT_SLEEP
            IPAddress apIp = WiFi.softAPIP();
            Serial.printf("[INFO] AP mode: SSID=%s IP=%u.%u.%u.%u\n", apSsid, apIp[0], apIp[1], apIp[2], apIp[3]);
#endif
            return;
        }

        // Trigger the STA connect. setAutoReconnect lets the WiFi driver
        // catch transient drops on its own; no more disconnect(true)+delay —
        // that power-cycled the radio and blocked for 1s per attempt.
        everTried = true;
#ifdef DISABLE_LIGHT_SLEEP
        Serial.printf("[INFO] STA connect: ssid=\"%s\"\n", ssid);
#endif
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(true);
        WiFi.begin(ssid, pw);
        unsigned long loopStart = millis();
        while (!isConnected() && millis() - loopStart < WIFI_CONNECTION_TIMEOUT)
        {
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        lastTry = millis();
        if (isConnected())
        {
            HttpServer::begin(); // idempotent -> no re-begin leak on reconnect
            // Modem sleep: prerequisite for esp_sleep_enable_wifi_wakeup.
            // The AP buffers, the station wakes at the DTIM beacon -> an
            // incoming TCP request wakes the sleeping C3.
            esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
        }
    }
    bool isConnected()
    {
        return WiFi.status() == WL_CONNECTED;
    }

    void heartbeat()
    {
        constexpr unsigned long HEARTBEAT_MS = 5000UL;
        static unsigned long last = 0;
        if (millis() - last < HEARTBEAT_MS)
            return;
        last = millis();
        if (isConnected())
        {
            IPAddress ip = WiFi.localIP(); // octets individually -> no String/heap
            Serial.printf("[INFO] up %lu ms | connected %u.%u.%u.%u | rssi %d\n",
                          millis(), ip[0], ip[1], ip[2], ip[3], WiFi.RSSI());
        }
        else
        {
            Serial.printf("[INFO] up %lu ms | DISCONNECTED\n", millis());
        }
    }
}