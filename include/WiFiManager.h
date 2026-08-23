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
 * @note Debug heartbeat: only call in the DISABLE_LIGHT_SLEEP build; the
 *       USB console dies during Light Sleep anyway.
 */
void heartbeat();
}



