#pragma once

namespace WiFiManager{
/**
 * @brief initializes or reconnects Wifi
 */
void initWifi();

/**
 * @brief checks, if WiFi is connected
 * @return true if wifi is connected
 */
bool isConnected();

/**
 * @brief prints connection state (IP/RSSI or DISCONNECTED) — self-throttled.
 * @note Debug-Heartbeat: nur im DISABLE_LIGHT_SLEEP-Build aufrufen; im
 *       Sleep-Build stirbt die USB-Konsole ohnehin.
 */
void heartbeat();
}



