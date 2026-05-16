#pragma once
#include <Arduino.h>

namespace SensorManager{

    constexpr uint8_t MAX_SENSORS = 4;

    /** 
     * @brief reads sensors and provides the data als Json
     * @param sensorDataJson buffer for data Json
     * @param length of the buffer
     * @return true, if data was written correctly
     */
    bool getSensorDataJson(char sensorDataJson[], size_t len);

    /**
     * @brief initializes all sensors.
     */
    void initSensors();
}