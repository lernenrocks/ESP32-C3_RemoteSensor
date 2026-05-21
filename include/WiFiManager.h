# pragma once

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
}



