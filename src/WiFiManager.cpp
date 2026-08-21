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
        // Diagnose: einmalig den Disconnect-Grund mitloggen. Zeigt in Klartext,
        // ob Abrisse vom Link kommen (200 = BEACON_TIMEOUT) oder von Auth/Router.
        static bool eventsRegistered = false;
        if (!eventsRegistered)
        {
            WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t info) {
                Serial.printf("[WARN] WiFi lost @%lu ms, reason: %u\n",
                              millis(), info.wifi_sta_disconnected.reason);
            }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
            eventsRegistered = true;
        }

        // AP-/Provisioning-Modus laeuft eigenstaendig — nichts nachzuziehen.
        if (WiFi.getMode() == WIFI_MODE_AP)
            return;

        // Cooldown gilt nur fuer Folge-Versuche; der erste Connect laeuft sofort
        // (frueher verzoegerte der ULONG_MAX-Startwert ihn um ~10 s).
        static unsigned long lastTry = 0;
        static bool everTried = false;
        if (everTried && millis() - lastTry < WIFI_CONNECTION_TRY_COOLDOWN)
            return;

        char ssid[SSID_LEN] = {};
        char pw[PW_LEN] = {};
        bool provisioned = false;
        InternalStorage::begin("Wifi", true);
        InternalStorage::readString("ssid", ssid, sizeof(ssid));
        InternalStorage::readString("password", pw, sizeof(pw));
        InternalStorage::readBool("provisioned", provisioned);
        InternalStorage::end();

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
            IPAddress apIp = WiFi.softAPIP();
            Serial.printf("[INFO] AP mode: SSID=%s IP=%u.%u.%u.%u\n", apSsid, apIp[0], apIp[1], apIp[2], apIp[3]);
            return;
        }

        // STA-Connect anstossen. setAutoReconnect laesst den WiFi-Treiber
        // transiente Drops selbst abfangen; kein disconnect(true)+Delay mehr —
        // das power-cyclete den Funk und blockierte 1 s pro Versuch.
        everTried = true;
        Serial.printf("[INFO] STA connect: ssid=\"%s\"\n", ssid);
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
            HttpServer::begin(); // idempotent -> kein Re-begin-Leak bei Reconnect
            Serial.println(WiFi.localIP());
            // Modem-Sleep: Voraussetzung fuer esp_sleep_enable_wifi_wakeup.
            // Der AP puffert, die Station wacht am DTIM-Beacon -> eingehende
            // TCP-Anfrage weckt den schlafenden C3.
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
            IPAddress ip = WiFi.localIP(); // Oktette einzeln -> kein String/Heap
            Serial.printf("[INFO] up %lu ms | connected %u.%u.%u.%u | rssi %d\n",
                          millis(), ip[0], ip[1], ip[2], ip[3], WiFi.RSSI());
        }
        else
        {
            Serial.printf("[INFO] up %lu ms | DISCONNECTED\n", millis());
        }
    }
}