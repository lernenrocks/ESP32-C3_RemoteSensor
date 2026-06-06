#pragma once

namespace HttpServer{
    /**
     * @brief starts server connection
     * @note call correspondes with Wifi.begin()
     */
    void begin();
    /**
     * @brief disconnects server, e.g. after Wifi connection lost
     */
    void end();
    /**
     * @brief handles incomming requests
     * @return true if a client was serviced, false if no connection was pending
     */
    bool handle();
}
