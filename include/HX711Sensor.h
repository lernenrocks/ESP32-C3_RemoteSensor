#pragma once
#include "HX711.h"
#include "SensorBase.h"

class HX711Sensor : public SensorBase{
public:
    HX711Sensor (uint8_t dout, uint8_t sck,uint8_t id);
    ~HX711Sensor() =default;

private:
HX711 _scale;
bool isValid() override;
void readRaw(float &buffer) override;
};