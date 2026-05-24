#pragma once
#include <Arduino.h>
#include "SensorBase.h"

namespace SensorType {
    constexpr const char* HX711      = "HX711";
    constexpr const char* DHT22_TEMP = "DHT22_TEMP";
    constexpr const char* DHT22_HUM  = "DHT22_HUMIDITY";
}

namespace SensorManager {

    constexpr uint8_t MAX_SENSORS = 4;

    struct SensorEntry {
        SensorBase* sensor;
        const char* type;
    };

    bool getSensorDataJson(char sensorDataJson[], size_t len);
    bool getCalibrationInfoJson(char buf[], size_t len);
    void initSensors();
    bool calibrateSensor(uint8_t idx, JsonObjectConst data);
    bool resetSensor(uint8_t idx);
}