#include <WiFi.h>
#include "SensorManager.h"
#include <ArduinoJson.h>
#include "InternalStorage.h"
#include "ProvisioningPage.h"
#include "DigestAuth.h"
#include "DigestCrypto.h"
#include "System.h"
#include "JsonEscape.h"

extern const char FIRMWARE_VERSION[];
extern uint32_t wakeCount;

constexpr uint BUFFER_SIZE = 1024;
constexpr uint BODY_SIZE = 256;
// WPA2-PSK requires at least 8 characters -- the device password also
// serves as the AP password, so it must respect this limit.
constexpr size_t MIN_PASSWORD_LEN = 8;
// WPA2-PSK's own real maximum (63 chars) — also keeps password well clear of
// the point where DigestCrypto::computeHa1()'s internal buffer would start
// silently truncating it (~92 chars), which would otherwise make the HA1
// (and thus the effective password) shorter than the user thinks it is,
// with no error anywhere.
constexpr size_t MAX_PASSWORD_LEN = 63;
// Inactivity timeout while reading the header: aborts if no (further) data
// arrives after connecting. Protects against half-open/dropped connections
// where client.connected() stays stuck (no RST).
constexpr unsigned long REQUEST_READ_TIMEOUT_MS = 1500UL;
// Timeout for the WLAN test-connect in POST /provision/wifi — deliberately
// the same as WIFI_CONNECTION_TIMEOUT in WiFiManager.cpp, but duplicated
// locally instead of shared: otherwise HttpServer would depend on
// WiFiManager, while WiFiManager already depends on HttpServer (starts/
// stops it) — a real include cycle at the module level.
constexpr unsigned long WIFI_TEST_CONNECT_TIMEOUT_MS = 5000UL;

// Debug-only logging, collapsed to one call site instead of a scattered
// #ifdef/Serial.printf/#endif block wherever a route wants to log something.
// A macro, not a no-op function: when DISABLE_LIGHT_SLEEP isn't defined, this
// vanishes at the preprocessor stage -- guaranteed zero cost, not just
// "probably optimized away" the way a no-op function call would be.
#ifdef DISABLE_LIGHT_SLEEP
#define DEBUG_LOG(...) Serial.printf(__VA_ARGS__)
#else
#define DEBUG_LOG(...)
#endif

namespace
{
    WiFiServer server(80);
    bool serverRunning = false;
    char requestHeader[BUFFER_SIZE] = {};

    // Tests STA credentials without permanently leaving the current mode.
    // Two call contexts per the REST API docs for /provision/wifi:
    //  - AP onboarding (provisioned=false): AP+STA concurrent mode, so the
    //    already-running onboarding AP stays active for the browser/phone.
    //  - Network change during live STA operation (provisioned=true,
    //    MainUnit move to a known target network): no AP present/configured
    //    -> plain STA test, otherwise WIFI_MODE_APSTA would briefly open an
    //    unconfigured, unprotected AP (forbidden, see Security in CLAUDE.md).
    // In both cases, the original mode is restored exactly at the end.
    bool testStaConnect(const char *ssid, const char *password)
    {
        wifi_mode_t originalMode = WiFi.getMode();
        wifi_mode_t testMode = (originalMode == WIFI_MODE_AP) ? WIFI_MODE_APSTA : WIFI_MODE_STA;
        WiFi.persistent(false); // just a test, no reason to write the WiFi NVS blob
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

    // --- GET handlers ---

    void handleRoot(WiFiClient &client)
    {
        // Page lives in flash (const char[]) -> stream directly, no heap.
        client.printf("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\nContent-Length: %u\r\n\r\n", strlen(PROVISIONING_HTML));
        client.print(PROVISIONING_HTML);
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
        JsonEscape::escape(name, nameEscaped, sizeof(nameEscaped));
        char ssid[33] = {};
        WiFi.SSID().toCharArray(ssid, sizeof(ssid));
        char ssidEscaped[33 * 6] = {};
        JsonEscape::escape(ssid, ssidEscaped, sizeof(ssidEscaped));
        bool provisioned = false;
        {
            InternalStorage::Session session("Wifi", true);
            InternalStorage::readBool("provisioned", provisioned);
        }
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

    // --- POST handlers ---

    void handleProvisionWifi(WiFiClient &client)
    {
        StaticJsonDocument<BODY_SIZE> doc;
        DeserializationError err = deserializeJson(doc, client);
        if (err || doc["ssid"].isNull() || doc["password"].isNull())
        {
            // Parse error or missing fields: write nothing, one response.
            client.print("HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
            return;
        }
        const char *ssid = doc["ssid"];
        const char *pw = doc["password"];
        bool connected = testStaConnect(ssid, pw);
        DEBUG_LOG("[INFO] WiFi test-connect ssid=\"%s\": %s\n", ssid, connected ? "ok" : "failed");
        // Only commit to NVS on success — on failure, the old
        // (working) credentials stay in place, so initWifi() finds
        // its way back to the old network by itself on the next
        // reconnect attempt, instead of getting stuck with broken
        // new data.
        if (connected)
        {
            InternalStorage::Session session("Wifi", false);
            InternalStorage::writeString("ssid", ssid);
            InternalStorage::writeString("password", pw);
        }
        char body[24] = {};
        snprintf(body, sizeof(body), "{\"connected\":%s}\n", connected ? "true" : "false");
        client.printf("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: %d\r\n\r\n", strlen(body));
        client.print(body);
    }

    void handleCalibrate(WiFiClient &client, uint8_t idx)
    {
        StaticJsonDocument<BODY_SIZE> doc;
        DeserializationError err = deserializeJson(doc, client);
        if (err || !SensorManager::calibrateSensor(idx, doc.as<JsonObjectConst>()))
            client.print("HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
        else
            client.print("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 3\r\n\r\n{}\n");
    }

    void handleReset(WiFiClient &client, uint8_t idx)
    {
        if (SensorManager::resetSensor(idx))
            client.print("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 3\r\n\r\n{}\n");
        else
            client.print("HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
    }

    void handleFactoryReset(WiFiClient &client)
    {
        InternalStorage::erase();
        client.print("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 3\r\n\r\n{}\n");
        client.stop();
        delay(100);
        DEBUG_LOG("[INFO] factory reset, rebooting\n");
        ESP.restart();
    }

    void handleProvisionFinish(WiFiClient &client)
    {
        char ha1[DigestCrypto::SHA256_HEX_LEN + 1] = {};
        if (!System::getActiveHa1(ha1, sizeof(ha1)))
        {
            DEBUG_LOG("[WARN] finish blocked: no device password set\n");
            client.print("HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
            return; // handle()'s trailing client.stop() closes the connection
        }
        {
            InternalStorage::Session session("Wifi", false);
            InternalStorage::writeBool("provisioned", true);
        }
        client.print("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 3\r\n\r\n{}\n");
        client.stop();
        delay(100);
        DEBUG_LOG("[INFO] provisioning finished, rebooting into STA\n");
        ESP.restart();
    }

    void handleWifiReset(WiFiClient &client)
    {
        {
            InternalStorage::Session session("Wifi", false);
            InternalStorage::writeBool("provisioned", false);
        }
        client.print("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 3\r\n\r\n{}\n");
        client.stop();
        delay(100);
        DEBUG_LOG("[INFO] wifi reset, rebooting into AP\n");
        ESP.restart();
    }

    void handleProvisionPassword(WiFiClient &client)
    {
        StaticJsonDocument<BODY_SIZE> doc;
        DeserializationError err = deserializeJson(doc, client);
        const char *pw = doc["password"];
        if (err || doc["password"].isNull() || strlen(pw) < MIN_PASSWORD_LEN || strlen(pw) > MAX_PASSWORD_LEN)
        {
            client.print("HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
            return;
        }
        char ha1[DigestCrypto::SHA256_HEX_LEN + 1] = {};
        DigestCrypto::computeHa1(pw, ha1, sizeof(ha1));
        System::storeDeviceHa1(ha1);
        System::storeDevicePassword(pw);
        DEBUG_LOG("[INFO] device password updated\n");
        client.print("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 3\r\n\r\n{}\n");
    }

    void handleProvisionName(WiFiClient &client)
    {
        StaticJsonDocument<BODY_SIZE> doc;
        DeserializationError err = deserializeJson(doc, client);
        const char *name = doc["name"];
        // >= (not >) because DEVICE_NAME_LEN includes the '\0' -- a name
        // that exactly fills the buffer would leave no room for it.
        if (err || doc["name"].isNull() || strlen(name) == 0 || strlen(name) >= System::DEVICE_NAME_LEN)
        {
            client.print("HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
            return;
        }
        System::storeDeviceName(name);
        DEBUG_LOG("[INFO] device name set to \"%s\"\n", name);
        client.print("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 3\r\n\r\n{}\n");
    }

    void handleSetSensorName(WiFiClient &client, uint8_t idx)
    {
        StaticJsonDocument<BODY_SIZE> doc;
        DeserializationError err = deserializeJson(doc, client);
        const char *name = doc["name"];
        if (err || doc["name"].isNull() || strlen(name) == 0 || strlen(name) >= SensorManager::SENSOR_NAME_LEN || !SensorManager::setSensorName(idx, name))
        {
            client.print("HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
            return;
        }
        DEBUG_LOG("[INFO] sensor name on index %d is set to \"%s\"\n", idx, name);
        client.print("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 3\r\n\r\n{}\n");
    }
    void handleSetValueOffset(WiFiClient &client, uint8_t idx)
    {
        StaticJsonDocument<BODY_SIZE> doc;
        DeserializationError err = deserializeJson(doc, client);
        const float value = doc["value"];
        if (err || doc["value"].isNull() || isnanf(value) || !SensorManager::setValueOffset(idx, value))
        {
            client.print("HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
            return;
        }
        DEBUG_LOG("[INFO] sensor offset on index %d is set to \"%f\"\n", idx, value);
        client.print("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 3\r\n\r\n{}\n");
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
                // No more data -> give up on the dropped/stalled connection.
                client.stop();
                return false;
            }
        }
        // Sized from DigestAuth's own limits, not chosen independently — it
        // hashes method+path into HA2, a mismatch here would silently
        // truncate that input and break auth for no visible reason.
        char method[DigestAuth::MAX_METHOD_LEN + 1] = {};
        char path[DigestAuth::MAX_URI_LEN + 1] = {};
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
        DEBUG_LOG("[INFO] %s %s | ha1: %s\n", method, path, ownPassword ? "own" : "default");
        if (!DigestAuth::verify(requestHeader, method, path, deviceHa1))
        {
            DEBUG_LOG("[WARN] auth failed\n");
            char wwwAuth[160] = {};
            DigestAuth::buildWwwAuthenticate(wwwAuth, sizeof(wwwAuth));
            client.printf("HTTP/1.1 401 Unauthorized\r\nWWW-Authenticate: %s\r\nConnection: close\r\nContent-Length: 0\r\n\r\n", wwwAuth);
            client.stop();
            return true;
        }

        uint8_t sensorIdx = 0;
        // Exact matches against the already-parsed method/path (not strstr on
        // the raw header) -- a prefix match would let e.g. "GET /statusXYZ"
        // hit the /status handler too. sscanf-based routes below are exact
        // by construction (%hhu only matches a path that's actually numeric).
        if (strcmp(method, "GET") == 0 && strcmp(path, "/") == 0)
        {
            handleRoot(client);
        }
        else if (strcmp(method, "GET") == 0 && strcmp(path, "/sensors") == 0)
        {
            printSensorInfo(client);
        }
        else if (strcmp(method, "GET") == 0 && strcmp(path, "/status") == 0)
        {
            printStatus(client);
        }
        else if (strcmp(method, "GET") == 0 && strcmp(path, "/calibrationinfo") == 0)
        {
            printCalibrationInfo(client);
        }
        else if (strcmp(method, "POST") == 0 && strcmp(path, "/provision/wifi") == 0)
        {
            handleProvisionWifi(client);
        }
        else if (sscanf(requestHeader, "POST /calibrate/%hhu", &sensorIdx) == 1)
        {
            handleCalibrate(client, sensorIdx);
        }
        else if (sscanf(requestHeader, "POST /reset/%hhu", &sensorIdx) == 1)
        {
            handleReset(client, sensorIdx);
        }
        else if (sscanf(requestHeader, "POST /rename/%hhu", &sensorIdx) == 1)
        {
            handleSetSensorName(client, sensorIdx);
        }
        else if (sscanf(requestHeader, "POST /valueoffset/%hhu", &sensorIdx) == 1)
        {
            handleSetValueOffset(client, sensorIdx);
        }
        else if (strcmp(method, "POST") == 0 && strcmp(path, "/factoryreset") == 0)
        {
            handleFactoryReset(client);
        }
        else if (strcmp(method, "POST") == 0 && strcmp(path, "/provision/finish") == 0)
        {
            handleProvisionFinish(client);
        }
        else if (strcmp(method, "POST") == 0 && strcmp(path, "/provision/wifi/reset") == 0)
        {
            handleWifiReset(client);
        }
        else if (strcmp(method, "POST") == 0 && strcmp(path, "/provision/password") == 0)
        {
            handleProvisionPassword(client);
        }
        else if (strcmp(method, "POST") == 0 && strcmp(path, "/provision/name") == 0)
        {
            handleProvisionName(client);
        }
        else
        {
            client.print("HTTP/1.1 404 Not Found\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
        }
        client.stop();
        return true;
    }
}
