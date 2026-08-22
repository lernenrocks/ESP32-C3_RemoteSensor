#pragma once
#include "HX711.h"
#include "SensorBase.h"

class HX711Sensor : public SensorBase
{
public:
    HX711Sensor(int dout, int sck, uint8_t pId);
    ~HX711Sensor() = default;
    bool getCalibrationJson(char *buffer, size_t len) override;
    bool getCalibrationValuesJson(char *buffer, size_t len) override;
    int getPrecision() override;
    const char* getUnit() override;
    bool calibrate(const JsonObjectConst data) override;
    void reset() override;

private:
    HX711 _scale;
    bool isValid() override;
    void readRaw(float &buffer) override;
};
