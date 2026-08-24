#pragma once
#include <Arduino.h>
#include "SensorBase.h"



namespace SensorManager {


    constexpr size_t SENSOR_NAME_LEN = 32;

    bool getSensorName(uint8_t idx, char *buffer, size_t len);
    bool setSensorName(uint8_t idx, const char *name);
    bool setValueOffset(uint8_t idx, float value);
    bool getSensorDataJson(char sensorDataJson[], size_t len);
    bool getCalibrationInfoJson(char buf[], size_t len);
    void initSensors();
    bool calibrateSensor(uint8_t idx, JsonObjectConst data);
    bool resetSensor(uint8_t idx);
}
